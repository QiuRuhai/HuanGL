# HuanGL Modular Pipeline Architecture Design

Date: 2026-05-26
Status: Approved

## Purpose

Refactor the rendering pipeline from a monolithic orchestrator with hardcoded pass members into a modular stage-based architecture. The primary goal is to make adding new rendering techniques (TAA, RSM, SSGI, VXGI, DDGI) a purely incremental operation — create one new file, register it, done — without modifying the pipeline core, the output structs, or other stages.

Secondary goals: decouple Camera from GLFW, centralize shader path management, and unify directory structure.

This serves a dual purpose: learning rendering techniques AND learning software architecture patterns (interface design, dependency injection, registry pattern, typed resource maps).

## Current Architecture Problems

### 1. RenderPipeline Monolith

`RenderPipeline` hardcodes all passes and techniques as member variables:

```cpp
ShadowPass      shadowPass_;
GBufferPass     gbufferPass_;
LightingPass    lightingPass_;
PostProcessPass postProcessPass_;
BloomTechnique  bloomTechnique_;
```

Each new technique requires: modifying `RenderPipeline.h` (add member + include), modifying `Execute()`, modifying `Resize()`, and extending `PipelineOutputs`. Five planned techniques (TAA, RSM, SSGI, VXGI, DDGI) would make this class unwieldy.

### 2. Inconsistent Pass Interfaces

Each pass has a different `Render()` / `Execute()` signature and different parameter lists. No common abstraction exists.

### 3. Camera Couples GLFW Into Pipeline

`Camera.h` is header-only, includes `<GLFW/glfw3.h>`, and takes `GLFWwindow*` in `Update()`. Since `FrameContext.h` depends on `UniformBuffer.h` which depends on `Camera.h` (via `CameraData`), GLFW headers propagate into the entire pipeline.

### 4. Hardcoded Shader Paths

Shader paths like `"../shader/lighting/pbr_ibl.frag"` are scattered across 6+ source files with no central management.

### 5. Unbounded PipelineOutputs

`PipelineOutputs` is a flat struct that must be modified for every new technique, causing recompilation of all consumers.

---

## Section 1 — IPipelineStage Interface

### Design

A unified interface for all pipeline stages:

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

### Stage Migration Mapping

| Current Class | New Class |
|---------------|-----------|
| `ShadowPass` | `ShadowStage : IPipelineStage` |
| `GBufferPass` | `GBufferStage : IPipelineStage` |
| `LightingPass` | `LightingStage : IPipelineStage` |
| `BloomTechnique` | `BloomStage : IPipelineStage` |
| `PostProcessPass` | `PostProcessStage : IPipelineStage` |

### Conventions

- All implementations use the `XxxStage` naming convention. No more "Pass" vs "Technique" distinction.
- All stages live in `src/pipeline/stages/`.
- `Execute()` replaces both `Render()` and the old `Execute()`.
- Whether a technique is active is decided inside its own `Execute()` by checking `FrameContext` settings (e.g., `bloom.enabled`).
- Stage ordering is explicit in `RenderPipeline` — no automatic dependency resolution.

### Non-Goals

- No `Enable()` / `Disable()` interface on the stage itself.
- No automatic dependency graph or topological sort.
- No stage-to-stage compile-time coupling through the interface.

---

## Section 2 — PipelineResources (Typed Resource Registry)

### Design

Replaces the flat `PipelineOutputs` struct with a type-indexed resource map. Each stage writes its output and reads upstream outputs through this registry.

```cpp
// src/pipeline/PipelineResources.h
#pragma once
#include <typeindex>
#include <unordered_map>
#include <any>
#include <stdexcept>

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

### Usage Pattern

```cpp
// ShadowStage::Execute()
void ShadowStage::Execute(PipelineResources& res, const FrameContext& frame) {
    const auto& scene = res.Get<RenderSceneView>();
    // ... render shadows ...
    res.Set(ShadowOutputs{shadowArrayID_, cascades_});
}

