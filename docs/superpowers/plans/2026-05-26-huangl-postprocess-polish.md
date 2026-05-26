# HuanGL Post-Process Polish Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a first Temporal Anti-Aliasing stage, AgX tone mapping, and the small pipeline/input fixes needed to make Phase 4 visibly more polished.

**Architecture:** The fixed `IPipelineStage` vector remains the orchestration model. `LightingStage` still emits raw HDR radiance, `TAAStage` resolves it against history, and Bloom/PostProcess consume the resolved HDR texture when available. Camera jitter lives in `FrameContext::camera`, while history invalidation is exposed through `RenderPipeline::InvalidateHistory()`.

**Tech Stack:** C++17, OpenGL 4.6 Direct State Access, GLAD2, GLFW, GLM, Dear ImGui, CMake/vcpkg on Windows.

---

## File Structure

- Create `src/pipeline/stages/TAAStage.h`: `IPipelineStage` implementation declaration, `TAAOutputs`, owned history resources, and history invalidation state.
- Create `src/pipeline/stages/TAAStage.cpp`: history texture allocation, fullscreen resolve execution, ping-pong copy, and fallback output behavior.
- Create `shader/taa/resolve.frag`: current HDR + depth reprojection resolve with 3x3 neighborhood clamp.
- Modify `src/renderer/FrameContext.h`: add `TAASettings`, add `AgX`, and include TAA settings in `RenderSettings`.
- Modify `src/renderer/UniformBuffer.h`: extend `CameraData` with unjittered, inverse view-proj, previous view-proj, and jitter data.
- Modify `shader/common/uniforms.glsl`: keep the GLSL UBO layout in sync with `CameraData`.
- Modify `src/core/Camera.h` and `src/core/Camera.cpp`: let `Camera::GetData()` accept a jitter offset and fill jittered/unjittered camera matrices.
- Modify `src/core/App.h` and `src/core/App.cpp`: own temporal camera state, generate Halton jitter, advance previous camera state after rendering, and invalidate history on scene changes.
- Modify `src/app/InputController.h` and `src/app/InputController.cpp`: pass current `deltaTime` directly into input handling.
- Read `src/app/SceneRegistry.h`: use the existing active scene index accessor for scene-switch detection.
- Modify `src/pipeline/IPipelineStage.h`: add a virtual no-op `InvalidateHistory()`.
- Modify `src/pipeline/PipelineResources.h`: improve missing-resource diagnostics.
- Modify `src/pipeline/RenderPipeline.h` and `src/pipeline/RenderPipeline.cpp`: add `TAAStage` to the order and expose `InvalidateHistory()`.
- Modify `src/pipeline/stages/BloomStage.cpp`: read `TAAOutputs` and use resolved HDR when available.
- Modify `src/pipeline/stages/PostProcessStage.cpp`: read `TAAOutputs` and use resolved HDR for final composition.
- Modify `src/ui/DebugUI.cpp`: add TAA controls and AgX tone-map option.
- Modify `shader/postprocess/postprocess.frag`: add AgX tone mapping and update mode comments.
- Modify `AGENTS.md` and `docs/architecture.md`: document the new Phase 4 pipeline state.

---

### Task 1: Add Runtime Settings and Fix Camera Delta Time Input

**Files:**
- Modify: `src/renderer/FrameContext.h`
- Modify: `src/app/InputController.h`
- Modify: `src/app/InputController.cpp`
- Modify: `src/core/App.cpp`

- [ ] **Step 1: Extend render settings**

In `src/renderer/FrameContext.h`, replace the current `ToneMapMode`, `BloomSettings`, and `RenderSettings` blocks with:

```cpp
enum class ToneMapMode {
    ACES = 0,
    Reinhard = 1,
    AgX = 2,
    None = 3,
};

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

inline int ToShaderToneMapMode(ToneMapMode mode) {
    return static_cast<int>(mode);
}

inline int ToShaderDebugView(DebugView view) {
    return static_cast<int>(view);
}

struct TAASettings {
    bool enabled = true;
    float feedback = 0.90f;
};

struct BloomSettings {
    bool enabled = true;
    float threshold = 1.0f;
    float softKnee = 0.5f;
    float intensity = 0.08f;
    int radius = 5;
    int mipCount = 5;
};

struct RenderSettings {
    ToneMapMode toneMapMode = ToneMapMode::ACES;
    float ambientStrength = 1.0f;
    int shadowResolution = 2048;
    float exposure = 1.0f;
    TAASettings taa;
    BloomSettings bloom;

    void CycleToneMap() {
        int next = (ToShaderToneMapMode(toneMapMode) + 1) % 4;
        toneMapMode = static_cast<ToneMapMode>(next);
    }
};
```

