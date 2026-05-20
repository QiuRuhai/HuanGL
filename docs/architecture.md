# HuanGL Architecture & Roadmap

This document is the durable reference for HuanGL: how the renderer is
put together today, and where it is going. It complements `README.md`
(first-impression overview) and `AGENTS.md` (file-level inventory).

## Vision and Non-Goals

**Vision.** HuanGL demonstrates a modern OpenGL 4.6 deferred PBR renderer
written from scratch on top of GLAD2 and Direct State Access, and
progressively implements four global-illumination techniques (RSM, SSGI,
VXGI, DDGI). The terminal state is Phase 8 complete plus a polished
demonstration scene, with documentation good enough that a reader can
understand the architecture and rationale without reading source.

**Non-goals.** HuanGL is not a production engine. It deliberately omits
an RHI abstraction layer, asset editors, scripting, networking, and
multi-window support. The project optimizes for clarity of demonstration
over engine-grade flexibility.

## Current Capability

As of Phase 2.5 the renderer can load glTF (external or `.glb` embedded),
OBJ, and FBX assets through Assimp; render them through a deferred PBR
pipeline with cascaded shadow maps, image-based lighting, and a
selectable tone-map operator; and switch between registered scenes at
runtime. Seven debug visualization modes inspect the GBuffer channels,
linear depth, and shadow cascades.

Runtime controls:

| Input | Action |
|-------|--------|
| `W` `A` `S` `D` | Camera translation |
| `Mouse` | Camera look |
| `N` | Cycle registered scenes |
| `T` | Cycle tone-map operator (ACES / Reinhard / linear) |
| `0` | Final composite |
| `1` | Albedo channel |
| `2` | World-space normal |
| `3` | Roughness |
| `4` | Metallic |
| `5` | Linear depth |
| `6` | Shadow cascade overlay |
| `Esc` | Quit |

## Render Pipeline

Each frame the active `World` is adapted into a read-only
`RenderSceneView`, and per-frame camera/settings/time data are packaged
into a `FrameContext`. `RenderPipeline` updates shared UBOs from those
contracts, then runs four passes in fixed order. Pass inputs and outputs
use the formats below:

```
World → RenderSceneView + FrameContext
        │
        ├─► ShadowPass     → ShadowOutputs
        │                    sampler2DArrayShadow (2048² × 4 cascades, D32)
        │
        ├─► GBufferPass    → GBufferOutputs
        │                    RT0 RGBA8   (albedo.rgb,  metallic.a)
        │                    RT1 RGBA16F (normal.rgb,  roughness.a)
        │                    Depth D24
        │
        └─► LightingPass   ← reads GBufferOutputs + ShadowOutputs + IBL
             │               writes LightingOutputs (RGBA16F HDR target)
             ▼
        PostProcessPass    ← reads Lighting/GBuffer/Shadow outputs
             │               tone map + gamma + debug overlay
             ▼
        Backbuffer
```

ShadowPass and GBufferPass are independent and could be reordered; the
current order matches frame-time profiling on typical scenes (shadow
first because GBuffer's depth is unused by the lighting pass — it
reconstructs world position from depth, not from a positions GBuffer).

IBL textures (diffuse irradiance cubemap, prefiltered specular cubemap,
BRDF LUT) are generated once in `LightingPass::Init` from an HDR
equirectangular environment, and reused every frame.

`PipelineOutputs` is the named handoff point for pass resources. The
passes still own their textures and framebuffers; the output structs are
lightweight handles for downstream passes and future debug tooling.

## Module Map

