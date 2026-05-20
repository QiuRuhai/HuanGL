# HuanGL Architecture Reset Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Refactor HuanGL around explicit application state, a lightweight inspectable world model, frame/pipeline contracts, and a decoupled ImGui debug UI while preserving the current renderer behavior.

**Architecture:** The reset keeps the OpenGL renderer direct and fixed-order, but separates `App`, runtime state, scene/world data, UI, and render passes. `RenderPipeline` consumes `RenderSceneView` and `FrameContext`, passes exchange explicit output structs, and ImGui edits state/world data instead of pass internals.

**Tech Stack:** C++17, OpenGL 4.6 with GLAD2 DSA APIs, GLFW, Dear ImGui, GLM, CMake/vcpkg on Windows.

---

## Source Spec

`docs/superpowers/specs/2026-05-20-huangl-architecture-reset-design.md`

## File Map

New files:

| File | Responsibility |
|------|----------------|
| `src/renderer/FrameContext.h` | Render-facing settings, debug view enum, per-frame context |
| `src/renderer/RenderSceneView.h` | Read-only renderable list consumed by passes |
| `src/pipeline/CascadeData.h` | Shared CSM cascade data type |
| `src/pipeline/PipelineOutputs.h` | Explicit pass output structs |
| `src/scene/World.h/cpp` | Lightweight `World`, `Entity`, `Transform`, `MeshRenderer` model |
| `src/app/ApplicationState.h` | Runtime state container |
| `src/app/SceneRegistry.h/cpp` | Demo-scene registration, active scene selection, soft-failure loading |
| `src/app/InputController.h/cpp` | Keyboard command mapping into `ApplicationState` |
| `src/ui/DebugUI.h/cpp` | ImGui debug panel that edits state/world data |

Modified files:

| File | Responsibility |
|------|----------------|
| `src/scene/Scene.h` | Owns `World`; temporary legacy getters remain until final cleanup |
| `src/scene/TestScene.h/cpp` | Builds `World` entities and materials |
| `src/scene/ModelScene.h/cpp` | Builds `World` entities and materials from MeshLoader |
| `src/pipeline/RenderPipeline.h/cpp` | Owns UBOs, accepts `RenderSceneView`/`FrameContext`, stores outputs |
| `src/pipeline/passes/ShadowPass.h/cpp` | Consumes `RenderSceneView`/`FrameContext`, returns `ShadowOutputs` |
| `src/pipeline/passes/GBufferPass.h/cpp` | Consumes `RenderSceneView`/`FrameContext`, returns `GBufferOutputs` |
| `src/pipeline/passes/LightingPass.h/cpp` | Consumes output structs/settings, returns `LightingOutputs` |
| `src/pipeline/passes/PostProcessPass.h/cpp` | Consumes output structs/settings; no owned UI state |
| `src/core/App.h/cpp` | Becomes thin composition root and frame scheduler |
| `src/core/Camera.h` | Keeps FOV accessors; no required structural change |
| `AGENTS.md` | Update current architecture notes after code lands |
| `docs/architecture.md` | Update module map/current architecture after code lands |

Because the project uses `GLOB_RECURSE`, rerun CMake configure after adding the new `.cpp` files.

---

## Task 1: Add Shared Renderer Contract Types

**Files:**
- Create: `src/renderer/FrameContext.h`
- Create: `src/renderer/RenderSceneView.h`
- Create: `src/pipeline/CascadeData.h`
- Create: `src/pipeline/PipelineOutputs.h`
- Modify: `src/pipeline/passes/ShadowPass.h`

- [ ] **Step 1: Create `src/renderer/FrameContext.h`**

```cpp
#pragma once
#include "UniformBuffer.h"

namespace HuanGL {

enum class ToneMapMode {
    ACES = 0,
    Reinhard = 1,
    None = 2,
};

enum class DebugView {
    Final = 0,
    Albedo = 1,
    Normal = 2,
    Roughness = 3,
    Metallic = 4,
    Depth = 5,
    Cascades = 6,
};

inline int ToShaderToneMapMode(ToneMapMode mode) {
    return static_cast<int>(mode);
}

inline int ToShaderDebugView(DebugView view) {
    return static_cast<int>(view);
}

struct RenderSettings {
    ToneMapMode toneMapMode = ToneMapMode::ACES;
    float ambientStrength = 1.0f;
    int shadowResolution = 2048;
    float exposure = 1.0f;

    void CycleToneMap() {
        int next = (ToShaderToneMapMode(toneMapMode) + 1) % 3;
        toneMapMode = static_cast<ToneMapMode>(next);
    }
};

struct DebugSettings {
    DebugView view = DebugView::Final;
    bool showImGui = true;
    bool freezeCamera = false;
};

struct FrameContext {
    int width = 0;
    int height = 0;
    float time = 0.0f;
    float deltaTime = 0.0f;

    CameraData camera;
    RenderSettings renderSettings;
    DebugSettings debugSettings;
};

} // namespace HuanGL
```

- [ ] **Step 2: Create `src/renderer/RenderSceneView.h`**

```cpp
#pragma once
#include "Schema.h"
#include <glm/glm.hpp>
#include <vector>

namespace HuanGL {

struct Renderable {
    const Mesh* mesh = nullptr;
    const std::vector<Material>* materials = nullptr;
    glm::mat4 modelMatrix = glm::mat4(1.0f);
};

struct RenderSceneView {
    std::vector<Renderable> renderables;
    DirectionalLight sunLight;
    glm::vec3 ambient = {0.03f, 0.03f, 0.05f};
};

} // namespace HuanGL
```

- [ ] **Step 3: Create `src/pipeline/CascadeData.h`**

```cpp
#pragma once
#include <glm/glm.hpp>

namespace HuanGL {

struct CascadeData {
    glm::mat4 viewProj;
    float farPlane = 0.0f;
    float pad[3] = {};
};

} // namespace HuanGL
```

- [ ] **Step 4: Create `src/pipeline/PipelineOutputs.h`**

