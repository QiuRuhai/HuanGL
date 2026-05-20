# HuanGL Technique Architecture Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a lightweight concrete technique layer and use Bloom as the first algorithm that proves the boundary.

**Architecture:** `RenderPipeline` remains the explicit frame orchestrator. Optional rendering algorithms live under `src/pipeline/techniques/`, own their own OpenGL resources, read `FrameContext` plus `PipelineOutputs`, and write typed output structs. `DebugUI` edits settings in `ApplicationState`; it never talks to pass or technique instances.

**Tech Stack:** C++17, OpenGL 4.6 DSA, GLAD2, GLFW, ImGui, CMake/vcpkg, GLSL 460.

---

## File Structure

- Modify `src/renderer/Texture.h/cpp`: add texture filtering helpers so Bloom targets can use linear filtering without changing GBuffer defaults.
- Modify `src/renderer/FrameContext.h`: add `BloomSettings` under `RenderSettings` and add a Bloom debug view enum.
- Modify `src/pipeline/PipelineOutputs.h`: add `BloomOutputs`.
- Modify `src/pipeline/passes/PostProcessPass.h/cpp`: consume the whole `PipelineOutputs` object and composite optional Bloom before tone mapping.
- Modify `shader/postprocess/postprocess.frag`: add exposure, Bloom composite, and Bloom debug view.
- Create `src/pipeline/techniques/BloomTechnique.h/cpp`: own bright-pass and blur framebuffers/textures.
- Create `shader/bloom/bright_extract.frag`: threshold HDR input into a half-resolution bright texture.
- Create `shader/bloom/blur.frag`: separable blur shader for the bright texture.
- Modify `src/pipeline/RenderPipeline.h/cpp`: own and execute `BloomTechnique`.
- Modify `src/ui/DebugUI.cpp`: expose Bloom controls by editing `state.renderSettings.bloom`.
- Modify `src/app/InputController.cpp`: add numeric debug key `7` for Bloom debug view.
- Modify `AGENTS.md` and `docs/architecture.md`: document the new technique boundary after implementation.

## Task 1: Add Contracts and Texture Filtering

**Files:**
- Modify: `src/renderer/Texture.h`
- Modify: `src/renderer/Texture.cpp`
- Modify: `src/renderer/FrameContext.h`
- Modify: `src/pipeline/PipelineOutputs.h`

- [ ] **Step 1: Add filtering helpers to `Texture`**

In `src/renderer/Texture.h`, add these public methods after `GenerateMipmaps()`:

```cpp
    void SetFilter(GLenum minFilter, GLenum magFilter) const;
    void SetWrap(GLenum wrapS, GLenum wrapT) const;
```

In `src/renderer/Texture.cpp`, add these definitions after `Texture::GenerateMipmaps()`:

```cpp
void Texture::SetFilter(GLenum minFilter, GLenum magFilter) const {
    glTextureParameteri(id_, GL_TEXTURE_MIN_FILTER, minFilter);
    glTextureParameteri(id_, GL_TEXTURE_MAG_FILTER, magFilter);
}

void Texture::SetWrap(GLenum wrapS, GLenum wrapT) const {
    glTextureParameteri(id_, GL_TEXTURE_WRAP_S, wrapS);
    glTextureParameteri(id_, GL_TEXTURE_WRAP_T, wrapT);
}
```

- [ ] **Step 2: Add Bloom settings and debug enum**

In `src/renderer/FrameContext.h`, extend `DebugView`:

```cpp
enum class DebugView {
    Final = 0,
    Albedo = 1,
    Normal = 2,
    Roughness = 3,
    Metallic = 4,
    Depth = 5,
    Cascades = 6,
    Bloom = 7,
};
```

Add `BloomSettings` before `RenderSettings`:

```cpp
struct BloomSettings {
    bool enabled = true;
    float threshold = 1.0f;
    float intensity = 0.08f;
    int radius = 5;
};
```

Then add this field to `RenderSettings` after `exposure`:

```cpp
    BloomSettings bloom;
```

- [ ] **Step 3: Add Bloom output contract**

In `src/pipeline/PipelineOutputs.h`, add this struct after `LightingOutputs`:

