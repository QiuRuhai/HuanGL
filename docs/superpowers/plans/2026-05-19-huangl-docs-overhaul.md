# HuanGL Docs Overhaul Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bring repository documentation in line with the Phase 2.5 state and establish a durable architecture+roadmap reference, so a new reader can answer "what is HuanGL, what can it do, and where is it going" without reading code.

**Architecture:** Four documents with distinct audiences and lifecycles. The Phase 2.5 implementation plan is archived from a user-local path into `docs/superpowers/plans/`. A new `docs/architecture.md` (current state + roadmap) is created first because it defines wording reused by `README.md` and `AGENTS.md`. Both top-level docs are then updated to point at it. Each document lands as its own commit so history reads as a coherent series.

**Tech Stack:** Markdown only. No code is written or modified.

**Source spec:** `docs/superpowers/specs/2026-05-19-huangl-docs-overhaul-design.md`

---

## File Map

```
NEW:    docs/superpowers/plans/2026-05-19-huangl-phase2.5-polish.md
NEW:    docs/architecture.md
MODIFY: AGENTS.md
MODIFY: README.md
```

No tests, no code, no build steps. Verification is by reading and link-checking.

---

## Task 1: Archive the Phase 2.5 implementation plan

**Files:**
- Create: `docs/superpowers/plans/2026-05-16-huangl-phase2.5-polish.md`

The Phase 2.5 work was driven by a temporary plan written to a user-local path during execution. That plan must be brought into the repository alongside the existing Phase 1, Phase 2, and repository-cleanup plans so the version-controlled record of "how the work was executed" is complete.

Per the spec's editing rules:
- Copy content verbatim.
- Rename the top-level heading to match the project convention (`# HuanGL Phase 2.5 Polish — Implementation Plan`).
- Do not add post-hoc commentary on what changed during execution.
- Do not add a "results" section. This is a frozen artifact.

**Source-file handling.** The original plan lives at a user-local path
(`C:\Users\<...>\.claude\plans\calm-fluttering-clarke.md`) under a
gitignored directory. Leave it in place; deletion is not required and
not part of this task. The content embedded below is the full source
verbatim apart from the heading rename, so the engineer does not need
to read the source file to complete this task.

- [ ] **Step 1: Create the archived plan file**

Create `docs/superpowers/plans/2026-05-16-huangl-phase2.5-polish.md` with the following content. The only deviation from the source is the top-level heading on line 1. (The outer fence below uses four backticks because the embedded content itself contains 3-backtick code fences.)

````markdown
# HuanGL Phase 2.5 Polish — Implementation Plan

## Context

Phase 2 rendering pipeline is complete (GBuffer, CSM, PBR+IBL). This plan addresses 4 gaps: tone mapping is hardcoded in the lighting shader, materials don't load textures from glTF, window resize doesn't rebuild FBOs, and there's no debug visualization. The design spec is at `docs/superpowers/specs/2026-05-16-huangl-phase2.5-polish-design.md`.

---

## Step 1: PostProcessPass + HDR FBO

### 1a. Create PostProcessPass shader

**Create `shader/postprocess/postprocess.frag`:**
```glsl
#version 460 core
in vec2 vUV;
out vec4 FragColor;

layout(binding = 0) uniform sampler2D uHDRInput;
layout(binding = 1) uniform sampler2D uAlbedoMetallic;
layout(binding = 2) uniform sampler2D uNormalRoughness;
layout(binding = 3) uniform sampler2D uDepth;
layout(binding = 4) uniform sampler2DArrayShadow uShadowMap;

uniform int uToneMapMode;  // 0=ACES, 1=Reinhard, 2=None
uniform int uDebugMode;    // 0=Final, 1=Albedo, 2=Normal, 3=Roughness, 4=Metallic, 5=Depth, 6=Cascades

uniform mat4 uView;
uniform float uCascadeFar[4];
uniform mat4 uInvViewProj;
uniform float uNearPlane;
uniform float uFarPlane;

vec3 ACESFilmic(vec3 x) {
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 Reinhard(vec3 x) {
    return x / (x + vec3(1.0));
}

float LinearizeDepth(float d, float near, float far) {
    float ndc = d * 2.0 - 1.0;
    return (2.0 * near * far) / (far + near - ndc * (far - near));
}

void main() {
    if (uDebugMode == 0) {
        // Final composite with tone mapping
        vec3 hdr = texture(uHDRInput, vUV).rgb;
        vec3 color;
        if (uToneMapMode == 0)      color = ACESFilmic(hdr);
        else if (uToneMapMode == 1) color = Reinhard(hdr);
        else                        color = clamp(hdr, 0.0, 1.0);
        color = pow(color, vec3(1.0 / 2.2));
        FragColor = vec4(color, 1.0);
    } else if (uDebugMode == 1) {
        FragColor = vec4(texture(uAlbedoMetallic, vUV).rgb, 1.0);
    } else if (uDebugMode == 2) {
        vec3 N = texture(uNormalRoughness, vUV).rgb;
        FragColor = vec4(N * 0.5 + 0.5, 1.0);
    } else if (uDebugMode == 3) {
        float r = texture(uNormalRoughness, vUV).a;
        FragColor = vec4(vec3(r), 1.0);
    } else if (uDebugMode == 4) {
        float m = texture(uAlbedoMetallic, vUV).a;
        FragColor = vec4(vec3(m), 1.0);
    } else if (uDebugMode == 5) {
        float d = texture(uDepth, vUV).r;
        float lin = LinearizeDepth(d, uNearPlane, uFarPlane);
        float vis = lin / uFarPlane;
        FragColor = vec4(vec3(vis), 1.0);
    } else if (uDebugMode == 6) {
        // Cascade overlay
        float d = texture(uDepth, vUV).r;
        vec4 clip = vec4(vUV * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
        vec4 world = uInvViewProj * clip;
        vec3 worldPos = world.xyz / world.w;
        float viewZ = -(uView * vec4(worldPos, 1.0)).z;
        vec3 cascadeColors[4] = vec3[4](vec3(1,0,0), vec3(0,1,0), vec3(0,0,1), vec3(1,1,0));
        int cascade = 3;
        for (int c = 0; c < 4; ++c) { if (viewZ < uCascadeFar[c]) { cascade = c; break; } }
        vec3 base = texture(uAlbedoMetallic, vUV).rgb;
        FragColor = vec4(mix(base, cascadeColors[cascade], 0.4), 1.0);
    }
}
```