```cpp
#pragma once
#include "CascadeData.h"
#include "../renderer/Texture.h"
#include <array>
#include <glad/glad.h>
#include <memory>

namespace HuanGL {

struct ShadowOutputs {
    GLuint shadowArray = 0;
    std::array<CascadeData, 4> cascades {};
};

struct GBufferOutputs {
    std::shared_ptr<Texture> albedoMetallic;
    std::shared_ptr<Texture> normalRoughness;
    std::shared_ptr<Texture> depth;
};

struct LightingOutputs {
    std::shared_ptr<Texture> hdrColor;
};

struct PipelineOutputs {
    ShadowOutputs shadow;
    GBufferOutputs gbuffer;
    LightingOutputs lighting;
};

} // namespace HuanGL
```

- [ ] **Step 5: Move `CascadeData` out of `ShadowPass.h`**

In `src/pipeline/passes/ShadowPass.h`, replace the local `CascadeData`
definition with an include:

```cpp
#include "../CascadeData.h"
```

The top of the file should keep `<array>` because `ShadowPass` stores:

```cpp
std::array<CascadeData, 4> cascades_;
```

- [ ] **Step 6: Configure and build**

Run:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

Expected: configure succeeds, then build succeeds with no compile errors.

- [ ] **Step 7: Commit**

```powershell
git add src/renderer/FrameContext.h `
        src/renderer/RenderSceneView.h `
        src/pipeline/CascadeData.h `
        src/pipeline/PipelineOutputs.h `
        src/pipeline/passes/ShadowPass.h
git commit -m "refactor: add renderer frame contract types"
```

---

## Task 2: Add Lightweight World and Migrate Scene Builders

**Files:**
- Create: `src/scene/World.h`
- Create: `src/scene/World.cpp`
- Modify: `src/scene/Scene.h`
- Modify: `src/scene/TestScene.cpp`
- Modify: `src/scene/ModelScene.cpp`

- [ ] **Step 1: Create `src/scene/World.h`**

```cpp
#pragma once
#include "../renderer/RenderSceneView.h"
#include "../renderer/Schema.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace HuanGL {

using EntityId = uint32_t;

struct Transform {
    glm::vec3 translation = {0.0f, 0.0f, 0.0f};
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale = {1.0f, 1.0f, 1.0f};

    glm::mat4 ToMatrix() const;
};

struct MeshRenderer {
    std::shared_ptr<Mesh> mesh;
};

struct Entity {
    EntityId id = 0;
    std::string name;
    Transform transform;
    std::optional<MeshRenderer> meshRenderer;
};

class World {
public:
    void Clear();
    void Update(float dt);

    Entity& CreateEntity(std::string name);

    std::vector<Entity>& GetEntities() { return entities_; }
    const std::vector<Entity>& GetEntities() const { return entities_; }

    std::vector<Material>& GetMaterials() { return materials_; }
    const std::vector<Material>& GetMaterials() const { return materials_; }

    DirectionalLight& GetSunLight() { return sunLight_; }
    const DirectionalLight& GetSunLight() const { return sunLight_; }

    glm::vec3& GetAmbient() { return ambient_; }
    const glm::vec3& GetAmbient() const { return ambient_; }

    RenderSceneView BuildRenderSceneView() const;

private:
    std::vector<Entity> entities_;
    std::vector<Material> materials_;
    DirectionalLight sunLight_;
    glm::vec3 ambient_ = {0.03f, 0.03f, 0.05f};
    EntityId nextEntityId_ = 1;
};

} // namespace HuanGL
```

- [ ] **Step 2: Create `src/scene/World.cpp`**

```cpp
#include "World.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <utility>

namespace HuanGL {

glm::mat4 Transform::ToMatrix() const {
    glm::mat4 t = glm::translate(glm::mat4(1.0f), translation);
    glm::mat4 r = glm::toMat4(rotation);
    glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);
    return t * r * s;
}

void World::Clear() {
    entities_.clear();
    materials_.clear();
    sunLight_ = DirectionalLight {};
    ambient_ = {0.03f, 0.03f, 0.05f};
    nextEntityId_ = 1;
}

void World::Update(float dt) {
    (void)dt;
}

Entity& World::CreateEntity(std::string name) {
    Entity entity;
    entity.id = nextEntityId_++;
    entity.name = std::move(name);
    entities_.push_back(std::move(entity));
    return entities_.back();
}

RenderSceneView World::BuildRenderSceneView() const {
    RenderSceneView view;
    view.sunLight = sunLight_;
    view.ambient = ambient_;
    for (const auto& entity : entities_) {
        if (!entity.meshRenderer || !entity.meshRenderer->mesh) {
            continue;
        }

        Renderable renderable;
        renderable.mesh = entity.meshRenderer->mesh.get();
        renderable.materials = &materials_;
        renderable.modelMatrix = entity.transform.ToMatrix();
        view.renderables.push_back(renderable);
    }
    return view;
}

} // namespace HuanGL
```

- [ ] **Step 3: Modify `src/scene/Scene.h` to own `World`**

Replace the parallel render arrays with `World world_` plus temporary
legacy accessors. The resulting class body should contain:

```cpp
#include "World.h"

class Scene {
public:
    virtual ~Scene() = default;
    virtual void Init(ResourceManager& rm) = 0;
    virtual void Update(float dt) { world_.Update(dt); }

    World& GetWorld() { return world_; }
    const World& GetWorld() const { return world_; }

    RenderSceneView BuildRenderSceneView() const {
        return world_.BuildRenderSceneView();
    }

    size_t GetMeshCount() const { return legacyView_.renderables.size(); }
    Mesh* GetMesh(size_t i) const {
        return const_cast<Mesh*>(legacyView_.renderables[i].mesh);
    }
    glm::mat4 GetModelMatrix(size_t i) const {
        return legacyView_.renderables[i].modelMatrix;
    }

    const std::vector<Material>& GetMaterials() const {
        return world_.GetMaterials();
    }
    const DirectionalLight& GetSunLight() const {
        return world_.GetSunLight();
    }
    DirectionalLight& GetMutableSunLight() {
        return world_.GetSunLight();
    }
    const glm::vec3& GetAmbient() const {
        return world_.GetAmbient();
    }

protected:
    World world_;
    RenderSceneView legacyView_;

    void SyncPtrs() {
        legacyView_ = world_.BuildRenderSceneView();
    }
};
```

Keep the legacy getters only so this checkpoint builds before the pipeline
is migrated. They are removed in Task 6.

- [ ] **Step 4: Update `TestScene::Init` to fill `world_`**

