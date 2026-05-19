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