| Subdirectory | Responsibility | Key types |
|--------------|----------------|-----------|
| `src/core/` | Window, input, app loop, camera | `App`, `Window`, `Input`, `Camera` |
| `src/app/` | Runtime state, scene registry, input command mapping | `ApplicationState`, `SceneRegistry`, `InputController` |
| `src/renderer/` | OpenGL RAII wrappers and shared schemas | `Shader`, `Buffer`, `Texture`, `Framebuffer`, `Material`, `Mesh` |
| `src/pipeline/` | Render passes and per-frame orchestration | `RenderPipeline`, `ShadowPass`, `GBufferPass`, `LightingPass`, `PostProcessPass` |
| `src/resource/` | Asset loading and caching | `ResourceManager`, `MeshLoader` |
| `src/scene/` | Lightweight world/entities and demo scene builders | `World`, `Entity`, `TestScene`, `ModelScene` |
| `src/ui/` | ImGui lifecycle and debug panels | `ImGuiLayer`, `DebugUI` |

File-level inventory lives in `AGENTS.md`; this table intentionally
stops at the subdirectory level so it does not have to be touched on
routine file additions.

## Key Design Decisions

Each decision lists what was chosen, why, and the trade-off accepted.

**Deferred shading with depth-reconstructed world position.** GBuffer
stores albedo+metallic and normal+roughness only; LightingPass
reconstructs world position from depth and the inverse view-projection
matrix. Saves one RGBA16F render target per frame at the cost of one
extra matrix multiply per fragment in the lighting pass.

**`sampler2DArrayShadow` for CSM with hardware comparison.** All four
cascades live in a single `GL_TEXTURE_2D_ARRAY` and the shader compares
in hardware via `texture(shadowMap, vec4(uv, layer, refDepth))`. Cascade
selection uses view-space Z, which avoids pushing screen-edge fragments
to a higher cascade than necessary.

**Direct State Access throughout.** `glCreate*`, `glNamed*`, and
`glBindTextureUnit` replace the bind-modify-unbind dance. Requires
OpenGL 4.5+; HuanGL targets 4.6.

**No RHI abstraction.** OpenGL is the demonstrated interface. Adding a
thin abstraction would dilute the educational purpose and slow
iteration. The cost is that porting to Vulkan or D3D12 would be a
rewrite, which is acceptable for a learning project.

**Tone mapping in a separate pass.** LightingPass writes raw HDR
radiance to an RGBA16F target; PostProcessPass applies the tone map and
gamma. Keeps the HDR signal available for future passes (Bloom, TAA).

**UI edits state, passes read frame contracts.** ImGui controls mutate
`ApplicationState` and the active `World`; render passes read
`FrameContext` and `RenderSceneView`. This keeps debug tooling from
depending on pass internals and makes future pass settings explicit.

**Tangent-space normal mapping with fragment-side Gram-Schmidt.** The
vertex shader passes raw (un-normalized, un-orthogonalized) tangent;
the fragment shader normalizes the interpolated N, re-orthogonalizes T
against N, then derives B = cross(N, T). Vertex-side orthogonalization
is wasted work because interpolation destroys it.

**Embedded textures via `*N` indices.** When Assimp returns a texture
path starting with `*`, the digits index into `aiScene::mTextures`. The
loader decodes those bytes from memory using stb_image, which is the
only correct way to load textures from `.glb` files.

**Soft-failure scene loading.** `App::RegisterScene` catches exceptions
from individual scene `Init` calls so missing or broken assets do not
prevent the app from starting with the remaining registered scenes.

## Roadmap

| Phase | Status | Theme |
|-------|--------|-------|
| 1 | ✅ Complete | Foundation (GLAD2, RAII wrappers, App loop) |
| 2 | ✅ Complete | Deferred render pipeline (GBuffer, CSM, PBR+IBL) |
| 2.5 | ✅ Complete | Pipeline polish (PostProcess, glTF materials, debug views) |
| 3 | ✅ Minimum Complete | Application state, lightweight World, ImGui debug UI |
| 4 | Planned | Bloom, TAA, improved tone mapping |
| 5 | Planned | RSM |
| 6 | Planned | SSGI |
| 7 | Planned | VXGI |
| 8 | Planned | DDGI |