```cpp
struct BloomOutputs {
    std::shared_ptr<Texture> bloom;
};
```

Then extend `PipelineOutputs`:

```cpp
struct PipelineOutputs {
    ShadowOutputs shadow;
    GBufferOutputs gbuffer;
    LightingOutputs lighting;
    BloomOutputs bloom;
};
```

- [ ] **Step 4: Build check**

Run:

```powershell
cmake --build build --config Debug
```

Expected: build succeeds. Existing MSVC `C4819` warnings may remain.

- [ ] **Step 5: Commit**

```powershell
git add src/renderer/Texture.h src/renderer/Texture.cpp src/renderer/FrameContext.h src/pipeline/PipelineOutputs.h
git commit -m "refactor: add bloom technique contracts"
```

## Task 2: Make PostProcess Consume PipelineOutputs

**Files:**
- Modify: `src/pipeline/passes/PostProcessPass.h`
- Modify: `src/pipeline/passes/PostProcessPass.cpp`
- Modify: `shader/postprocess/postprocess.frag`
- Modify: `src/pipeline/RenderPipeline.cpp`

- [ ] **Step 1: Change the PostProcess API**

Replace the `Render` declaration in `src/pipeline/passes/PostProcessPass.h` with:

```cpp
    void Render(const PipelineOutputs& outputs,
                const FrameContext& frame);
```

- [ ] **Step 2: Update `PostProcessPass.cpp` to unpack outputs**

Replace the function signature and first local lines with this shape:

```cpp
void PostProcessPass::Render(const PipelineOutputs& outputs,
                             const FrameContext& frame) {
    const LightingOutputs& lighting = outputs.lighting;
    const GBufferOutputs& gbuffer = outputs.gbuffer;
    const ShadowOutputs& shadow = outputs.shadow;
    const BloomOutputs& bloom = outputs.bloom;

    Framebuffer::BindDefault();
    Renderer::SetViewport(0, 0, frame.width, frame.height);
    Renderer::EnableDepthTest(false);
    Renderer::EnableCullFace(false);
```

After setting `uDebugMode`, add:

```cpp
    const bool bloomAvailable = frame.renderSettings.bloom.enabled &&
                                bloom.bloom != nullptr;
    shader_->SetBool("uBloomEnabled", bloomAvailable);
    shader_->SetFloat("uBloomIntensity", frame.renderSettings.bloom.intensity);
    shader_->SetFloat("uExposure", frame.renderSettings.exposure);
```

After binding `gbuffer.depth` to slot 3, bind Bloom only when available:

```cpp
    if (bloomAvailable) {
        bloom.bloom->Bind(4);
    }
```

- [ ] **Step 3: Update `RenderPipeline.cpp` call site**

Replace:

```cpp
    postProcessPass_.Render(outputs_.lighting, outputs_.gbuffer,
                            outputs_.shadow, frame);
```

with:

```cpp
    postProcessPass_.Render(outputs_, frame);
```

- [ ] **Step 4: Update postprocess shader uniforms and final color path**

In `shader/postprocess/postprocess.frag`, add:

```glsl
layout(binding = 4) uniform sampler2D uBloomInput;

uniform bool uBloomEnabled;
uniform float uBloomIntensity;
uniform float uExposure;
```

Update the debug mode comment to include mode 7:

```glsl
uniform int uDebugMode;    // 0=Final, 1=Albedo, 2=Normal, 3=Roughness, 4=Metallic, 5=Depth, 6=Cascades, 7=Bloom
```

Replace the final-color branch with:

```glsl
    if (uDebugMode == 0) {
        vec3 hdr = texture(uHDRInput, vUV).rgb;
        if (uBloomEnabled) {
            hdr += texture(uBloomInput, vUV).rgb * uBloomIntensity;
        }
        hdr *= uExposure;

        vec3 color;
        if (uToneMapMode == 0)      color = ACESFilmic(hdr);
        else if (uToneMapMode == 1) color = Reinhard(hdr);
        else                        color = clamp(hdr, 0.0, 1.0);
        color = pow(color, vec3(1.0 / 2.2));
        FragColor = vec4(color, 1.0);
        return;
    }
```

