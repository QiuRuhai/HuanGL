# HuanGL Architecture Reset: World and Pipeline Contracts

## Context

HuanGL has reached a useful Phase 2.5 renderer baseline: deferred PBR,
cascaded shadows, IBL, tone mapping, debug views, model loading, and
multi-scene registration all work. Phase 3 introduced an initial ImGui
debug panel, but that panel exposed a broader architecture problem rather
than causing it.

The current design has four pressure points:

- `App` owns too many responsibilities: window lifecycle, scene
  registration, soft-failure scene loading, input hotkeys, camera update,
  UBO update, render invocation, ImGui panel construction, and frame
  presentation.
- `Scene` is a render-data container, not an inspectable world model. It
  stores mesh pointers, model matrices, materials, and light state in
  parallel arrays, which is adequate for drawing but weak for tools,
  transforms, scene inspection, and future GI debugging.
- `RenderPipeline` is readable today, but pass input/output contracts are
  implicit. Future Bloom, TAA, RSM, SSGI, VXGI, and DDGI work will add
  more intermediate resources and debug surfaces. Those should not be
  discovered by reaching into pass internals.
- The first ImGui panel directly mutates `Scene`, `Camera`,
  `LightingPass`, and `PostProcessPass`. UI should edit application state,
  world data, and settings; renderer passes should read frame inputs.

This reset keeps HuanGL's identity intact: it remains a direct OpenGL 4.6
learning renderer, not a production engine. The goal is to make the code
base easier to extend with real-time rendering algorithms without adding
an RHI, a full ECS, a scripting system, or a render graph.

## Goals

1. Make `App` a thin composition root and frame scheduler.
2. Move runtime state into explicit data structures:
   `ApplicationState`, `RenderSettings`, `DebugSettings`, and
   `FrameStats`.
3. Move ImGui code into a dedicated `DebugUI` module that edits state and
   world data, not renderer pass internals.
4. Replace the current `Scene` array container with a lightweight
   `World`/`Entity` model that is inspectable but not a full ECS.
5. Feed the renderer through read-only contracts:
   `RenderSceneView` and `FrameContext`.
6. Give each pipeline pass explicit output structures so future passes
   can consume named resources without coupling to pass implementation
   details.
7. Complete this reset in one implementation effort, covering all
   migration phases A through E, while keeping staged commits and build
   verification after each checkpoint.

## Non-Goals

- No RHI abstraction. OpenGL remains the API being learned and shown.
- No full ECS framework. The world model should be simple enough to read
  in one sitting.
- No render graph in this reset. The pipeline remains fixed order for
  now.
- No asset editor, serialization format, scripting layer, undo/redo
  stack, or multi-window tooling.
- No visual redesign of ImGui beyond moving it behind the right boundary.
  The first priority is correct ownership.
- No renderer feature work such as Bloom, TAA, or GI during the reset.

## Target Architecture

The target design has three layers.

### Application Layer

The application layer owns process-level orchestration.

- `App` creates and destroys systems, runs the main loop, and calls the
  frame stages in order.
- `ApplicationState` stores cross-system runtime state.
- `InputController` maps keyboard and mouse input into state mutations.
- `DebugUI` draws ImGui panels and edits `ApplicationState` plus the
  active `World`.
- `SceneRegistry` owns the list of demo scenes and handles soft-failure
  loading.

`App` should not contain ImGui widgets, hardcoded debug hotkey blocks,
scene construction details, or pass-specific settings.

### World / Scene Layer

The world layer owns inspectable scene data.

- `World` contains entities, lights, ambient data, and scene-level update
  hooks.
- `Entity` is a simple data record with an id, name, transform, and
  optional render component.
- `Transform` stores translation, rotation, and scale.
- `MeshRenderer` points at a mesh and its material set.
- Demo scene builders create `World` instances instead of exposing render
  arrays directly.

This is not a full ECS. It is a lightweight, tool-friendly model that can
support a scene inspector and future transform editing without hiding the
data behind complex machinery.

### Renderer / Pipeline Layer

The renderer layer owns OpenGL resources and rendering.

- `RenderSceneView` is a read-only adapter built from the active `World`.
  Render passes consume this view and do not know how entities are stored.