- [ ] **Step 2: Change `InputController` signature**

In `src/app/InputController.h`, replace the public method declaration with:

```cpp
void Update(ApplicationState& state, float deltaTime);
```

- [ ] **Step 3: Use current delta time for camera movement**

In `src/app/InputController.cpp`, change the function signature to:

```cpp
void InputController::Update(ApplicationState& state, float deltaTime) {
```

Then replace the final camera move call:

```cpp
state.camera.Move(move, state.frameStats.deltaTime);
```

with:

```cpp
state.camera.Move(move, deltaTime);
```

- [ ] **Step 4: Pass `dt` from the app loop**

In `src/core/App.cpp`, replace:

```cpp
inputController_.Update(state_);
```

with:

```cpp
inputController_.Update(state_, dt);
```

- [ ] **Step 5: Build**

Run:

```powershell
cmake --build build --config Debug
```

Expected: build succeeds and links `HuanGL.exe`.

- [ ] **Step 6: Commit**

Run:

```powershell
git add src/renderer/FrameContext.h src/app/InputController.h src/app/InputController.cpp src/core/App.cpp
git commit -m "feat: add TAA settings and fix camera delta time"
```

---

### Task 2: Add Jittered Camera Data

**Files:**
- Modify: `src/renderer/UniformBuffer.h`
- Modify: `shader/common/uniforms.glsl`
- Modify: `src/core/Camera.h`
- Modify: `src/core/Camera.cpp`
- Modify: `src/core/App.h`
- Modify: `src/core/App.cpp`

- [ ] **Step 1: Extend `CameraData`**

In `src/renderer/UniformBuffer.h`, replace `struct CameraData` with:

```cpp
struct CameraData {
    glm::mat4 view {};
    glm::mat4 proj {};
    glm::mat4 viewProj {};
    glm::mat4 invView {};
    glm::mat4 invProj {};
    glm::mat4 invViewProj {};
    glm::mat4 unjitteredProj {};
    glm::mat4 unjitteredViewProj {};
    glm::mat4 prevViewProj {};
    glm::vec4 jitter {}; // xy = current jitter, zw = previous jitter
    glm::vec3 camPos {};
    float near_ = 0.1f;
    float far_ = 100.f;
    float pad[3] = {};
};
```

- [ ] **Step 2: Keep GLSL UBO in sync**

In `shader/common/uniforms.glsl`, replace the `CameraUBO` block with:

```glsl
layout(std140, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    mat4 invView;
    mat4 invProj;
    mat4 invViewProj;
    mat4 unjitteredProj;
    mat4 unjitteredViewProj;
    mat4 prevViewProj;
    vec4 jitter; // xy = current jitter, zw = previous jitter
    vec3 camPos;
    float near_;
    float far_;
    float _pad[3];
};
```

- [ ] **Step 3: Update the camera API**

In `src/core/Camera.h`, replace:

```cpp
CameraData GetData(float aspect) const;
```

with:

```cpp
CameraData GetData(float aspect, glm::vec2 jitter = glm::vec2(0.0f)) const;
```

- [ ] **Step 4: Fill jittered and unjittered matrices**

In `src/core/Camera.cpp`, replace `Camera::GetData` with:

```cpp
CameraData Camera::GetData(float aspect, glm::vec2 jitter) const {
    CameraData d;
    d.view = glm::lookAt(pos_, pos_ + front_, worldUp_);
    d.unjitteredProj = glm::perspective(fov_, aspect, near_, far_);
    d.proj = d.unjitteredProj;
    d.proj[2][0] += jitter.x;
    d.proj[2][1] += jitter.y;
    d.viewProj = d.proj * d.view;
    d.unjitteredViewProj = d.unjitteredProj * d.view;
    d.invView = glm::inverse(d.view);
    d.invProj = glm::inverse(d.proj);
    d.invViewProj = glm::inverse(d.viewProj);
    d.prevViewProj = d.viewProj;
    d.jitter = glm::vec4(jitter, 0.0f, 0.0f);
    d.camPos = pos_;
    d.near_ = near_;
    d.far_ = far_;
    return d;
}
```

- [ ] **Step 5: Add temporal camera state to `App`**