Before the magenta fallback, add:

```glsl
    if (uDebugMode == 7) {
        vec3 bloom = uBloomEnabled ? texture(uBloomInput, vUV).rgb : vec3(0.0);
        FragColor = vec4(bloom, 1.0);
        return;
    }
```

- [ ] **Step 5: Build check**

Run:

```powershell
cmake --build build --config Debug
```

Expected: build succeeds. Visual output should be unchanged because no Bloom output exists yet.

- [ ] **Step 6: Commit**

```powershell
git add src/pipeline/passes/PostProcessPass.h src/pipeline/passes/PostProcessPass.cpp shader/postprocess/postprocess.frag src/pipeline/RenderPipeline.cpp
git commit -m "refactor: route postprocess through pipeline outputs"
```

## Task 3: Add BloomTechnique and Shaders

**Files:**
- Create: `src/pipeline/techniques/BloomTechnique.h`
- Create: `src/pipeline/techniques/BloomTechnique.cpp`
- Create: `shader/bloom/bright_extract.frag`
- Create: `shader/bloom/blur.frag`

- [ ] **Step 1: Create `BloomTechnique.h`**

```cpp
#pragma once
#include "../../renderer/FrameContext.h"
#include "../PipelineOutputs.h"
#include <memory>

namespace HuanGL {

class Framebuffer;
class Shader;
class Texture;
class VertexArray;

class BloomTechnique {
public:
    void Init(int width, int height);
    void Resize(int width, int height);

    BloomOutputs Execute(const FrameContext& frame,
                         const PipelineOutputs& inputs,
                         const BloomSettings& settings);

    BloomOutputs GetOutputs() const { return outputs_; }

private:
    void CreateResources(int width, int height);
    void DrawFullscreen() const;

    int width_ = 0;
    int height_ = 0;

    std::unique_ptr<Shader> extractShader_;
    std::unique_ptr<Shader> blurShader_;
    std::unique_ptr<VertexArray> dummyVAO_;

    std::unique_ptr<Framebuffer> brightFBO_;
    std::unique_ptr<Framebuffer> pingFBO_;
    std::unique_ptr<Framebuffer> pongFBO_;

    std::shared_ptr<Texture> brightTexture_;
    std::shared_ptr<Texture> pingTexture_;
    std::shared_ptr<Texture> pongTexture_;

    BloomOutputs outputs_;
};

} // namespace HuanGL
```

- [ ] **Step 2: Create `bright_extract.frag`**

```glsl
#version 460 core
in vec2 vUV;
out vec4 FragColor;

layout(binding = 0) uniform sampler2D uHDRInput;
uniform float uThreshold;

void main() {
    vec3 hdr = texture(uHDRInput, vUV).rgb;
    float luma = dot(hdr, vec3(0.2126, 0.7152, 0.0722));
    float contribution = max(luma - uThreshold, 0.0);
    vec3 bright = luma > 0.0 ? hdr * (contribution / luma) : vec3(0.0);
    FragColor = vec4(bright, 1.0);
}
```

- [ ] **Step 3: Create `blur.frag`**

```glsl
#version 460 core
in vec2 vUV;
out vec4 FragColor;

layout(binding = 0) uniform sampler2D uInput;
uniform bool uHorizontal;
uniform int uRadius;
uniform vec2 uTexelSize;

void main() {
    int radius = clamp(uRadius, 1, 16);
    vec2 direction = uHorizontal ? vec2(uTexelSize.x, 0.0)
                                 : vec2(0.0, uTexelSize.y);

    vec3 color = texture(uInput, vUV).rgb;
    float totalWeight = 1.0;

    for (int i = 1; i <= radius; ++i) {
        float x = float(i);
        float weight = exp(-(x * x) / 32.0);
        vec2 offset = direction * x;
        color += texture(uInput, vUV + offset).rgb * weight;
        color += texture(uInput, vUV - offset).rgb * weight;
        totalWeight += weight * 2.0;
    }

    FragColor = vec4(color / totalWeight, 1.0);
}
```

- [ ] **Step 4: Create `BloomTechnique.cpp`**