- `FrameContext` contains camera data, viewport, time, render settings,
  and debug settings for one frame.
- `RenderPipeline` remains a fixed sequence:
  `ShadowPass`, `GBufferPass`, `LightingPass`, `PostProcessPass`.
- Each pass returns or stores an explicit output structure.
- UI and input code do not access concrete pass instances to mutate
  settings.

The renderer remains easy to step through in a debugger. The reset adds
clear data boundaries, not a scheduling framework.

## Core Data Types

The exact field names can change during implementation, but the ownership
model should remain stable.

### ApplicationState

```cpp
struct ApplicationState {
    bool running = true;

    SceneRegistry sceneRegistry;
    size_t activeSceneIndex = 0;

    Camera camera;
    RenderSettings renderSettings;
    DebugSettings debugSettings;
    FrameStats frameStats;
};
```

`ApplicationState` is the shared runtime state for application-level
systems. It should not own OpenGL pass resources.

### RenderSettings

```cpp
struct RenderSettings {
    float ambientStrength = 1.0f;
    int shadowResolution = 2048;
    int toneMapMode = 0;
    float exposure = 1.0f;
};
```

Future settings can include Bloom, TAA, RSM, SSGI, VXGI, and DDGI
toggles and parameters. These settings affect rendered output.

### DebugSettings

```cpp
enum class DebugView {
    Final,
    Albedo,
    Normal,
    Roughness,
    Metallic,
    Depth,
    Cascades
};

struct DebugSettings {
    DebugView view = DebugView::Final;
    bool showImGui = true;
    bool freezeCamera = false;
};
```

Debug settings control inspection and visualization. They should remain
separate from real render settings so debug behavior does not leak into
production-like rendering choices.

### FrameStats

```cpp
struct FrameStats {
    float deltaTime = 0.0f;
    float frameTimeMs = 0.0f;
    float fps = 0.0f;
};
```

`DebugUI` reads frame stats. Rendering passes should not depend on them.

### World and Entity

```cpp
using EntityId = uint32_t;

struct Transform {
    glm::vec3 translation = {0.0f, 0.0f, 0.0f};
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale = {1.0f, 1.0f, 1.0f};
};

struct MeshRenderer {
    std::shared_ptr<Mesh> mesh;
    std::vector<Material> materials;
};

struct Entity {
    EntityId id = 0;
    std::string name;
    Transform transform;
    std::optional<MeshRenderer> meshRenderer;
};

class World {
public:
    void Update(float dt);

    Entity& CreateEntity(std::string name);
    std::span<Entity> GetEntities();
    std::span<const Entity> GetEntities() const;

    DirectionalLight& GetSunLight();
    const DirectionalLight& GetSunLight() const;

    glm::vec3& GetAmbient();
    const glm::vec3& GetAmbient() const;
};
```

The implementation can use vectors internally. Stable entity handles are
not required in the first reset unless the code needs deletion or complex
selection semantics.

### RenderSceneView

```cpp
struct Renderable {
    const Mesh* mesh = nullptr;
    const std::vector<Material>* materials = nullptr;
    glm::mat4 modelMatrix = glm::mat4(1.0f);
};

struct RenderSceneView {
    std::span<const Renderable> renderables;
    DirectionalLight sunLight;
    glm::vec3 ambient = {0.03f, 0.03f, 0.05f};
};
```

`RenderSceneView` is built from the active `World` once per frame. Passes
draw from the view. They do not traverse or mutate entities.

### FrameContext

```cpp
struct FrameContext {
    int width = 0;
    int height = 0;
    float time = 0.0f;
    float deltaTime = 0.0f;

    CameraData camera;
    RenderSettings renderSettings;
    DebugSettings debugSettings;
};
```

`FrameContext` is copied or passed by const reference to pipeline code.
It is the renderer's per-frame input contract.

### Pipeline Outputs

```cpp
struct ShadowOutputs {
    GLuint shadowArray = 0;
    std::span<const CascadeData> cascades;
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
```

The first implementation can keep ownership inside pass classes and
return lightweight output structures that reference those resources.
Future passes should consume these output structs instead of querying
unrelated pass objects.