// LightingStage::Execute()
void LightingStage::Execute(PipelineResources& res, const FrameContext& frame) {
    const auto& gbuffer = res.Get<GBufferOutputs>();
    const auto& shadow  = res.Get<ShadowOutputs>();
    const auto& scene   = res.Get<RenderSceneView>();
    // ... render lighting ...
    res.Set(LightingOutputs{hdrFBO_->GetColor(0)});
}
```

### Output Struct Ownership

Each output struct is defined in its stage's header file:

```cpp
// ShadowStage.h
struct ShadowOutputs { GLuint shadowArray; std::array<CascadeData, 4> cascades; };
class ShadowStage : public IPipelineStage { ... };
```

If another stage needs to read `ShadowOutputs`, it includes `ShadowStage.h`. This is a data dependency (struct definition only), not a behavioral dependency.

### Trade-offs

**Gained:**
- Adding new techniques does not modify `PipelineResources` itself
- Each stage only includes the output types it actually reads
- Stages are decoupled at the compile level

**Paid:**
- Runtime `std::any_cast` overhead (negligible: 5-10 casts per frame)
- Ordering errors fail at runtime, not compile time (but fail immediately on first frame — fail-fast)

---

## Section 3 — RenderPipeline Refactor

### New Design

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

### Stage Registration

```cpp
void RenderPipeline::BuildStages(const std::string& hdrPath) {
    stages_.push_back(std::make_unique<ShadowStage>(2048));
    stages_.push_back(std::make_unique<GBufferStage>());
    stages_.push_back(std::make_unique<LightingStage>(hdrPath));
    stages_.push_back(std::make_unique<BloomStage>());
    stages_.push_back(std::make_unique<PostProcessStage>());
}
```

### Execute Loop

```cpp
void RenderPipeline::Execute(const RenderSceneView& scene, const FrameContext& frame) {
    resources_.Clear();
    resources_.Set(scene);
    UpdateUniformBuffers(scene, frame);

    for (auto& stage : stages_) {
        Renderer::PushDebugGroup(stage->GetName());
        stage->Execute(resources_, frame);
        Renderer::PopDebugGroup();
    }
}
```

### Init Lifecycle

Stages receive stage-specific configuration through their constructor. The `Init(width, height)` method is called separately to create GPU resources:

```cpp
void RenderPipeline::Init(int w, int h, const std::string& hdrPath) {
    cameraUBO_ = std::make_unique<CameraUBO>();
    lightsUBO_ = std::make_unique<LightsUBO>();
    timeUBO_   = std::make_unique<TimeUBO>();

    BuildStages(hdrPath);
    for (auto& stage : stages_)
        stage->Init(w, h);
}
```

This two-phase construction (constructor for config, `Init()` for GPU resources) allows stages to be constructed before the OpenGL context is fully ready, and keeps stage-specific parameters out of the `IPipelineStage` interface.

### Key Points

- `RenderSceneView` is placed into the resource registry so stages that need renderables or light data can access it uniformly.
- UBO updates remain in `RenderPipeline::Execute()` because UBOs are global shared binding points — pipeline-level management is appropriate.
- `RenderPipeline.h` no longer includes any stage headers. It only includes `IPipelineStage.h` and `PipelineResources.h`.
- Stage headers are only included in `RenderPipeline.cpp` (for `BuildStages()`).

### Adding a New Technique (Example: TAA)

1. Create `src/pipeline/stages/TAAStage.h/cpp`
2. Define `TAAOutputs` struct in `TAAStage.h`
3. Add `stages_.push_back(std::make_unique<TAAStage>())` in `BuildStages()` at the correct position
4. Zero changes to any other file

---

## Section 4 — Camera / Input Decoupling

### Problem

`Camera.h` includes `<GLFW/glfw3.h>`, calls `glfwGetCursorPos()` and `glfwGetKey()` in its `Update()` method, and is fully inline in the header. This GLFW dependency propagates into the entire pipeline through the include chain.

### Design

Split Camera into a pure math component and move input handling to `InputController`.

**Camera (pure math, no GLFW):**

```cpp
// src/core/Camera.h
#pragma once
#include "../renderer/UniformBuffer.h"  // for CameraData
#include <glm/glm.hpp>

namespace HuanGL {

class Camera {
public:
    Camera(float fovDeg = 60.f, float nearP = 0.1f, float farP = 100.f);

    void Look(float yawDelta, float pitchDelta);
    void Move(glm::vec3 localDelta, float dt);
    CameraData GetData(float aspect) const;

