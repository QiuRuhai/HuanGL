# Phase 2.5: Pipeline Polish & Material Completion

## Context

Phase 2 rendering pipeline is feature-complete: deferred GBuffer, CSM shadows, PBR+IBL lighting all work. However several gaps limit visual quality and development velocity:

- Tone mapping (Reinhard) and gamma are hardcoded in the lighting shader — can't swap algorithms or add post-processing later
- Materials use only PBR factors (no texture maps loaded from glTF)
- Normal mapping not supported (vertex shader doesn't pass tangent, fragment shader doesn't sample normal map)
- Window resize doesn't rebuild framebuffers (causes stretched/incorrect rendering)
- No debug visualization to inspect GBuffer, shadows, or intermediate passes

This phase separates tone mapping into a dedicated PostProcessPass, completes the material pipeline, fixes resize, and adds debug views.

---

## 1. PostProcessPass — Tone Mapping + Gamma + Debug Views

### Architecture Change

```
Before: Shadow → GBuffer → Lighting → backbuffer
After:  Shadow → GBuffer → Lighting → HDR FBO → PostProcess → backbuffer
```

### LightingPass Changes

- Add an HDR framebuffer (RGBA16F) as render target
- Remove tone mapping and gamma from `pbr_ibl.frag` (lines 112-113)
- Output raw HDR `vec3 color` to the new FBO
- Add `Resize(int w, int h)` method (same pattern as GBufferPass)
- Expose `GetHDROutput()` accessor

### PostProcessPass (New)

**File:** `src/pipeline/passes/PostProcessPass.h|cpp`
**Shader:** `shader/postprocess/postprocess.frag`

**Responsibilities:**
1. Read HDR texture from LightingPass output
2. Apply selected tone mapping operator
3. Apply sRGB gamma correction (pow 1/2.2)
4. Debug visualization modes (bypass tone mapping, show GBuffer channels)

**Tone Mapping Operators (uniform int switch):**
- 0: ACES Filmic (default)
- 1: Reinhard
- 2: None (linear clamp, for debug)

**Debug Visualization Modes (uniform int):**
- 0: Final composite (default)
- 1: Albedo (from GBuffer RT0.rgb)
- 2: Normal (from GBuffer RT1.rgb, remapped to [0,1])
- 3: Roughness (from GBuffer RT1.a)
- 4: Metallic (from GBuffer RT0.a)
- 5: Depth (linearized, from GBuffer depth)
- 6: Shadow cascade overlay (colorize by cascade index)

**Keyboard Toggle:** Number keys 0-6 switch debug mode, T cycles tone map operator.

**Rendering:** Fullscreen triangle (same gl_VertexID trick as LightingPass).

### ACES Implementation

```glsl
vec3 ACESFilmic(vec3 x) {
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}
```

---

## 2. glTF Material Completion

### MeshLoader Enhancement

**Goal:** Extract PBR textures from Assimp materials when loading a glTF/OBJ file.

**Changes to `MeshLoader::Load()`:**
- After loading mesh geometry, iterate `aiscene->mMaterials`
- For each `aiMaterial`, extract:
  - `aiTextureType_BASE_COLOR` (or `DIFFUSE` fallback) → `Material::albedoMap`
  - `aiTextureType_NORMALS` (or `HEIGHT` fallback) → `Material::normalMap`
  - `aiTextureType_METALNESS` → `Material::metallicMap`
  - `aiTextureType_DIFFUSE_ROUGHNESS` → `Material::roughnessMap`
  - Combined metallic-roughness: `aiTextureType_UNKNOWN` with key `$tex.file` → split or sample G/B channels
- Also extract `AI_MATKEY_BASE_COLOR` factor, roughness/metallic factors
- Load textures via `ResourceManager` (sRGB for albedo, linear for normal/metallic/roughness)
- Return a `std::vector<Material>` alongside the Mesh

**API Change:**

```cpp
struct LoadResult {
    std::shared_ptr<Mesh> mesh;
    std::vector<Material> materials;
};
LoadResult MeshLoader::Load(const std::string& path, ResourceManager& rm);
```

### GBuffer Normal Mapping

**Vertex Shader Changes (`gbuffer.vert`):**
- Accept `layout(location = 3) in vec3 aTangent` (already in Vertex struct)
- Compute TBN matrix: `T = normalize(mat3(model) * aTangent)`, `N = ...`, `B = cross(N, T)`
- Pass `mat3 vTBN` to fragment shader (or pass T, B, N separately)