## Frame Flow

Each frame should follow this order:

1. `Input::Update()` and `Window::PollEvents()`.
2. `InputController::Update(state, window)` mutates application state.
3. Active `World` and `Camera` update.
4. `FrameContext` is built from viewport, time, camera, and settings.
5. `RenderSceneView` is built from the active `World`.
6. UBOs are updated at the renderer boundary from `FrameContext` and
   `RenderSceneView`.
7. `RenderPipeline::Execute(sceneView, frameContext)` runs the fixed pass
   sequence and updates `PipelineOutputs`.
8. `DebugUI::Draw(state, activeWorld, pipelineOutputs)` draws ImGui.
9. The window swaps buffers.

The UI step may stay after rendering, as it is today, so ImGui draws on
top of the backbuffer. UI changes affect the next frame unless a control
is read before the relevant render stage in a future layout.

## Module Map

The reset should add or reshape modules as follows:

| Directory | Responsibility |
|-----------|----------------|
| `src/core/` | `App`, `Window`, `Input`, `Camera`, frame loop primitives |
| `src/app/` | `ApplicationState`, `InputController`, `SceneRegistry`, runtime settings |
| `src/scene/` | `World`, `Entity`, `Transform`, scene builders |
| `src/renderer/` | OpenGL wrappers, schemas, UBO helpers, render-facing data |
| `src/pipeline/` | `RenderPipeline`, pass outputs, pass orchestration |
| `src/ui/` | `ImGuiLayer`, `DebugUI` |

If adding `src/app/` feels too heavy during implementation, the same files
can temporarily live under `src/core/`. The preferred final boundary is
`src/app/` because it separates runtime orchestration from low-level
window/input primitives.

## UI Rules

The debug UI should follow these constraints:

- `DebugUI` may edit `ApplicationState`, `RenderSettings`,
  `DebugSettings`, `Camera`, `World`, entities, transforms, lights, and
  materials.
- `DebugUI` must not directly mutate `RenderPipeline`, `LightingPass`,
  `PostProcessPass`, `GBufferPass`, or `ShadowPass`.
- Debug view and tone map controls write to settings, not pass fields.
- Light controls write to the active `World`.
- Stats controls read `FrameStats`.
- Future texture previews should read `PipelineOutputs` or a dedicated
  debug texture registry, not pass internals.

This rule removes the reason for accessors like
`RenderPipeline::GetLighting()` and `LightingPass::SetAmbientStrength()`.

## Pipeline Rules

The pipeline should follow these constraints:

- `RenderPipeline::Execute` takes `const RenderSceneView&` and
  `const FrameContext&`.
- Passes receive only the data they need.
- Pass settings come from `FrameContext.renderSettings` and
  `FrameContext.debugSettings`.
- Pass outputs are represented by explicit structs.
- The pipeline may keep pass objects and resource ownership internally.
- The pipeline remains fixed order in this reset.

This keeps algorithm work straightforward: adding a pass means defining
its input and output data, then wiring it into the fixed order.

## Migration Plan

This reset covers all five phases in one implementation effort. Each
phase should be a separate checkpoint with a fresh build before moving to
the next phase.

### Phase A: Settings and DebugUI

Goal: remove ImGui widget code and renderer setting mutation from `App`.

Deliverables:

- `RenderSettings`
- `DebugSettings`
- `FrameStats`
- `DebugUI`
- `App::BuildDebugPanel` removed
- `LightingPass::ambientStrength_` removed
- tone map and debug view state moved out of `PostProcessPass` or read
  from settings during render

Expected behavior:

- Existing hotkeys still work.
- Existing debug panel controls still exist.
- Render output remains equivalent.

### Phase B: ApplicationState and InputController

Goal: make `App` a frame scheduler instead of a behavior container.

Deliverables:

- `ApplicationState`
- `InputController`
- scene cycling moved out of `App`
- quit/tone/debug hotkeys moved out of `App`
- frame stats update centralized

Expected behavior:

- `Esc`, `N`, `T`, and `0` through `6` keep their current behavior.
- `App::Run` becomes a short sequence of frame stages.

