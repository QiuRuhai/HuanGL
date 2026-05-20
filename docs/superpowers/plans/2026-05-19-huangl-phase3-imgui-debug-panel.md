# Phase 3 (Minimum): ImGui Debug Panel — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Integrate Dear ImGui as a runtime overlay exposing the renderer's tunables — tone map, debug mode, sun light, ambient strength, camera FOV, and scene cycling — through a single "HuanGL Debug" panel, while keeping all existing keyboard bindings working.

**Architecture:** ImGui is pulled in via vcpkg manifest. A thin `ImGuiLayer` module owns context lifetime and the GLFW+OpenGL3 backends. `App` gets one new member (`imguiLayer_`), three new lifecycle calls, and `BuildDebugPanel()`. No changes to `RenderPipeline` internals; new mutable accessors on `Scene`, `Camera`, `LightingPass`, and `RenderPipeline` provide panel read/write access. The inline hotkey block in `App::Run` is extracted to `HandleHotkeys()` to keep the loop body readable.

**Tech Stack:** Dear ImGui (vcpkg `imgui[glfw-binding,opengl3-binding]`), GLFW, OpenGL 4.6.

**Source spec:** `docs/superpowers/specs/2026-05-19-huangl-phase3-imgui-debug-panel-design.md`

**Note on testing:** This codebase has no test infrastructure, and ImGui/OpenGL code requires a live GL context. Verification takes the form of: build succeeds → run the app → manual visual inspection against the spec's §5 checklist.

---

## File Map

```
NEW:    vcpkg.json
NEW:    src/ui/ImGuiLayer.h
NEW:    src/ui/ImGuiLayer.cpp
MODIFY: CMakeLists.txt                          — add imgui find_package + link
MODIFY: src/scene/Scene.h                       — add GetMutableSunLight()
MODIFY: src/core/Camera.h                       — add GetFov() / SetFov(float)
MODIFY: src/pipeline/passes/LightingPass.h      — ambientStrength_ member + getter/setter
MODIFY: src/pipeline/passes/LightingPass.cpp    — use ambientStrength_ instead of hardcoded 1.0f
MODIFY: src/pipeline/RenderPipeline.h           — add GetLighting() accessor
MODIFY: src/core/App.h                          — forward-decl, imguiLayer_ member, 2 new method decls
MODIFY: src/core/App.cpp                        — include, Init/Shutdown wiring, HandleHotkeys, Run, BuildDebugPanel
```

---

### Task 1: Add Dear ImGui via vcpkg

**Files:**
- Create: `vcpkg.json`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create `vcpkg.json` in the project root**

Create `vcpkg.json` with this content:

```json
{
  "name": "huangl",
  "version-string": "0.0.0",
  "dependencies": [
    "glfw3",
    "assimp",
    {
      "name": "imgui",
      "features": ["glfw-binding", "opengl3-binding"]
    }
  ]
}
```

- [ ] **Step 2: Add imgui to `CMakeLists.txt`**

Open `CMakeLists.txt`. After the assimp block (lines 54–59, ending with the `endif()`), insert these two lines:

```cmake
find_package(imgui CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE imgui::imgui)
```

The resulting block looks like:

```cmake
find_package(assimp REQUIRED CONFIG)
if(TARGET assimp::assimp)
    target_link_libraries(${PROJECT_NAME} PRIVATE assimp::assimp)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE assimp)
endif()

find_package(imgui CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE imgui::imgui)
```

- [ ] **Step 3: Reconfigure and build to confirm imgui links**

Run from the project root (adjust `build` to your cmake binary dir):

```powershell
cmake -S . -B build
cmake --build build --config Release
```

Expected: configure prints `-- Found imgui: ...`; build succeeds with no new errors.

If cmake cannot find imgui, install it first:

```powershell
& "$env:VCPKG_ROOT\vcpkg" install "imgui[glfw-binding,opengl3-binding]:x64-windows"
```

Then re-run the configure command.

- [ ] **Step 4: Commit**

```powershell
git add vcpkg.json CMakeLists.txt
git commit -m @'
build: add Dear ImGui via vcpkg manifest

Introduces vcpkg.json declaring the project's three vcpkg dependencies
(glfw3, assimp, imgui[glfw-binding,opengl3-binding]) so ImGui feature
selection is declarative and reproducible. Wires imgui::imgui into the
CMake link step.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
'@
```

