# Phase 4.5: GI Foundations — Design Spec

**Date:** 2026-06-01
**Status:** Approved (design)

## Purpose

Before adding global-illumination techniques (RSM, SSGI, VXGI, DDGI), close
the load-bearing correctness and measurement gaps that every GI technique
depends on. GI is extremely sensitive to correct normals and linear-space
lighting; building it on subtly-wrong inputs means debugging GI artifacts
whose root cause is upstream. And the project's stated value — A/B comparison
of GI techniques — requires per-stage GPU timing that does not exist yet.

This phase is deliberately narrow. It is the first half of a two-part
foundation:

- **Phase 4.5 (this spec):** correctness fixes + GPU profiling.
- **Phase 4.6 (future):** scalability + showcase — frustum culling, draw
  batching, a committed showcase scene and asset-loading story.

Motion vectors / a velocity buffer are **explicitly out of scope**. All
current scenes are static; the existing TAA depth-reprojection (depth +
previous/current view-projection) is already correct for static geometry
under camera motion. A dedicated velocity buffer only pays off with
animated or moving content, so it is deferred until such content exists.

## Goals

1. **Correct surface normals under non-uniform scaling** — fix the normal
   matrix in the GBuffer vertex shader.
2. **Correct, non-duplicated texture caching** — make the resource cache key
   include the sRGB flag so linear textures (normal/roughness/metallic) are
   cached instead of bypassing the cache.
3. **Per-stage GPU timing** — measure each pipeline stage's GPU cost without
   stalling the pipeline, and display it in the debug UI.
4. **Roadmap + convention update** — record Phase 4.5/4.6 in the roadmap,
   remove the two now-fixed entries from Known Limitations, and make the
   "teaching comments are encouraged" convention explicit.

## Non-Goals

- Motion vectors / velocity buffer (deferred until dynamic content exists).
- Frustum culling, draw batching (Phase 4.6).
- A committed showcase scene or asset pipeline (Phase 4.6).
- Any GI technique (Phase 5+).
- CPU-side profiling (only GPU stage timing is in scope).

## Components

### 1. GpuProfiler (new)

**Files:** `src/renderer/GpuProfiler.h`, `src/renderer/GpuProfiler.cpp`

A small helper that measures GPU time per named stage using `GL_TIMESTAMP`
query objects, double/triple-buffered across frames so results are read back
without forcing a CPU↔GPU sync.

**Approach (chosen):** `GL_TIMESTAMP` timestamp queries in a ring buffer.
Rejected alternatives: `GL_TIME_ELAPSED` begin/end (cannot nest, same-frame
readback stalls); CPU `glFinish` timing (serializes CPU/GPU, measurements
are meaningless).

**Why this fits:** `RenderPipeline::Execute` already wraps each stage with
`Renderer::PushDebugGroup` / `PopDebugGroup`. The profiler brackets the same
spot with `BeginStage` / `EndStage`, so **no stage's own code changes**.

**Public interface:**

```cpp
namespace HuanGL {

struct StageTiming {
    std::string name;
    double ms = 0.0;
};

class GpuProfiler {
public:
    void BeginFrame();
    void BeginStage(const char* name);
    void EndStage();
    void EndFrame();
    // Results for the most recently completed (resolved) frame.
    const std::vector<StageTiming>& GetResults() const;

private:
    static constexpr int kFrameDepth = 3; // ring depth to avoid GPU stalls
    // ... per-frame timestamp query pairs, ring index, resolved results
};

} // namespace HuanGL
```

**Behavior:**
- `BeginFrame` selects the current ring slot and, if that slot holds queries
  from `kFrameDepth` frames ago, reads them back (guaranteed available, no
  stall) and computes each stage's elapsed ms into the resolved results.
- `BeginStage(name)` / `EndStage()` issue `glQueryCounter(GL_TIMESTAMP)` into
  the current slot, recording the stage name.
- Query objects are created lazily/grown as the number of stages is learned;
  the stage list is stable across frames so this stabilizes after frame 1.
- Before `kFrameDepth` frames have elapsed, `GetResults()` returns an empty
  or zeroed list — the UI shows "warming up" / dashes.

**Dependencies:** GL (`glad`) + `Renderer` only. No window, no scene types.

### 2. RenderPipeline integration

**Files:** `src/pipeline/RenderPipeline.h`, `src/pipeline/RenderPipeline.cpp`

- `RenderPipeline` owns a `GpuProfiler profiler_`.
- `Execute` wraps the existing loop:

```cpp
profiler_.BeginFrame();
for (auto& stage : stages_) {
    Renderer::PushDebugGroup(stage->GetName());
    profiler_.BeginStage(stage->GetName());
    stage->Execute(resources_, frame);
    profiler_.EndStage();
    Renderer::PopDebugGroup();
}
profiler_.EndFrame();
```