In `src/core/App.h`, add this include:

```cpp
#include <cstdint>
#include <glm/glm.hpp>
```

Add these private declarations:

```cpp
glm::vec2 ComputeTAAJitter(int width, int height) const;
void StorePreviousCameraState(const CameraData& camera);
void InvalidateTemporalHistory();
```

Add these private members:

```cpp
glm::mat4 previousViewProj_ = glm::mat4(1.0f);
glm::vec2 previousJitter_ = glm::vec2(0.0f);
uint32_t taaFrameIndex_ = 0;
bool hasPreviousCamera_ = false;
```

- [ ] **Step 6: Make `BuildFrameContext` non-const**

In `src/core/App.h`, replace:

```cpp
FrameContext BuildFrameContext(float dt) const;
```

with:

```cpp
FrameContext BuildFrameContext(float dt);
```

In `src/core/App.cpp`, replace:

```cpp
FrameContext App::BuildFrameContext(float dt) const {
```

with:

```cpp
FrameContext App::BuildFrameContext(float dt) {
```

- [ ] **Step 7: Add Halton jitter helpers**

In `src/core/App.cpp`, add these file-local helpers after `namespace HuanGL {`:

```cpp
namespace {

float Halton(uint32_t index, uint32_t base) {
    float f = 1.0f;
    float result = 0.0f;
    while (index > 0) {
        f /= static_cast<float>(base);
        result += f * static_cast<float>(index % base);
        index /= base;
    }
    return result;
}

} // namespace
```

Then add these `App` methods before `BuildFrameContext`:

```cpp
glm::vec2 App::ComputeTAAJitter(int width, int height) const {
    if (!state_.renderSettings.taa.enabled || width <= 0 || height <= 0) {
        return glm::vec2(0.0f);
    }

    const uint32_t sampleIndex = (taaFrameIndex_ % 8u) + 1u;
    glm::vec2 sample {
        Halton(sampleIndex, 2u) - 0.5f,
        Halton(sampleIndex, 3u) - 0.5f
    };
    return glm::vec2(
        sample.x * 2.0f / static_cast<float>(width),
        sample.y * 2.0f / static_cast<float>(height)
    );
}

void App::StorePreviousCameraState(const CameraData& camera) {
    previousViewProj_ = camera.viewProj;
    previousJitter_ = glm::vec2(camera.jitter.x, camera.jitter.y);
    hasPreviousCamera_ = true;
    ++taaFrameIndex_;
}

void App::InvalidateTemporalHistory() {
    hasPreviousCamera_ = false;
    taaFrameIndex_ = 0;
    previousJitter_ = glm::vec2(0.0f);
    previousViewProj_ = glm::mat4(1.0f);
}
```

- [ ] **Step 8: Use jitter in `BuildFrameContext`**

In `App::BuildFrameContext`, replace:

```cpp
frame.camera = state_.camera.GetData(aspect);
```

with:

```cpp
const glm::vec2 jitter = ComputeTAAJitter(frame.width, frame.height);
frame.camera = state_.camera.GetData(aspect, jitter);
frame.camera.prevViewProj = hasPreviousCamera_ ? previousViewProj_ : frame.camera.viewProj;
frame.camera.jitter.z = previousJitter_.x;
frame.camera.jitter.w = previousJitter_.y;
```

- [ ] **Step 9: Store previous camera after rendering**

In `App::Render`, after:

```cpp
pipeline_->Execute(sceneView, frame);
```

add:

```cpp
StorePreviousCameraState(frame.camera);
```

- [ ] **Step 10: Build**

Run:

```powershell
cmake --build build --config Debug
```

Expected: build succeeds and links `HuanGL.exe`.

- [ ] **Step 11: Commit**

Run:

```powershell
git add src/renderer/UniformBuffer.h shader/common/uniforms.glsl src/core/Camera.h src/core/Camera.cpp src/core/App.h src/core/App.cpp
git commit -m "feat: add jittered camera frame data"
```

---

### Task 3: Add History Invalidation and Better Resource Diagnostics

**Files:**
- Modify: `src/pipeline/IPipelineStage.h`
- Modify: `src/pipeline/PipelineResources.h`
- Modify: `src/pipeline/RenderPipeline.h`
- Modify: `src/pipeline/RenderPipeline.cpp`

- [ ] **Step 1: Add stage history invalidation hook**

In `src/pipeline/IPipelineStage.h`, add this virtual method after `Resize`:

```cpp
virtual void InvalidateHistory() {}
```