For each planned phase the entries below list the goal, the deliverables
expected to land in source, the prior phases the work depends on, and
the largest known risk. Plans for completed phases live under
`docs/superpowers/plans/`.

### Phase 3 — Scene System and ImGui Debug UI

**Goal.** Replace the keyboard-only debug controls with a real
inspectable UI, and introduce enough world/entity structure that scenes,
transforms, lights, and render settings can be edited without coupling
ImGui to render pass internals.

**Deliverables.**
- `src/ui/ImGuiLayer.h/cpp` wrapping Dear ImGui setup, frame begin/end,
  and the GLFW + OpenGL3 backends.
- `src/ui/DebugUI.h/cpp` exposing tone-map operator, debug mode, ambient
  strength, sun direction/color/intensity, camera FOV, frame stats, and
  basic entity transform controls.
- `src/app/ApplicationState.h`, `SceneRegistry`, and `InputController`
  so `App` stays focused on lifecycle and frame scheduling.
- `src/scene/World.h/cpp` with lightweight `Entity`, `Transform`, and
  `MeshRenderer` data.
- `FrameContext`, `RenderSceneView`, and `PipelineOutputs` contracts so
  the fixed render pipeline has explicit inputs and resource handoffs.
- Optional future ImGuizmo integration for manipulating selected
  entities.

**Depends on.** Phase 2.5 (PostProcessPass exposes the runtime knobs the
UI will drive).

**Independent of.** Phase 4.

**Risk.** Letting the debug UI become an editor framework. Mitigation:
keep `World` simple, avoid a full ECS, and defer serialization, undo/redo,
asset browsing, and ImGuizmo until the rendering roadmap needs them.

### Phase 4 — Bloom, TAA, Improved Tone Mapping

**Goal.** First real post-processing chain on top of the HDR target.

**Deliverables.**
- Bright-pass extract + separable Gaussian blur (or Kawase) producing a
  half-resolution bloom buffer that is added back to the HDR target
  pre-tone-map.
- Temporal Anti-Aliasing with a history buffer and jittered projection
  matrix; resolves to the same RGBA16F target.
- Additional tone-map operators (Uncharted 2 filmic, AgX) selectable
  alongside ACES and Reinhard.

**Depends on.** Phase 2.5 (HDR target, PostProcess pass).

**Independent of.** Phase 3.

**Risk.** TAA history invalidation on scene swap or window resize.
Mitigation: clear the history buffer on resize and on `N`-key press.

### Phase 5 — Reflective Shadow Maps (RSM)

**Goal.** First indirect-lighting technique. Treats the directional
shadow map as a flux+normal source for a low-frequency one-bounce
indirect term.

**Deliverables.**
- ShadowPass extension or sibling pass that also writes flux and
  view-space normal to two extra render targets at shadow-map
  resolution.
- A gather pass that samples N virtual point lights per fragment and
  accumulates indirect irradiance.
- Optional importance sampling and screen-space denoise.

**Depends on.** Phase 4 (TAA helps mask sample noise).

**Risk.** Sample budget: 64–256 VPLs per fragment can be expensive at
shadow-map resolutions of 2048².

### Phase 6 — Screen-Space Global Illumination (SSGI)

**Goal.** Add a second indirect-lighting technique on the PostProcess
chain. Different cost/quality trade-off from RSM; the project will end
up with both for A/B comparison.

**Deliverables.**
- Hi-Z buffer construction over the GBuffer depth.
- Ray-marched indirect diffuse + glossy reflections sampled from the
  HDR target.
- Temporal accumulation reusing the Phase 4 history infrastructure.

**Depends on.** Phase 4 (PostProcess chain, TAA history).

**Independent of.** Phase 5.

**Risk.** Disocclusion artifacts on fast camera motion.

### Phase 7 — Voxel-based Global Illumination (VXGI)

**Goal.** Scene voxelization into a 3D texture, then cone-tracing for
indirect diffuse and specular.

