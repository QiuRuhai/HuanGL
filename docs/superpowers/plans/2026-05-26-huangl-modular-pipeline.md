# Modular Pipeline Architecture Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Refactor HuanGL's monolithic render pipeline into a modular stage-based architecture with typed resource passing, enabling incremental addition of future rendering techniques.

**Architecture:** Each render pass/technique becomes an `IPipelineStage` implementation that reads and writes typed resources through a `PipelineResources` registry. `RenderPipeline` iterates a `vector<unique_ptr<IPipelineStage>>` instead of hardcoding member passes. Camera is decoupled from GLFW, shader paths are centralized.

**Tech Stack:** C++17, OpenGL 4.6, GLAD2, GLM, GLFW, Assimp, Dear ImGui. No test framework — verification is compile + visual.

**Spec:** `docs/superpowers/specs/2026-05-26-huangl-modular-pipeline-design.md`

---

### Task 1: Create IPipelineStage Interface

**Files:**
- Create: `src/pipeline/IPipelineStage.h`

- [ ] **Step 1: Create the interface header**

```cpp
// src/pipeline/IPipelineStage.h
#pragma once

namespace HuanGL {

class PipelineResources;
struct FrameContext;

class IPipelineStage {
public:
    virtual ~IPipelineStage() = default;
    virtual const char* GetName() const = 0;
    virtual void Init(int width, int height) = 0;
    virtual void Resize(int width, int height) = 0;
    virtual void Execute(PipelineResources& resources, const FrameContext& frame) = 0;
};

} // namespace HuanGL
```

- [ ] **Step 2: Verify compilation**

Run:
```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

Expected: builds successfully. The new header is not included anywhere yet so it has no effect.

- [ ] **Step 3: Commit**

```powershell
git add src/pipeline/IPipelineStage.h
git commit -m "refactor: add IPipelineStage interface"
```

---

### Task 2: Create PipelineResources Registry

**Files:**
- Create: `src/pipeline/PipelineResources.h`

- [ ] **Step 1: Create the typed resource registry**

```cpp
// src/pipeline/PipelineResources.h
#pragma once
#include <any>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>

namespace HuanGL {

class PipelineResources {
public:
    template<typename T>
    void Set(T resource) {
        resources_[std::type_index(typeid(T))] = std::move(resource);
    }

    template<typename T>
    const T& Get() const {
        auto it = resources_.find(std::type_index(typeid(T)));
        if (it == resources_.end())
            throw std::runtime_error("PipelineResources: missing resource");
        return std::any_cast<const T&>(it->second);
    }

    template<typename T>
    bool Has() const {
        return resources_.count(std::type_index(typeid(T))) > 0;
    }

    void Clear() { resources_.clear(); }

private:
    std::unordered_map<std::type_index, std::any> resources_;
};

} // namespace HuanGL
```

- [ ] **Step 2: Verify compilation**

Run:
```powershell
cmake --build build --config Debug
```

Expected: builds successfully. Not included anywhere yet.

- [ ] **Step 3: Commit**

```powershell
git add src/pipeline/PipelineResources.h
git commit -m "refactor: add PipelineResources typed registry"
```

---

### Task 3: Add Shader Base Path

**Files:**
- Modify: `src/renderer/Shader.h`
- Modify: `src/renderer/Shader.cpp`
- Modify: `src/core/App.cpp`

- [ ] **Step 1: Add static base path member and setter to Shader.h**

Add after the `GetID()` method, inside the `private:` section:

```cpp
// In Shader.h, add to public section:
    static void SetBasePath(const std::string& basePath);

// In Shader.h, add to private section:
    static std::string basePath_;
```

- [ ] **Step 2: Implement SetBasePath and update Compile in Shader.cpp**

Add the static member definition and setter at the top of `Shader.cpp` (after the namespace opening):

```cpp
std::string Shader::basePath_;