The interface should become:

```cpp
class IPipelineStage {
public:
    virtual ~IPipelineStage() = default;
    virtual const char* GetName() const = 0;
    virtual void Init(int width, int height) = 0;
    virtual void Resize(int width, int height) = 0;
    virtual void InvalidateHistory() {}
    virtual void Execute(PipelineResources& resources, const FrameContext& frame) = 0;
};
```

- [ ] **Step 2: Improve missing-resource diagnostics**

In `src/pipeline/PipelineResources.h`, add:

```cpp
#include <string>
```

Then replace `Get()` with:

```cpp
template<typename T>
const T& Get() const {
    auto it = resources_.find(std::type_index(typeid(T)));
    if (it == resources_.end()) {
        throw std::runtime_error(
            std::string("PipelineResources: missing resource: ") + typeid(T).name());
    }
    return std::any_cast<const T&>(it->second);
}
```

- [ ] **Step 3: Add pipeline invalidation API**

In `src/pipeline/RenderPipeline.h`, add this public method after `Resize`:

```cpp
void InvalidateHistory();
```

- [ ] **Step 4: Implement pipeline invalidation**

In `src/pipeline/RenderPipeline.cpp`, add this method after `Resize`:

```cpp
void RenderPipeline::InvalidateHistory() {
    for (auto& stage : stages_) {
        stage->InvalidateHistory();
    }
}
```

- [ ] **Step 5: Build**

Run:

```powershell
cmake --build build --config Debug
```

Expected: build succeeds and links `HuanGL.exe`.

- [ ] **Step 6: Commit**

Run:

```powershell
git add src/pipeline/IPipelineStage.h src/pipeline/PipelineResources.h src/pipeline/RenderPipeline.h src/pipeline/RenderPipeline.cpp
git commit -m "feat: add pipeline history invalidation hook"
```

---

### Task 4: Add TAA Stage and Resolve Shader

**Files:**
- Create: `src/pipeline/stages/TAAStage.h`
- Create: `src/pipeline/stages/TAAStage.cpp`
- Create: `shader/taa/resolve.frag`
- Modify: `src/pipeline/RenderPipeline.cpp`

- [ ] **Step 1: Create `TAAStage.h`**

Create `src/pipeline/stages/TAAStage.h`:

```cpp
#pragma once
#include "../IPipelineStage.h"
#include "../../renderer/Buffer.h"
#include "../../renderer/Framebuffer.h"
#include "../../renderer/Shader.h"
#include "../../renderer/Texture.h"
#include <array>
#include <memory>

namespace HuanGL {

struct TAAOutputs {
    std::shared_ptr<Texture> resolvedHdr;
};

class TAAStage : public IPipelineStage {
public:
    const char* GetName() const override { return "TAAStage"; }
    void Init(int width, int height) override;
    void Resize(int width, int height) override;
    void InvalidateHistory() override;
    void Execute(PipelineResources& resources, const FrameContext& frame) override;

private:
    void CreateResources(int width, int height);
    void DrawFullscreen() const;

    std::unique_ptr<Shader> shader_;
    std::unique_ptr<VertexArray> dummyVAO_;
    std::shared_ptr<Texture> resolved_;
    std::unique_ptr<Framebuffer> resolvedFBO_;
    std::array<std::shared_ptr<Texture>, 2> history_;

    int width_ = 0;
    int height_ = 0;
    int historyReadIndex_ = 0;
    bool historyValid_ = false;
    bool wasEnabled_ = false;
};

} // namespace HuanGL
```

- [ ] **Step 2: Create `TAAStage.cpp`**

Create `src/pipeline/stages/TAAStage.cpp`:

```cpp
#include "TAAStage.h"
#include "GBufferStage.h"
#include "LightingStage.h"
#include "../PipelineResources.h"
#include "../../renderer/Buffer.h"
#include "../../renderer/FrameContext.h"
#include "../../renderer/Renderer.h"
#include <algorithm>
#include <stdexcept>

namespace HuanGL {

void TAAStage::Init(int width, int height) {
    shader_ = std::make_unique<Shader>("lighting/fullscreen.vert", "taa/resolve.frag");
    dummyVAO_ = std::make_unique<VertexArray>();
    CreateResources(width, height);
}

void TAAStage::Resize(int width, int height) {
    CreateResources(width, height);
}

void TAAStage::InvalidateHistory() {
    historyValid_ = false;
    wasEnabled_ = false;
}

void TAAStage::CreateResources(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);

    resolved_ = Texture::Create2D(width_, height_, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    resolved_->SetFilter(GL_LINEAR, GL_LINEAR);
    resolved_->SetWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

    resolvedFBO_ = std::make_unique<Framebuffer>(width_, height_);
    resolvedFBO_->AttachColor(resolved_, 0);
    resolvedFBO_->SetDrawBuffers({GL_COLOR_ATTACHMENT0});
    if (!resolvedFBO_->IsComplete()) {
        throw std::runtime_error("[TAAStage] resolved framebuffer incomplete");
    }

    for (auto& historyTexture : history_) {
        historyTexture = Texture::Create2D(width_, height_, GL_RGBA16F, GL_RGBA, GL_FLOAT);
        historyTexture->SetFilter(GL_LINEAR, GL_LINEAR);
        historyTexture->SetWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
    }

    historyReadIndex_ = 0;
    InvalidateHistory();
}

void TAAStage::DrawFullscreen() const {
    dummyVAO_->Bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);
    dummyVAO_->Unbind();
}

void TAAStage::Execute(PipelineResources& resources, const FrameContext& frame) {
    const auto& lighting = resources.Get<LightingOutputs>();
    const auto& gbuffer = resources.Get<GBufferOutputs>();

    TAAOutputs outputs;
    if (!frame.renderSettings.taa.enabled || !lighting.hdrColor || !gbuffer.depth) {
        InvalidateHistory();
        resources.Set(outputs);
        return;
    }

    if (!wasEnabled_) {
        historyValid_ = false;
    }

    const int historyWriteIndex = 1 - historyReadIndex_;

    resolvedFBO_->Bind();
    Renderer::SetViewport(0, 0, width_, height_);
    Renderer::Clear(true, false, false);
    Renderer::EnableDepthTest(false);
    Renderer::EnableCullFace(false);

    shader_->Use();
    lighting.hdrColor->Bind(0);
    gbuffer.depth->Bind(1);
    history_[historyReadIndex_]->Bind(2);
    shader_->SetMat4("uInvViewProj", frame.camera.invViewProj);
    shader_->SetMat4("uPrevViewProj", frame.camera.prevViewProj);
    shader_->SetBool("uHistoryValid", historyValid_);
    shader_->SetFloat("uFeedback", std::clamp(frame.renderSettings.taa.feedback, 0.0f, 0.98f));
    DrawFullscreen();

    glCopyImageSubData(resolved_->GetID(), GL_TEXTURE_2D, 0, 0, 0, 0,
                       history_[historyWriteIndex]->GetID(), GL_TEXTURE_2D, 0, 0, 0, 0,
                       width_, height_, 1);

    Framebuffer::BindDefault();
    Renderer::SetViewport(0, 0, frame.width, frame.height);
    Renderer::EnableCullFace(true);
    Renderer::EnableDepthTest(true);

    historyReadIndex_ = historyWriteIndex;
    historyValid_ = true;
    wasEnabled_ = true;
    outputs.resolvedHdr = resolved_;
    resources.Set(outputs);
}

} // namespace HuanGL
```

- [ ] **Step 3: Create resolve shader**

Create `shader/taa/resolve.frag`:

```glsl
#version 460 core
in vec2 vUV;
out vec4 FragColor;

layout(binding = 0) uniform sampler2D uCurrentHdr;
layout(binding = 1) uniform sampler2D uDepth;
layout(binding = 2) uniform sampler2D uHistory;

uniform mat4 uInvViewProj;
uniform mat4 uPrevViewProj;
uniform bool uHistoryValid;
uniform float uFeedback;

vec3 WorldPosFromDepth(vec2 uv, float depth, mat4 invVP) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 world = invVP * clip;
    return world.xyz / world.w;
}

void NeighborhoodBounds(vec2 uv, out vec3 mn, out vec3 mx) {
    ivec2 size = textureSize(uCurrentHdr, 0);
    vec2 texel = 1.0 / vec2(size);
    mn = vec3(1e20);
    mx = vec3(-1e20);
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec3 c = texture(uCurrentHdr, uv + vec2(x, y) * texel).rgb;
            mn = min(mn, c);
            mx = max(mx, c);
        }
    }
}

void main() {
    vec3 current = texture(uCurrentHdr, vUV).rgb;

    if (!uHistoryValid) {
        FragColor = vec4(current, 1.0);
        return;
    }

    float depth = texture(uDepth, vUV).r;
    if (depth >= 1.0 - 1e-6) {
        FragColor = vec4(current, 1.0);
        return;
    }

    vec3 worldPos = WorldPosFromDepth(vUV, depth, uInvViewProj);
    vec4 prevClip = uPrevViewProj * vec4(worldPos, 1.0);
    if (prevClip.w <= 0.0) {
        FragColor = vec4(current, 1.0);
        return;
    }

    vec2 historyUV = prevClip.xy / prevClip.w * 0.5 + 0.5;
    if (any(lessThan(historyUV, vec2(0.0))) || any(greaterThan(historyUV, vec2(1.0)))) {
        FragColor = vec4(current, 1.0);
        return;
    }

    vec3 history = texture(uHistory, historyUV).rgb;
    vec3 mn, mx;
    NeighborhoodBounds(vUV, mn, mx);
    history = clamp(history, mn, mx);

    vec3 resolved = mix(current, history, clamp(uFeedback, 0.0, 0.98));
    FragColor = vec4(resolved, 1.0);
}
```