### 1b. Create PostProcessPass C++ class

**Create `src/pipeline/passes/PostProcessPass.h`:**
- Members: `shader_`, `dummyVAO_`, `int toneMapMode_`, `int debugMode_`
- API: `Init()`, `Resize(int w, int h)` (no-op since it renders to backbuffer), `Render(...)`, `SetToneMapMode(int)`, `SetDebugMode(int)`, `CycleToneMap()`, `CycleDebugMode()`

**Create `src/pipeline/passes/PostProcessPass.cpp`:**
- Init: load shader (`shader/lighting/fullscreen.vert` + `shader/postprocess/postprocess.frag`), create dummy VAO
- Render: bind shader, set uniforms, bind HDR texture + GBuffer textures + shadow data, draw 3 verts, unbind

### 1c. Modify LightingPass to output HDR

**Modify `src/pipeline/passes/LightingPass.h`:**
- Add: `std::unique_ptr<Framebuffer> hdrFBO_`, `int width_`, `int height_`
- Add: `void Resize(int w, int h)`, `std::shared_ptr<Texture> GetHDROutput() const`

**Modify `src/pipeline/passes/LightingPass.cpp`:**
- In `Init()`: create `hdrFBO_` with RGBA16F color attachment + depth renderbuffer
- In `Render()`: bind `hdrFBO_` before drawing, unbind after
- Add `Resize()`: recreate hdrFBO_
- Add `GetHDROutput()`: return `hdrFBO_->GetColor(0)`

**Modify `shader/lighting/pbr_ibl.frag`:**
- Remove lines 112-113 (Reinhard tone mapping + gamma)
- Final output becomes just `FragColor = vec4(color, 1.0);` (raw HDR)

### 1d. Wire into RenderPipeline

**Modify `src/pipeline/RenderPipeline.h`:**
- Add `#include "passes/PostProcessPass.h"`
- Add member: `PostProcessPass postProcessPass_`
- Add: `PostProcessPass& GetPostProcess() { return postProcessPass_; }`