    float GetFov() const;
    void SetFov(float deg);
    glm::vec3 GetPosition() const;

private:
    glm::vec3 pos_, front_, worldUp_;
    float yaw_, pitch_, fov_, near_, far_, moveSpeed_;
};

} // namespace HuanGL
```

**InputController handles camera input:**

```cpp
// In InputController::Update()
if (state.cameraActive) {
    auto [dx, dy] = Input::GetMouseDelta();
    state.camera.Look(dx * 0.1f, dy * 0.1f);

    glm::vec3 move{0};
    if (Input::IsKeyDown(GLFW_KEY_W)) move.z += 1.f;
    if (Input::IsKeyDown(GLFW_KEY_S)) move.z -= 1.f;
    if (Input::IsKeyDown(GLFW_KEY_A)) move.x -= 1.f;
    if (Input::IsKeyDown(GLFW_KEY_D)) move.x += 1.f;
    if (Input::IsKeyDown(GLFW_KEY_E)) move.y += 1.f;
    if (Input::IsKeyDown(GLFW_KEY_Q)) move.y -= 1.f;
    state.camera.Move(move, state.frameStats.deltaTime);
}
```

### Required Input Extension

`Input` class needs a `GetMouseDelta()` method that returns the frame's cursor delta and resets it. The current `Input` class already tracks key state via GLFW callbacks; mouse delta tracking follows the same pattern.

### Benefits

- `Camera.h` no longer includes GLFW — pipeline code compiles without GLFW headers
- Camera becomes a pure math component (testable independently)
- All input logic centralized in `InputController`

---

## Section 5 — Shader Path Management

### Design

Add a static base path to the `Shader` class:

```cpp
// Shader.h addition
class Shader {
public:
    static void SetBasePath(const std::string& basePath);
    // Constructors now accept relative paths:
    // Shader("lighting/fullscreen.vert", "lighting/pbr_ibl.frag")
    // Internally resolves to: basePath_ + "lighting/fullscreen.vert"
    // ...
private:
    static std::string basePath_;
};
```

Set once in `App::Init()`:

```cpp
Shader::SetBasePath("../shader/");
```

### Non-Goals

- No shader cache (compiled program persistence)
- No shader include/preprocessor (`#include "uniforms.glsl"` support)
- No shader variant system

These are deferred until the shader count warrants them.

---

## Section 6 — Directory Structure

### Current

```
src/pipeline/
  passes/
    ShadowPass.h/cpp
    GBufferPass.h/cpp
    LightingPass.h/cpp
    PostProcessPass.h/cpp
  techniques/
    BloomTechnique.h/cpp
  RenderPipeline.h/cpp
  PipelineOutputs.h
  CascadeData.h
```

### New

```
src/pipeline/
  stages/
    ShadowStage.h/cpp
    GBufferStage.h/cpp
    LightingStage.h/cpp
    BloomStage.h/cpp
    PostProcessStage.h/cpp
  IPipelineStage.h
  PipelineResources.h
  RenderPipeline.h/cpp
  CascadeData.h
```

### Changes

- `passes/` and `techniques/` merged into `stages/`
- `PipelineOutputs.h` deleted — output structs move into their respective stage headers
- Classes renamed to `XxxStage`
- New files: `IPipelineStage.h`, `PipelineResources.h`
- All other directories (`src/core/`, `src/app/`, `src/renderer/`, `src/resource/`, `src/scene/`, `src/ui/`) remain unchanged

---

## Section 7 — Scope and Migration Strategy

### Explicitly Out of Scope

| Unchanged | Reason |
|-----------|--------|
| `Renderer` static class | Adequate for OpenGL state helpers |
| `ResourceManager` | Functional; sRGB issue is a separate fix |
| `Scene` / `World` / Entity model | Sufficient for current needs |
| `ImGuiLayer` / `DebugUI` | Orthogonal to pipeline refactor |
| GLSL shader content | Only C++ side changes |
| UBO structures (`CameraData`, `LightsData`, `TimeData`) | No change needed |
| `Framebuffer` / `Texture` / `Buffer` wrappers | RAII quality is good |
| `Shader` (beyond base path addition) | No change needed |

### Migration Strategy

Three steps, each ending with a compilable and runnable project:

**Step 1: Infrastructure**
- Create `IPipelineStage.h` and `PipelineResources.h`
- Move Camera implementation to `.cpp`, remove GLFW dependency
- Add `Shader::SetBasePath()`
- Extend `Input` with mouse delta tracking

**Step 2: Stage Migration**
- Migrate each pass/technique to `XxxStage : IPipelineStage` one at a time
- Update `RenderPipeline` incrementally
- Final `RenderPipeline` cleanup: `vector<unique_ptr<IPipelineStage>>` loop
- Remove old `passes/` and `techniques/` directories

**Step 3: Cleanup**
- Update AGENTS.md and architecture.md
- Clean up header includes
- Verify all functionality

### Verification Criteria

After refactoring, all of the following must work:

1. All registered scenes render correctly (TestScene, DamagedHelmet, Sponza)
2. All 8 debug views functional
3. Bloom toggle on/off works
4. Window resize does not crash
5. Scene cycling with N key works
6. Right-click camera mode works
7. ImGui panels are interactive and control rendering parameters