In `src/scene/TestScene.cpp`, replace direct writes to `materials_`,
`meshesOwned_`, `modelMatrices_`, and `sunLight_` with writes to
`world_`.

Use this pattern for materials:

```cpp
auto& materials = world_.GetMaterials();
materials.push_back({{},{},{},{}, {0.56f,0.36f,0.25f,1}, 0.8f, 0.0f});
materials.push_back({{},{},{},{}, {0.95f,0.64f,0.54f,1}, 0.4f, 1.0f});
materials.push_back({{},{},{},{}, {1.0f,0.84f,0.0f,1},  0.1f, 1.0f});
materials.push_back({{},{},{},{}, {0.9f,0.9f,0.9f,1},   0.95f,0.0f});
```

Use this pattern for each renderable:

```cpp
auto& floor = world_.CreateEntity("Floor");
floor.meshRenderer = MeshRenderer { BuildMesh(pv, {0,1,2, 0,2,3}, 3) };

auto& ironSphere = world_.CreateEntity("Rusty Iron Sphere");
ironSphere.transform.translation = {-3.0f, 2.0f, 0.0f};
ironSphere.meshRenderer = MeshRenderer { BuildMesh(sv, si, 0) };
```

Set the light through `world_`:

```cpp
auto& sun = world_.GetSunLight();
sun.direction = glm::normalize(glm::vec3(0.4f, -1.0f, -0.3f));
sun.color = {1.0f, 0.95f, 0.85f};
sun.intensity = 8.0f;
```

Leave the final call:

```cpp
SyncPtrs();
```

- [ ] **Step 5: Update `ModelScene::Init` to fill `world_`**

In `src/scene/ModelScene.cpp`, replace direct writes to `materials_`,
`meshesOwned_`, `modelMatrices_`, and `sunLight_` with writes to
`world_`.

Use this pattern for model materials and entity:

```cpp
auto& materials = world_.GetMaterials();
const uint32_t modelMaterialBase = static_cast<uint32_t>(materials.size());
for (auto& mat : loaded.materials) {
    materials.push_back(std::move(mat));
}

if (modelMaterialBase != 0) {
    for (auto& sub : loaded.mesh->subMeshes) {
        sub.materialIndex += modelMaterialBase;
    }
}

auto& model = world_.CreateEntity(sceneName_);
model.transform.scale = {modelScale_, modelScale_, modelScale_};
model.meshRenderer = MeshRenderer { loaded.mesh };
```

Use this pattern for the optional floor:

```cpp
Material floorMat;
floorMat.baseColorFactor = {0.7f, 0.7f, 0.7f, 1.0f};
floorMat.roughnessFactor = 0.9f;
floorMat.metallicFactor = 0.0f;
uint32_t floorMatIdx = static_cast<uint32_t>(materials.size());
materials.push_back(std::move(floorMat));

auto& floor = world_.CreateEntity("Floor");
floor.transform.translation = {0.0f, -1.0f, 0.0f};
floor.meshRenderer = MeshRenderer { BuildFloor(floorMatIdx, 10.0f) };
```

Set the sun through `world_.GetSunLight()` and keep the final `SyncPtrs()`.

- [ ] **Step 6: Configure and build**

Run:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

Expected: build succeeds. The legacy `Scene` getters still allow the old
passes to compile.

- [ ] **Step 7: Commit**

```powershell
git add src/scene/World.h src/scene/World.cpp `
        src/scene/Scene.h src/scene/TestScene.cpp src/scene/ModelScene.cpp
git commit -m "refactor: introduce lightweight world model"
```

---

## Task 3: Route Pipeline Through FrameContext, RenderSceneView, and Outputs

**Files:**
- Modify: `src/pipeline/RenderPipeline.h`
- Modify: `src/pipeline/RenderPipeline.cpp`
- Modify: `src/pipeline/passes/ShadowPass.h/cpp`
- Modify: `src/pipeline/passes/GBufferPass.h/cpp`
- Modify: `src/pipeline/passes/LightingPass.h/cpp`
- Modify: `src/pipeline/passes/PostProcessPass.h/cpp`

- [ ] **Step 1: Update `GBufferPass` signature and output**

In `GBufferPass.h`, change the render signature and add output:

```cpp
#include "../PipelineOutputs.h"
#include "../../renderer/FrameContext.h"
#include "../../renderer/RenderSceneView.h"

GBufferOutputs Render(const RenderSceneView& scene, const FrameContext& frame);
GBufferOutputs GetOutputs() const;
```

In `GBufferPass.cpp`, replace the scene loops with:

```cpp
shader_->SetMat4("viewProj", frame.camera.viewProj);

for (const auto& renderable : scene.renderables) {
    if (!renderable.mesh || !renderable.materials) {
        continue;
    }

    const Mesh* mesh = renderable.mesh;
    shader_->SetMat4("model", renderable.modelMatrix);

    mesh->vao->Bind();
    for (const auto& sub : mesh->subMeshes) {
        const Material& mat = (*renderable.materials)[sub.materialIndex];
        // Keep the existing material uniform and texture binding block here.
        glDrawElements(GL_TRIANGLES, sub.indexCount, GL_UNSIGNED_INT,
                       (void*)(uintptr_t)(sub.indexOffset * sizeof(uint32_t)));
    }
    mesh->vao->Unbind();
}
```

At the end of `Render`, return:

```cpp
return GetOutputs();
```

Implement:

```cpp
GBufferOutputs GBufferPass::GetOutputs() const {
    GBufferOutputs outputs;
    outputs.albedoMetallic = fbo_->GetColor(0);
    outputs.normalRoughness = fbo_->GetColor(1);
    outputs.depth = fbo_->GetDepth();
    return outputs;
}
```

Keep `GetAlbedoMetallic`, `GetNormalRoughness`, and `GetDepth` for this
checkpoint only if another file still calls them. Remove them in Task 6.

- [ ] **Step 2: Update `ShadowPass` signature and output**

In `ShadowPass.h`, include the new contracts and change render:

```cpp
#include "../PipelineOutputs.h"
#include "../../renderer/FrameContext.h"
#include "../../renderer/RenderSceneView.h"