- [ ] **Step 4: Register `TAAStage`**

In `src/pipeline/RenderPipeline.cpp`, add:

```cpp
#include "stages/TAAStage.h"
```

Then in `BuildStages`, insert TAA after Lighting:

```cpp
stages_.push_back(std::make_unique<ShadowStage>(2048));
stages_.push_back(std::make_unique<GBufferStage>());
stages_.push_back(std::make_unique<LightingStage>(hdrPath));
stages_.push_back(std::make_unique<TAAStage>());
stages_.push_back(std::make_unique<BloomStage>());
stages_.push_back(std::make_unique<PostProcessStage>());
```

- [ ] **Step 5: Build**

Run:

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

Expected: configure sees the new `TAAStage.cpp`; build succeeds and links `HuanGL.exe`.

- [ ] **Step 6: Commit**

Run:

```powershell
git add src/pipeline/stages/TAAStage.h src/pipeline/stages/TAAStage.cpp shader/taa/resolve.frag src/pipeline/RenderPipeline.cpp
git commit -m "feat: add temporal anti-aliasing stage"
```

---

### Task 5: Route TAA Resolved HDR into Bloom and PostProcess

**Files:**
- Modify: `src/pipeline/stages/BloomStage.cpp`
- Modify: `src/pipeline/stages/PostProcessStage.cpp`

- [ ] **Step 1: Include TAA outputs in Bloom**

In `src/pipeline/stages/BloomStage.cpp`, add:

```cpp
#include "TAAStage.h"
```

- [ ] **Step 2: Select HDR source in Bloom**

In `BloomStage::Execute`, after:

```cpp
const auto& lighting = resources.Get<LightingOutputs>();
```

add:

```cpp
const auto& taa = resources.Get<TAAOutputs>();
const std::shared_ptr<Texture>& hdrInput = taa.resolvedHdr ? taa.resolvedHdr : lighting.hdrColor;
```

Replace:

```cpp
if (!settings.enabled || !lighting.hdrColor || downMips_.empty()) {
```

with:

```cpp
if (!settings.enabled || !hdrInput || downMips_.empty()) {
```

Replace:

```cpp
lighting.hdrColor->Bind(0);
```

with:

```cpp
hdrInput->Bind(0);
```

- [ ] **Step 3: Include TAA outputs in PostProcess**

In `src/pipeline/stages/PostProcessStage.cpp`, add:

```cpp
#include "TAAStage.h"
```

- [ ] **Step 4: Select HDR source in PostProcess**

In `PostProcessStage::Execute`, after:

```cpp
const auto& lighting = resources.Get<LightingOutputs>();
```

add:

```cpp
const auto& taa = resources.Get<TAAOutputs>();
const std::shared_ptr<Texture>& hdrInput = taa.resolvedHdr ? taa.resolvedHdr : lighting.hdrColor;
```

Replace:

```cpp
lighting.hdrColor->Bind(0);
```

with:

```cpp
hdrInput->Bind(0);
```

- [ ] **Step 5: Build**

Run:

```powershell
cmake --build build --config Debug
```

Expected: build succeeds and links `HuanGL.exe`.

- [ ] **Step 6: Commit**

Run:

```powershell
git add src/pipeline/stages/BloomStage.cpp src/pipeline/stages/PostProcessStage.cpp
git commit -m "feat: feed TAA output into post processing"
```

---

### Task 6: Add AgX Tone Mapping and UI Controls