**Modify `src/pipeline/RenderPipeline.cpp`:**
- `Init()`: add `postProcessPass_.Init()`
- `Resize()`: add `lightingPass_.Resize(w, h)` (postprocess doesn't need resize — renders to backbuffer)
- `Execute()`: after lightingPass, call `postProcessPass_.Render(lightingPass_, gbufferPass_, shadowPass_, camera)`

### Verification
- Build and run: scene should look same as before (ACES ≈ Reinhard at moderate HDR values)
- Press T: visual difference between operators
- Press 1-6: debug views show correct channels

---

## Step 2: Window Resize

### 2a. Fix App resize callback

**Modify `src/core/App.cpp`:**
- Change resize callback to also call `pipeline_->Resize(w, h)`
- Guard against zero size (minimized window)
- Store pipeline as accessible from lambda (capture `this`)

### 2b. Verify LightingPass resize

Already added in Step 1c. GBufferPass already has Resize().

### Verification
- Drag window edge: rendering scales correctly, no stretch
- Minimize: no crash or GL error

---

## Step 3: glTF Material Loading

### 3a. Modify MeshLoader API

**Modify `src/resource/MeshLoader.h`:**
```cpp
struct LoadResult {
    std::shared_ptr<Mesh> mesh;
    std::vector<Material> materials;
};
static LoadResult Load(const std::string& path, ResourceManager& rm);
```

### 3b. Implement texture extraction

**Modify `src/resource/MeshLoader.cpp`:**
- After geometry loading, iterate `aiscene->mNumMaterials`
- For each `aiMaterial*`:
  - Get base color texture path (`aiTextureType_BASE_COLOR` or `aiTextureType_DIFFUSE`)
  - Get normal map path (`aiTextureType_NORMALS` or `aiTextureType_HEIGHT`)
  - Get metallic-roughness: try `aiTextureType_UNKNOWN` (glTF packed), fallback to separate
  - Resolve texture paths relative to model directory
  - Load via `ResourceManager::Get<Texture>()` or `Texture::Load2D()`
  - Extract factor values from `AI_MATKEY_BASE_COLOR`, `AI_MATKEY_ROUGHNESS_FACTOR`, `AI_MATKEY_METALLIC_FACTOR`

### 3c. Handle packed metallic-roughness

**Modify `src/renderer/Schema.h`:**
- Add `bool packedMetallicRoughness = false` to Material struct

**Modify `shader/gbuffer/gbuffer.frag`:**
- Add `uniform int uPackedMetallicRoughness`
- If packed: sample roughnessMap, use `.g` for roughness and `.b` for metallic

### 3d. Update Scene/TestScene to use new API

**Modify `src/scene/TestScene.cpp` (or Scene interface):**
- Adapt to new `LoadResult` return type where applicable
- TestScene can remain with hardcoded materials (procedural geometry)

### Verification
- Download DamagedHelmet.glb to `resources/models/`
- Load it in TestScene (or create a simple ModelScene)
- Verify textures appear: colored surfaces, metallic/rough variation visible

---

## Step 4: Normal Mapping

### 4a. Update GBuffer vertex shader

**Modify `shader/gbuffer/gbuffer.vert`:**
- Add input: `layout(location = 3) in vec3 aTangent`
- Compute world-space T, N, B
- Output `mat3 vTBN` (via 3 vec3 outputs: `vTangent`, `vBitangent`, `vNormal`)

### 4b. Update GBuffer fragment shader

**Modify `shader/gbuffer/gbuffer.frag`:**
- Add inputs matching vertex outputs
- Add `layout(binding = 3) uniform sampler2D uNormalMap` + `uniform int uHasNormalTex`
- If normal map present: sample, remap [0,1]→[-1,1], multiply by TBN
- Output transformed normal to RT1.rgb

### 4c. Bind normal map in GBufferPass

**Modify `src/pipeline/passes/GBufferPass.cpp`:**
- Set `uHasNormalTex` uniform
- Bind `mat.normalMap` to slot 3

### Verification
- Load model with normal map (DamagedHelmet has one)
- Debug mode 2 (Normal view): should show per-pixel detail, not flat face normals
- Lighting should show bumpy surfaces

---

## Step 5: Debug Visualization Polish

### 5a. Add keyboard input handling

**Modify `src/core/App.cpp`:**
- In `Update()` or `Run()`: check number keys 0-6 → `pipeline_->GetPostProcess().SetDebugMode(n)`
- Check T key → `pipeline_->GetPostProcess().CycleToneMap()`

### 5b. Pass cascade data to PostProcessPass

- PostProcessPass::Render needs `uCascadeFar[4]` and `uView` — get from ShadowPass and camera
- Pass near/far plane from camera data

### Verification
- All 7 debug modes display correctly
- T key cycles through tone map operators visibly
- No crash when switching modes rapidly

---

## Step 6: Integration Test

- Load DamagedHelmet.glb with full PBR textures
- Verify: albedo texture, normal map detail, metallic/roughness from packed texture
- Resize window multiple times
- Cycle all debug modes
- Compare ACES vs Reinhard on HDR content (bright specular highlights should differ)

---

## Critical Files Summary

**Create:**
- `src/pipeline/passes/PostProcessPass.h`
- `src/pipeline/passes/PostProcessPass.cpp`
- `shader/postprocess/postprocess.frag`

**Modify:**
- `src/pipeline/passes/LightingPass.h` — add HDR FBO, Resize, GetHDROutput
- `src/pipeline/passes/LightingPass.cpp` — create/use HDR FBO, add Resize
- `shader/lighting/pbr_ibl.frag` — remove tone mapping + gamma (2 lines)
- `src/pipeline/RenderPipeline.h` — add PostProcessPass member
- `src/pipeline/RenderPipeline.cpp` — init/resize/execute PostProcessPass
- `src/core/App.cpp` — resize callback, keyboard toggles
- `src/resource/MeshLoader.h` — LoadResult struct, new API
- `src/resource/MeshLoader.cpp` — texture extraction from Assimp
- `src/renderer/Schema.h` — add packedMetallicRoughness to Material
- `shader/gbuffer/gbuffer.vert` — tangent input, TBN computation
- `shader/gbuffer/gbuffer.frag` — normal map sampling, packed metallic-roughness
- `src/pipeline/passes/GBufferPass.cpp` — bind normal map, set packed uniform
````

- [ ] **Step 2: Verify the archived plan reads as a frozen artifact**

Run:

```bash
head -5 docs/superpowers/plans/2026-05-16-huangl-phase2.5-polish.md
```

Expected first line:

```
# HuanGL Phase 2.5 Polish — Implementation Plan
```

Expected second line (blank), third line starts with `## Context`. Confirm there is no reference to `calm-fluttering-clarke` anywhere:

```bash
grep -i "calm-fluttering-clarke" docs/superpowers/plans/2026-05-16-huangl-phase2.5-polish.md
```

Expected: no output.

- [ ] **Step 3: Commit**

```bash
git add docs/superpowers/plans/2026-05-16-huangl-phase2.5-polish.md
git commit -m "$(cat <<'EOF'
docs: archive Phase 2.5 implementation plan

Brings the implementation plan that drove Phase 2.5 into the repository
alongside the Phase 1, repository cleanup, and Phase 2 plans. Content
is the frozen plan as executed; no retrospective edits.

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Create `docs/architecture.md`

**Files:**
- Create: `docs/architecture.md`

This is the durable reference doc combining current architecture and the forward-looking roadmap. It is built first because `AGENTS.md` and `README.md` will both link to its Roadmap section and reuse its Phase status table wording.

The doc has eight sections in this order, matching the spec.

- [ ] **Step 1: Create the file with the front matter and Vision/Non-Goals section**

Create `docs/architecture.md` with this initial content:

```markdown
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
```

- [ ] **Step 2: Append the Current Capability section**

Append to `docs/architecture.md`:

```markdown

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
| Mouse | Camera look |
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
```

- [ ] **Step 3: Append the Render Pipeline section**

Append to `docs/architecture.md`:

````markdown

## Render Pipeline

Each frame the `RenderPipeline` orchestrator runs four passes in fixed
order. Pass inputs and outputs use the formats below:

```
Scene ──┬─► ShadowPass     → sampler2DArrayShadow (2048² × 4 cascades, D32)
        │
        ├─► GBufferPass    → RT0 RGBA8   (albedo.rgb,  metallic.a)
        │                    RT1 RGBA16F (normal.rgb,  roughness.a)
        │                    Depth D24
        │
        └─► LightingPass   ← reads GBuffer + shadow array + IBL cubemaps
                             writes RGBA16F HDR target
                                    │
                                    ▼
                             PostProcessPass
                                    │
                             tone map + gamma + debug overlay
                                    │
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
````

- [ ] **Step 4: Append the Module Map section**

Append to `docs/architecture.md`:

```markdown

## Module Map

| Subdirectory | Responsibility | Key types |
|--------------|----------------|-----------|
| `src/core/` | Window, input, app loop, camera | `App`, `Window`, `Input`, `Camera` |
| `src/renderer/` | OpenGL RAII wrappers and shared schemas | `Shader`, `Buffer`, `Texture`, `Framebuffer`, `Material`, `Mesh` |
| `src/pipeline/` | Render passes and per-frame orchestration | `RenderPipeline`, `ShadowPass`, `GBufferPass`, `LightingPass`, `PostProcessPass` |
| `src/resource/` | Asset loading and caching | `ResourceManager`, `MeshLoader` |
| `src/scene/` | Scene definitions | `Scene`, `TestScene`, `ModelScene` |
| `src/ui/` | Reserved for ImGui (Phase 3) | — |

File-level inventory lives in `AGENTS.md`; this table intentionally
stops at the subdirectory level so it does not have to be touched on
routine file additions.
```

- [ ] **Step 5: Append the Key Design Decisions section**

Append to `docs/architecture.md`:

```markdown

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
```

- [ ] **Step 6: Append the Roadmap section**

Append to `docs/architecture.md`:

```markdown

## Roadmap

| Phase | Status | Theme |
|-------|--------|-------|
| 1 | ✅ Complete | Foundation (GLAD2, RAII wrappers, App loop) |
| 2 | ✅ Complete | Deferred render pipeline (GBuffer, CSM, PBR+IBL) |
| 2.5 | ✅ Complete | Pipeline polish (PostProcess, glTF materials, debug views) |
| 3 | Planned | Scene system and ImGui debug UI |
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
inspectable UI, and grow `ModelScene` into a small scene manager that
exposes per-entity transforms and material parameters.

**Deliverables.**
- `src/ui/ImGuiLayer.h/cpp` wrapping Dear ImGui setup, frame begin/end,
  and the GLFW + OpenGL3 backends.
- A debug panel exposing tone-map operator, debug mode, ambient
  strength, sun direction/color/intensity, and camera FOV.
- A scene inspector panel listing registered scenes and (eventually)
  per-mesh transforms.
- Optional ImGuizmo integration for manipulating selected entities.

**Depends on.** Phase 2.5 (PostProcessPass exposes the runtime knobs the
UI will drive).

**Independent of.** Phase 4.

**Risk.** Integrating Dear ImGui through vcpkg without disturbing the
existing GLFW setup. Mitigation: add the imgui port to vcpkg.json and
include only the GLFW + OpenGL3 backend headers.

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
```

- [ ] **Step 7: Append the Known Limitations section**

Append to `docs/architecture.md`:

```markdown

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
```

- [ ] **Step 8: Append the closing "What This Doc Is NOT" section**

Append to `docs/architecture.md`:

```markdown

## What This Doc Is NOT

- **Not an API reference.** Source-code comments and module headers
  fill that role.
- **Not a per-phase implementation detail document.** Phase plans under
  `docs/superpowers/plans/` cover that.
- **Not a changelog.** `git log` is the changelog.
```

- [ ] **Step 9: Verify content matches the spec**

Run these checks:

```bash
# 1. All major section headings present
grep -E "^## " docs/architecture.md
```

Expected output (exact order):

```
## Vision and Non-Goals
## Current Capability
## Render Pipeline
## Module Map
## Key Design Decisions
## Roadmap
## Known Limitations
## What This Doc Is NOT
```

```bash
# 2. Roadmap covers Phases 3–8
grep -E "^### Phase " docs/architecture.md
```

Expected: six `### Phase N — ...` headings plus a `### Ordering Rationale`.

```bash
# 3. Key bindings table appears in Current Capability
grep -c "^| `" docs/architecture.md
```

Expected: at least 12 (one per key binding row).

```bash
# 4. No TBD or TODO placeholders
grep -E "TBD|TODO|FIXME" docs/architecture.md
```

Expected: no output.

```bash
# 5. Length matches the spec budget for this doc
wc -l docs/architecture.md
```

Expected: 300–500 lines per the spec. If significantly shorter, a
section was skipped; if significantly longer, sections grew beyond
their summary intent and should be tightened.

- [ ] **Step 10: Commit**

```bash
git add docs/architecture.md
git commit -m "$(cat <<'EOF'
docs: add architecture and roadmap reference

A durable reference covering the current renderer architecture
(vision/non-goals, pipeline diagram, module map, key design decisions)
and the forward-looking roadmap for Phases 3 through 8 with
deliverables, dependencies, and ordering rationale.

This document is the source-of-truth for "where HuanGL is going."
README.md and AGENTS.md will link here for the roadmap rather than
duplicating it.

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Update `AGENTS.md`

**Files:**
- Modify: `AGENTS.md`

`AGENTS.md` is the file-level inventory for AI agents and contributors.
After this task it lists every module shipped in Phase 2 and Phase 2.5,
records the new technical decisions, marks the phase status, and points
at `architecture.md` for the forward-looking roadmap.

The work is done as four targeted edits rather than a wholesale rewrite,
to preserve unchanged sections (Project Summary, Build Method on
Windows, Repository Baseline).

- [ ] **Step 1: Read the current `AGENTS.md` for context**

Run:

```bash
wc -l AGENTS.md
```

Expected: about 139 lines (the pre-edit baseline). If significantly
different, stop and reconcile.

- [ ] **Step 2: Extend the `Key Technical Decisions` section**

Open `AGENTS.md`. The `## Key Technical Decisions` section currently
contains six bullets. The last bullet in that list reads:

```
- MSVC does not accept single-line ternary statements such as `{ e ? glEnable(...) : glDisable(...); }`; use `if`/`else`.
```

Immediately after that line (and before the blank line that precedes
the next `## Current Progress` heading), append these new bullets:

```markdown
- Reconstruct world position from depth and inverse view-proj in the lighting pass; do not write world position to a GBuffer attachment.
- Use `sampler2DArrayShadow` for CSM and let the hardware perform the depth comparison; cascade selection uses view-space Z.
- Tone mapping lives in `PostProcessPass`, not in the lighting shader. `LightingPass` writes raw HDR radiance to an RGBA16F target so future passes (Bloom, TAA) can read it.
- Re-orthogonalize the TBN basis in the fragment shader, not the vertex shader. Interpolation destroys vertex-side orthogonality.
- Treat Assimp texture paths starting with `*` as indices into `aiScene::mTextures` and decode the embedded bytes via `Texture::Load2DFromMemory`. This is the only correct way to load textures from `.glb`.
- `Material::packedMetallicRoughness` flag signals the glTF convention (G = roughness, B = metallic) so the shader samples the right channels.
- `App` registers scenes with soft failure: if a model file is missing or fails to load, the app logs and skips, continuing with the remaining registered scenes.
- `packed`, `near`, and `far` are reserved or potentially reserved names in GLSL across drivers. Avoid them as GLSL identifiers.
```

Do not modify the existing bullets above; they remain accurate.

- [ ] **Step 3: Replace the `Current Progress` section**

Open `AGENTS.md`. The `## Current Progress` section currently contains
a single subsection `### Phase 1: Foundation Complete` followed by a
file table. The last row of that table reads:

```
| `src/main.cpp` | Minimal entry calling `App::Run()` |
```

Keep everything in this section up to and including that row. After
that row (and any blank line that follows it), and BEFORE the next
section heading `## Current Directory Structure`, append two new
subsections:

````markdown

### Phase 2: Render Pipeline — Complete

| File | Responsibility |
|------|----------------|
| `src/pipeline/RenderPipeline.h/cpp` | Pass orchestrator, owns Shadow/GBuffer/Lighting/PostProcess passes |
| `src/pipeline/passes/ShadowPass.h/cpp` | Four-cascade CSM with `sampler2DArrayShadow` |
| `src/pipeline/passes/GBufferPass.h/cpp` | Deferred MRT fill (RGBA8 albedo+metallic, RGBA16F normal+roughness, D24 depth) |
| `src/pipeline/passes/LightingPass.h/cpp` | Cook-Torrance PBR + IBL (irradiance + prefilter + BRDF LUT) |
| `src/resource/ResourceManager.h/cpp` | `weak_ptr` texture and mesh cache with GC |
| `src/resource/MeshLoader.h/cpp` | Assimp wrapper, returns `LoadResult { mesh, materials }` |
| `src/scene/Scene.h` | Scene interface with mesh/material/light access |
| `src/scene/TestScene.h/cpp` | Procedural test scene (floor + spheres + PBR factor materials) |
| `src/renderer/Schema.h` | `Mesh`, `SubMesh`, `Material`, `DirectionalLight`, `Vertex` schemas |
| `src/core/Camera.h/cpp` | Free-fly camera, WASD plus mouse look |
| `shader/gbuffer/*.{vert,frag}` | GBuffer fill |
| `shader/shadow/csm.vert` | Cascade depth render |
| `shader/lighting/*.{vert,frag}` | PBR+IBL, IBL precompute (equirect→cubemap, irradiance, prefilter, BRDF LUT), fullscreen helper |

### Phase 2.5: Pipeline Polish — Complete

| File | Responsibility |
|------|----------------|
| `src/pipeline/passes/PostProcessPass.h/cpp` | Tone mapping, gamma, debug visualization |
| `src/scene/ModelScene.h/cpp` | Loads a model via `MeshLoader`, adds optional floor |
| `shader/postprocess/postprocess.frag` | ACES / Reinhard / linear tone map plus seven debug modes |
| `src/renderer/Texture.h/cpp` | Adds `Load2DFromMemory` for `.glb` embedded textures |
| `src/resource/MeshLoader.h/cpp` | Extends to extract PBR textures and handle `*N` embedded paths |
| `src/core/Input.h/cpp` | Adds `IsKeyJustPressed` via GLFW key callback |
| `src/core/App.h/cpp` | Multi-scene registration with `N`-key cycling, debug-key handling, resize propagation |
````

- [ ] **Step 4: Replace the `Planned Phases` section body**

Open `AGENTS.md`. The `## Planned Phases` section currently contains
four subsections, in order: `### Phase 2: Render Pipeline`,
`### Phase 3: Scene System`, `### Phase 4: Post-Processing`, and
`### Phase 5-8: GI Algorithms`. Replace everything between the
`## Planned Phases` heading and the next top-level section
`## Design Documents` (i.e. the four subsections and any blank lines
between them) with this:

```markdown
| Phase | Status | Theme |
|-------|--------|-------|
| 1 | ✅ Complete | Foundation (GLAD2, RAII wrappers, App loop) |
| 2 | ✅ Complete | Deferred render pipeline (GBuffer, CSM, PBR+IBL) |
| 2.5 | ✅ Complete | Pipeline polish (PostProcess, glTF materials, debug views) |
| 3 | Planned | Scene system and ImGui debug UI |
| 4 | Planned | Bloom, TAA, improved tone mapping |
| 5 | Planned | RSM |
| 6 | Planned | SSGI |
| 7 | Planned | VXGI |
| 8 | Planned | DDGI |

For phase deliverables, dependencies, and ordering rationale see
[`docs/architecture.md`](docs/architecture.md).
```

Leave the `## Planned Phases` heading itself in place.

- [ ] **Step 5: Replace the `Design Documents` section body**

Open `AGENTS.md`. The `## Design Documents` section is the last
section in the file. Replace its body (the existing bullet list — every
line after the `## Design Documents` heading through the end of the
file) with this complete inventory:

```markdown
- Refactor design: `docs/superpowers/specs/2026-05-13-huangl-refactor-design.md`
- Repository cleanup design: `docs/superpowers/specs/2026-05-13-huangl-repository-cleanup-design.md`
- Phase 2 pipeline design: `docs/superpowers/specs/2026-05-14-huangl-phase2-pipeline-design.md`
- Phase 2.5 polish design: `docs/superpowers/specs/2026-05-16-huangl-phase2.5-polish-design.md`
- Docs overhaul design: `docs/superpowers/specs/2026-05-19-huangl-docs-overhaul-design.md`
- Phase 1 plan: `docs/superpowers/plans/2026-05-13-huangl-phase1-foundation.md`
- Repository cleanup plan: `docs/superpowers/plans/2026-05-13-huangl-repository-cleanup.md`
- Phase 2 pipeline plan: `docs/superpowers/plans/2026-05-14-huangl-phase2-pipeline.md`
- Phase 2.5 polish plan: `docs/superpowers/plans/2026-05-16-huangl-phase2.5-polish.md`
- Architecture and roadmap: `docs/architecture.md`
```

- [ ] **Step 6: Verify the updated file**

Run:

```bash
# Length is in the expected range
wc -l AGENTS.md
```

Expected: 160–200 lines (up from a 139-line baseline; the actual added
content is two module tables plus eight new technical-decision bullets
plus the design-doc inventory). If the count lands outside this range,
something major was added or omitted — re-read the spec sections rather
than nudging the threshold.

```bash
# All three progress subsections present
grep -E "^### Phase " AGENTS.md
```

Expected:

```
### Phase 1: Foundation Complete
### Phase 2: Render Pipeline — Complete
### Phase 2.5: Pipeline Polish — Complete
```

```bash
# Phase status table present
grep -E "✅ Complete" AGENTS.md
```

Expected: three lines (Phase 1, 2, 2.5).

```bash
# Architecture doc linked from Planned Phases section
grep -c "docs/architecture.md" AGENTS.md
```

Expected: at least 2 (Planned Phases reference + Design Documents
listing).

- [ ] **Step 7: Commit**

```bash
git add AGENTS.md
git commit -m "$(cat <<'EOF'
docs: update AGENTS.md for Phase 2/2.5 state

Adds module inventory tables for Phase 2 (RenderPipeline, ShadowPass,
GBufferPass, LightingPass, ResourceManager, MeshLoader, Scene,
TestScene, Schema, Camera) and Phase 2.5 (PostProcessPass, ModelScene,
postprocess shader, embedded-texture loading, IsKeyJustPressed,
multi-scene App). Records the new technical decisions (deferred world
pos, sampler2DArrayShadow, HDR-target tone mapping split, fragment-side
TBN, *N embedded textures, soft-failure scene loading, GLSL reserved
identifiers). Replaces the planned-phases bullet list with a status
table and links to docs/architecture.md for the detailed roadmap.

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Update `README.md`

**Files:**
- Modify: `README.md`

`README.md` is the GitHub first impression. After this task it states
the current Phase 2.5 capability instead of "Phase 1 complete," teaches
a new reader how to run the binary (model placement and key bindings),
and presents a phase status table that matches `AGENTS.md` and
`architecture.md`.

Build instructions, the repository layout tree, and the Technical
Direction section are unchanged.

- [ ] **Step 1: Replace the `Current Status` section**

Open `README.md`. The `## Current Status` section currently ends with
this exact line:

```
Phase 2 will introduce the render pipeline, beginning with deferred rendering infrastructure.
```

Replace everything between the `## Current Status` heading and the next
top-level section `## Repository Layout` (i.e. all the bullets, the
prose, and the blank lines in between) with this:

```markdown
Phases 1, 2, and 2.5 are complete. The renderer currently supports:

- Deferred PBR shading with metallic-roughness workflow (Cook-Torrance).
- Image-based lighting from a single HDR equirectangular environment
  (diffuse irradiance cubemap, prefiltered specular cubemap, BRDF LUT).
- Cascaded shadow maps (four cascades, `sampler2DArrayShadow`, 3×3 PCF).
- A post-processing pass with selectable tone mapping (ACES Filmic,
  Reinhard, linear) and sRGB-approximate gamma.
- glTF, OBJ, and FBX loading through Assimp, including PBR factor
  extraction, normal-map sampling with TBN reconstructed in the
  fragment shader, and packed metallic-roughness textures.
- `.glb` embedded textures via Assimp `*N` indices.
- Multi-scene registration with runtime cycling.
- Seven runtime debug views: final composite, albedo, world-space
  normal, roughness, metallic, linear depth, and shadow cascade
  overlay.
- Window resize propagated through the entire pipeline.

For architecture, key design decisions, and the forward-looking
roadmap (Phases 3 through 8), see
[`docs/architecture.md`](docs/architecture.md).
```

Leave the `## Current Status` heading itself in place.

- [ ] **Step 2: Insert a new `Running` section after the build instructions**

Open `README.md`. The Build section ends with this exact line:

```
When new `.cpp` files are added, re-run the configure command before building because the project currently uses `GLOB_RECURSE`.
```

Immediately after that line and the blank line that follows it (so that
the new section sits on its own), insert the new `## Running` section
below, placed before the existing `## Technical Direction` heading.
(The outer fence below uses four backticks because the embedded content
contains 3-backtick code fences.)

````markdown
## Running

Models are not vendored in the repository. Place `.gltf`, `.glb`, or
similar files under `resources/models/` (the directory is gitignored).
Two known-good test assets:

```powershell
# DamagedHelmet (small, fully embedded .glb)
curl -L -o resources/models/DamagedHelmet.glb `
  "https://github.com/KhronosGroup/glTF-Sample-Assets/raw/main/Models/DamagedHelmet/glTF-Binary/DamagedHelmet.glb"

# Sponza (larger, multi-texture scene; uses git sparse-checkout)
cd resources/models
git clone --depth 1 --filter=blob:none --sparse `
  https://github.com/KhronosGroup/glTF-Sample-Assets.git _assets
cd _assets
git sparse-checkout set Models/Sponza/glTF
cd ..
mv _assets/Models/Sponza Sponza
rm -r -force _assets
```

`App` registers any model it finds at startup and skips the rest with a
log message, so the binary runs even if neither model is present.

Runtime controls:

| Input | Action |
|-------|--------|
| `W` `A` `S` `D` | Camera translation |
| Mouse | Camera look |
| `N` | Cycle registered scenes |
| `T` | Cycle tone-map operator (ACES / Reinhard / linear) |
| `0` | Final composite |
| `1` | Albedo |
| `2` | World-space normal |
| `3` | Roughness |
| `4` | Metallic |
| `5` | Linear depth |
| `6` | Shadow cascade overlay |
| `Esc` | Quit |
````

- [ ] **Step 3: Replace the `Planned Rendering Phases` section body**

Open `README.md`. The `## Planned Rendering Phases` section is the last
section in the file. Replace its body (the existing bullet list — every
line after the heading through the end of the file) with this:

```markdown
| Phase | Status | Theme |
|-------|--------|-------|
| 1 | ✅ Complete | Foundation (GLAD2, RAII wrappers, App loop) |
| 2 | ✅ Complete | Deferred render pipeline (GBuffer, CSM, PBR+IBL) |
| 2.5 | ✅ Complete | Pipeline polish (PostProcess, glTF materials, debug views) |
| 3 | Planned | Scene system and ImGui debug UI |
| 4 | Planned | Bloom, TAA, improved tone mapping |
| 5 | Planned | RSM |
| 6 | Planned | SSGI |
| 7 | Planned | VXGI |
| 8 | Planned | DDGI |

For phase deliverables and ordering rationale see
[`docs/architecture.md`](docs/architecture.md).
```

Leave the `## Planned Rendering Phases` heading itself in place.

- [ ] **Step 4: Verify the updated file**

Run:

```bash
# Length is in the expected range
wc -l README.md
```

Expected: 130–160 lines (up from 101 in the baseline).

```bash
# No stale Phase 1 wording
grep -E "Phase 1 is complete|Phase 2 will introduce" README.md
```

Expected: no output.

```bash
# Running section present with key bindings
grep -E "^## Running$" README.md
grep -c "^| \`" README.md
```

Expected: the heading appears (`## Running`); the second command
returns at least 12 (one per key binding row).

```bash
# Status table consistent with AGENTS.md
diff <(grep "^| [0-9]" README.md) <(grep "^| [0-9]" AGENTS.md)
```

Expected: no diff output (the two phase status tables match exactly).

- [ ] **Step 5: Commit**

```bash
git add README.md
git commit -m "$(cat <<'EOF'
docs: refresh README for Phase 2.5

Replaces the stale "Phase 1 is complete / Phase 2 will introduce..."
status with the current Phase 2.5 capability summary. Adds a Running
section covering model placement (resources/models/) with curl and git
sparse-checkout commands for the two known-good test assets, plus the
full runtime key-binding table. Replaces the planned-phases bullet list
with a status table aligned with AGENTS.md and architecture.md.

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Final cross-document verification

This task does no writing. It runs the consistency checks the spec
listed as proxies for "documentation is correct" and creates no commit
unless one of them surfaces a real issue.

- [ ] **Step 1: Verify the phase status tables agree across all three documents**

Run:

```bash
# Compare README and AGENTS phase tables row-by-row
diff <(grep "^| [0-9]" README.md) <(grep "^| [0-9]" AGENTS.md)
```

Expected: empty.

```bash
# Compare README and architecture.md phase tables row-by-row
diff <(grep "^| [0-9]" README.md) <(grep "^| [0-9]" docs/architecture.md)
```

Expected: empty.

```bash
# Verify architecture.md's roadmap covers Phase 3 through Phase 8 in detail
grep -E "^### Phase [3-8] " docs/architecture.md | wc -l
```

Expected: 6.

- [ ] **Step 2: Verify every spec and plan is listed in `AGENTS.md`**

Run:

```bash
# Every file under docs/superpowers/ appears in the Design Documents list
for f in docs/superpowers/specs/*.md docs/superpowers/plans/*.md; do
  base=$(basename "$f")
  grep -q "$base" AGENTS.md || echo "MISSING from AGENTS.md: $f"
done
```

Expected: no `MISSING` output. If any file is missing, return to Task 3
Step 5 and append it.

- [ ] **Step 3: Verify no broken in-repo links in the four documents**

Run:

```bash
# Collect every (path) reference of the form (docs/...) or (./...) and
# confirm the file exists.
for doc in README.md AGENTS.md docs/architecture.md \
           docs/superpowers/plans/2026-05-16-huangl-phase2.5-polish.md; do
  grep -oE "\(([a-zA-Z0-9._/-]+\.md|[a-zA-Z0-9._/-]+\.glsl|src/[^)]+|shader/[^)]+|docs/[^)]+)\)" "$doc" \
    | sed -E "s/^\(|\)$//g" \
    | while read -r ref; do
        [ -e "$ref" ] || echo "BROKEN in $doc: $ref"
      done
done
```

Expected: no `BROKEN` output.

- [ ] **Step 4: Verify no placeholder strings remain**

Run:

```bash
grep -rE "TBD|TODO|FIXME|XXX" \
  README.md AGENTS.md docs/architecture.md \
  docs/superpowers/plans/2026-05-16-huangl-phase2.5-polish.md
```

Expected: no output. If anything turns up, fix it inline in the
offending document and amend that document's commit (do not create a
new commit just for the fix).

- [ ] **Step 5: Confirm git history reads as a coherent series**

Run:

```bash
git log --oneline -6
```

Expected (top four should be from this plan, in this order):

```
<sha> docs: refresh README for Phase 2.5
<sha> docs: update AGENTS.md for Phase 2/2.5 state
<sha> docs: add architecture and roadmap reference
<sha> docs: archive Phase 2.5 implementation plan
<sha> docs: add docs overhaul design spec
<sha> feat(app): add ModelScene and runtime scene/debug controls
```

(The fifth and sixth lines are the existing spec commit and the last
Phase 2.5 feature commit; they confirm we are on the right branch.)

This task does not commit. If everything passes the plan is complete.

---

## Summary

Five tasks, four commits. After execution:

- `docs/superpowers/plans/2026-05-16-huangl-phase2.5-polish.md` exists
  and freezes the Phase 2.5 work as executed.
- `docs/architecture.md` is the durable architecture-plus-roadmap
  reference.
- `AGENTS.md` reflects the actual Phase 2 and Phase 2.5 module set, the
  full design-doc inventory, and a phase status table that links to
  `architecture.md`.
- `README.md` no longer claims Phase 1 is the latest state, teaches a
  new reader how to run the binary, and matches `AGENTS.md` on the
  phase status table.
- Git log shows four `docs:` commits in a coherent order on top of the
  existing spec commit.