ShadowOutputs Render(const RenderSceneView& scene, const FrameContext& frame);
ShadowOutputs GetOutputs() const;
```

In `ShadowPass.cpp`, change the first lines of `Render` to:

```cpp
ShadowOutputs ShadowPass::Render(const RenderSceneView& scene,
                                  const FrameContext& frame) {
    const CameraData& camera = frame.camera;
    const DirectionalLight& light = scene.sunLight;
```

Replace the draw loop with:

```cpp
for (const auto& renderable : scene.renderables) {
    if (!renderable.mesh) {
        continue;
    }

    shader_->SetMat4("model", renderable.modelMatrix);
    const Mesh* mesh = renderable.mesh;
    mesh->vao->Bind();
    for (const auto& sub : mesh->subMeshes) {
        glDrawElements(GL_TRIANGLES, sub.indexCount, GL_UNSIGNED_INT,
                       (void*)(uintptr_t)(sub.indexOffset * sizeof(uint32_t)));
    }
    mesh->vao->Unbind();
}
```

At the end of `Render`, return `GetOutputs()`.

Implement:

```cpp
ShadowOutputs ShadowPass::GetOutputs() const {
    ShadowOutputs outputs;
    outputs.shadowArray = shadowArrayID_;
    outputs.cascades = cascades_;
    return outputs;
}
```

- [ ] **Step 3: Update `LightingPass` signature and remove UI-owned state**

In `LightingPass.h`, remove:

```cpp
float GetAmbientStrength() const { return ambientStrength_; }
void  SetAmbientStrength(float v) { ambientStrength_ = v; }
float ambientStrength_ = 1.0f;
```

Add:

```cpp
#include "../PipelineOutputs.h"
#include "../../renderer/FrameContext.h"
#include "../../renderer/RenderSceneView.h"

LightingOutputs Render(const GBufferOutputs& gbuffer,
                       const ShadowOutputs& shadow,
                       const RenderSceneView& scene,
                       const FrameContext& frame);
LightingOutputs GetOutputs() const;
```

In `LightingPass.cpp`, update the render inputs:

```cpp
LightingOutputs LightingPass::Render(const GBufferOutputs& gbuffer,
                                      const ShadowOutputs& shadow,
                                      const RenderSceneView& scene,
                                      const FrameContext& frame) {
```

Replace texture reads:

```cpp
gbuffer.albedoMetallic->Bind(0);
gbuffer.normalRoughness->Bind(1);
gbuffer.depth->Bind(2);
```

Replace shadow reads:

```cpp
glBindTextureUnit(3, shadow.shadowArray);
for (int c = 0; c < 4; ++c) {
    pbrShader_->SetMat4("uCascadeViewProj[" + std::to_string(c) + "]",
                        shadow.cascades[c].viewProj);
    pbrShader_->SetFloat("uCascadeFar[" + std::to_string(c) + "]",
                         shadow.cascades[c].farPlane);
}
```

Replace light and camera reads:

```cpp
const auto& light = scene.sunLight;
pbrShader_->SetVec3("uLightDir", light.direction);
pbrShader_->SetVec3("uLightColor", light.color * light.intensity);

pbrShader_->SetMat4("uView", frame.camera.view);
pbrShader_->SetMat4("uInvViewProj", glm::inverse(frame.camera.viewProj));
pbrShader_->SetVec3("uCamPos", frame.camera.camPos);
pbrShader_->SetFloat("uAmbientStrength", frame.renderSettings.ambientStrength);
```

At the end of `Render`, return `GetOutputs()`.

Implement:

```cpp
LightingOutputs LightingPass::GetOutputs() const {
    LightingOutputs outputs;
    outputs.hdrColor = hdrFBO_->GetColor(0);
    return outputs;
}
```

- [ ] **Step 4: Update `PostProcessPass` to read settings from `FrameContext`**

In `PostProcessPass.h`, remove `toneMapMode_`, `debugMode_`, and their
setters/getters/cyclers.

Add:

```cpp
#include "../PipelineOutputs.h"
#include "../../renderer/FrameContext.h"

void Render(const LightingOutputs& lighting,
            const GBufferOutputs& gbuffer,
            const ShadowOutputs& shadow,
            const FrameContext& frame);
```

In `PostProcessPass.cpp`, set shader modes from frame settings:

```cpp
shader_->SetInt("uToneMapMode", ToShaderToneMapMode(frame.renderSettings.toneMapMode));
shader_->SetInt("uDebugMode", ToShaderDebugView(frame.debugSettings.view));
```

Replace camera and cascade reads:

```cpp
shader_->SetMat4("uView", frame.camera.view);
shader_->SetMat4("uInvViewProj", glm::inverse(frame.camera.viewProj));
shader_->SetFloat("uNearPlane", frame.camera.near_);
shader_->SetFloat("uFarPlane", frame.camera.far_);

for (int c = 0; c < 4; ++c) {
    shader_->SetFloat("uCascadeFar[" + std::to_string(c) + "]",
                      shadow.cascades[c].farPlane);
}
```

Replace texture reads:

```cpp
lighting.hdrColor->Bind(0);
gbuffer.albedoMetallic->Bind(1);
gbuffer.normalRoughness->Bind(2);
gbuffer.depth->Bind(3);
```

- [ ] **Step 5: Move UBO ownership into `RenderPipeline`**

In `RenderPipeline.h`, include `../renderer/UniformBuffer.h` and add:

```cpp
const PipelineOutputs& Execute(const RenderSceneView& scene,
                               const FrameContext& frame);
const PipelineOutputs& GetOutputs() const { return outputs_; }

void UpdateUniformBuffers(const RenderSceneView& scene, const FrameContext& frame);

std::unique_ptr<CameraUBO> cameraUBO_;
std::unique_ptr<LightsUBO> lightsUBO_;
std::unique_ptr<TimeUBO> timeUBO_;
PipelineOutputs outputs_;
```

In `RenderPipeline::Init`, create the UBOs:

```cpp
cameraUBO_ = std::make_unique<CameraUBO>();
lightsUBO_ = std::make_unique<LightsUBO>();
timeUBO_ = std::make_unique<TimeUBO>();
```

Implement:

```cpp
void RenderPipeline::UpdateUniformBuffers(const RenderSceneView& scene,
                                          const FrameContext& frame) {
    cameraUBO_->Update(frame.camera);

    LightsData lightData;
    lightData.dirLightDir = scene.sunLight.direction;
    lightData.dirLightColor = scene.sunLight.color;
    lightData.dirLightIntensity = scene.sunLight.intensity;
    lightsUBO_->Update(lightData);

    TimeData timeData;
    timeData.time = frame.time;
    timeData.deltaTime = frame.deltaTime;
    timeUBO_->Update(timeData);
}
```

Update `Execute`:

```cpp
const PipelineOutputs& RenderPipeline::Execute(const RenderSceneView& scene,
                                               const FrameContext& frame) {
    UpdateUniformBuffers(scene, frame);
    outputs_.shadow = shadowPass_.Render(scene, frame);
    outputs_.gbuffer = gbufferPass_.Render(scene, frame);
    outputs_.lighting = lightingPass_.Render(outputs_.gbuffer, outputs_.shadow,
                                             scene, frame);
    postProcessPass_.Render(outputs_.lighting, outputs_.gbuffer,
                            outputs_.shadow, frame);
    return outputs_;
}
```

- [ ] **Step 6: Temporarily adapt `App` to own settings until Task 4**

Before `ApplicationState` exists, keep the current `App` fields and add
temporary settings members to `App.h`:

```cpp
#include "../renderer/FrameContext.h"

RenderSettings renderSettings_;
DebugSettings debugSettings_;
```

Update `HandleHotkeys()` so it no longer calls `PostProcessPass`:

```cpp
if (Input::IsKeyJustPressed(GLFW_KEY_T)) {
    renderSettings_.CycleToneMap();
}
if (Input::IsKeyJustPressed(GLFW_KEY_0) || Input::IsKeyJustPressed(GLFW_KEY_KP_0)) {
    debugSettings_.view = DebugView::Final;
}
if (Input::IsKeyJustPressed(GLFW_KEY_1) || Input::IsKeyJustPressed(GLFW_KEY_KP_1)) {
    debugSettings_.view = DebugView::Albedo;
}
if (Input::IsKeyJustPressed(GLFW_KEY_2) || Input::IsKeyJustPressed(GLFW_KEY_KP_2)) {
    debugSettings_.view = DebugView::Normal;
}
if (Input::IsKeyJustPressed(GLFW_KEY_3) || Input::IsKeyJustPressed(GLFW_KEY_KP_3)) {
    debugSettings_.view = DebugView::Roughness;
}
if (Input::IsKeyJustPressed(GLFW_KEY_4) || Input::IsKeyJustPressed(GLFW_KEY_KP_4)) {
    debugSettings_.view = DebugView::Metallic;
}
if (Input::IsKeyJustPressed(GLFW_KEY_5) || Input::IsKeyJustPressed(GLFW_KEY_KP_5)) {
    debugSettings_.view = DebugView::Depth;
}
if (Input::IsKeyJustPressed(GLFW_KEY_6) || Input::IsKeyJustPressed(GLFW_KEY_KP_6)) {
    debugSettings_.view = DebugView::Cascades;
}
```

Update the existing `BuildDebugPanel()` render controls so they mutate
the temporary settings:

```cpp
static const char* toneModes[] = { "ACES", "Reinhard", "None" };
int toneMode = ToShaderToneMapMode(renderSettings_.toneMapMode);
if (ImGui::Combo("Tone Map", &toneMode, toneModes, 3)) {
    renderSettings_.toneMapMode = static_cast<ToneMapMode>(toneMode);
}

static const char* debugModes[] = {
    "Final", "Albedo", "Normal", "Roughness", "Metallic", "Depth", "Cascades"
};
int debugMode = ToShaderDebugView(debugSettings_.view);
if (ImGui::Combo("Debug Mode", &debugMode, debugModes, 7)) {
    debugSettings_.view = static_cast<DebugView>(debugMode);
}

ImGui::DragFloat("Ambient Strength", &renderSettings_.ambientStrength,
                 0.01f, 0.0f, 2.0f);
```

Then build the new contracts locally in `App::Render()`:

```cpp
Scene& activeScene = *scenes_[activeSceneIdx_];
RenderSceneView sceneView = activeScene.BuildRenderSceneView();

FrameContext frame;
frame.width = w;
frame.height = h;
frame.time = static_cast<float>(glfwGetTime());
frame.deltaTime = lastTime_;
frame.camera = camera_->GetData(aspect);
frame.renderSettings = renderSettings_;
frame.debugSettings = debugSettings_;

pipeline_->Execute(sceneView, frame);
```

These temporary members are removed in Task 4 when `ApplicationState`
becomes the source of runtime settings.

- [ ] **Step 7: Configure and build**

Run:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

Expected: build succeeds. Existing UI may still live in `App`, but pass
state now flows through `FrameContext`.

- [ ] **Step 8: Commit**

```powershell
git add src/pipeline src/renderer/FrameContext.h src/renderer/RenderSceneView.h `
        src/core/App.h src/core/App.cpp
git commit -m "refactor: route rendering through frame context"
```

---

## Task 4: Extract ApplicationState, SceneRegistry, and InputController

**Files:**
- Create: `src/app/ApplicationState.h`
- Create: `src/app/SceneRegistry.h`
- Create: `src/app/SceneRegistry.cpp`
- Create: `src/app/InputController.h`
- Create: `src/app/InputController.cpp`
- Modify: `src/core/App.h`
- Modify: `src/core/App.cpp`

- [ ] **Step 1: Create `src/app/ApplicationState.h`**

```cpp
#pragma once
#include "SceneRegistry.h"
#include "../core/Camera.h"
#include "../renderer/FrameContext.h"

namespace HuanGL {

struct FrameStats {
    float deltaTime = 0.0f;
    float frameTimeMs = 0.0f;
    float fps = 0.0f;
};

struct ApplicationState {
    bool running = true;
    SceneRegistry sceneRegistry;
    Camera camera {60.0f, 0.1f, 100.0f};
    RenderSettings renderSettings;
    DebugSettings debugSettings;
    FrameStats frameStats;
};

} // namespace HuanGL
```

- [ ] **Step 2: Create `src/app/SceneRegistry.h`**

```cpp
#pragma once
#include "../scene/Scene.h"
#include <memory>
#include <string>
#include <vector>

namespace HuanGL {

class ResourceManager;

class SceneRegistry {
public:
    void RegisterRequired(std::unique_ptr<Scene> scene,
                          std::string name,
                          ResourceManager& resources);
    void RegisterOptional(std::unique_ptr<Scene> scene,
                          std::string name,
                          ResourceManager& resources);

    void Cycle();

    bool Empty() const { return scenes_.empty(); }
    size_t GetActiveIndex() const { return activeSceneIdx_; }
    const std::string& GetActiveName() const;

    Scene* GetActiveScene();
    const Scene* GetActiveScene() const;
    World* GetActiveWorld();
    const World* GetActiveWorld() const;

private:
    std::vector<std::unique_ptr<Scene>> scenes_;
    std::vector<std::string> sceneNames_;
    size_t activeSceneIdx_ = 0;
};

} // namespace HuanGL
```

- [ ] **Step 3: Create `src/app/SceneRegistry.cpp`**

```cpp
#include "SceneRegistry.h"
#include "../resource/ResourceManager.h"
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <utility>

namespace HuanGL {

void SceneRegistry::RegisterRequired(std::unique_ptr<Scene> scene,
                                     std::string name,
                                     ResourceManager& resources) {
    scene->Init(resources);
    scenes_.push_back(std::move(scene));
    sceneNames_.push_back(std::move(name));
    std::printf("[App] Registered scene '%s' (index %zu)\n",
                sceneNames_.back().c_str(), scenes_.size() - 1);
}

void SceneRegistry::RegisterOptional(std::unique_ptr<Scene> scene,
                                     std::string name,
                                     ResourceManager& resources) {
    try {
        RegisterRequired(std::move(scene), std::move(name), resources);
    } catch (const std::exception& e) {
        std::printf("[App] Skipped optional scene: %s\n", e.what());
    } catch (...) {
        std::printf("[App] Skipped optional scene: unknown exception\n");
    }
}

void SceneRegistry::Cycle() {
    if (scenes_.empty()) {
        return;
    }
    activeSceneIdx_ = (activeSceneIdx_ + 1) % scenes_.size();
    std::printf("[App] Switched to scene '%s' (index %zu)\n",
                sceneNames_[activeSceneIdx_].c_str(), activeSceneIdx_);
}

const std::string& SceneRegistry::GetActiveName() const {
    if (scenes_.empty()) {
        throw std::runtime_error("[SceneRegistry] no active scene");
    }
    return sceneNames_[activeSceneIdx_];
}

Scene* SceneRegistry::GetActiveScene() {
    return scenes_.empty() ? nullptr : scenes_[activeSceneIdx_].get();
}

const Scene* SceneRegistry::GetActiveScene() const {
    return scenes_.empty() ? nullptr : scenes_[activeSceneIdx_].get();
}

World* SceneRegistry::GetActiveWorld() {
    Scene* scene = GetActiveScene();
    return scene ? &scene->GetWorld() : nullptr;
}

const World* SceneRegistry::GetActiveWorld() const {
    const Scene* scene = GetActiveScene();
    return scene ? &scene->GetWorld() : nullptr;
}

} // namespace HuanGL
```

- [ ] **Step 4: Create `src/app/InputController.h`**

```cpp
#pragma once

namespace HuanGL {

struct ApplicationState;

class InputController {
public:
    void Update(ApplicationState& state);
};

} // namespace HuanGL
```

- [ ] **Step 5: Create `src/app/InputController.cpp`**

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
}

} // namespace HuanGL
```

- [ ] **Step 6: Update `App.h` members**

Remove camera, scene vectors, UBOs, `running_`, `HandleHotkeys`, and
`CycleScene` from `App`. Add:

```cpp
#include "../app/ApplicationState.h"
#include "../app/InputController.h"

