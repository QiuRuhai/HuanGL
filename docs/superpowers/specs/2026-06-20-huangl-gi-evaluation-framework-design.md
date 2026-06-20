# GI Evaluation Framework — Design Spec

**Date:** 2026-06-20
**Status:** Approved (design)

## Purpose

The project's stated value is A/B comparison of global-illumination
techniques. Today that comparison has no *ground truth*: RSM, SSGI, and DDGI
would only be comparable to each other, not to a correct reference. This spec
builds the evaluation framework that turns the renderer into a GI testbed —
an in-engine reference path tracer, a quantitative error-metric module, and a
comparison harness — so that every later GI technique can be measured against
a converged path-traced image with numbers (RMSE/MAPE) and per-pixel error
heatmaps.

This is the **first** of several sub-projects. It deliberately ships **no new
GI technique**. Its first "contestant" against ground truth is the *current
renderer itself* (direct sun + IBL ambient, no interreflection): comparing it
to the reference immediately exercises the full loop and reveals the missing
indirect light as error. RSM, SSGI, and DDGI each become their own later spec
that plugs a new contestant into this framework.

## Goals

1. **In-engine reference path tracer** — a compute path tracer that shares the
   scene, camera, BRDF, sun, and environment with the realtime renderer and
   progressively accumulates a converged HDR reference.
2. **Quantitative error metrics** — per-pixel error image plus aggregate scalar
   metrics (RMSE, MAPE) computed in the meaningful linear-HDR space.
3. **Comparison harness** — a DebugUI panel and a comparison stage to switch
   between realtime / reference / split / error-heatmap views, show sample
   count and metrics, and reset accumulation.
4. **Validation scene** — a Cornell-box-style primitive scene with constant
   colored materials, the canonical GI test where color bleeding is visible.
5. **CPU geometry retention** — extend the loader so triangles are available
   on the CPU for BVH construction.

## Non-Goals

- Any GI technique (RSM/SSGI/DDGI are later specs).
- Textured albedo inside the path tracer (factors only in v1; the validation
  scene is untextured by design).
- Glossy/specular *indirect* bounces (direct specular is evaluated; indirect
  is diffuse-only — matching what RSM/SSGI/DDGI target).
- FLIP perceptual metric (documented stretch; v1 ships RMSE + MAPE + a simple
  perceptual sRGB delta).
- SAH BVH (v1 uses median-split; SAH is a stretch).
- Animated/dynamic content and motion vectors (still out of scope project-wide).

## Architecture

The framework attaches as a **tail bypass** to the existing six-stage
pipeline; the main pipeline is unchanged. Three new units, all respecting the
established `IPipelineStage` + `PipelineResources` + "render produces data, UI
reads state" conventions:

| Unit | Type | Responsibility |
|------|------|----------------|
| `PathTracerScene` | non-stage component | CPU geometry → world-space triangles → BVH → SSBO upload; material factors SSBO |
| `PathTracerStage` | `IPipelineStage` | compute path tracer; progressive accumulation into RGBA32F; writes `ReferenceOutputs` |
| `ComparisonStage` | `IPipelineStage` | reads realtime HDR + reference; computes error image + scalars; outputs backbuffer per view mode |

`PathTracerStage` and `ComparisonStage` run after `PostProcessStage`. When the
path tracer is disabled (default), both stages early-out at ~zero cost and the
backbuffer shows the normal realtime image.

### Data flow

```
(existing pipeline) … → PostProcessStage → backbuffer (realtime)
                                   │
RenderSceneView ─► PathTracerScene (BVH + SSBOs, rebuilt on scene change)
                                   │
                                   ▼
              PathTracerStage (compute, +N spp/frame → accumulation RGBA32F)
                                   │  writes ReferenceOutputs
                                   ▼
              ComparisonStage (error compute + reduction)
                   │                         │
                   ▼                         ▼
            backbuffer (per view mode)   ApplicationState (metrics, spp)
                                             │
                                             ▼
                                   DebugUI "GI Comparison" panel
```

## Components

### 1. PathTracerScene (new)

**Files:** `src/pipeline/PathTracerScene.h/cpp`

Builds GPU acceleration data from a `RenderSceneView`.

- **CPU geometry source.** `Mesh` (Schema.h) holds only GL handles. Extend
  `MeshLoader::LoadResult` with a CPU geometry record (positions, normals, uv,
  indices) retained alongside the GPU `Mesh`. Primitive scene builders
  (Cornell) populate it directly.
- **Build.** For each `Renderable`, transform its triangles to world space by
  `modelMatrix`, concatenate across the scene, build a **median-split BVH**,
  and flatten to GPU arrays: node array (AABB + child/leaf indices), triangle
  array (3 positions + 3 normals or a vertex-indexed layout), and a per-
  triangle material index.
- **Materials.** A material SSBO of `{ baseColorFactor, metallic, roughness }`
  per material. Textured albedo is **not** sampled in v1.
- **Upload.** Use `Buffer(GL_SHADER_STORAGE_BUFFER)` + `BindBase` to bind nodes,
  triangles, and materials to fixed SSBO binding points.
- **Lifetime.** Rebuilt when the active scene changes; static within a scene.

### 2. PathTracerStage (new)

**Files:** `src/pipeline/stages/PathTracerStage.h/cpp`,
`shader/pathtracer/pathtrace.comp`

A unidirectional compute path tracer matching the realtime renderer.

- **Camera.** Primary rays from `FrameContext.camera` via inverse view-proj —
  the same reconstruction math LightingStage uses from depth. No TAA jitter.
- **BRDF.** Same Cook-Torrance (Lambert diffuse + GGX specular) as the lighting
  shader, sharing the metallic-roughness parameterization.