- Expose results to the UI. `RenderPipeline` gains a
  `const std::vector<StageTiming>& GetStageTimings() const` accessor that
  returns `profiler_.GetResults()`. After calling `pipeline.Execute(...)`,
  `App` copies the timings into `ApplicationState` (alongside the existing
  frame stats); `DebugUI` reads them from `ApplicationState`. This follows
  the established "render produces data, UI reads state" convention — no new
  ownership model is introduced.

### 3. DebugUI display

**Files:** `src/ui/DebugUI.h/cpp`

- Add a "GPU Timing" collapsing header.
- Render a two-column table: stage name → milliseconds (3 decimal places),
  plus a final row summing all stages ("Frame GPU total").
- If results are empty (warming up), show a single "measuring…" line.

### 4. Normal matrix fix

**Files:** `shader/gbuffer/gbuffer.vert`

- Replace `mat3(model)` (used to transform the normal/tangent into world
  space) with `mat3(transpose(inverse(model)))`.
- Add a teaching comment: per-vertex inverse is wasteful; production code
  computes a `normalMatrix` on the CPU once per draw and uploads it. Inlined
  here for learning clarity, and because current vertex counts are small.
- CSM (`shader/shadow/csm.vert`) is unaffected — it uses `model` only to
  transform position, never normals.

### 5. sRGB cache key fix

**Files:** `src/resource/ResourceManager.h/cpp`

- The texture cache currently keys on path alone, and linear loads bypass the
  cache. Change the cache key to the pair `(path, sRGB)` so both color and
  linear textures are cached, and a path loaded under two different sRGB flags
  yields two distinct cache entries.
- Keep the existing `weak_ptr` + GC semantics; only the key type changes.

### 6. Documentation + convention

**Files:** `docs/architecture.md`, `AGENTS.md`

- Roadmap: insert
  - `4.5 — GI foundations: correctness + GPU profiling`
  - `4.6 — Scalability + showcase: frustum culling, draw batching, showcase scene`
  - Renumber nothing else; Phase 5 (RSM) and beyond keep their numbers and
    now depend on 4.5/4.6.
- Add brief Phase 4.5 and 4.6 sections (goal / deliverables / depends-on /
  risk) in the same style as existing phases.
- Known Limitations: remove #1 (`mat3(model)` normal matrix) and #5
  (sRGB cache). Keep the rest. Add a forward note that culling/batching
  (current #3/#4) are scheduled for Phase 4.6.
- `AGENTS.md` conventions: add a line making the teaching-comment policy
  explicit — *"This is a learning project: comments that explain WHY and the
  reasoning behind a rendering technique or algorithm are considered an asset.
  Prefer clear teaching comments over terse minimalism."* This deliberately
  supersedes the earlier implicit "minimal comments" assumption.

## Data Flow

```
RenderPipeline::Execute
  profiler.BeginFrame()                         ← resolve frame N-3 queries
    for each stage:
      PushDebugGroup → profiler.BeginStage       ← glQueryCounter(TIMESTAMP)
        stage->Execute(...)
      profiler.EndStage → PopDebugGroup          ← glQueryCounter(TIMESTAMP)
  profiler.EndFrame()
        │
        ▼
  RenderPipeline::GetStageTimings()  →  App  →  ApplicationState frame stats
        │
        ▼
  DebugUI "GPU Timing" table
```

## Testing / Verification

No unit-test framework exists; verification is compile + visual runtime check.

1. **Build clean** (MSVC Debug).
2. **GPU timing sane:** DebugUI "GPU Timing" shows nonzero per-stage ms after
   a few frames; totals roughly track frame complexity (Sponza > TestScene);
   toggling Bloom/TAA off makes those rows drop to ~0 / disappear.
3. **sRGB cache:** load a scene whose materials share a linear texture; verify
   (via a temporary log or the existing resource accounting) the texture is
   loaded once, not per-material.
4. **Normal matrix:** add a temporary entity with a non-uniform scale (e.g.
   `{2,1,1}`); confirm its lighting/normals look correct (no over/under-bright
   stretching) in the normal debug view. Remove the temporary entity after.
5. **No regressions:** existing scenes, debug views, tone-map modes, Bloom,
   TAA, and resize all still behave as before.

## Risks

- **Timestamp query portability:** `GL_TIMESTAMP` is core in 4.6; safe.
- **Ring-buffer off-by-one:** reading a slot before its queries are available
  would stall or assert. Mitigation: only read slots that are exactly
  `kFrameDepth` frames old; return empty until warmed up.
- **Scope creep into Phase 4.6:** resist adding culling/batching/scene work
  here. They are explicitly deferred.