---

### Task 2: Create `ImGuiLayer` module

**Files:**
- Create: `src/ui/ImGuiLayer.h`
- Create: `src/ui/ImGuiLayer.cpp`

- [ ] **Step 1: Create `src/ui/ImGuiLayer.h`**

```cpp
#pragma once
struct GLFWwindow;

namespace HuanGL {

class ImGuiLayer {
public:
    ImGuiLayer() = default;
    ~ImGuiLayer();
    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;

    void Init(GLFWwindow* window);
    void Shutdown();

    void BeginFrame();
    void EndFrame();

private:
    bool initialized_ = false;
};

} // namespace HuanGL
```

- [ ] **Step 2: Create `src/ui/ImGuiLayer.cpp`**

```cpp
#include "ImGuiLayer.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

namespace HuanGL {

ImGuiLayer::~ImGuiLayer() { Shutdown(); }

void ImGuiLayer::Init(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");
    initialized_ = true;
}

void ImGuiLayer::Shutdown() {
    if (!initialized_) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    initialized_ = false;
}

void ImGuiLayer::BeginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::EndFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

} // namespace HuanGL
```

- [ ] **Step 3: Build to confirm new files compile**

```powershell
cmake --build build --config Release
```

Expected: build succeeds; `ImGuiLayer.cpp` compiles with no errors.

- [ ] **Step 4: Commit**

```powershell
git add src/ui/ImGuiLayer.h src/ui/ImGuiLayer.cpp
git commit -m @'
feat(ui): add ImGuiLayer wrapper

Thin class owning the ImGui context lifetime and the GLFW + OpenGL3
backends. Init installs GLFW callbacks via the chain-callbacks path so
existing HuanGL input handlers (Input::Init key/cursor/scroll callbacks)
continue to work alongside ImGui.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
'@
```

---

### Task 3: Add mutable accessors for runtime tunables

**Files:**
- Modify: `src/scene/Scene.h`
- Modify: `src/core/Camera.h`
- Modify: `src/pipeline/passes/LightingPass.h`
- Modify: `src/pipeline/passes/LightingPass.cpp`
- Modify: `src/pipeline/RenderPipeline.h`

- [ ] **Step 1: Add `GetMutableSunLight()` to `Scene`**

Open `src/scene/Scene.h`. After the const accessor:

```cpp
const DirectionalLight& GetSunLight()  const { return sunLight_; }
```

add:

```cpp
DirectionalLight& GetMutableSunLight() { return sunLight_; }
```

- [ ] **Step 2: Add `GetFov()` / `SetFov()` to `Camera`**

Open `src/core/Camera.h`. After `glm::vec3 GetPos() const { return pos_; }`, add:

```cpp
float GetFov() const       { return glm::degrees(fov_); }
void  SetFov(float deg)    { fov_ = glm::radians(deg); }
```