**Files:**
- Modify: `shader/postprocess/postprocess.frag`
- Modify: `src/ui/DebugUI.cpp`

- [ ] **Step 1: Add AgX GLSL helper**

In `shader/postprocess/postprocess.frag`, replace:

```glsl
uniform int uToneMapMode;  // 0=ACES, 1=Reinhard, 2=None
```

with:

```glsl
uniform int uToneMapMode;  // 0=ACES, 1=Reinhard, 2=AgX, 3=None
```

After `Reinhard`, add:

```glsl
vec3 AgXDefaultContrastApprox(vec3 x) {
    vec3 x2 = x * x;
    vec3 x4 = x2 * x2;
    return 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 -
           6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232;
}

vec3 AgX(vec3 color) {
    const mat3 agxInputMatrix = mat3(
        0.842479062253094, 0.0423282422610123, 0.0423756549057051,
        0.0784335999999992, 0.878468636469772, 0.0784336,
        0.0792237451477643, 0.0791661274605434, 0.879142973793104);

    const mat3 agxOutputMatrix = mat3(
        1.19687900512017, -0.0528968517574562, -0.0529716355144438,
        -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
        -0.0990297440797205, -0.0989611768448433, 1.15107367264116);

    color = agxInputMatrix * color;
    color = max(color, vec3(1e-10));
    color = log2(color);
    color = (color + 12.47393) / (12.47393 + 4.026069);
    color = clamp(color, 0.0, 1.0);
    color = AgXDefaultContrastApprox(color);
    color = agxOutputMatrix * color;
    return max(color, vec3(0.0));
}
```

- [ ] **Step 2: Wire AgX mode**

In the final-mode tone-map block, replace:

```glsl
if (uToneMapMode == 0)      color = ACESFilmic(hdr);
else if (uToneMapMode == 1) color = Reinhard(hdr);
else                        color = clamp(hdr, 0.0, 1.0);
```

with:

```glsl
if (uToneMapMode == 0)      color = ACESFilmic(hdr);
else if (uToneMapMode == 1) color = Reinhard(hdr);
else if (uToneMapMode == 2) color = AgX(hdr);
else                        color = clamp(hdr, 0.0, 1.0);
```

- [ ] **Step 3: Add AgX to DebugUI tone-map combo**

In `src/ui/DebugUI.cpp`, replace:

```cpp
static const char* toneModes[] = { "ACES", "Reinhard", "None" };
int toneMode = ToneMapIndex(state.renderSettings.toneMapMode);
if (ImGui::Combo("Tone Map", &toneMode, toneModes, 3)) {
```

with:

```cpp
static const char* toneModes[] = { "ACES", "Reinhard", "AgX", "None" };
int toneMode = ToneMapIndex(state.renderSettings.toneMapMode);
if (ImGui::Combo("Tone Map", &toneMode, toneModes, 4)) {
```

- [ ] **Step 4: Add TAA controls to DebugUI**

In the `Techniques` collapsing header, before the Bloom controls, add:

```cpp
ImGui::Checkbox("TAA", &state.renderSettings.taa.enabled);
ImGui::SliderFloat("TAA Feedback", &state.renderSettings.taa.feedback,
                   0.0f, 0.98f, "%.2f");
```

- [ ] **Step 5: Build**

Run:

```powershell
cmake --build build --config Debug
```

Expected: build succeeds and links `HuanGL.exe`.

- [ ] **Step 6: Commit**

Run:

```powershell
git add shader/postprocess/postprocess.frag src/ui/DebugUI.cpp
git commit -m "feat: add AgX tone mapping and TAA controls"
```

---

### Task 7: Add Scene-Switch Invalidation, Documentation, and Verification

**Files:**
- Read: `src/app/SceneRegistry.h`
- Modify: `src/core/App.cpp`
- Modify: `AGENTS.md`
- Modify: `docs/architecture.md`

- [ ] **Step 1: Confirm active scene index API**

Open `src/app/SceneRegistry.h` and confirm this method already exists:

```cpp
size_t GetActiveIndex() const { return activeSceneIdx_; }
```

- [ ] **Step 2: Forward app invalidation to pipeline stages**

In `App::InvalidateTemporalHistory`, after:

```cpp
previousViewProj_ = glm::mat4(1.0f);
```

add:

```cpp
if (pipeline_) {
    pipeline_->InvalidateHistory();
}
```

- [ ] **Step 3: Invalidate history after keyboard scene changes**