### Phase C: World and Entity

Goal: replace the current `Scene` arrays with an inspectable world model.

Deliverables:

- `World`
- `EntityId`
- `Transform`
- `MeshRenderer`
- updated `TestScene` and `ModelScene` builders
- active scene access through `SceneRegistry`

Expected behavior:

- TestScene, DamagedHelmet, and Sponza registration still soft-fail or
  load as they do today.
- Rendered transforms remain visually equivalent.
- The UI can inspect at least entity names and basic transform/light data.

### Phase D: RenderSceneView and FrameContext

Goal: route renderer inputs through explicit per-frame contracts.

Deliverables:

- `FrameContext`
- `Renderable`
- `RenderSceneView`
- world-to-render-view adapter
- `RenderPipeline::Execute(sceneView, frameContext)`
- UBO updates moved to the renderer boundary or a dedicated frame
  preparation helper

Expected behavior:

- The renderer no longer depends on the old `Scene` interface.
- Camera, light, time, tone map, and debug data all flow through
  `FrameContext` or `RenderSceneView`.

### Phase E: Pipeline Outputs

Goal: formalize pass outputs for future post-processing and GI work.

Deliverables:

- `ShadowOutputs`
- `GBufferOutputs`
- `LightingOutputs`
- `PipelineOutputs`
- pass render methods returning output structs or exposing a single
  `GetOutputs` method
- `PostProcessPass` consumes output structs and `FrameContext`

Expected behavior:

- Existing debug views still work.
- No UI code reaches into pass internals.
- Future Bloom/TAA/GI work has named resource inputs.

## Verification Strategy

Each implementation checkpoint should run:

```powershell
cmake --build build --config Debug
```

After adding new `.cpp` files, rerun configure first:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

Manual visual verification after the full reset:

- app launches without a crash
- TestScene renders
- optional model scenes soft-fail or load as before
- `N` cycles scenes
- `T` cycles tone mapping
- `0` through `6` select debug views
- ImGui panel opens
- ImGui render settings change the output
- light direction/color/intensity edits affect the active world
- camera FOV edits affect the camera
- window resize still resizes the pipeline
- minimized window does not crash

Because this project has no automated rendering tests, build checks and
manual visual checks are the required verification gates.

## Risks and Mitigations

### Risk: The reset becomes an engine rewrite

Mitigation: do not add serialization, an editor command system, undo/redo,
runtime component registration, reflection, or a render graph. The world
model is plain C++ data for a renderer showcase.

### Risk: World migration breaks draw ordering or materials

Mitigation: keep `MeshRenderer` close to the current `Mesh` plus
`materials` model. Convert one scene at a time and compare visual output.

### Risk: Settings become a new global bucket

Mitigation: split render settings, debug settings, and frame stats. Only
put cross-system state in `ApplicationState`; pass-owned GPU resources
stay inside pipeline classes.

### Risk: Pipeline outputs duplicate ownership

Mitigation: output structs are views/handles. Pass classes continue to own
their textures and framebuffers.

### Risk: Too many files move at once

Mitigation: implement as five staged commits, each with a build check.
The design covers the full reset, but the execution remains reviewable.

## Success Criteria

The reset is successful when:

- `App` no longer contains ImGui widget code, hardcoded scene registration
  blocks, or pass-specific debug controls.
- `DebugUI` talks to state and world data, not pass internals.
- `LightingPass` and `PostProcessPass` do not store UI-owned settings.
- Active scene data is represented by a lightweight `World` model.
- `RenderPipeline::Execute` is driven by `RenderSceneView` and
  `FrameContext`.
- Pass resource handoff is represented by output structs.
- The current visual behavior is preserved.
- The next rendering feature can be added by defining settings, inputs,
  outputs, and a pass without expanding `App`.

## Out of Scope for the First Implementation Plan

The implementation plan that follows this spec should not include:

- Bloom, TAA, RSM, SSGI, VXGI, or DDGI implementation.
- Full scene serialization.
- Full ECS query/storage abstractions.
- Render graph scheduling.
- ImGuizmo integration.
- Asset browser UI.

Those can build on this reset after the ownership boundaries are in
place.
