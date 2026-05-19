# Phase 3 (Minimum): ImGui Debug Panel

## Context

The renderer currently exposes its runtime knobs only through keyboard
bindings (`T` to cycle tone-map operator, `0`–`6` to switch debug view,
`N` to cycle scene). Adjusting anything else — sun direction, ambient
strength, camera FOV — requires editing source and rebuilding. As more
phases (Bloom, RSM, SSGI, VXGI, DDGI) add their own tunables, the
"edit + rebuild" loop becomes a real friction point.

This spec scopes the **minimum** Phase 3: integrate Dear ImGui as a
runtime overlay and put one debug panel on screen that exposes the
existing tunables. Two larger Phase 3 chunks called out in
`docs/architecture.md` are intentionally deferred:

- **Scene inspector** with per-entity transforms and material editing.
- **ImGuizmo** for in-scene 3D handles.

Both depend on first introducing the notion of a selectable "entity"
and a stable per-entity ID, which `Scene` does not currently have. The
work needed to land that abstraction is larger than this whole spec.
Pulling it into a separate sub-phase keeps each chunk reviewable.

The output is one new dependency, one new module, one new App
integration point, and one runtime panel. The renderer's existing
keyboard bindings continue to work and stay in sync with the panel.

---

## 1. Dependency: Dear ImGui via vcpkg

ImGui ships C++ source rather than headers-only, and the project already
uses vcpkg for non-trivial dependencies (GLFW, Assimp). Vendoring would
introduce a third dependency strategy alongside vcpkg and the
`external/` header-only libraries, which is not justified for a library
already available as a well-maintained vcpkg port.

### Build system changes

**`vcpkg.json`** (create — the project does not currently use a
manifest; vcpkg dependencies are resolved through the toolchain file
alone). Creating a manifest makes the ImGui feature selection
declarative and reproducible across builds:

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

**`CMakeLists.txt`** — add after the existing `find_package(assimp ...)`
block:

```cmake
find_package(imgui CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE imgui::imgui)
```

No other build-side changes. The two `feature` selections bring in
`imgui_impl_glfw` and `imgui_impl_opengl3` directly from the vcpkg-built
ImGui library, so the project does not need to vendor or compile those
backends itself.

---

## 2. New module: `src/ui/ImGuiLayer.{h,cpp}`

A thin wrapper hiding ImGui setup, per-frame begin/end, and shutdown.
Lives under `src/ui/` (a placeholder directory listed in
`docs/architecture.md`'s module map for exactly this purpose).

### Public API

```cpp
namespace HuanGL {

class ImGuiLayer {
public:
    ImGuiLayer() = default;
    ~ImGuiLayer();
    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;

    void Init(GLFWwindow* window);   // Creates context, installs backends
    void Shutdown();                  // Idempotent: safe to call from dtor or App::Shutdown

    void BeginFrame();   // ImGui_ImplOpenGL3_NewFrame; ImGui_ImplGlfw_NewFrame; ImGui::NewFrame
    void EndFrame();     // ImGui::Render; ImGui_ImplOpenGL3_RenderDrawData(GetDrawData)
};

} // namespace HuanGL
```

The class owns no GL resources directly. It owns the ImGui context
lifetime through `Init`/`Shutdown`; the backends own GL state inside
their own translation units.

### Implementation notes

- Use `IMGUI_VERSION_NUM`-gated calls minimally; current vcpkg ImGui
  versions support the patterns used here.
- Disable docking and viewports features (default off in stock vcpkg
  port) — they require more setup and we do not need them for one panel.
- Save the `imgui.ini` file in the executable's working directory; do
  not customize the location.
- Install the GLFW callbacks via `ImGui_ImplGlfw_InstallCallbacks =
  true` so ImGui receives keyboard / mouse events. This **chains** with
  HuanGL's existing callbacks (Input::Init's `glfwSetCursorPosCallback`,
  `glfwSetScrollCallback`, `glfwSetKeyCallback`) rather than replacing
  them, because ImGui's backend uses chained callbacks by default. As a
  result, `Input::IsKeyJustPressed` and the existing camera mouse-look
  continue to work when the cursor is **outside** ImGui windows.
- When the cursor is over an ImGui window, ImGui sets
  `io.WantCaptureMouse` / `io.WantCaptureKeyboard`. We do **not** gate
  HuanGL's existing input on these — for a minimum panel it is fine if
  WASD continues to drive the camera even while hovering the panel.
  Gating can be added later if it becomes annoying in practice.

---

## 3. App integration

`App` gains one new member, three new lifecycle calls, and one new
private method that builds the panel.

### Header changes (`src/core/App.h`)

Add:

```cpp
class ImGuiLayer;  // forward-decl

// In private members:
std::unique_ptr<ImGuiLayer> imguiLayer_;

// In private methods:
void BuildDebugPanel();
```

### Init / Shutdown

In `App::Init` (after `Window` is constructed, after `Input::Init`):

```cpp
imguiLayer_ = std::make_unique<ImGuiLayer>();
imguiLayer_->Init(window_->GetHandle());
```