**Deliverables.**
- Voxelization pass writing into a 3D texture (`Texture::Create3D` is
  already in place from Phase 1) using rasterization with conservative
  rasterization or atomic image stores.
- Mipmap chain on the voxel grid.
- Cone-tracing gather pass.

**Depends on.** Phase 4 (PostProcess chain).

**Independent of.** Phase 5 and Phase 6.

**Risk.** The largest infrastructure step on the roadmap. 3D textures
introduce a new resource class unfamiliar to the existing passes, and
conservative rasterization is GPU-specific.

### Phase 8 — Dynamic Diffuse Global Illumination (DDGI)

**Goal.** Probe-based diffuse irradiance field with ray-traced visibility.

**Deliverables.**
- Probe grid resource and per-probe octahedral irradiance + depth
  textures.
- Per-frame ray generation (either trace into the voxel grid from
  Phase 7 or do screen-space ray tracing).
- Probe relighting and sample blending into the lighting pass.

**Depends on.** Phase 7 (visibility tracing benefits from the voxel
grid; otherwise needs a separate ray-tracing path).

**Risk.** Probe placement strategy and update budget.

### Ordering Rationale

Phase 3 and Phase 4 are independent and order-flexible. Phase 5 (RSM)
goes before Phase 6 (SSGI) because RSM extends an existing pass
(ShadowPass) while SSGI extends an existing chain (PostProcess);
completing each one validates a single extension axis before introducing
both at once. Phase 7 (VXGI) is placed after both indirect techniques
because it introduces a new resource class (3D textures) unfamiliar to
the existing passes. Phase 8 (DDGI) is last because its preferred
visibility source is the voxel grid built in Phase 7.

## Known Limitations

The list below tracks correctness and performance issues that are known
today but deferred. Each entry lists what is wrong, the impact, and an
estimated fix cost.

1. **`mat3(model)` used as normal matrix in `gbuffer.vert`.** Incorrect
   for non-uniform scaling. *Impact:* distorted lighting on non-uniformly
   scaled models. *Fix cost:* low (compute and upload the
   inverse-transpose, or compute in shader).

2. **`Vertex::tangent` is `vec3`, missing glTF tangent handedness (`w`).**
   The fragment shader recomputes the bitangent via `cross(N, T)`,
   correct up to sign but may be flipped on mirrored UV regions.
   *Impact:* localized normal-mapping inversion on mirrored geometry.
   *Fix cost:* medium (schema change, mesh loader change, vertex layout,
   shader change).

3. **No frustum culling.** Every registered mesh is drawn each pass.
   *Impact:* purely performance — fine at current scales, will become
   limiting in Sponza-scale assets at higher cascades. *Fix cost:*
   medium (AABB accumulation in `Mesh`, plus per-pass cull).

4. **No mesh batching.** Each sub-mesh issues its own `glDrawElements`.
   *Impact:* CPU overhead with many sub-meshes. *Fix cost:* medium.

5. **`ResourceManager` always loads textures as sRGB.** Linear textures
   (normal, roughness, metallic) bypass the cache via direct
   `Texture::Load2D` calls. *Impact:* duplicate texture loads when a
   non-sRGB texture is shared between materials. *Fix cost:* low
   (extend cache key to include the sRGB flag).

6. **PostProcess pass leaves backbuffer depth state unspecified.** The
   pass clears via `Framebuffer::BindDefault` then renders a fullscreen
   triangle; no explicit handling of the depth buffer if a future pass
   needs to read it post-tone-map. *Impact:* none today; latent. *Fix
   cost:* low (documented in the pass itself).

## What This Doc Is NOT

- **Not an API reference.** Source-code comments and module headers
  fill that role.
- **Not a per-phase implementation detail document.** Phase plans under
  `docs/superpowers/plans/` cover that.
- **Not a changelog.** `git log` is the changelog.