void RegisterScenes();
FrameContext BuildFrameContext(float dt) const;

ApplicationState state_;
InputController inputController_;
```

Keep `BuildDebugPanel()` until Task 5 if `DebugUI` is not wired yet.

- [ ] **Step 7: Update scene registration in `App.cpp`**

Move the hardcoded registration block into `App::RegisterScenes()`:

```cpp
void App::RegisterScenes() {
    state_.sceneRegistry.RegisterRequired(
        std::make_unique<TestScene>(), "TestScene", *resourceManager_);

    const char* helmetPath = "../resources/models/DamagedHelmet.glb";
    std::printf("[App] Attempting DamagedHelmet from '%s' (exists=%d)\n",
                helmetPath, std::filesystem::exists(helmetPath) ? 1 : 0);
    state_.sceneRegistry.RegisterOptional(
        std::make_unique<ModelScene>(helmetPath, "DamagedHelmet", true, 1.0f),
        "DamagedHelmet", *resourceManager_);

    const char* sponzaPath = "../resources/models/Sponza/glTF/Sponza.gltf";
    std::printf("[App] Attempting Sponza from '%s' (exists=%d)\n",
                sponzaPath, std::filesystem::exists(sponzaPath) ? 1 : 0);
    state_.sceneRegistry.RegisterOptional(
        std::make_unique<ModelScene>(sponzaPath, "Sponza", false, 0.02f),
        "Sponza", *resourceManager_);
}
```

- [ ] **Step 8: Build `FrameContext` in `App.cpp`**

Add:

```cpp
FrameContext App::BuildFrameContext(float dt) const {
    FrameContext frame;
    frame.width = window_->GetWidth();
    frame.height = window_->GetHeight();
    frame.time = static_cast<float>(glfwGetTime());
    frame.deltaTime = dt;
    frame.renderSettings = state_.renderSettings;
    frame.debugSettings = state_.debugSettings;

    float aspect = frame.height > 0
        ? static_cast<float>(frame.width) / static_cast<float>(frame.height)
        : 1.0f;
    frame.camera = state_.camera.GetData(aspect);
    return frame;
}
```

- [ ] **Step 9: Update `App::Run`, `Update`, and `Render`**

`App::Run` should call the input controller:

```cpp
Input::Update();
window_->PollEvents();
inputController_.Update(state_);