In `App::Shutdown` (before `ResourceManager::Shutdown` so OpenGL still
has a valid context when ImGui's GL backend tears down):

```cpp
if (imguiLayer_) imguiLayer_->Shutdown();
```

### Main loop

In `App::Run`, the per-frame body becomes:

```cpp
Input::Update();
window_->PollEvents();
HandleHotkeys();            // existing inline block — extract to a method, see below

Update(dt);
Render();                   // pipeline_->Execute, unchanged

imguiLayer_->BeginFrame();
BuildDebugPanel();
imguiLayer_->EndFrame();    // renders into the default framebuffer

window_->SwapBuffers();
```

The current `App::Run` already contains a sizable inline hotkey block
(ESC, T, 0–6, N). Extracting it into a private `HandleHotkeys()` method
is a small, in-scope improvement that keeps `Run()` readable once
ImGui's panel-build call is added beneath it. This is the only
non-Phase-3 cleanup the spec adopts.

ImGui renders **after** `PostProcessPass` because `PostProcessPass`
binds the default framebuffer and writes a fullscreen quad into it.
ImGui then draws over the same framebuffer. ImGui does not enter the
`RenderPipeline` — `RenderPipeline` stays focused on 3D scene
rendering, and `App` owns the UI overlay just like it owns the camera
and the scenes.

### Panel construction (`App::BuildDebugPanel`)

One `ImGui::Begin("HuanGL Debug")` window containing five
`CollapsingHeader` groups in this order:

1. **Render**
   - Tone mapping combo (`ImGui::Combo`) with three entries (ACES,
     Reinhard, None). The current selection is read from
     `pipeline_->GetPostProcess().GetToneMapMode()`; assignment calls
     `SetToneMapMode`.
   - Debug mode combo with seven entries (Final, Albedo, Normal,
     Roughness, Metallic, Depth, Cascades). Read via `GetDebugMode`,
     assign via `SetDebugMode`.
2. **Lighting**
   - Sun direction — `ImGui::DragFloat3("Direction", &dir.x, 0.01f,
     -1.f, 1.f)`. After the drag, normalize to length 1.
   - Sun color — `ImGui::ColorEdit3("Color", &color.r)`.
   - Sun intensity — `ImGui::DragFloat("Intensity", &intensity, 0.05f,
     0.f, 20.f)`.
   - Ambient strength — drag, range 0..2 (passes through to lighting
     shader's `uAmbientStrength`).
3. **Camera**
   - Field of view — `ImGui::SliderFloat("FOV", &fov, 30.f, 120.f)`.
4. **Scene**
   - Display the active scene name as plain text.
   - `ImGui::Button("Next")` advancing through registered scenes
     (equivalent to the `N` key).
5. **Stats**
   - Display `1.0 / io.DeltaTime` as FPS and `io.DeltaTime * 1000` as
     frame-time milliseconds, both via `ImGui::Text`.

### Data accessors needed

The panel needs read/write access to a few state-holders that today
expose getters only:

- `Scene::GetMutableSunLight()` — new non-const accessor returning
  `DirectionalLight&`. Yes, this introduces a small mutable-state
  break, but the alternative (a setter per field plus dirty-flag
  plumbing) is overkill for a one-light scene system.
- `Camera::SetFov(float)` and `Camera::GetFov()` — pair of methods on
  `Camera`. The camera already owns the FOV value; it just lacks
  accessors.
- `App::CycleScene` — already exists as a private method. Promote to
  `public` (or expose via a friend-style accessor; `public` is simpler).

`PostProcessPass` already exposes `GetToneMapMode` / `SetToneMapMode`
and `GetDebugMode` / `SetDebugMode`. The new "ambient strength" value
lives in the lighting shader as a uniform set unconditionally to `1.0f`
in `LightingPass::Render`. Pipe it through:

- Add `float ambientStrength_ = 1.0f` and getter/setter to
  `LightingPass`.
- Read it inside `Render()` instead of the hard-coded `1.0f`.
- Have the panel mutate that value.

---

## 4. Out of scope

These would belong to a follow-up Phase 3 chunk, not this minimum:

- **Scene inspector / entity selection.** Scene currently provides
  meshes via `GetMesh(i)` and model matrices via `GetModelMatrix(i)`
  but has no notion of a selectable entity with a stable identity. A
  proper inspector requires that abstraction.
- **Material editing UI.** Same dependency.
- **ImGuizmo integration.** Same dependency.
- **Window docking / multi-viewport.** Default ImGui is enough.
- **Theme / styling.** Default theme is fine for now.
- **imgui.ini persistence to a custom location.** Default behavior is
  fine.
- **Mouse-capture handoff** between camera fly-through and ImGui.
  Acceptable for the minimum panel that WASD continues to drive the
  camera even while hovering ImGui; revisit if it becomes annoying.

---

## 5. Verification

- Build succeeds on both MSVC and clang-cl Ninja configurations.
- App opens. The ImGui window titled "HuanGL Debug" is visible at
  startup, positioned at ImGui's default (top-left corner).
- Tone-map combo cycles through ACES / Reinhard / None and the visible
  output matches the existing `T` hotkey behavior. Pressing `T`
  externally updates the combo's displayed selection on the next frame.
- Debug-mode combo selects each of the seven modes with the same
  results as the existing `0`–`6` hotkeys.
- Dragging "Sun Direction" rotates the directional light in the scene
  visibly, with shadows tracking correctly.
- Changing "Sun Color" tints lit surfaces.
- "Sun Intensity" at `0` produces a black scene (modulo IBL ambient);
  at `20` blows out exposure pre-tone-map.
- "Ambient Strength" at `0` removes the IBL contribution; at `2`
  brightens it noticeably.
- "FOV" slider visibly widens / narrows the camera FOV.
- "Next" button in Scene group cycles through registered scenes and
  produces the same console log as the `N` hotkey.
- "Stats" group shows non-zero FPS and frame time that fluctuate with
  scene complexity.
- Window resize: ImGui follows the resize and the panel does not
  visually corrupt. Drag panel's title bar; new position persists in
  `imgui.ini` across runs.
- Minimize window: no crash (the existing `App::Render` already
  short-circuits when the framebuffer area is zero).