void Shader::SetBasePath(const std::string& basePath) {
    basePath_ = basePath;
}
```

Update `Shader::Compile` to prepend the base path. Change the first line of `Compile`:

```cpp
GLuint Shader::Compile(const std::string& path, GLenum type) const {
    std::string resolvedPath = basePath_.empty() ? path : basePath_ + path;
    std::string src = ReadFile(resolvedPath);
```

Also update the error message in Compile to use `resolvedPath`:

```cpp
        throw std::runtime_error("[Shader] Compile error in " + resolvedPath + ":\n" + log);
```

And update ReadFile's error to use the resolved path — but since ReadFile receives whatever string it's given, the error in ReadFile already shows the path correctly. No change needed there since Compile now passes `resolvedPath` to `ReadFile`.

Wait — `ReadFile` is called with the raw `path` parameter currently. We need to pass `resolvedPath` instead. Let me correct: change the line from `std::string src = ReadFile(path);` to `std::string src = ReadFile(resolvedPath);`.

- [ ] **Step 3: Call SetBasePath in App::Init()**

In `src/core/App.cpp`, in `App::Init()`, add before any pipeline/scene initialization (right after `Renderer::Init()`):

```cpp
    Shader::SetBasePath("../shader/");
```

Also add `#include "../renderer/Shader.h"` at the top of App.cpp if not already present. (It is not currently present — App.cpp includes Renderer.h but not Shader.h.)

- [ ] **Step 4: Update all shader paths to remove the "../shader/" prefix**

These are the exact files and paths to change:

**`src/pipeline/passes/ShadowPass.cpp:23-24`** — Change:
```cpp
    shader_ = std::make_unique<Shader>("shadow/csm.vert",
                                       "shadow/csm.frag");
```

**`src/pipeline/passes/GBufferPass.cpp:13-14`** — Change:
```cpp
    shader_ = std::make_unique<Shader>("gbuffer/gbuffer.vert",
                                       "gbuffer/gbuffer.frag");
```

**`src/pipeline/passes/LightingPass.cpp:45-52`** — Change:
```cpp
    pbrShader_ = std::make_unique<Shader>("lighting/fullscreen.vert",
                                          "lighting/pbr_ibl.frag");
    irrShader_ = std::make_unique<Shader>("lighting/cube.vert",
                                          "lighting/irradiance.frag");
    pfShader_ = std::make_unique<Shader>("lighting/cube.vert",
                                         "lighting/prefilter.frag");
    brdfShader_ = std::make_unique<Shader>("lighting/fullscreen.vert",
                                           "lighting/brdf_lut.frag");
```

**`src/pipeline/passes/LightingPass.cpp:91`** (inside `GenerateIBL`) — Change:
```cpp
    Shader eqShader("lighting/cube.vert",
                    "lighting/equirect_to_cubemap.frag");
```

**`src/pipeline/passes/PostProcessPass.cpp:10-11`** — Change:
```cpp
    shader_ = std::make_unique<Shader>("lighting/fullscreen.vert",
                                       "postprocess/postprocess.frag");
```

**`src/pipeline/techniques/BloomTechnique.cpp:15-20`** — Change:
```cpp
    extractShader_ = std::make_unique<Shader>("lighting/fullscreen.vert",
                                              "bloom/bright_extract.frag");
    downsampleShader_ = std::make_unique<Shader>("lighting/fullscreen.vert",
                                                 "bloom/downsample.frag");
    upsampleShader_ = std::make_unique<Shader>("lighting/fullscreen.vert",
                                               "bloom/upsample.frag");
```

- [ ] **Step 5: Verify compilation and run**

```powershell
cmake --build build --config Debug
.\build\Debug\HuanGL.exe
```

Expected: compiles and renders the same as before. Shader path resolution is transparent.

- [ ] **Step 6: Commit**

```powershell
git add src/renderer/Shader.h src/renderer/Shader.cpp src/core/App.cpp src/pipeline/passes/ShadowPass.cpp src/pipeline/passes/GBufferPass.cpp src/pipeline/passes/LightingPass.cpp src/pipeline/passes/PostProcessPass.cpp src/pipeline/techniques/BloomTechnique.cpp
git commit -m "refactor: centralize shader base path"
```

---

### Task 4: Decouple Camera from GLFW

**Files:**
- Modify: `src/core/Camera.h` (rewrite to pure math, move impl to .cpp)
- Create: `src/core/Camera.cpp`
- Modify: `src/app/InputController.h`
- Modify: `src/app/InputController.cpp`
- Modify: `src/core/App.cpp`

- [ ] **Step 1: Rewrite Camera.h as a pure math header (no GLFW)**

Replace the entire contents of `src/core/Camera.h` with:

```cpp
#pragma once
#include "../renderer/UniformBuffer.h"
#include <glm/glm.hpp>

namespace HuanGL {

class Camera {
public:
    Camera(float fovDeg = 60.f, float nearP = 0.1f, float farP = 100.f);

    void Look(float yawDelta, float pitchDelta);
    void Move(glm::vec3 localDelta, float dt);

    CameraData GetData(float aspect) const;

    glm::vec3 GetPosition() const { return pos_; }
    float GetFov() const;
    void SetFov(float deg);

private:
    glm::vec3 pos_ = {0, 3, 10};
    glm::vec3 front_ = {0, 0, -1};
    glm::vec3 worldUp_ = {0, 1, 0};
    float yaw_ = -90.f;
    float pitch_ = 0.f;
    float fov_;
    float near_;
    float far_;
    float moveSpeed_ = 5.f;
};

} // namespace HuanGL
```

- [ ] **Step 2: Create Camera.cpp**

```cpp
// src/core/Camera.cpp
#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>

namespace HuanGL {

Camera::Camera(float fovDeg, float nearP, float farP)
    : fov_(glm::radians(fovDeg)), near_(nearP), far_(farP) {}

void Camera::Look(float yawDelta, float pitchDelta) {
    yaw_ += yawDelta;
    pitch_ += pitchDelta;
    pitch_ = glm::clamp(pitch_, -89.f, 89.f);

    front_.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    front_.y = sin(glm::radians(pitch_));
    front_.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    front_ = glm::normalize(front_);
}

void Camera::Move(glm::vec3 localDelta, float dt) {
    glm::vec3 right = glm::normalize(glm::cross(front_, worldUp_));
    glm::vec3 up = glm::cross(right, front_);
    float spd = moveSpeed_ * dt;
    pos_ += front_ * localDelta.z * spd;
    pos_ += right * localDelta.x * spd;
    pos_ += up * localDelta.y * spd;
}

CameraData Camera::GetData(float aspect) const {
    CameraData d;
    d.view     = glm::lookAt(pos_, pos_ + front_, worldUp_);
    d.proj     = glm::perspective(fov_, aspect, near_, far_);
    d.viewProj = d.proj * d.view;
    d.invView  = glm::inverse(d.view);
    d.invProj  = glm::inverse(d.proj);
    d.camPos   = pos_;
    d.near_    = near_;
    d.far_     = far_;
    return d;
}

float Camera::GetFov() const { return glm::degrees(fov_); }
void Camera::SetFov(float deg) { fov_ = glm::radians(deg); }

} // namespace HuanGL
```

- [ ] **Step 3: Update InputController to handle camera movement and RMB toggle**

Update `src/app/InputController.h`:

```cpp
#pragma once

namespace HuanGL {

struct ApplicationState;

class InputController {
public:
    void Update(ApplicationState& state);

private:
    bool wasCameraActive_ = false;
};

} // namespace HuanGL
```

Replace the entire contents of `src/app/InputController.cpp`:

```cpp
#include "InputController.h"
#include "ApplicationState.h"
#include "../core/Input.h"
#include <GLFW/glfw3.h>

namespace HuanGL {

void InputController::Update(ApplicationState& state) {
    if (Input::IsKeyPressed(GLFW_KEY_ESCAPE)) {
        state.running = false;
    }

    if (Input::IsKeyJustPressed(GLFW_KEY_N)) {
        state.sceneRegistry.Cycle();
    }

    if (Input::IsKeyJustPressed(GLFW_KEY_T)) {
        state.renderSettings.CycleToneMap();
    }

    if (Input::IsKeyJustPressed(GLFW_KEY_0) || Input::IsKeyJustPressed(GLFW_KEY_KP_0)) {
        state.debugSettings.view = DebugView::Final;
    }
    if (Input::IsKeyJustPressed(GLFW_KEY_1) || Input::IsKeyJustPressed(GLFW_KEY_KP_1)) {
        state.debugSettings.view = DebugView::Albedo;
    }
    if (Input::IsKeyJustPressed(GLFW_KEY_2) || Input::IsKeyJustPressed(GLFW_KEY_KP_2)) {
        state.debugSettings.view = DebugView::Normal;
    }
    if (Input::IsKeyJustPressed(GLFW_KEY_3) || Input::IsKeyJustPressed(GLFW_KEY_KP_3)) {
        state.debugSettings.view = DebugView::Roughness;
    }
    if (Input::IsKeyJustPressed(GLFW_KEY_4) || Input::IsKeyJustPressed(GLFW_KEY_KP_4)) {
        state.debugSettings.view = DebugView::Metallic;
    }
    if (Input::IsKeyJustPressed(GLFW_KEY_5) || Input::IsKeyJustPressed(GLFW_KEY_KP_5)) {
        state.debugSettings.view = DebugView::Depth;
    }
    if (Input::IsKeyJustPressed(GLFW_KEY_6) || Input::IsKeyJustPressed(GLFW_KEY_KP_6)) {
        state.debugSettings.view = DebugView::Cascades;
    }
    if (Input::IsKeyJustPressed(GLFW_KEY_7) || Input::IsKeyJustPressed(GLFW_KEY_KP_7)) {
        state.debugSettings.view = DebugView::Bloom;
    }

    // Right-click camera mode
    bool rmb = Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
    state.cameraActive = rmb;

    if (rmb && !wasCameraActive_) {
        Input::SetCursorCaptured(true);
    }
    if (!rmb && wasCameraActive_) {
        Input::SetCursorCaptured(false);
    }
    wasCameraActive_ = rmb;

    // Camera movement (only while RMB held and camera not frozen)
    if (state.cameraActive && !state.debugSettings.freezeCamera) {
        glm::vec2 delta = Input::GetMouseDelta();
        state.camera.Look(delta.x * 0.1f, delta.y * 0.1f);

        glm::vec3 move{0};
        if (Input::IsKeyPressed(GLFW_KEY_W)) move.z += 1.f;
        if (Input::IsKeyPressed(GLFW_KEY_S)) move.z -= 1.f;
        if (Input::IsKeyPressed(GLFW_KEY_A)) move.x -= 1.f;
        if (Input::IsKeyPressed(GLFW_KEY_D)) move.x += 1.f;
        if (Input::IsKeyPressed(GLFW_KEY_E)) move.y += 1.f;
        if (Input::IsKeyPressed(GLFW_KEY_Q)) move.y -= 1.f;
        state.camera.Move(move, state.frameStats.deltaTime);
    }
}

} // namespace HuanGL
```

- [ ] **Step 4: Update App.cpp — remove camera update and cursor capture**

In `src/core/App.cpp`, make these changes:

1. Remove `Input::SetCursorCaptured(true);` from `App::Init()`.

2. Replace the `App::Update` method:

```cpp
void App::Update(float dt) {
    if (Scene* scene = state_.sceneRegistry.GetActiveScene()) {
        scene->Update(dt);
    }

    state_.frameStats.deltaTime = dt;
    state_.frameStats.frameTimeMs = dt * 1000.0f;
    state_.frameStats.fps = dt > 0.0f ? 1.0f / dt : 0.0f;
}
```

This removes the line `state_.camera.Update(dt, window_->GetHandle(), ...)` which was the Camera/GLFW coupling. Camera movement is now handled entirely by `InputController::Update()` which runs before `App::Update()` in the main loop.

- [ ] **Step 5: Reconfigure CMake (new .cpp file)**

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Debug
```

- [ ] **Step 6: Build and verify**

```powershell
cmake --build build --config Debug
.\build\Debug\HuanGL.exe
```

Expected:
- App starts with visible cursor (not captured)
- Camera does NOT move when pressing WASD (because RMB is not held)
- Hold right mouse button: cursor hides, camera rotates with mouse, WASD moves camera
- Release right mouse button: cursor reappears, camera stops
- ImGui panels are clickable without holding RMB

- [ ] **Step 7: Commit**

```powershell
git add src/core/Camera.h src/core/Camera.cpp src/app/InputController.h src/app/InputController.cpp src/core/App.cpp
git commit -m "refactor: decouple Camera from GLFW, add right-click camera mode"
```

---

### Task 5: Create ShadowStage

**Files:**
- Create: `src/pipeline/stages/ShadowStage.h`
- Create: `src/pipeline/stages/ShadowStage.cpp`

The following Tasks 5 through 9 create new stage files alongside the old pass files. The old files are not yet deleted — compilation is unchanged because the new files are not included by anything yet. The switchover happens in Task 10.

- [ ] **Step 1: Create ShadowStage.h**

```cpp
// src/pipeline/stages/ShadowStage.h
#pragma once
#include "../IPipelineStage.h"
#include "../CascadeData.h"
#include "../../renderer/Schema.h"
#include <array>
#include <memory>
#include <glad/glad.h>

namespace HuanGL {

class Shader;
class Framebuffer;

struct ShadowOutputs {
    GLuint shadowArray = 0;
    std::array<CascadeData, 4> cascades {};
};

class ShadowStage : public IPipelineStage {
public:
    explicit ShadowStage(int resolution = 2048);
    ~ShadowStage() override;

    const char* GetName() const override { return "ShadowStage"; }
    void Init(int width, int height) override;
    void Resize(int width, int height) override;
    void Execute(PipelineResources& resources, const FrameContext& frame) override;

private:
    std::unique_ptr<Shader>      shader_;
    std::unique_ptr<Framebuffer> fbo_;
    GLuint                       shadowArrayID_ = 0;
    std::array<CascadeData, 4>   cascades_;
    int resolution_;
};

} // namespace HuanGL
```

- [ ] **Step 2: Create ShadowStage.cpp**

Port the logic from `src/pipeline/passes/ShadowPass.cpp`. The key changes:
- Constructor stores `resolution_` (previously passed to `Init`)
- `Init(int width, int height)` ignores width/height (shadow maps use their own resolution)
- `Execute()` reads `RenderSceneView` from `PipelineResources`, writes `ShadowOutputs`

```cpp
// src/pipeline/stages/ShadowStage.cpp
#include "ShadowStage.h"
#include "../PipelineResources.h"
#include "../../renderer/Shader.h"
#include "../../renderer/Framebuffer.h"
#include "../../renderer/Renderer.h"
#include "../../renderer/FrameContext.h"
#include "../../renderer/RenderSceneView.h"
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>

namespace HuanGL {

static std::array<float, 4> ComputeCascadeSplits(float nearP, float farP, float lambda = 0.75f) {
    std::array<float, 4> splits;
    for (int i = 0; i < 4; ++i) {
        float p = (i + 1) / 4.f;
        float logSplit  = nearP * pow(farP / nearP, p);
        float linSplit  = nearP + (farP - nearP) * p;
        splits[i] = glm::mix(linSplit, logSplit, lambda);
    }
    return splits;
}

static glm::mat4 LightViewProj(const DirectionalLight& light,
                                const std::array<glm::vec3, 8>& frustumCorners) {
    glm::vec3 center(0);
    for (auto& c : frustumCorners) center += c;
    center /= (float)frustumCorners.size();

    glm::vec3 lightPos = center - light.direction * 50.f;
    glm::mat4 lightView = glm::lookAt(lightPos, center, {0, 1, 0});

    glm::vec3 mn(1e9f), mx(-1e9f);
    for (auto& c : frustumCorners) {
        glm::vec3 ls = glm::vec3(lightView * glm::vec4(c, 1));
        mn = glm::min(mn, ls); mx = glm::max(mx, ls);
    }
    mn.z -= 50.f; mx.z += 50.f;

    glm::mat4 lightProj = glm::ortho(mn.x, mx.x, mn.y, mx.y, mn.z, mx.z);
    return lightProj * lightView;
}

static std::array<glm::vec3, 8> SubFrustumCorners(const glm::mat4& invViewProj,
                                                   float nearRatio, float farRatio) {
    std::array<glm::vec3, 8> full;
    int idx = 0;
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                glm::vec4 ndc(x * 2.f - 1.f, y * 2.f - 1.f,
                              z == 0 ? -1.f : 1.f, 1.f);
                glm::vec4 world = invViewProj * ndc;
                full[idx++] = glm::vec3(world) / world.w;
            }
        }
    }
    std::array<glm::vec3, 8> sub;
    for (int i = 0; i < 4; ++i) {
        sub[i]     = glm::mix(full[i], full[i + 4], nearRatio);
        sub[i + 4] = glm::mix(full[i], full[i + 4], farRatio);
    }
    return sub;
}