Update(dt);
Render(dt);
```

`Update` should use state:

```cpp
state_.camera.Update(dt, window_->GetHandle(), !state_.debugSettings.freezeCamera);
if (Scene* scene = state_.sceneRegistry.GetActiveScene()) {
    scene->Update(dt);
}

state_.frameStats.deltaTime = dt;
state_.frameStats.frameTimeMs = dt * 1000.0f;
state_.frameStats.fps = dt > 0.0f ? 1.0f / dt : 0.0f;
```

`Render` should use the active scene and frame context:

```cpp
int w = window_->GetWidth();
int h = window_->GetHeight();
if (w <= 0 || h <= 0) return;

Scene* activeScene = state_.sceneRegistry.GetActiveScene();
if (!activeScene) return;

RenderSceneView sceneView = activeScene->BuildRenderSceneView();
FrameContext frame = BuildFrameContext(state_.frameStats.deltaTime);

Renderer::SetClearColor(0.1f, 0.1f, 0.15f, 1.0f);
Renderer::Clear();

pipeline_->Execute(sceneView, frame);
```

- [ ] **Step 10: Configure and build**

Run:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

Expected: build succeeds. `App` now uses state and input controller.

- [ ] **Step 11: Commit**

```powershell
git add src/app src/core/App.h src/core/App.cpp
git commit -m "refactor: extract application state and input controller"
```

---

## Task 5: Extract DebugUI and Remove ImGui from App

**Files:**
- Create: `src/ui/DebugUI.h`
- Create: `src/ui/DebugUI.cpp`
- Modify: `src/core/App.h`
- Modify: `src/core/App.cpp`

- [ ] **Step 1: Create `src/ui/DebugUI.h`**

```cpp
#pragma once