**Fragment Shader Changes (`gbuffer.frag`):**
- Add `layout(binding = 3) uniform sampler2D uNormalMap` + `uniform int uHasNormalTex`
- If normal map present: sample, remap from [0,1] to [-1,1], transform by TBN
- Output transformed normal to RT1

### glTF Metallic-Roughness Packed Texture

glTF packs metallic (B channel) and roughness (G channel) in a single texture. Handle this:
- If the file has a combined metallicRoughness texture, store it as `roughnessMap` in Material
- In GBuffer shader: sample `.g` for roughness, `.b` for metallic from the same texture
- Add `uniform int uPackedMetallicRoughness` flag

---

## 3. Window Resize

### Framebuffer Changes

Remove `const` from `width_`, `height_` (already non-const per current code). Add:

```cpp
void Framebuffer::Resize(int w, int h);
```

Implementation: destroy old FBO + RBO, recreate with new dimensions. Detach all textures (they'll be replaced by the owning Pass).

Alternative (simpler): each Pass recreates its own FBO on resize (current GBufferPass pattern). Keep this pattern — just extend to all passes.

### Pipeline Resize Propagation

```cpp
void RenderPipeline::Resize(int w, int h) {
    gbufferPass_.Resize(w, h);
    lightingPass_.Resize(w, h);
    postProcessPass_.Resize(w, h);
    // ShadowPass is resolution-independent (fixed 2048²)
}
```

### App Integration

```cpp
window_->SetResizeCallback([this](int w, int h) {
    Renderer::SetViewport(0, 0, w, h);
    if (w > 0 && h > 0)
        pipeline_->Resize(w, h);
});
```

Handle zero-size (minimized window) by skipping render.

---

## 4. Debug Visualization

Integrated into PostProcessPass. Needs access to GBuffer textures and shadow data.

### PostProcessPass::Render Signature

```cpp
void Render(const LightingPass& lighting, const GBufferPass& gbuffer,
            const ShadowPass& shadow, const CameraData& camera);
```

### Cascade Visualization

In debug mode 6, compute view-space Z per pixel (reconstruct from depth), compare against cascade far planes, output a distinct color per cascade:
- Cascade 0: Red
- Cascade 1: Green
- Cascade 2: Blue
- Cascade 3: Yellow

---

## 5. Implementation Order

1. **PostProcessPass + HDR FBO** — Create pass, move tone mapping out of lighting shader, wire into pipeline
2. **Window resize** — Propagate to all passes, handle minimized
3. **glTF material loading** — Enhance MeshLoader, load textures
4. **Normal mapping** — Update GBuffer vertex/fragment shaders
5. **Debug visualization** — Add modes to PostProcessPass shader
6. **Integration test** — Load a real glTF model (DamagedHelmet), verify all modes work

---

## Files to Create

| File | Purpose |
|------|---------|
| `src/pipeline/passes/PostProcessPass.h` | PostProcess pass header |
| `src/pipeline/passes/PostProcessPass.cpp` | PostProcess pass implementation |
| `shader/postprocess/postprocess.frag` | Tone mapping + debug vis fragment shader |

**Note:** PostProcessPass reuses `shader/lighting/fullscreen.vert` (gl_VertexID fullscreen triangle).

## Files to Modify

| File | Changes |
|------|---------|
| `src/pipeline/passes/LightingPass.h/cpp` | Add HDR FBO output, Resize(), remove tone mapping |
| `src/pipeline/RenderPipeline.h/cpp` | Add PostProcessPass, fix Resize() |
| `src/core/App.cpp` | Wire resize callback to pipeline, add key toggles |
| `src/resource/MeshLoader.h/cpp` | Extract textures from Assimp materials |
| `shader/lighting/pbr_ibl.frag` | Remove lines 112-113 (tone mapping + gamma) |
| `shader/gbuffer/gbuffer.vert` | Add tangent attribute, compute TBN |
| `shader/gbuffer/gbuffer.frag` | Add normal map sampling, packed metallic-roughness |
| `src/renderer/Schema.h` | Add `packedMetallicRoughness` flag to Material |

---

## Verification

1. **Tone mapping**: Load scene, press T to cycle operators — visual difference between ACES/Reinhard/None
2. **Gamma**: Verify mid-gray (0.5 linear) maps to ~0.73 in output (sRGB)
3. **Resize**: Drag window edge, verify no stretching or black bars
4. **Minimize**: Minimize window, verify no crash
5. **glTF textures**: Load DamagedHelmet.glb, verify textures appear (not flat color)
6. **Normal mapping**: Inspect normal debug view — should show per-pixel detail from normal map
7. **Debug views**: Press 0-6, verify each channel displays correctly
8. **Cascade overlay**: Verify 4 distinct color bands at appropriate distances