- **Direct lighting.** Next-event estimation toward the single
  `DirectionalLight` (sun) with a BVH shadow ray; same direction/color/
  intensity as realtime.
- **Environment.** On ray miss, sample the **same HDR equirectangular env** the
  IBL is baked from; env also contributes to indirect via BRDF/cosine sampling.
- **Indirect.** Multi-bounce **diffuse only**, cosine-weighted hemisphere
  sampling, Russian roulette after a few bounces. Glossy indirect not traced.
- **Accumulation.** Each frame adds N spp into an RGBA32F accumulation image
  (image load/store); display divides by accumulated sample count. Per-pixel
  RNG seeded by pixel + sample/frame index.
- **Reset.** Accumulation clears on camera move, scene change, or relevant
  settings change — driven through `InvalidateHistory()` / `freezeCamera`
  (FrameContext). Freezing the camera lets the reference converge noise-free.
- **Output.** Writes `ReferenceOutputs { Texture hdr; uint32_t sampleCount; }`
  into `PipelineResources`.
- **Disabled path.** When the PT toggle is off, the stage early-outs without
  dispatching.

### 3. ComparisonStage (new)

**Files:** `src/pipeline/stages/ComparisonStage.h/cpp`,
`shader/comparison/error.comp`, `shader/comparison/composite.frag`

- **Error compute.** Reads the realtime resolved HDR (the TAA output when TAA
  is enabled, otherwise the LightingStage HDR output — pre-tone-map in both
  cases) and the reference HDR; computes a per-pixel error image in **linear
  HDR space**. Both images are anti-aliased (TAA on the realtime side, many
  spp on the reference side), so the comparison is fair despite TAA jitter.
- **Metrics.** RMSE (linear), MAPE (relative), and a simple perceptual delta on
  tone-mapped sRGB. Aggregate via glReadPixels + CPU reduction (sufficient for a
  debug tool) into `ApplicationState`.
- **View modes.** `Realtime | Reference | Split | ErrorHeatmap`. Split shows
  realtime left / reference right. ErrorHeatmap maps per-pixel error through a
  color ramp with a UI-controlled scale.
- **Output.** Writes the selected view to the backbuffer. When the PT is off,
  outputs realtime unchanged.

### 4. DebugUI panel

**Files:** `src/ui/DebugUI.h/cpp`, `src/app/ApplicationState.h`,
`src/renderer/FrameContext.h`

- New "GI Comparison" collapsing header: PT enable toggle, view-mode selector,
  `accumulating… N spp` readout, **Reset** button, RMSE/MAPE readouts, and an
  error-heatmap color-scale slider.
- New fields live in `RenderSettings`/`DebugSettings` (FrameContext) for the
  toggle, view mode, and scale; metrics + spp live in `ApplicationState`
  alongside existing frame stats. UI mutates state; stages read frame
  contracts — no new ownership model.

### 5. Cornell validation scene (new)

**Files:** `src/scene/CornellScene.h/cpp`

A primitive Cornell box (colored walls, a couple of boxes), constant-color
materials (factors only, no textures), registered like existing scenes. This
is the canonical diffuse-GI test where color bleeding is the headline result,
and it makes the v1 "factors-only" PT material model sufficient.

### 6. Loader / schema changes

**Files:** `src/resource/MeshLoader.h/cpp`, `src/renderer/Schema.h`

- Add a CPU geometry record to `LoadResult` (and a place for primitive builders
  to fill it) without changing the realtime `Mesh` GPU path.

## Testing / Verification

No unit-test framework exists; verification is compile + visual/numeric runtime
check.

1. **Build clean** (MSVC Debug).
2. **Convergence.** On Cornell, freezing the camera drives the reference to a
   noise-free image as spp accumulates; the spp readout climbs.
3. **Energy sanity (white-furnace-style).** With a uniform environment and the
   sun disabled, diffuse surfaces converge toward the environment color (no
   energy gain/loss beyond albedo).
4. **Error reveals missing GI.** With the current renderer as the contestant,
   the error heatmap shows elevated error exactly where interreflection is
   missing; Cornell corners show color bleeding in the *reference* that the
   realtime image lacks.
5. **Split alignment.** Split view is pixel-aligned between realtime and
   reference.
6. **Reset.** Camera motion or scene switch resets accumulation (spp returns to
   a low count); freezing re-converges.
7. **Zero-cost when off.** Disabling the PT returns the backbuffer to the normal
   realtime image and the new stages add no measurable GPU time (verify via the
   existing GPU Timing panel).
8. **No regressions.** Existing scenes, debug views, tone-map modes, Bloom, TAA,
   and resize all behave as before.

## Risks

- **BVH/path-tracer correctness.** A subtle BVH traversal or shadow-ray bug
  silently biases the "ground truth." Mitigation: the white-furnace and
  Cornell-convergence checks above; start with median-split (simple to verify)
  before any SAH optimization.
- **Material/light misalignment.** If the PT BRDF or sun/env handling diverges
  from the realtime shader, error reflects the mismatch, not GI. Mitigation:
  share the exact BRDF math and the same env texture; validate on an untextured
  scene first.
- **Accumulation buffer lifetime vs resize.** Resizing mid-accumulation must
  reallocate and reset. Mitigation: route through the same Resize/Invalidate
  hooks the other stages use.
- **Scope creep into a GI technique.** This spec ships the framework only.
  Resist adding RSM/SSGI/DDGI here.

## Roadmap Position

This sub-project sits between Phase 4.6 and Phase 5. The roadmap's Phase 5
(RSM), Phase 6 (SSGI), and Phase 8 (DDGI) are reframed: each plugs a contestant
into this framework and is validated against its reference, rather than being
compared only to each other. The roadmap table in `docs/architecture.md` will
be updated when this lands.