namespace HuanGL {

struct ApplicationState;

class DebugUI {
public:
    void Draw(ApplicationState& state);
};

} // namespace HuanGL
```

- [ ] **Step 2: Create `src/ui/DebugUI.cpp`**

```cpp
#include "DebugUI.h"
#include "../app/ApplicationState.h"
#include "../scene/World.h"
#include <glm/geometric.hpp>
#include <imgui.h>

namespace HuanGL {

static int ToneMapIndex(ToneMapMode mode) {
    return ToShaderToneMapMode(mode);
}

static ToneMapMode ToneMapFromIndex(int mode) {
    return static_cast<ToneMapMode>(mode);
}

static int DebugViewIndex(DebugView view) {
    return ToShaderDebugView(view);
}

static DebugView DebugViewFromIndex(int view) {
    return static_cast<DebugView>(view);
}

void DebugUI::Draw(ApplicationState& state) {
    if (!state.debugSettings.showImGui) {
        return;
    }

    ImGui::Begin("HuanGL Debug");

    if (ImGui::CollapsingHeader("Render")) {
        static const char* toneModes[] = { "ACES", "Reinhard", "None" };
        int toneMode = ToneMapIndex(state.renderSettings.toneMapMode);
        if (ImGui::Combo("Tone Map", &toneMode, toneModes, 3)) {
            state.renderSettings.toneMapMode = ToneMapFromIndex(toneMode);
        }

        static const char* debugModes[] = {
            "Final", "Albedo", "Normal", "Roughness", "Metallic", "Depth", "Cascades"
        };
        int debugMode = DebugViewIndex(state.debugSettings.view);
        if (ImGui::Combo("Debug Mode", &debugMode, debugModes, 7)) {
            state.debugSettings.view = DebugViewFromIndex(debugMode);
        }

        ImGui::DragFloat("Ambient Strength", &state.renderSettings.ambientStrength,
                         0.01f, 0.0f, 2.0f);
    }

    if (ImGui::CollapsingHeader("Lighting")) {
        World* world = state.sceneRegistry.GetActiveWorld();
        if (world) {
            auto& sun = world->GetSunLight();
            ImGui::DragFloat3("Direction", &sun.direction.x, 0.01f, -1.0f, 1.0f);
            if (glm::length(sun.direction) > 0.0f) {
                sun.direction = glm::normalize(sun.direction);
            }
            ImGui::ColorEdit3("Color", &sun.color.r);
            ImGui::DragFloat("Intensity", &sun.intensity, 0.05f, 0.0f, 20.0f);
        }
    }

    if (ImGui::CollapsingHeader("Camera")) {
        float fov = state.camera.GetFov();
        if (ImGui::SliderFloat("FOV", &fov, 30.0f, 120.0f)) {
            state.camera.SetFov(fov);
        }
        ImGui::Checkbox("Freeze Camera", &state.debugSettings.freezeCamera);
    }

    if (ImGui::CollapsingHeader("Scene")) {
        if (!state.sceneRegistry.Empty()) {
            ImGui::Text("Active: %s", state.sceneRegistry.GetActiveName().c_str());
            if (ImGui::Button("Next")) {
                state.sceneRegistry.Cycle();
            }
        }

        World* world = state.sceneRegistry.GetActiveWorld();
        if (world) {
            for (auto& entity : world->GetEntities()) {
                ImGui::PushID(static_cast<int>(entity.id));
                if (ImGui::TreeNode(entity.name.c_str())) {
                    ImGui::DragFloat3("Translation", &entity.transform.translation.x, 0.05f);
                    ImGui::DragFloat3("Scale", &entity.transform.scale.x, 0.01f, 0.01f, 100.0f);
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }
    }

    if (ImGui::CollapsingHeader("Stats")) {
        ImGui::Text("FPS: %.1f", state.frameStats.fps);
        ImGui::Text("Frame: %.2f ms", state.frameStats.frameTimeMs);
    }

    ImGui::End();
}

} // namespace HuanGL
```

- [ ] **Step 3: Update `App.h` to own `DebugUI`**

Forward declare or include `DebugUI`, then add:

```cpp
std::unique_ptr<DebugUI> debugUI_;
```

Remove:

```cpp
void BuildDebugPanel();
```

- [ ] **Step 4: Update `App.cpp` ImGui frame**

Include:

```cpp
#include "../ui/DebugUI.h"
```

In `Init`, create:

```cpp
debugUI_ = std::make_unique<DebugUI>();
```

Replace:

```cpp
BuildDebugPanel();
```

with:

```cpp
debugUI_->Draw(state_);
```

Delete the entire `App::BuildDebugPanel()` function and remove
`#include <imgui.h>` from `App.cpp`.

- [ ] **Step 5: Configure and build**

Run:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

Expected: build succeeds. `App.cpp` no longer contains ImGui widget calls.

- [ ] **Step 6: Commit**

```powershell
git add src/ui/DebugUI.h src/ui/DebugUI.cpp src/core/App.h src/core/App.cpp
git commit -m "refactor: move debug panel out of app"
```

---

## Task 6: Remove Transitional Interfaces and Update Architecture Docs

**Files:**
- Modify: `src/scene/Scene.h`
- Modify: `src/pipeline/RenderPipeline.h`
- Modify: `src/pipeline/passes/GBufferPass.h/cpp`
- Modify: `src/pipeline/passes/LightingPass.h/cpp`
- Modify: `src/pipeline/passes/PostProcessPass.h/cpp`
- Modify: `docs/architecture.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Search for transitional APIs**

Run:

```powershell
rg -n "GetMeshCount|GetMesh\\(|GetModelMatrix|GetMaterials\\(|GetMutableSunLight|GetPostProcess|GetLighting|SetAmbientStrength|GetAmbientStrength|CycleToneMap|SetDebugMode|GetDebugMode|SetToneMapMode|GetToneMapMode" src
```

Expected before cleanup: matches only in declarations or old compatibility
code. Any match in `App`, `DebugUI`, or `InputController` means the
boundary is still wrong and must be fixed before continuing.

- [ ] **Step 2: Remove legacy getters from `Scene.h`**

After no render pass uses them, reduce `Scene` to:

```cpp
class Scene {
public:
    virtual ~Scene() = default;
    virtual void Init(ResourceManager& rm) = 0;
    virtual void Update(float dt) { world_.Update(dt); }

    World& GetWorld() { return world_; }
    const World& GetWorld() const { return world_; }

    RenderSceneView BuildRenderSceneView() const {
        return world_.BuildRenderSceneView();
    }

protected:
    World world_;
};
```

Remove `SyncPtrs()` calls from `TestScene.cpp` and `ModelScene.cpp`.

- [ ] **Step 3: Remove pass state accessors**

Remove these methods if they still exist:

```cpp
RenderPipeline::GetPostProcess()
RenderPipeline::GetLighting()
LightingPass::GetAmbientStrength()
LightingPass::SetAmbientStrength(float)
PostProcessPass::SetToneMapMode(int)
PostProcessPass::GetToneMapMode() const
PostProcessPass::SetDebugMode(int)
PostProcessPass::GetDebugMode() const
PostProcessPass::CycleToneMap()
PostProcessPass::CycleDebugMode()
```

The only runtime path for these values should be:

```text
InputController or DebugUI -> ApplicationState -> FrameContext -> RenderPipeline
```

- [ ] **Step 4: Remove compatibility texture getters if output structs replaced them**

If `rg` shows no users outside a pass implementation, remove:

```cpp
GBufferPass::GetAlbedoMetallic()
GBufferPass::GetNormalRoughness()
GBufferPass::GetDepth()
LightingPass::GetHDROutput()
ShadowPass::GetShadowMapArray()
ShadowPass::GetCascades()
```

Keep a getter only if it is still necessary for a deliberate debug path.
If one remains, document why next to the declaration.

- [ ] **Step 5: Update `docs/architecture.md` module map**

Update the `Module Map` table so it includes:

```markdown
| `src/app/` | Runtime state, scene registry, input command mapping | `ApplicationState`, `SceneRegistry`, `InputController` |
| `src/scene/` | Lightweight world/entities and demo scene builders | `World`, `Entity`, `TestScene`, `ModelScene` |
| `src/ui/` | ImGui lifecycle and debug panels | `ImGuiLayer`, `DebugUI` |
```

Update the render pipeline section to mention `RenderSceneView`,
`FrameContext`, and `PipelineOutputs`.

- [ ] **Step 6: Update `AGENTS.md` current progress**

Add the new files to the Phase 3 progress table:

```markdown
| `src/app/ApplicationState.h` | Runtime state container for camera, settings, scene registry, frame stats |
| `src/app/SceneRegistry.h/cpp` | Demo scene registration, active scene selection, soft-failure loading |
| `src/app/InputController.h/cpp` | Keyboard command mapping into ApplicationState |
| `src/scene/World.h/cpp` | Lightweight entity/world model used by renderer and DebugUI |
| `src/renderer/FrameContext.h` | Per-frame render settings, debug settings, camera data |
| `src/renderer/RenderSceneView.h` | Read-only renderable view consumed by passes |
| `src/pipeline/PipelineOutputs.h` | Explicit pass output structs for future passes |
| `src/ui/DebugUI.h/cpp` | HuanGL Debug panel that edits state/world data |
```

Add a key technical decision:

```markdown
- Debug UI edits `ApplicationState` and `World`; render passes read
  `FrameContext` and `RenderSceneView`. Do not let ImGui mutate pass
  internals directly.
```

- [ ] **Step 7: Configure and build**

Run:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

Expected: build succeeds with no transitional API references from app/UI
to pass internals.

- [ ] **Step 8: Manual visual verification**

Run:

```powershell
.\build\HuanGL.exe
```

If the generator places the executable under the config folder, run:

```powershell
.\build\Debug\HuanGL.exe
```

Verify:

- app launches without a crash
- TestScene renders
- optional model scenes load or soft-fail as before
- `N` cycles scenes
- `T` cycles tone mapping
- `0` through `6` select debug views
- ImGui panel opens
- ImGui render settings change the output
- light direction/color/intensity edits affect the active world
- camera FOV edits affect the camera
- window resize still resizes the pipeline
- minimized window does not crash

- [ ] **Step 9: Commit**

```powershell
git add src docs\\architecture.md AGENTS.md
git commit -m "refactor: finish architecture boundary cleanup"
```

---

## Final Verification

After Task 6, run:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

Expected: configure and build both succeed.

Then run the manual visual checklist from Task 6 Step 8. Record any
visual deviations in the final response. If visual verification cannot be
completed because the environment has no working OpenGL display, state
that explicitly and report the build result only.

## Completion Criteria

The reset is complete when:

- `App` is limited to lifecycle, frame scheduling, and system composition.
- `DebugUI` contains ImGui widget code; `App.cpp` does not.
- `InputController` owns keyboard command mapping.
- `ApplicationState` owns camera, scene registry, render settings, debug
  settings, and frame stats.
- `World` owns inspectable entities, transforms, materials, ambient, and
  sun light.
- `RenderPipeline::Execute` takes `RenderSceneView` and `FrameContext`.
- Passes exchange `ShadowOutputs`, `GBufferOutputs`, `LightingOutputs`,
  and `PipelineOutputs`.
- UI code has no direct dependency on concrete render passes.
- Current renderer behavior is preserved by build and manual visual checks.