ShadowStage::ShadowStage(int resolution) : resolution_(resolution) {}

ShadowStage::~ShadowStage() {
    if (shadowArrayID_) glDeleteTextures(1, &shadowArrayID_);
}

void ShadowStage::Init(int /*width*/, int /*height*/) {
    shader_ = std::make_unique<Shader>("shadow/csm.vert", "shadow/csm.frag");

    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &shadowArrayID_);
    glTextureStorage3D(shadowArrayID_, 1, GL_DEPTH_COMPONENT24,
                       resolution_, resolution_, 4);
    glTextureParameteri(shadowArrayID_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(shadowArrayID_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(shadowArrayID_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTextureParameteri(shadowArrayID_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border[] = {1,1,1,1};
    glTextureParameterfv(shadowArrayID_, GL_TEXTURE_BORDER_COLOR, border);
    glTextureParameteri(shadowArrayID_, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTextureParameteri(shadowArrayID_, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    fbo_ = std::make_unique<Framebuffer>(resolution_, resolution_);
    glNamedFramebufferTexture(fbo_->GetID(), GL_DEPTH_ATTACHMENT, shadowArrayID_, 0);
    glNamedFramebufferDrawBuffer(fbo_->GetID(), GL_NONE);
    glNamedFramebufferReadBuffer(fbo_->GetID(), GL_NONE);
    if (!fbo_->IsComplete())
        throw std::runtime_error("[ShadowStage] FBO incomplete");
}

void ShadowStage::Resize(int /*width*/, int /*height*/) {
    // Shadow maps use their own fixed resolution, not the window size.
}

void ShadowStage::Execute(PipelineResources& resources, const FrameContext& frame) {
    const auto& scene = resources.Get<RenderSceneView>();
    const CameraData& camera = frame.camera;
    const DirectionalLight& light = scene.sunLight;
    auto splits = ComputeCascadeSplits(camera.near_, camera.far_);
    auto invVP  = glm::inverse(camera.viewProj);

    Renderer::EnableCullFace(true);
    Renderer::EnableDepthTest(true);
    Renderer::EnableDepthWrite(true);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 4.0f);

    shader_->Use();

    float prevSplit = camera.near_;
    float range = camera.far_ - camera.near_;
    for (int c = 0; c < 4; ++c) {
        float nearRatio = (prevSplit - camera.near_) / range;
        float farRatio  = (splits[c]  - camera.near_) / range;
        auto corners = SubFrustumCorners(invVP, nearRatio, farRatio);
        glm::mat4 lightVP = LightViewProj(light, corners);

        cascades_[c].viewProj = lightVP;
        cascades_[c].farPlane = splits[c];

        glNamedFramebufferTextureLayer(fbo_->GetID(), GL_DEPTH_ATTACHMENT,
                                       shadowArrayID_, 0, c);
        fbo_->Bind();
        Renderer::SetViewport(0, 0, resolution_, resolution_);
        Renderer::Clear(false, true, false);

        shader_->SetMat4("lightViewProj", lightVP);

        for (const auto& renderable : scene.renderables) {
            if (!renderable.mesh) continue;
            shader_->SetMat4("model", renderable.modelMatrix);
            const Mesh* mesh = renderable.mesh;
            mesh->vao->Bind();
            for (const auto& sub : mesh->subMeshes)
                glDrawElements(GL_TRIANGLES, sub.indexCount, GL_UNSIGNED_INT,
                               (void*)(uintptr_t)(sub.indexOffset * sizeof(uint32_t)));
            mesh->vao->Unbind();
        }
        prevSplit = splits[c];
    }

    glDisable(GL_POLYGON_OFFSET_FILL);
    Framebuffer::BindDefault();

    ShadowOutputs outputs;
    outputs.shadowArray = shadowArrayID_;
    outputs.cascades = cascades_;
    resources.Set(outputs);
}

} // namespace HuanGL
```

- [ ] **Step 3: Commit (does not affect build yet)**

```powershell
git add src/pipeline/stages/ShadowStage.h src/pipeline/stages/ShadowStage.cpp
git commit -m "refactor: add ShadowStage (port of ShadowPass)"
```

---

### Task 6: Create GBufferStage

**Files:**
- Create: `src/pipeline/stages/GBufferStage.h`
- Create: `src/pipeline/stages/GBufferStage.cpp`

- [ ] **Step 1: Create GBufferStage.h**

```cpp
// src/pipeline/stages/GBufferStage.h
#pragma once
#include "../IPipelineStage.h"
#include "../../renderer/Texture.h"
#include <memory>

namespace HuanGL {

class Shader;
class Framebuffer;

struct GBufferOutputs {
    std::shared_ptr<Texture> albedoMetallic;
    std::shared_ptr<Texture> normalRoughness;
    std::shared_ptr<Texture> depth;
};

class GBufferStage : public IPipelineStage {
public:
    const char* GetName() const override { return "GBufferStage"; }
    void Init(int width, int height) override;
    void Resize(int width, int height) override;
    void Execute(PipelineResources& resources, const FrameContext& frame) override;

private:
    std::unique_ptr<Framebuffer> fbo_;
    std::unique_ptr<Shader>      shader_;
    int width_ = 0, height_ = 0;
};

} // namespace HuanGL
```

- [ ] **Step 2: Create GBufferStage.cpp**

```cpp
// src/pipeline/stages/GBufferStage.cpp
#include "GBufferStage.h"
#include "../PipelineResources.h"
#include "../../renderer/Shader.h"
#include "../../renderer/Framebuffer.h"
#include "../../renderer/Renderer.h"
#include "../../renderer/Schema.h"
#include "../../renderer/FrameContext.h"
#include "../../renderer/RenderSceneView.h"
#include <stdexcept>
#include <glm/gtc/type_ptr.hpp>

namespace HuanGL {

void GBufferStage::Init(int w, int h) {
    width_ = w; height_ = h;
    shader_ = std::make_unique<Shader>("gbuffer/gbuffer.vert",
                                       "gbuffer/gbuffer.frag");
    fbo_ = std::make_unique<Framebuffer>(w, h);

    auto rt0 = Texture::Create2D(w, h, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
    auto rt1 = Texture::Create2D(w, h, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    auto dtx = Texture::Create2D(w, h, GL_DEPTH_COMPONENT24,
                                 GL_DEPTH_COMPONENT, GL_FLOAT);
    fbo_->AttachColor(rt0, 0);
    fbo_->AttachColor(rt1, 1);
    fbo_->AttachDepth(dtx);
    fbo_->SetDrawBuffers({GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1});
    if (!fbo_->IsComplete())
        throw std::runtime_error("[GBufferStage] FBO incomplete");
}

void GBufferStage::Resize(int w, int h) { Init(w, h); }

void GBufferStage::Execute(PipelineResources& resources, const FrameContext& frame) {
    const auto& scene = resources.Get<RenderSceneView>();

    fbo_->Bind();
    Renderer::SetViewport(0, 0, width_, height_);
    Renderer::Clear(true, true, false);
    Renderer::EnableDepthTest(true);
    Renderer::EnableDepthWrite(true);
    Renderer::SetDepthFunc(GL_LESS);

    shader_->Use();
    shader_->SetMat4("viewProj", frame.camera.viewProj);

    for (const auto& renderable : scene.renderables) {
        if (!renderable.mesh || !renderable.materials) continue;

        const Mesh* mesh = renderable.mesh;
        shader_->SetMat4("model", renderable.modelMatrix);

        mesh->vao->Bind();
        for (const auto& sub : mesh->subMeshes) {
            const Material& mat = (*renderable.materials)[sub.materialIndex];

            shader_->SetInt("uHasAlbedoTex",    mat.albedoMap    ? 1 : 0);
            shader_->SetInt("uHasRoughnessTex", mat.roughnessMap ? 1 : 0);
            shader_->SetInt("uHasMetallicTex",  mat.metallicMap  ? 1 : 0);
            shader_->SetInt("uHasNormalTex",    mat.normalMap    ? 1 : 0);
            shader_->SetInt("uPackedMetallicRoughness", mat.packedMetallicRoughness ? 1 : 0);
            shader_->SetVec4("uBaseColor",  mat.baseColorFactor);
            shader_->SetFloat("uRoughness", mat.roughnessFactor);
            shader_->SetFloat("uMetallic",  mat.metallicFactor);

            if (mat.albedoMap)    mat.albedoMap->Bind(0);
            if (mat.roughnessMap) mat.roughnessMap->Bind(1);
            if (mat.metallicMap)  mat.metallicMap->Bind(2);
            if (mat.normalMap)    mat.normalMap->Bind(3);

            glDrawElements(GL_TRIANGLES, sub.indexCount, GL_UNSIGNED_INT,
                           (void*)(uintptr_t)(sub.indexOffset * sizeof(uint32_t)));
        }
        mesh->vao->Unbind();
    }
    Framebuffer::BindDefault();

    GBufferOutputs outputs;
    outputs.albedoMetallic = fbo_->GetColor(0);
    outputs.normalRoughness = fbo_->GetColor(1);
    outputs.depth = fbo_->GetDepth();
    resources.Set(outputs);
}

} // namespace HuanGL
```

- [ ] **Step 3: Commit**

```powershell
git add src/pipeline/stages/GBufferStage.h src/pipeline/stages/GBufferStage.cpp
git commit -m "refactor: add GBufferStage (port of GBufferPass)"
```

---

### Task 7: Create LightingStage

**Files:**
- Create: `src/pipeline/stages/LightingStage.h`
- Create: `src/pipeline/stages/LightingStage.cpp`

- [ ] **Step 1: Create LightingStage.h**

```cpp
// src/pipeline/stages/LightingStage.h
#pragma once
#include "../IPipelineStage.h"
#include "../../renderer/Texture.h"
#include <memory>
#include <string>
#include <glm/glm.hpp>

namespace HuanGL {

class Shader;
class VertexArray;
class Buffer;
class Framebuffer;

struct LightingOutputs {
    std::shared_ptr<Texture> hdrColor;
};

class LightingStage : public IPipelineStage {
public:
    explicit LightingStage(std::string hdrPath);

    const char* GetName() const override { return "LightingStage"; }
    void Init(int width, int height) override;
    void Resize(int width, int height) override;
    void Execute(PipelineResources& resources, const FrameContext& frame) override;

private:
    void GenerateIBL(const std::string& hdrPath);
    void CreateHDRFBO(int w, int h);

    std::string hdrPath_;
    std::unique_ptr<Shader> pbrShader_;
    std::unique_ptr<Shader> irrShader_;
    std::unique_ptr<Shader> pfShader_;
    std::unique_ptr<Shader> brdfShader_;
    std::shared_ptr<Texture> irradianceMap_;
    std::shared_ptr<Texture> prefilterMap_;
    std::shared_ptr<Texture> brdfLUT_;
    std::unique_ptr<Framebuffer> captureFBO_;
    std::unique_ptr<Framebuffer> hdrFBO_;
    std::unique_ptr<VertexArray> cubeVAO_;
    std::unique_ptr<Buffer>      cubeVBO_;
    std::unique_ptr<VertexArray> dummyVAO_;
    int width_ = 0, height_ = 0;
};

} // namespace HuanGL
```

- [ ] **Step 2: Create LightingStage.cpp**

Port from `LightingPass.cpp`. The constructor stores `hdrPath_`. `Init()` calls `GenerateIBL(hdrPath_)`. `Execute()` reads `GBufferOutputs`, `ShadowOutputs`, and `RenderSceneView` from resources, writes `LightingOutputs`.

```cpp
// src/pipeline/stages/LightingStage.cpp
#include "LightingStage.h"
#include "ShadowStage.h"
#include "GBufferStage.h"
#include "../PipelineResources.h"
#include "../../renderer/Shader.h"
#include "../../renderer/Framebuffer.h"
#include "../../renderer/Buffer.h"
#include "../../renderer/Renderer.h"
#include "../../renderer/FrameContext.h"
#include "../../renderer/RenderSceneView.h"
#include <glm/gtc/matrix_transform.hpp>

namespace HuanGL {

static const glm::mat4 kCaptureViews[6] = {
    glm::lookAt(glm::vec3(0,0,0), glm::vec3( 1, 0, 0), glm::vec3(0,-1, 0)),
    glm::lookAt(glm::vec3(0,0,0), glm::vec3(-1, 0, 0), glm::vec3(0,-1, 0)),
    glm::lookAt(glm::vec3(0,0,0), glm::vec3( 0, 1, 0), glm::vec3(0, 0, 1)),
    glm::lookAt(glm::vec3(0,0,0), glm::vec3( 0,-1, 0), glm::vec3(0, 0,-1)),
    glm::lookAt(glm::vec3(0,0,0), glm::vec3( 0, 0, 1), glm::vec3(0,-1, 0)),
    glm::lookAt(glm::vec3(0,0,0), glm::vec3( 0, 0,-1), glm::vec3(0,-1, 0)),
};
static const glm::mat4 kCaptureProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

LightingStage::LightingStage(std::string hdrPath)
    : hdrPath_(std::move(hdrPath)) {}

void LightingStage::CreateHDRFBO(int w, int h) {
    hdrFBO_ = std::make_unique<Framebuffer>(w, h);
    auto hdrTex = Texture::Create2D(w, h, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    hdrFBO_->AttachColor(hdrTex, 0);
    hdrFBO_->AttachDepthRenderbuffer();
}

void LightingStage::Init(int width, int height) {
    width_ = width; height_ = height;
    CreateHDRFBO(width, height);

    pbrShader_ = std::make_unique<Shader>("lighting/fullscreen.vert",
                                          "lighting/pbr_ibl.frag");
    irrShader_ = std::make_unique<Shader>("lighting/cube.vert",
                                          "lighting/irradiance.frag");
    pfShader_ = std::make_unique<Shader>("lighting/cube.vert",
                                         "lighting/prefilter.frag");
    brdfShader_ = std::make_unique<Shader>("lighting/fullscreen.vert",
                                           "lighting/brdf_lut.frag");

    captureFBO_ = std::make_unique<Framebuffer>(512, 512);
    captureFBO_->AttachDepthRenderbuffer();

    static const float kCube[108] = {
        -1,-1,-1, -1,-1, 1, -1, 1, 1, -1,-1,-1, -1, 1, 1, -1, 1,-1,
         1,-1, 1,  1,-1,-1,  1, 1,-1,  1,-1, 1,  1, 1,-1,  1, 1, 1,
        -1,-1, 1,  1,-1, 1,  1, 1, 1, -1,-1, 1,  1, 1, 1, -1, 1, 1,
         1,-1,-1, -1,-1,-1, -1, 1,-1,  1,-1,-1, -1, 1,-1,  1, 1,-1,
        -1, 1, 1,  1, 1, 1,  1, 1,-1, -1, 1, 1,  1, 1,-1, -1, 1,-1,
        -1,-1,-1,  1,-1,-1,  1,-1, 1, -1,-1,-1,  1,-1, 1, -1,-1, 1,
    };
    cubeVAO_ = std::make_unique<VertexArray>();
    cubeVBO_ = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
    cubeVBO_->Upload(kCube, sizeof(kCube));
    cubeVAO_->Bind();
    cubeVBO_->Bind();
    cubeVAO_->BindVertexBuffer(0, cubeVBO_->GetID(), 3 * sizeof(float), 0);
    cubeVAO_->AddAttribute(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);
    cubeVAO_->Unbind();

    dummyVAO_ = std::make_unique<VertexArray>();
    GenerateIBL(hdrPath_);
}

void LightingStage::Resize(int w, int h) {
    width_ = w; height_ = h;
    CreateHDRFBO(w, h);
}

void LightingStage::GenerateIBL(const std::string& hdrPath) {
    Renderer::EnableDepthTest(false);
    Renderer::EnableCullFace(false);

    cubeVAO_->Bind();

    auto hdrTex = Texture::LoadHDR(hdrPath);

    auto envCubemap = Texture::CreateCubemap(512, GL_RGB16F, true);
    Shader eqShader("lighting/cube.vert",
                    "lighting/equirect_to_cubemap.frag");
    eqShader.Use();
    hdrTex->Bind(0);

    for (int i = 0; i < 6; ++i) {
        eqShader.SetMat4("uViewProj", kCaptureProj * kCaptureViews[i]);
        glNamedFramebufferTextureLayer(captureFBO_->GetID(), GL_COLOR_ATTACHMENT0,
                                       envCubemap->GetID(), 0, i);
        captureFBO_->Bind();
        Renderer::SetViewport(0, 0, 512, 512);
        Renderer::Clear(true, false, false);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glGenerateTextureMipmap(envCubemap->GetID());

    irradianceMap_ = Texture::CreateCubemap(32, GL_RGB16F, false);
    irrShader_->Use();
    envCubemap->Bind(0);

    for (int i = 0; i < 6; ++i) {
        irrShader_->SetMat4("uViewProj", kCaptureProj * kCaptureViews[i]);
        glNamedFramebufferTextureLayer(captureFBO_->GetID(), GL_COLOR_ATTACHMENT0,
                                       irradianceMap_->GetID(), 0, i);
        captureFBO_->Bind();
        Renderer::SetViewport(0, 0, 32, 32);
        Renderer::Clear(true, false, false);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    prefilterMap_ = Texture::CreateCubemap(128, GL_RGB16F, true);
    glGenerateTextureMipmap(prefilterMap_->GetID());
    pfShader_->Use();
    envCubemap->Bind(0);

    const int maxMips = 5;
    for (int mip = 0; mip < maxMips; ++mip) {
        int mipSize = 128 >> mip;
        auto pfFBO = std::make_unique<Framebuffer>(mipSize, mipSize);
        pfFBO->AttachDepthRenderbuffer();

        float roughness = (float)mip / (float)(maxMips - 1);
        pfShader_->SetFloat("uRoughness", roughness);

        for (int i = 0; i < 6; ++i) {
            pfShader_->SetMat4("uViewProj", kCaptureProj * kCaptureViews[i]);
            glNamedFramebufferTextureLayer(pfFBO->GetID(), GL_COLOR_ATTACHMENT0,
                                           prefilterMap_->GetID(), mip, i);
            pfFBO->Bind();
            Renderer::SetViewport(0, 0, mipSize, mipSize);
            Renderer::Clear(true, false, false);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
    }

    brdfLUT_ = Texture::Create2D(512, 512, GL_RG16F, GL_RG, GL_FLOAT);
    auto brdfFBO = std::make_unique<Framebuffer>(512, 512);
    brdfFBO->AttachColor(brdfLUT_, 0);
    brdfFBO->AttachDepthRenderbuffer();
    brdfShader_->Use();
    brdfFBO->Bind();
    Renderer::SetViewport(0, 0, 512, 512);
    Renderer::Clear(true, false, false);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    cubeVAO_->Unbind();
    Framebuffer::BindDefault();
    Renderer::EnableCullFace(true);
    Renderer::EnableDepthTest(true);
}

void LightingStage::Execute(PipelineResources& resources, const FrameContext& frame) {
    const auto& gbuffer = resources.Get<GBufferOutputs>();
    const auto& shadow = resources.Get<ShadowOutputs>();
    const auto& scene = resources.Get<RenderSceneView>();

    hdrFBO_->Bind();
    Renderer::SetViewport(0, 0, width_, height_);
    Renderer::Clear(true, true, false);
    Renderer::EnableDepthTest(false);
    Renderer::EnableCullFace(false);

    pbrShader_->Use();

    gbuffer.albedoMetallic->Bind(0);
    gbuffer.normalRoughness->Bind(1);
    gbuffer.depth->Bind(2);

    glBindTextureUnit(3, shadow.shadowArray);
    auto& cascades = shadow.cascades;
    for (int c = 0; c < 4; ++c) {
        pbrShader_->SetMat4("uCascadeViewProj[" + std::to_string(c) + "]", cascades[c].viewProj);
        pbrShader_->SetFloat("uCascadeFar[" + std::to_string(c) + "]", cascades[c].farPlane);
    }
    auto& light = scene.sunLight;
    pbrShader_->SetVec3("uLightDir", light.direction);
    pbrShader_->SetVec3("uLightColor", light.color * light.intensity);

    irradianceMap_->Bind(4);
    prefilterMap_->Bind(5);
    brdfLUT_->Bind(6);

    pbrShader_->SetMat4("uView", frame.camera.view);
    pbrShader_->SetMat4("uInvViewProj", glm::inverse(frame.camera.viewProj));
    pbrShader_->SetVec3("uCamPos", frame.camera.camPos);
    pbrShader_->SetFloat("uAmbientStrength", frame.renderSettings.ambientStrength);

    dummyVAO_->Bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);
    dummyVAO_->Unbind();

    Framebuffer::BindDefault();
    Renderer::EnableCullFace(true);
    Renderer::EnableDepthTest(true);

    LightingOutputs outputs;
    outputs.hdrColor = hdrFBO_->GetColor(0);
    resources.Set(outputs);
}

} // namespace HuanGL
```

- [ ] **Step 3: Commit**

```powershell
git add src/pipeline/stages/LightingStage.h src/pipeline/stages/LightingStage.cpp
git commit -m "refactor: add LightingStage (port of LightingPass)"
```

---

### Task 8: Create BloomStage

**Files:**
- Create: `src/pipeline/stages/BloomStage.h`
- Create: `src/pipeline/stages/BloomStage.cpp`

- [ ] **Step 1: Create BloomStage.h**

```cpp
// src/pipeline/stages/BloomStage.h
#pragma once
#include "../IPipelineStage.h"
#include "../../renderer/Buffer.h"
#include "../../renderer/Framebuffer.h"
#include "../../renderer/Shader.h"
#include "../../renderer/Texture.h"
#include <memory>
#include <vector>

namespace HuanGL {

struct BloomOutputs {
    std::shared_ptr<Texture> bloom;
};

class BloomStage : public IPipelineStage {
public:
    const char* GetName() const override { return "BloomStage"; }
    void Init(int width, int height) override;
    void Resize(int width, int height) override;
    void Execute(PipelineResources& resources, const FrameContext& frame) override;

private:
    struct BloomMip {
        int width = 1;
        int height = 1;
        std::shared_ptr<Texture> texture;
        std::unique_ptr<Framebuffer> fbo;
    };

    void CreateResources(int width, int height);
    BloomMip CreateMip(int width, int height, const char* label) const;
    void DrawFullscreen() const;

    std::unique_ptr<Shader> extractShader_;
    std::unique_ptr<Shader> downsampleShader_;
    std::unique_ptr<Shader> upsampleShader_;
    std::unique_ptr<VertexArray> dummyVAO_;

    std::vector<BloomMip> downMips_;
    std::vector<BloomMip> upMips_;
};

} // namespace HuanGL
```

- [ ] **Step 2: Create BloomStage.cpp**

```cpp
// src/pipeline/stages/BloomStage.cpp
#include "BloomStage.h"
#include "LightingStage.h"
#include "../PipelineResources.h"
#include "../../renderer/Renderer.h"
#include "../../renderer/FrameContext.h"
#include <algorithm>
#include <glm/vec2.hpp>
#include <stdexcept>
#include <string>

namespace HuanGL {

namespace {
constexpr int kMaxBloomMips = 6;
}

void BloomStage::Init(int width, int height) {
    extractShader_ = std::make_unique<Shader>("lighting/fullscreen.vert",
                                              "bloom/bright_extract.frag");
    downsampleShader_ = std::make_unique<Shader>("lighting/fullscreen.vert",
                                                 "bloom/downsample.frag");
    upsampleShader_ = std::make_unique<Shader>("lighting/fullscreen.vert",
                                               "bloom/upsample.frag");
    dummyVAO_ = std::make_unique<VertexArray>();
    CreateResources(width, height);
}

void BloomStage::Resize(int width, int height) {
    CreateResources(width, height);
}

BloomStage::BloomMip BloomStage::CreateMip(int width, int height,
                                           const char* label) const {
    BloomMip mip;
    mip.width = std::max(1, width);
    mip.height = std::max(1, height);
    mip.texture = Texture::Create2D(mip.width, mip.height, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    mip.texture->SetFilter(GL_LINEAR, GL_LINEAR);
    mip.texture->SetWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

    mip.fbo = std::make_unique<Framebuffer>(mip.width, mip.height);
    mip.fbo->AttachColor(mip.texture);
    mip.fbo->SetDrawBuffers({GL_COLOR_ATTACHMENT0});
    if (!mip.fbo->IsComplete()) {
        throw std::runtime_error(std::string("[BloomStage] ") +
                                 label + " framebuffer incomplete");
    }
    return mip;
}

void BloomStage::CreateResources(int width, int height) {
    downMips_.clear();
    upMips_.clear();

    int mipWidth = std::max(1, width / 2);
    int mipHeight = std::max(1, height / 2);

    for (int i = 0; i < kMaxBloomMips; ++i) {
        downMips_.push_back(CreateMip(mipWidth, mipHeight, "downsample"));
        upMips_.push_back(CreateMip(mipWidth, mipHeight, "upsample"));

        if (mipWidth == 1 && mipHeight == 1) break;

        mipWidth = std::max(1, mipWidth / 2);
        mipHeight = std::max(1, mipHeight / 2);
    }
}

void BloomStage::DrawFullscreen() const {
    dummyVAO_->Bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);
    dummyVAO_->Unbind();
}

void BloomStage::Execute(PipelineResources& resources, const FrameContext& frame) {
    const auto& settings = frame.renderSettings.bloom;
    const auto& lighting = resources.Get<LightingOutputs>();

    if (!settings.enabled || !lighting.hdrColor || downMips_.empty()) {
        resources.Set(BloomOutputs{});
        return;
    }

    const int activeCount = std::clamp(settings.mipCount, 1,
                                       static_cast<int>(downMips_.size()));
    const float upsampleRadius = std::clamp(static_cast<float>(settings.radius),
                                            0.25f, 16.0f);

    Renderer::EnableDepthTest(false);
    Renderer::EnableCullFace(false);

    BloomMip& firstMip = downMips_[0];
    firstMip.fbo->Bind();
    Renderer::SetViewport(0, 0, firstMip.width, firstMip.height);
    Renderer::Clear(true, false, false);
    extractShader_->Use();
    extractShader_->SetFloat("uThreshold", settings.threshold);
    extractShader_->SetFloat("uSoftKnee", settings.softKnee);
    lighting.hdrColor->Bind(0);
    DrawFullscreen();

    downsampleShader_->Use();
    for (int i = 1; i < activeCount; ++i) {
        const BloomMip& source = downMips_[i - 1];
        BloomMip& target = downMips_[i];

        target.fbo->Bind();
        Renderer::SetViewport(0, 0, target.width, target.height);
        Renderer::Clear(true, false, false);
        downsampleShader_->SetVec2("uTexelSize",
                                   glm::vec2(1.0f / source.width,
                                             1.0f / source.height));
        source.texture->Bind(0);
        DrawFullscreen();
    }

    BloomOutputs outputs;

    if (activeCount == 1) {
        Framebuffer::BindDefault();
        Renderer::SetViewport(0, 0, frame.width, frame.height);
        Renderer::EnableCullFace(true);
        Renderer::EnableDepthTest(true);
        outputs.bloom = downMips_[0].texture;
        resources.Set(outputs);
        return;
    }

    upsampleShader_->Use();
    for (int i = activeCount - 2; i >= 0; --i) {
        const BloomMip& lowSource = (i == activeCount - 2)
            ? downMips_[i + 1]
            : upMips_[i + 1];
        const BloomMip& highSource = downMips_[i];
        BloomMip& target = upMips_[i];

        target.fbo->Bind();
        Renderer::SetViewport(0, 0, target.width, target.height);
        Renderer::Clear(true, false, false);
        upsampleShader_->SetVec2("uLowTexelSize",
                                 glm::vec2(1.0f / lowSource.width,
                                           1.0f / lowSource.height));
        upsampleShader_->SetFloat("uRadius", upsampleRadius);
        lowSource.texture->Bind(0);
        highSource.texture->Bind(1);
        DrawFullscreen();
    }

    Framebuffer::BindDefault();
    Renderer::SetViewport(0, 0, frame.width, frame.height);
    Renderer::EnableCullFace(true);
    Renderer::EnableDepthTest(true);

    outputs.bloom = upMips_[0].texture;
    resources.Set(outputs);
}

} // namespace HuanGL
```

- [ ] **Step 3: Commit**

```powershell
git add src/pipeline/stages/BloomStage.h src/pipeline/stages/BloomStage.cpp
git commit -m "refactor: add BloomStage (port of BloomTechnique)"
```

---

### Task 9: Create PostProcessStage

**Files:**
- Create: `src/pipeline/stages/PostProcessStage.h`
- Create: `src/pipeline/stages/PostProcessStage.cpp`

- [ ] **Step 1: Create PostProcessStage.h**

```cpp
// src/pipeline/stages/PostProcessStage.h
#pragma once
#include "../IPipelineStage.h"
#include <memory>

namespace HuanGL {

class Shader;
class VertexArray;

class PostProcessStage : public IPipelineStage {
public:
    const char* GetName() const override { return "PostProcessStage"; }
    void Init(int width, int height) override;
    void Resize(int width, int height) override;
    void Execute(PipelineResources& resources, const FrameContext& frame) override;

private:
    std::unique_ptr<Shader>      shader_;
    std::unique_ptr<VertexArray>  dummyVAO_;
};

} // namespace HuanGL
```

- [ ] **Step 2: Create PostProcessStage.cpp**

```cpp
// src/pipeline/stages/PostProcessStage.cpp
#include "PostProcessStage.h"
#include "ShadowStage.h"
#include "GBufferStage.h"
#include "LightingStage.h"
#include "BloomStage.h"
#include "../PipelineResources.h"
#include "../../renderer/Shader.h"
#include "../../renderer/Renderer.h"
#include "../../renderer/Framebuffer.h"
#include "../../renderer/FrameContext.h"
#include "../../renderer/Buffer.h"
#include <glm/gtc/matrix_transform.hpp>

namespace HuanGL {

void PostProcessStage::Init(int /*width*/, int /*height*/) {
    shader_ = std::make_unique<Shader>("lighting/fullscreen.vert",
                                       "postprocess/postprocess.frag");
    dummyVAO_ = std::make_unique<VertexArray>();
}

void PostProcessStage::Resize(int /*width*/, int /*height*/) {
    // PostProcess renders to the default framebuffer, no owned resources to resize.
}

void PostProcessStage::Execute(PipelineResources& resources, const FrameContext& frame) {
    const auto& lighting = resources.Get<LightingOutputs>();
    const auto& gbuffer = resources.Get<GBufferOutputs>();
    const auto& shadow = resources.Get<ShadowOutputs>();
    const auto& bloom = resources.Get<BloomOutputs>();

    Framebuffer::BindDefault();
    Renderer::SetViewport(0, 0, frame.width, frame.height);
    Renderer::EnableDepthTest(false);
    Renderer::EnableCullFace(false);

    shader_->Use();
    shader_->SetInt("uToneMapMode", ToShaderToneMapMode(frame.renderSettings.toneMapMode));
    shader_->SetInt("uDebugMode", ToShaderDebugView(frame.debugSettings.view));
    const bool bloomAvailable = frame.renderSettings.bloom.enabled &&
                                bloom.bloom != nullptr;
    shader_->SetBool("uBloomEnabled", bloomAvailable);
    shader_->SetFloat("uBloomIntensity", frame.renderSettings.bloom.intensity);
    shader_->SetFloat("uExposure", frame.renderSettings.exposure);

    shader_->SetMat4("uView", frame.camera.view);
    shader_->SetMat4("uInvViewProj", glm::inverse(frame.camera.viewProj));
    shader_->SetFloat("uNearPlane", frame.camera.near_);
    shader_->SetFloat("uFarPlane", frame.camera.far_);

    auto& cascades = shadow.cascades;
    for (int c = 0; c < 4; ++c) {
        shader_->SetFloat("uCascadeFar[" + std::to_string(c) + "]", cascades[c].farPlane);
    }

    lighting.hdrColor->Bind(0);
    gbuffer.albedoMetallic->Bind(1);
    gbuffer.normalRoughness->Bind(2);
    gbuffer.depth->Bind(3);
    if (bloomAvailable) {
        bloom.bloom->Bind(4);
    }

    dummyVAO_->Bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);
    dummyVAO_->Unbind();

    Renderer::EnableCullFace(true);
    Renderer::EnableDepthTest(true);
}

} // namespace HuanGL
```

- [ ] **Step 3: Commit**

```powershell
git add src/pipeline/stages/PostProcessStage.h src/pipeline/stages/PostProcessStage.cpp
git commit -m "refactor: add PostProcessStage (port of PostProcessPass)"
```

---

### Task 10: Switch RenderPipeline to Stage Loop and Remove Old Files

**Files:**
- Modify: `src/pipeline/RenderPipeline.h`
- Modify: `src/pipeline/RenderPipeline.cpp`
- Delete: `src/pipeline/PipelineOutputs.h`
- Delete: `src/pipeline/passes/ShadowPass.h`
- Delete: `src/pipeline/passes/ShadowPass.cpp`
- Delete: `src/pipeline/passes/GBufferPass.h`
- Delete: `src/pipeline/passes/GBufferPass.cpp`
- Delete: `src/pipeline/passes/LightingPass.h`
- Delete: `src/pipeline/passes/LightingPass.cpp`
- Delete: `src/pipeline/passes/PostProcessPass.h`
- Delete: `src/pipeline/passes/PostProcessPass.cpp`
- Delete: `src/pipeline/techniques/BloomTechnique.h`
- Delete: `src/pipeline/techniques/BloomTechnique.cpp`

- [ ] **Step 1: Rewrite RenderPipeline.h**

Replace entire contents:

```cpp
// src/pipeline/RenderPipeline.h
#pragma once
#include "IPipelineStage.h"
#include "PipelineResources.h"
#include "../renderer/FrameContext.h"
#include "../renderer/RenderSceneView.h"
#include "../renderer/UniformBuffer.h"
#include <memory>
#include <string>
#include <vector>

namespace HuanGL {

class RenderPipeline {
public:
    void Init(int width, int height, const std::string& hdrPath);
    void Resize(int width, int height);
    void Execute(const RenderSceneView& scene, const FrameContext& frame);

private:
    void BuildStages(const std::string& hdrPath);
    void UpdateUniformBuffers(const RenderSceneView& scene, const FrameContext& frame);

    std::vector<std::unique_ptr<IPipelineStage>> stages_;
    PipelineResources resources_;

    std::unique_ptr<CameraUBO> cameraUBO_;
    std::unique_ptr<LightsUBO> lightsUBO_;
    std::unique_ptr<TimeUBO>   timeUBO_;
};

} // namespace HuanGL
```

- [ ] **Step 2: Rewrite RenderPipeline.cpp**

Replace entire contents:

```cpp
// src/pipeline/RenderPipeline.cpp
#include "RenderPipeline.h"
#include "stages/ShadowStage.h"
#include "stages/GBufferStage.h"
#include "stages/LightingStage.h"
#include "stages/BloomStage.h"
#include "stages/PostProcessStage.h"
#include "../renderer/Renderer.h"

namespace HuanGL {

void RenderPipeline::BuildStages(const std::string& hdrPath) {
    stages_.push_back(std::make_unique<ShadowStage>(2048));
    stages_.push_back(std::make_unique<GBufferStage>());
    stages_.push_back(std::make_unique<LightingStage>(hdrPath));
    stages_.push_back(std::make_unique<BloomStage>());
    stages_.push_back(std::make_unique<PostProcessStage>());
}

void RenderPipeline::Init(int w, int h, const std::string& hdrPath) {
    cameraUBO_ = std::make_unique<CameraUBO>();
    lightsUBO_ = std::make_unique<LightsUBO>();
    timeUBO_   = std::make_unique<TimeUBO>();

    BuildStages(hdrPath);
    for (auto& stage : stages_)
        stage->Init(w, h);
}

void RenderPipeline::Resize(int w, int h) {
    for (auto& stage : stages_)
        stage->Resize(w, h);
}

void RenderPipeline::UpdateUniformBuffers(const RenderSceneView& scene,
                                          const FrameContext& frame) {
    cameraUBO_->Update(frame.camera);

    LightsData lightData;
    lightData.dirLightDir       = scene.sunLight.direction;
    lightData.dirLightColor     = scene.sunLight.color;
    lightData.dirLightIntensity = scene.sunLight.intensity;
    lightsUBO_->Update(lightData);

    TimeData timeData;
    timeData.time      = frame.time;
    timeData.deltaTime = frame.deltaTime;
    timeUBO_->Update(timeData);
}

void RenderPipeline::Execute(const RenderSceneView& scene,
                              const FrameContext& frame) {
    resources_.Clear();
    resources_.Set(scene);
    UpdateUniformBuffers(scene, frame);

    for (auto& stage : stages_) {
        Renderer::PushDebugGroup(stage->GetName());
        stage->Execute(resources_, frame);
        Renderer::PopDebugGroup();
    }
}

} // namespace HuanGL
```

- [ ] **Step 3: Delete old pass/technique files and PipelineOutputs.h**

```powershell
git rm src/pipeline/PipelineOutputs.h
git rm src/pipeline/passes/ShadowPass.h src/pipeline/passes/ShadowPass.cpp
git rm src/pipeline/passes/GBufferPass.h src/pipeline/passes/GBufferPass.cpp
git rm src/pipeline/passes/LightingPass.h src/pipeline/passes/LightingPass.cpp
git rm src/pipeline/passes/PostProcessPass.h src/pipeline/passes/PostProcessPass.cpp
git rm src/pipeline/techniques/BloomTechnique.h src/pipeline/techniques/BloomTechnique.cpp
```

- [ ] **Step 4: Reconfigure CMake and build**

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

Expected: compiles clean. The GLOB_RECURSE picks up the new `stages/*.cpp` files and drops the deleted files.

- [ ] **Step 5: Run and verify all features**

```powershell
.\build\Debug\HuanGL.exe
```

Verification checklist:
1. TestScene renders with PBR spheres and shadows
2. Press `N` — scene cycles (DamagedHelmet if present)
3. Press `N` — scene cycles (Sponza if present)
4. Right-click + mouse — camera rotates
5. Right-click + WASD — camera moves
6. Release right-click — cursor appears, ImGui clickable
7. Press `0` through `7` — all debug views work
8. Press `T` — tone map cycles (ACES / Reinhard / None)
9. ImGui: toggle Bloom checkbox — Bloom turns on/off
10. Resize window — no crash, pipeline resizes

- [ ] **Step 6: Commit**

```powershell
git add src/pipeline/RenderPipeline.h src/pipeline/RenderPipeline.cpp
git commit -m "refactor: switch RenderPipeline to stage loop, remove old passes"
```

---

### Task 11: Update Documentation

**Files:**
- Modify: `AGENTS.md`
- Modify: `docs/architecture.md`

- [ ] **Step 1: Update AGENTS.md**

In the "Key Technical Decisions" section, add after the existing entry about technique modules:

Replace:
```
- Optional rendering algorithms live as concrete technique modules under `src/pipeline/techniques/`; do not add a full render graph or RHI.
```

With:
```
- All render passes and techniques implement `IPipelineStage` and live under `src/pipeline/stages/`. `RenderPipeline` iterates a vector of stages — ordering is explicit, not graph-resolved. Do not add a full render graph or RHI.
```

In the "Phase 3.5" section, update the file table to reference stages instead of techniques:

Replace:
```
| `src/pipeline/techniques/BloomTechnique.h/cpp` | Multi-mip Bloom technique with soft-knee bright extract, downsample chain, and upsample combine |
```

With:
```
| `src/pipeline/stages/BloomStage.h/cpp` | Multi-mip Bloom stage with soft-knee bright extract, downsample chain, and upsample combine |
```

Replace the "Current Directory Structure" section:

```text
src/
  app/
  core/
  renderer/
  pipeline/
    stages/
  resource/
  scene/
  ui/
```

Update the Phase 3 file table: replace `ShadowPass`, `GBufferPass`, `LightingPass`, `PostProcessPass` references with their Stage equivalents. Replace `src/pipeline/passes/` paths with `src/pipeline/stages/`.

Add a new section after Phase 3.5:

```markdown
### Phase 3.6: Modular Pipeline Architecture — Complete

| File | Responsibility |
|------|----------------|
| `src/pipeline/IPipelineStage.h` | Common stage interface (Init/Resize/Execute) |
| `src/pipeline/PipelineResources.h` | Typed resource registry for inter-stage data passing |
| `src/pipeline/stages/ShadowStage.h/cpp` | Four-cascade CSM with `sampler2DArrayShadow` |
| `src/pipeline/stages/GBufferStage.h/cpp` | Deferred MRT fill |
| `src/pipeline/stages/LightingStage.h/cpp` | Cook-Torrance PBR + IBL |
| `src/pipeline/stages/BloomStage.h/cpp` | Multi-mip HDR Bloom |
| `src/pipeline/stages/PostProcessStage.h/cpp` | Tone mapping, gamma, debug visualization |
```

- [ ] **Step 2: Update docs/architecture.md**

Update the "Render Pipeline" section diagram to show stages instead of passes.

Update the "Module Map" table row for `src/pipeline/`:

```
| `src/pipeline/` | IPipelineStage interface, typed resource registry, concrete stages, per-frame orchestration | `RenderPipeline`, `IPipelineStage`, `PipelineResources`, `ShadowStage`, `GBufferStage`, `LightingStage`, `BloomStage`, `PostProcessStage` |
```

Update "Key Design Decisions" — replace the "Concrete technique modules" entry:

```
**Modular stage pipeline.** All render passes and optional algorithms implement
`IPipelineStage` and live under `src/pipeline/stages/`. Each stage reads upstream
outputs and writes its own output through a typed `PipelineResources` registry.
`RenderPipeline` iterates a `vector<unique_ptr<IPipelineStage>>` — ordering is
explicit, not graph-resolved. Adding a new technique means creating one stage
file and registering it; no other files change.
```

Add Phase 3.6 to the roadmap table.

- [ ] **Step 3: Add design document reference to AGENTS.md**

In the "Design Documents" section at the bottom of AGENTS.md, add:

```
- Modular pipeline design: `docs/superpowers/specs/2026-05-26-huangl-modular-pipeline-design.md`
- Modular pipeline plan: `docs/superpowers/plans/2026-05-26-huangl-modular-pipeline.md`
```

- [ ] **Step 4: Commit**

```powershell
git add AGENTS.md docs/architecture.md
git commit -m "docs: update architecture docs for modular pipeline"
```