```cpp
#include "BloomTechnique.h"
#include "../../renderer/Buffer.h"
#include "../../renderer/Framebuffer.h"
#include "../../renderer/Renderer.h"
#include "../../renderer/Shader.h"
#include "../../renderer/Texture.h"
#include <algorithm>
#include <stdexcept>

namespace HuanGL {

void BloomTechnique::Init(int width, int height) {
    extractShader_ = std::make_unique<Shader>("../shader/lighting/fullscreen.vert",
                                              "../shader/bloom/bright_extract.frag");
    blurShader_ = std::make_unique<Shader>("../shader/lighting/fullscreen.vert",
                                           "../shader/bloom/blur.frag");
    dummyVAO_ = std::make_unique<VertexArray>();
    CreateResources(width, height);
}

void BloomTechnique::Resize(int width, int height) {
    CreateResources(width, height);
}

void BloomTechnique::CreateResources(int width, int height) {
    width_ = std::max(1, width / 2);
    height_ = std::max(1, height / 2);

    auto makeTarget = [this]() {
        auto texture = Texture::Create2D(width_, height_, GL_RGBA16F, GL_RGBA, GL_FLOAT);
        texture->SetFilter(GL_LINEAR, GL_LINEAR);
        texture->SetWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
        return texture;
    };

    brightTexture_ = makeTarget();
    pingTexture_ = makeTarget();
    pongTexture_ = makeTarget();

    brightFBO_ = std::make_unique<Framebuffer>(width_, height_);
    brightFBO_->AttachColor(brightTexture_);
    brightFBO_->SetDrawBuffers({GL_COLOR_ATTACHMENT0});
    if (!brightFBO_->IsComplete()) {
        throw std::runtime_error("[BloomTechnique] bright framebuffer incomplete");
    }

    pingFBO_ = std::make_unique<Framebuffer>(width_, height_);
    pingFBO_->AttachColor(pingTexture_);
    pingFBO_->SetDrawBuffers({GL_COLOR_ATTACHMENT0});
    if (!pingFBO_->IsComplete()) {
        throw std::runtime_error("[BloomTechnique] ping framebuffer incomplete");
    }

    pongFBO_ = std::make_unique<Framebuffer>(width_, height_);
    pongFBO_->AttachColor(pongTexture_);
    pongFBO_->SetDrawBuffers({GL_COLOR_ATTACHMENT0});
    if (!pongFBO_->IsComplete()) {
        throw std::runtime_error("[BloomTechnique] pong framebuffer incomplete");
    }

    outputs_ = {};
}

void BloomTechnique::DrawFullscreen() const {
    dummyVAO_->Bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);
    dummyVAO_->Unbind();
}

BloomOutputs BloomTechnique::Execute(const FrameContext& frame,
                                     const PipelineOutputs& inputs,
                                     const BloomSettings& settings) {
    outputs_ = {};
    if (!settings.enabled || !inputs.lighting.hdrColor) {
        return outputs_;
    }

    const int radius = std::clamp(settings.radius, 1, 16);

    Renderer::SetViewport(0, 0, width_, height_);
    Renderer::EnableDepthTest(false);
    Renderer::EnableCullFace(false);

    brightFBO_->Bind();
    Renderer::Clear(true, false, false);
    extractShader_->Use();
    extractShader_->SetFloat("uThreshold", settings.threshold);
    inputs.lighting.hdrColor->Bind(0);
    DrawFullscreen();

    blurShader_->Use();
    blurShader_->SetInt("uRadius", radius);
    blurShader_->SetVec2("uTexelSize", glm::vec2(1.0f / width_, 1.0f / height_));

    pingFBO_->Bind();
    Renderer::Clear(true, false, false);
    blurShader_->SetBool("uHorizontal", true);
    brightTexture_->Bind(0);
    DrawFullscreen();

    pongFBO_->Bind();
    Renderer::Clear(true, false, false);
    blurShader_->SetBool("uHorizontal", false);
    pingTexture_->Bind(0);
    DrawFullscreen();

    Framebuffer::BindDefault();
    Renderer::SetViewport(0, 0, frame.width, frame.height);
    Renderer::EnableCullFace(true);
    Renderer::EnableDepthTest(true);

    outputs_.bloom = pongTexture_;
    return outputs_;
}

} // namespace HuanGL
```

