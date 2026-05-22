# HuanGL UX Fix & New Sponza Scene Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix the ImGui mouse capture bug (right-click-to-look), register Intel New Sponza as a demo scene, and tighten the Bloom defaults for a better first impression.

**Architecture:** Add `cameraActive` to `ApplicationState`; `InputController` toggles it on right-mouse-button state change; `Camera::Update` receives the flag and gates all mouse/WASD input on it. Scene registration is a one-liner in `App::RegisterScenes`. Bloom defaults live in `BloomSettings` field initialisers.

**Tech Stack:** C++17, OpenGL 4.6 DSA, GLAD2, GLSL 460, Dear ImGui, GLFW, CMake/vcpkg on Windows (MSVC).

---

## File Structure

- Modify `src/app/ApplicationState.h` — add `bool cameraActive = false`.
- Modify `src/app/InputController.cpp` — detect RMB state change, call `Input::SetCursorCaptured`, write `cameraActive`.
- Modify `src/core/App.cpp` — remove startup cursor capture; gate `Camera::Update` on `cameraActive`; register NewSponza scene.
- Modify `src/core/Camera.h` — add `prevCapture_` bool to reset `first_` on re-entry and prevent view jump.
- Modify `src/renderer/FrameContext.h` — tighten `BloomSettings` defaults.

---

## Task 1: Add `cameraActive` to Application State

**Files:**
- Modify: `src/app/ApplicationState.h`

- [ ] **Step 1: Add `cameraActive` field**

In `src/app/ApplicationState.h`, add `cameraActive` to the struct:

```cpp
struct ApplicationState {
    bool running      = true;
    bool cameraActive = false;
    SceneRegistry sceneRegistry;
    Camera camera {60.0f, 0.1f, 100.0f};
    RenderSettings renderSettings;
    DebugSettings debugSettings;
    FrameStats frameStats;
};
```

- [ ] **Step 2: Build to confirm no compile errors**

```
cmake --build build --config Debug
```

Expected: build succeeds (zero new errors).

- [ ] **Step 3: Commit**

```
git add src/app/ApplicationState.h
git commit -m "feat: add cameraActive flag to ApplicationState"
```

---

## Task 2: Toggle Camera Mode on Right Mouse Button

**Files:**
- Modify: `src/app/InputController.cpp`

- [ ] **Step 1: Add RMB handling to `InputController::Update`**

In `src/app/InputController.cpp`, add the RMB check **before** the existing key checks:

```cpp
void InputController::Update(ApplicationState& state) {
    bool rmb = Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
    if (rmb != state.cameraActive) {
        state.cameraActive = rmb;
        Input::SetCursorCaptured(rmb);
    }

    if (Input::IsKeyPressed(GLFW_KEY_ESCAPE)) {
        state.running = false;
    }
    // ... rest of the existing key handling unchanged
```

- [ ] **Step 2: Build**

```
cmake --build build --config Debug
```

Expected: build succeeds.

- [ ] **Step 3: Commit**

```
git add src/app/InputController.cpp
git commit -m "feat: toggle camera mode on right mouse button"
```

---

## Task 3: Remove Startup Cursor Capture and Gate Camera on `cameraActive`

**Files:**
- Modify: `src/core/App.cpp`

- [ ] **Step 1: Remove `Input::SetCursorCaptured(true)` from `App::Init`**

In `src/core/App.cpp`, delete the line:

```cpp
Input::SetCursorCaptured(true);
```

It appears after `Input::Init(window_->GetHandle());` in `App::Init`. The cursor should start visible so ImGui is immediately usable.

- [ ] **Step 2: Gate `Camera::Update` on `cameraActive`**

In `App::Update`, change:

```cpp
state_.camera.Update(dt, window_->GetHandle(), !state_.debugSettings.freezeCamera);
```

to:

```cpp
state_.camera.Update(dt, window_->GetHandle(),
                     state_.cameraActive && !state_.debugSettings.freezeCamera);
```

- [ ] **Step 3: Build**

```
cmake --build build --config Debug
```

Expected: build succeeds.

- [ ] **Step 4: Run and verify UI mode**

```
.\build\Debug\HuanGL.exe
```

Run from the project root. Expected on launch:
- Cursor is **visible** and moves freely over the window.
- Camera does **not** rotate when moving the mouse.
- ImGui "HuanGL Debug" panel is clickable.
- WASD does not move the camera.

- [ ] **Step 5: Verify camera mode**

Hold the **right mouse button** inside the window. Expected:
- Cursor disappears (locked).
- Camera rotates with mouse movement.
- WASD translates the camera.

Release RMB. Expected:
- Cursor reappears.
- Camera stops rotating.

- [ ] **Step 6: Commit**

```
git add src/core/App.cpp
git commit -m "feat: remove startup cursor capture, gate camera on cameraActive"
```

---

## Task 4: Fix Camera Jump on Re-entry

**Files:**
- Modify: `src/core/Camera.h`

When cursor mode is re-enabled after being in UI mode, the raw GLFW cursor position may have moved, causing a large delta spike on the first captured frame. The fix: reset `first_` whenever capture transitions from false to true.

- [ ] **Step 1: Add `prevCapture_` member and reset logic**

In `src/core/Camera.h`, add `bool prevCapture_ = false;` to the private members and insert the transition reset at the top of `Update`:

```cpp
void Update(float dt, GLFWwindow* window, bool capture = true) {
    if (capture && !prevCapture_) first_ = true;
    prevCapture_ = capture;

    if (capture) {
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        float x = (float)mx, y = (float)my;
        if (first_) { lastX_ = x; lastY_ = y; first_ = false; }
        float dx = x - lastX_, dy = lastY_ - y;
        lastX_ = x; lastY_ = y;
        yaw_ += dx * 0.1f; pitch_ += dy * 0.1f;
        pitch_ = glm::clamp(pitch_, -89.f, 89.f);
    }
    front_.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    front_.y = sin(glm::radians(pitch_));
    front_.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    front_ = glm::normalize(front_);
    glm::vec3 right = glm::normalize(glm::cross(front_, worldUp_));
    glm::vec3 up    = glm::cross(right, front_);
    float spd = moveSpeed_ * dt;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) pos_ += front_ * spd;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) pos_ -= front_ * spd;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) pos_ -= right * spd;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) pos_ += right * spd;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) pos_ += up * spd;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) pos_ -= up * spd;
}
```

And update the private section to include `prevCapture_`:

```cpp
private:
    glm::vec3 pos_ = {0, 3, 10}, front_ = {0, 0, -1}, worldUp_ = {0, 1, 0};
    float yaw_ = -90, pitch_ = 0, fov_, near_, far_, moveSpeed_ = 5;
    float lastX_ = 0, lastY_ = 0;
    bool first_ = true;
    bool prevCapture_ = false;
```

- [ ] **Step 2: Build**

```
cmake --build build --config Debug
```

Expected: build succeeds.

- [ ] **Step 3: Run and verify no jump**

Launch the app. Move the mouse in UI mode. Hold RMB. Confirm the camera view does **not** jump when entering camera mode — it should begin rotating smoothly from wherever you were looking.

Release and re-enter camera mode several times to confirm stability.

- [ ] **Step 4: Commit**

```
git add src/core/Camera.h
git commit -m "fix: reset camera first_ on capture re-entry to prevent view jump"
```

---

## Task 5: Register Intel New Sponza Scene

**Files:**
- Modify: `src/core/App.cpp`

**Prerequisite:** Download Intel Graphics Research New Sponza and extract it so the following path exists relative to the project root:

```
resources/models/NewSponza/NewSponza_Main_glTF/NewSponza_Main.gltf
```

If the archive has a different internal folder structure, adjust the path constant below to match.

- [ ] **Step 1: Add NewSponza registration**

In `src/core/App.cpp`, inside `App::RegisterScenes()`, add after the existing Sponza block:

```cpp
const char* newSponzaPath = "../resources/models/NewSponza/NewSponza_Main_glTF/NewSponza_Main.gltf";
std::printf("[App] Attempting NewSponza from '%s' (exists=%d)\n",
            newSponzaPath, std::filesystem::exists(newSponzaPath) ? 1 : 0);
state_.sceneRegistry.RegisterOptional(
    std::make_unique<ModelScene>(newSponzaPath, "NewSponza", false, 0.01f),
    "NewSponza", *resourceManager_);
```

- [ ] **Step 2: Build**

```
cmake --build build --config Debug
```

Expected: build succeeds.

- [ ] **Step 3: Run and cycle to NewSponza**

Launch the app. Press `N` repeatedly until the console shows `[SceneRegistry]` activating `NewSponza` (or until the scene changes). Expected: the Intel New Sponza scene renders with visible columns, drapes, and detailed surfaces.

If the asset path is wrong, the console will print `exists=0` and the scene will be skipped — adjust the path and rebuild.

- [ ] **Step 4: Commit**

```
git add src/core/App.cpp
git commit -m "feat: register Intel New Sponza as optional demo scene"
```

---

## Task 6: Tighten Bloom Defaults for Visual Polish

**Files:**
- Modify: `src/renderer/FrameContext.h`

Note: `ToneMapMode::ACES` is already the default (`toneMapMode = ToneMapMode::ACES` in `RenderSettings`). No tone map change is needed.

- [ ] **Step 1: Update `BloomSettings` defaults**

In `src/renderer/FrameContext.h`, replace the `BloomSettings` struct:

```cpp
struct BloomSettings {
    bool enabled = true;
    float threshold = 0.8f;
    float softKnee = 0.5f;
    float intensity = 0.12f;
    int radius = 5;
    int mipCount = 5;
};
```

(Changes from current: `threshold` 1.0 → 0.8, `intensity` 0.08 → 0.12.)

- [ ] **Step 2: Build**

```
cmake --build build --config Debug
```

Expected: build succeeds.

- [ ] **Step 3: Run and verify Bloom**

Launch the app, cycle to NewSponza. Press `7` to enter Bloom debug view. Confirm bright surfaces (lamp flames, sky patches) show glow. Press `0` to return to Final view. Confirm the image has visible but not overwhelming bloom glow. Use ImGui → Techniques to tweak live if needed.

- [ ] **Step 4: Commit**

```
git add src/renderer/FrameContext.h
git commit -m "feat: tighten bloom defaults — threshold 0.8, intensity 0.12"
```

---

## Final Verification

- [ ] Build clean: `cmake --build build --config Debug`
- [ ] Launch. Cursor is visible. ImGui panel opens and is fully clickable.
- [ ] Hold RMB — camera rotates smoothly, no jump. Release — cursor returns.
- [ ] Press `N` to reach NewSponza. Scene loads and renders.
- [ ] Bloom debug view (`7`) shows glow on bright areas in NewSponza.
- [ ] No crash on window resize.