`fov_` is stored in radians internally; these accessors convert at the boundary so callers work in degrees (matching the panel's 30–120 range).

- [ ] **Step 3: Add `ambientStrength_` to `LightingPass.h`**

Open `src/pipeline/passes/LightingPass.h`.

In the `public` section after `std::shared_ptr<Texture> GetHDROutput() const;`, add:

```cpp
float GetAmbientStrength() const { return ambientStrength_; }
void  SetAmbientStrength(float v) { ambientStrength_ = v; }
```

In the `private` section, after `int width_ = 0, height_ = 0;`, add:

```cpp
float ambientStrength_ = 1.0f;
```

- [ ] **Step 4: Use `ambientStrength_` in `LightingPass::Render`**

Open `src/pipeline/passes/LightingPass.cpp`. Find the line (currently line 200):

```cpp
pbrShader_->SetFloat("uAmbientStrength", 1.0f);
```

Replace with:

```cpp
pbrShader_->SetFloat("uAmbientStrength", ambientStrength_);
```

- [ ] **Step 5: Add `GetLighting()` to `RenderPipeline`**

Open `src/pipeline/RenderPipeline.h`. In the `public` section after `PostProcessPass& GetPostProcess() { return postProcessPass_; }`, add:

```cpp
LightingPass& GetLighting() { return lightingPass_; }
```

- [ ] **Step 6: Build to confirm all changes compile**

```powershell
cmake --build build --config Release
```

Expected: build succeeds with no errors.

- [ ] **Step 7: Commit**

```powershell
git add src/scene/Scene.h src/core/Camera.h `
        src/pipeline/passes/LightingPass.h `
        src/pipeline/passes/LightingPass.cpp `
        src/pipeline/RenderPipeline.h
git commit -m @'
feat: add mutable accessors for ImGui panel tunables

Scene::GetMutableSunLight() — non-const DirectionalLight reference.
Camera::GetFov/SetFov — degree-level FOV read/write (fov_ stays radians
internally; conversion at the boundary).
LightingPass::ambientStrength_ with getter/setter, replacing the
hardcoded 1.0f passed to uAmbientStrength in Render().
RenderPipeline::GetLighting() — exposes LightingPass to App.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
'@
```

---

### Task 4: Wire `ImGuiLayer` into `App` + extract `HandleHotkeys`

**Files:**
- Modify: `src/core/App.h`
- Modify: `src/core/App.cpp`

- [ ] **Step 1: Update `App.h`**

Open `src/core/App.h`. Make three targeted changes:

**Change 1** — After the existing forward-declarations block (before `class App {`), add:

```cpp
class ImGuiLayer;
```

**Change 2** — In the `private` methods section, after `void CycleScene();`, add:

```cpp
void HandleHotkeys();
void BuildDebugPanel();
```

**Change 3** — In the `private` members section, after `std::unique_ptr<ResourceManager> resourceManager_;`, add:

```cpp
std::unique_ptr<ImGuiLayer>      imguiLayer_;
```

The resulting private section of `App.h` should look like:

```cpp
private:
    void Init();
    void Shutdown();
    void Update(float dt);
    void Render();
    void RegisterScene(std::unique_ptr<Scene> scene, std::string name);
    void CycleScene();
    void HandleHotkeys();
    void BuildDebugPanel();

    std::unique_ptr<Window>          window_;
    std::unique_ptr<Camera>          camera_;
    std::unique_ptr<RenderPipeline>  pipeline_;
    std::unique_ptr<ResourceManager> resourceManager_;
    std::unique_ptr<ImGuiLayer>      imguiLayer_;

    std::vector<std::unique_ptr<Scene>> scenes_;
    std::vector<std::string>            sceneNames_;
    size_t                              activeSceneIdx_ = 0;

    std::unique_ptr<CameraUBO> cameraUBO_;
    std::unique_ptr<LightsUBO> lightsUBO_;
    std::unique_ptr<TimeUBO>   timeUBO_;

    float lastTime_ = 0.0f;
    bool  running_  = true;
```

- [ ] **Step 2: Add includes to `App.cpp`**

Open `src/core/App.cpp`. After the existing includes block (after `#include <filesystem>`), add:

```cpp
#include "../ui/ImGuiLayer.h"
#include <imgui.h>
```

- [ ] **Step 3: Wire `imguiLayer_` into `App::Init`**

In `App::Init`, after `Input::Init(window_->GetHandle());` (line 39), insert:

```cpp
imguiLayer_ = std::make_unique<ImGuiLayer>();
imguiLayer_->Init(window_->GetHandle());
```

- [ ] **Step 4: Wire `imguiLayer_` into `App::Shutdown`**

In `App::Shutdown`, before `ResourceManager::Shutdown();`, insert:

```cpp
if (imguiLayer_) imguiLayer_->Shutdown();
```

The ResourceManager must still have a valid GL context when ImGui's OpenGL backend tears down, so ImGui shuts down first.

- [ ] **Step 5: Add `HandleHotkeys()` method**

Add this new method above `App::Run()` in `App.cpp`. It contains the entire inline block extracted from `Run`:

```cpp
void App::HandleHotkeys() {
    if (Input::IsKeyPressed(GLFW_KEY_ESCAPE))
        running_ = false;

    if (Input::IsKeyJustPressed(GLFW_KEY_N))
        CycleScene();

    auto& pp = pipeline_->GetPostProcess();
    if (Input::IsKeyJustPressed(GLFW_KEY_T))
        pp.CycleToneMap();
    if (Input::IsKeyJustPressed(GLFW_KEY_0) || Input::IsKeyJustPressed(GLFW_KEY_KP_0))
        pp.SetDebugMode(0);
    if (Input::IsKeyJustPressed(GLFW_KEY_1) || Input::IsKeyJustPressed(GLFW_KEY_KP_1))
        pp.SetDebugMode(1);
    if (Input::IsKeyJustPressed(GLFW_KEY_2) || Input::IsKeyJustPressed(GLFW_KEY_KP_2))
        pp.SetDebugMode(2);
    if (Input::IsKeyJustPressed(GLFW_KEY_3) || Input::IsKeyJustPressed(GLFW_KEY_KP_3))
        pp.SetDebugMode(3);
    if (Input::IsKeyJustPressed(GLFW_KEY_4) || Input::IsKeyJustPressed(GLFW_KEY_KP_4))
        pp.SetDebugMode(4);
    if (Input::IsKeyJustPressed(GLFW_KEY_5) || Input::IsKeyJustPressed(GLFW_KEY_KP_5))
        pp.SetDebugMode(5);
    if (Input::IsKeyJustPressed(GLFW_KEY_6) || Input::IsKeyJustPressed(GLFW_KEY_KP_6))
        pp.SetDebugMode(6);
}
```

- [ ] **Step 6: Update `App::Run`**

Replace the body of `App::Run` with the following. The inline hotkey block is gone; the ImGui frame wraps between `Render()` and `SwapBuffers()`:

```cpp
void App::Run() {
    while (!window_->ShouldClose() && running_) {
        float now = static_cast<float>(glfwGetTime());
        float dt  = now - lastTime_;
        lastTime_ = now;

        Input::Update();
        window_->PollEvents();
        HandleHotkeys();

        Update(dt);
        Render();

        imguiLayer_->BeginFrame();
        BuildDebugPanel();
        imguiLayer_->EndFrame();

        window_->SwapBuffers();
    }
}
```

- [ ] **Step 7: Add a stub `BuildDebugPanel()` so it links**

Add this stub below `App::Run()`. The full implementation comes in Task 5:

```cpp
void App::BuildDebugPanel() {
}
```

- [ ] **Step 8: Build**

```powershell
cmake --build build --config Release
```

Expected: build succeeds. Running the app shows normal rendering with no crash; ImGui context is live but `BuildDebugPanel` draws nothing yet.

- [ ] **Step 9: Commit**

```powershell
git add src/core/App.h src/core/App.cpp
git commit -m @'
feat(app): wire ImGuiLayer into app lifecycle

Adds ImGuiLayer member to App; Init creates the context right after the
GLFW window is up, Shutdown tears it down before ResourceManager so a
valid GL context exists during ImGui's OpenGL backend cleanup. Run gains
BeginFrame/BuildDebugPanel/EndFrame between Render() and SwapBuffers().
Inline hotkey block extracted to HandleHotkeys() to keep Run() readable.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
'@
```

---

### Task 5: Implement `BuildDebugPanel`

**Files:**
- Modify: `src/core/App.cpp`

- [ ] **Step 1: Replace the stub with the full implementation**

Replace the stub `BuildDebugPanel` in `App.cpp` with:

```cpp
void App::BuildDebugPanel() {
    ImGui::Begin("HuanGL Debug");

    if (ImGui::CollapsingHeader("Render")) {
        auto& pp = pipeline_->GetPostProcess();

        static const char* toneModes[] = { "ACES", "Reinhard", "None" };
        int toneMode = pp.GetToneMapMode();
        if (ImGui::Combo("Tone Map", &toneMode, toneModes, 3))
            pp.SetToneMapMode(toneMode);

        static const char* debugModes[] = {
            "Final", "Albedo", "Normal", "Roughness", "Metallic", "Depth", "Cascades"
        };
        int debugMode = pp.GetDebugMode();
        if (ImGui::Combo("Debug Mode", &debugMode, debugModes, 7))
            pp.SetDebugMode(debugMode);
    }

    if (ImGui::CollapsingHeader("Lighting")) {
        if (!scenes_.empty()) {
            auto& sun = scenes_[activeSceneIdx_]->GetMutableSunLight();
            ImGui::DragFloat3("Direction", &sun.direction.x, 0.01f, -1.f, 1.f);
            if (glm::length(sun.direction) > 0.0f)
                sun.direction = glm::normalize(sun.direction);
            ImGui::ColorEdit3("Color", &sun.color.r);
            ImGui::DragFloat("Intensity", &sun.intensity, 0.05f, 0.f, 20.f);

            float ambient = pipeline_->GetLighting().GetAmbientStrength();
            if (ImGui::DragFloat("Ambient Strength", &ambient, 0.01f, 0.f, 2.f))
                pipeline_->GetLighting().SetAmbientStrength(ambient);
        }
    }

    if (ImGui::CollapsingHeader("Camera")) {
        float fov = camera_->GetFov();
        if (ImGui::SliderFloat("FOV", &fov, 30.f, 120.f))
            camera_->SetFov(fov);
    }

    if (ImGui::CollapsingHeader("Scene")) {
        if (!scenes_.empty()) {
            ImGui::Text("Active: %s", sceneNames_[activeSceneIdx_].c_str());
            if (ImGui::Button("Next"))
                CycleScene();
        }
    }

    if (ImGui::CollapsingHeader("Stats")) {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("FPS: %.1f", 1.0f / io.DeltaTime);
        ImGui::Text("Frame: %.2f ms", io.DeltaTime * 1000.0f);
    }

    ImGui::End();
}
```

The `glm/glm.hpp` header is already included transitively; `glm::length` and `glm::normalize` are available from the existing includes in `App.cpp`.

- [ ] **Step 2: Build**

```powershell
cmake --build build --config Release
```

Expected: build succeeds with no errors or warnings.

- [ ] **Step 3: Commit**

```powershell
git add src/core/App.cpp
git commit -m @'
feat(app): implement HuanGL Debug panel

Five CollapsingHeader groups: Render (tone map + debug mode combos that
stay in sync with the T/0-6 hotkeys), Lighting (sun dir/color/intensity
DragFloat controls + ambient strength), Camera (FOV slider 30-120 deg),
Scene (active name label + Next button), Stats (FPS + frame time from
ImGuiIO::DeltaTime). Sun direction is normalized after every drag to
keep the unit-length invariant.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
'@
```

---

### Task 6: Visual verification

No new code. Run the app and verify every item in the spec's §5 checklist.

- [ ] **Step 1: Build and run**

```powershell
cmake --build build --config Release
.\build\Release\HuanGL.exe
```

- [ ] **Step 2: Work through the spec checklist**

- [ ] "HuanGL Debug" window visible at startup, positioned near top-left corner.
- [ ] Tone Map combo shows `ACES` by default. Cycling ACES → Reinhard → None produces a visible output change. Pressing `T` updates the combo's displayed selection on the next frame.
- [ ] Debug Mode combo selects each of the 7 modes (Final/Albedo/Normal/Roughness/Metallic/Depth/Cascades) and produces the same output as the `0`–`6` hotkeys.
- [ ] Dragging "Direction" rotates the sun visibly; shadow direction follows. Panel values remain unit-length after drag.
- [ ] "Color" picker tints lit surfaces.
- [ ] "Intensity" at `0` produces a black scene (modulo IBL ambient); at `20` blows out pre-tone-map exposure.
- [ ] "Ambient Strength" at `0` removes the IBL contribution visibly; at `2` brightens the scene noticeably.
- [ ] "FOV" slider from 30 to 120 visibly widens and narrows the camera frustum.
- [ ] "Active:" label shows current scene name. "Next" button cycles scenes and emits the same `[App] Switched to scene` console log as the `N` key.
- [ ] Stats group shows non-zero FPS and frame-time values that change when switching scenes.
- [ ] Window resize: ImGui panel follows the resize without visual corruption.
- [ ] Drag the panel title bar to reposition it; restart the app; panel reappears at the saved position (confirming `imgui.ini` persistence in the working directory).
- [ ] Minimize window: no crash.

This task has no commit. The plan is complete when all checklist items pass.

---

## Summary

Six tasks, five commits. After execution:

- `vcpkg.json` declares ImGui as a manifest dependency, making the build reproducible.
- `src/ui/ImGuiLayer.h/cpp` owns the Dear ImGui context and backends; existing GLFW input callbacks chain correctly.
- New accessors on `Scene`, `Camera`, `LightingPass`, and `RenderPipeline` give the panel mutable access to all tunables without exposing internals unnecessarily.
- `App` integrates ImGui into its lifecycle with the hotkey block cleanly extracted to `HandleHotkeys()`.
- The "HuanGL Debug" panel exposes Render, Lighting, Camera, Scene, and Stats groups. All keyboard bindings remain functional and stay in sync with panel state.