In `src/core/App.cpp`, replace:

```cpp
inputController_.Update(state_, dt);
```

with:

```cpp
const size_t sceneBeforeInput = state_.sceneRegistry.GetActiveIndex();
inputController_.Update(state_, dt);
if (!state_.sceneRegistry.Empty() &&
    sceneBeforeInput != state_.sceneRegistry.GetActiveIndex()) {
    InvalidateTemporalHistory();
}
```

- [ ] **Step 4: Invalidate history after DebugUI scene changes**

In `App::Run`, replace:

```cpp
imguiLayer_->BeginFrame();
debugUI_->Draw(state_);
imguiLayer_->EndFrame();
```

with:

```cpp
imguiLayer_->BeginFrame();
const size_t sceneBeforeUI = state_.sceneRegistry.GetActiveIndex();
debugUI_->Draw(state_);
if (!state_.sceneRegistry.Empty() &&
    sceneBeforeUI != state_.sceneRegistry.GetActiveIndex()) {
    InvalidateTemporalHistory();
}
imguiLayer_->EndFrame();
```

- [ ] **Step 5: Invalidate history on resize**

In the resize callback in `App::Init`, replace:

```cpp
if (w > 0 && h > 0 && pipeline_)
    pipeline_->Resize(w, h);
```

with:

```cpp
if (w > 0 && h > 0 && pipeline_) {
    pipeline_->Resize(w, h);
    InvalidateTemporalHistory();
}
```

- [ ] **Step 6: Update `AGENTS.md` inventory**

In `AGENTS.md`, add rows to the Phase 3.5/4 file inventory for:

```markdown
| `src/pipeline/stages/TAAStage.h/cpp` | Temporal Anti-Aliasing stage with jittered reprojection, history ping-pong, and neighborhood clamp |
| `shader/taa/resolve.frag` | TAA resolve shader using depth reprojection and 3x3 history clamp |
```

Update the Phase 4 status row to:

```markdown
| 4 | In Progress | Bloom, TAA, improved tone mapping |
```

- [ ] **Step 7: Update `docs/architecture.md` pipeline diagram**

In `docs/architecture.md`, update the pipeline section so the middle of the diagram reads:

```text
        └─► LightingStage   ← reads GBufferOutputs + ShadowOutputs + IBL
             │                 writes LightingOutputs (RGBA16F HDR target)
             ▼
        TAAStage           → TAAOutputs
             │                 resolved HDR target with temporal history
             ▼
        BloomStage          → BloomOutputs
             │                 multi-mip HDR bloom texture (level 0 composite)
             ▼
        PostProcessStage    ← reads TAA/Lighting/GBuffer/Shadow/Bloom outputs
             │                 composite + tone map + gamma + debug overlay
             ▼
        Backbuffer
```

- [ ] **Step 8: Update `docs/architecture.md` Phase 4 deliverables**

In the Phase 4 section, replace the TAA and tone-map deliverables with:

```markdown
- Temporal Anti-Aliasing through `TAAStage`: 8-sample Halton jitter,
  depth reprojection, RGBA16F history ping-pong, resize/scene-switch
  invalidation, and 3x3 neighborhood history clamp.
- Additional tone-map operator: AgX, selectable alongside ACES,
  Reinhard, and linear output.
```

- [ ] **Step 9: Run MSVC verification build**

Run:

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

Expected: configure succeeds and build links `build\HuanGL.exe`.

- [ ] **Step 10: Run clang-cl verification build**

Run:

```powershell
cmake -S . -B build-clang -G Ninja `
  -DCMAKE_C_COMPILER=clang-cl `
  -DCMAKE_CXX_COMPILER=clang-cl `
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-clang
```

Expected: configure succeeds and build links `build-clang\HuanGL.exe`.

- [ ] **Step 11: Runtime smoke check**

Run:

```powershell
.\build\HuanGL.exe
```

Expected manual checks:

- App opens on the active scene.
- `RMB` camera movement works and does not feel one frame behind.
- `T` cycles ACES, Reinhard, AgX, and None.
- DebugUI TAA checkbox toggles final output without black frames.
- Resize the window; final output remains valid after resize.
- Press `N`; the new scene does not show a stale ghost of the previous scene.
- Bloom on/off still changes final output and debug view `7`.

- [ ] **Step 12: Commit**

Run:

```powershell
git add src/core/App.cpp AGENTS.md docs/architecture.md
git commit -m "docs: document post-process polish pipeline"
```