- [ ] **Step 5: Add missing include for glm vec2**

If `BloomTechnique.cpp` does not compile because `glm::vec2` is incomplete, add:

```cpp
#include <glm/vec2.hpp>
```

next to the other includes in `BloomTechnique.cpp`.

- [ ] **Step 6: Reconfigure and build**

New `.cpp` files require CMake configure because the project uses `GLOB_RECURSE`.

Run:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

Expected: build succeeds and produces `build\Debug\HuanGL.exe` for Visual Studio generator or `build\HuanGL.exe` for Ninja generator.

- [ ] **Step 7: Commit**

```powershell
git add src/pipeline/techniques/BloomTechnique.h src/pipeline/techniques/BloomTechnique.cpp shader/bloom/bright_extract.frag shader/bloom/blur.frag
git commit -m "feat: add bloom technique module"
```

## Task 4: Wire BloomTechnique into RenderPipeline

**Files:**
- Modify: `src/pipeline/RenderPipeline.h`
- Modify: `src/pipeline/RenderPipeline.cpp`

- [ ] **Step 1: Include and own the technique**

In `src/pipeline/RenderPipeline.h`, add:

```cpp
#include "techniques/BloomTechnique.h"
```

Then add this member after `PostProcessPass postProcessPass_;`:

```cpp
    BloomTechnique bloomTechnique_;
```

- [ ] **Step 2: Add the history invalidation API**

In the public section of `src/pipeline/RenderPipeline.h`, add this method after `Resize`:

```cpp
    void InvalidateHistory();
```

In `src/pipeline/RenderPipeline.cpp`, add this definition after `Resize`:

```cpp
void RenderPipeline::InvalidateHistory() {
    // History-based techniques will reset their temporal resources here.
}
```

- [ ] **Step 3: Initialize and resize the technique**

In `RenderPipeline::Init`, after `lightingPass_.Init(w, h, hdrPath);`, add:

```cpp
    bloomTechnique_.Init(w, h);
```

In `RenderPipeline::Resize`, after `lightingPass_.Resize(w, h);`, add:

```cpp
    bloomTechnique_.Resize(w, h);
```

- [ ] **Step 4: Execute Bloom before PostProcess**

In `RenderPipeline::Execute`, after `outputs_.lighting = ...`, add:

```cpp
    outputs_.bloom = bloomTechnique_.Execute(frame, outputs_,
                                             frame.renderSettings.bloom);
```

Then keep the final postprocess call as:

```cpp
    postProcessPass_.Render(outputs_, frame);
```

- [ ] **Step 5: Build check**

Run:

```powershell
cmake --build build --config Debug
```

Expected: build succeeds.

- [ ] **Step 6: Commit**

```powershell
git add src/pipeline/RenderPipeline.h src/pipeline/RenderPipeline.cpp
git commit -m "refactor: execute bloom as pipeline technique"
```

## Task 5: Add UI Controls and Docs

**Files:**
- Modify: `src/ui/DebugUI.cpp`
- Modify: `src/app/InputController.cpp`
- Modify: `AGENTS.md`
- Modify: `docs/architecture.md`

- [ ] **Step 1: Extend debug view selection**

In `src/ui/DebugUI.cpp`, replace the debug modes list with:

```cpp
        static const char* debugModes[] = {
            "Final", "Albedo", "Normal", "Roughness",
            "Metallic", "Depth", "Cascades", "Bloom"
        };
        int debugMode = DebugViewIndex(state.debugSettings.view);
        if (ImGui::Combo("Debug Mode", &debugMode, debugModes, 8)) {
            state.debugSettings.view = DebugViewFromIndex(debugMode);
        }
```

- [ ] **Step 2: Add a Techniques section**

In `DebugUI::Draw`, after the Render section and before Lighting, add:

```cpp
    if (ImGui::CollapsingHeader("Techniques")) {
        ImGui::Checkbox("Bloom", &state.renderSettings.bloom.enabled);
        ImGui::DragFloat("Bloom Threshold", &state.renderSettings.bloom.threshold,
                         0.05f, 0.0f, 20.0f);
        ImGui::DragFloat("Bloom Intensity", &state.renderSettings.bloom.intensity,
                         0.01f, 0.0f, 5.0f);
        ImGui::SliderInt("Bloom Radius", &state.renderSettings.bloom.radius,
                         1, 16);
        ImGui::DragFloat("Exposure", &state.renderSettings.exposure,
                         0.01f, 0.0f, 10.0f);
    }
```

- [ ] **Step 3: Add keyboard debug key 7**

In `src/app/InputController.cpp`, after the key 6 block, add:

```cpp
    if (Input::IsKeyJustPressed(GLFW_KEY_7) || Input::IsKeyJustPressed(GLFW_KEY_KP_7)) {
        state.debugSettings.view = DebugView::Bloom;
    }
```

- [ ] **Step 4: Update `AGENTS.md`**

Add this key technical decision near the existing pipeline decisions:

```markdown
- Optional rendering algorithms live as concrete technique modules under `src/pipeline/techniques/`; do not add a full render graph or RHI.
```

Add this row to the current progress section:

```markdown
| `src/pipeline/techniques/BloomTechnique.h/cpp` | First concrete technique module, bright-pass plus separable blur Bloom |
```

- [ ] **Step 5: Update `docs/architecture.md`**

In the Render Pipeline section, update the flow so Bloom appears between Lighting and PostProcess:

```text
LightingPass -> LightingOutputs (RGBA16F HDR)
BloomTechnique -> BloomOutputs (half-resolution blurred bright buffer)
PostProcessPass -> composite + tone map + debug views
```

Add a short decision note:

```markdown
**Concrete technique modules.** Optional algorithms such as Bloom, TAA,
RSM, SSGI, VXGI, and DDGI live under `src/pipeline/techniques/`.
They own their resources and expose typed output structs. `RenderPipeline`
keeps the frame order explicit and does not become the owner of every
algorithm's internal framebuffers.
```

- [ ] **Step 6: Build and boundary checks**

Run:

```powershell
cmake --build build --config Debug
rg -n "GetPostProcess|GetLighting|SetBloom|BloomTechnique" src/ui src/core src/app
rg -n "ImGui::|<imgui.h>" src/pipeline src/renderer src/scene src/app
```

Expected:
- Build succeeds.
- First `rg` may show `BloomTechnique` only if a forbidden include leaked; expected result is no matches.
- Second `rg` has no matches, proving ImGui stayed in `src/ui`.

- [ ] **Step 7: Commit**

```powershell
git add src/ui/DebugUI.cpp src/app/InputController.cpp AGENTS.md docs/architecture.md
git commit -m "feat: expose bloom technique controls"
```

## Task 6: Final Verification

**Files:**
- No code edits unless verification exposes a defect.

- [ ] **Step 1: Full configure and build**

Run:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

Expected: build succeeds. Existing `C4819` warnings may remain.

- [ ] **Step 2: Contract boundary search**

Run:

```powershell
rg -n "GetPostProcess|GetLighting|GetHDROutput|GetAlbedoMetallic|GetNormalRoughness|GetShadowMapArray|GetCascades" src
rg -n "ImGui::|<imgui.h>" src/pipeline src/renderer src/scene src/app
rg -n "class IRenderTechnique|RenderGraph|RHI" src
```

Expected:
- First command has no matches.
- Second command has no matches.
- Third command has no matches.

- [ ] **Step 3: Optional runtime smoke test**

Run the app from the build output if an OpenGL window is available:

```powershell
.\build\Debug\HuanGL.exe
```

Expected:
- App starts.
- Final view still renders.
- Debug Mode `Bloom` shows a black or bright-only texture depending on scene brightness and threshold.
- Toggling Bloom off returns to the pre-Bloom postprocess path.
- Resizing the window does not crash.

- [ ] **Step 4: Final status**

Run:

```powershell
git status --short --branch --untracked-files=all
```

Expected: clean worktree on `codex/technique-architecture`.
