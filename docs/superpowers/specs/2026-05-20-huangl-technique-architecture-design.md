# HuanGL Rendering Technique Architecture Design

Date: 2026-05-20
Status: Draft for review

## Purpose

The first architecture reset separated app state, world data, frame
contracts, pipeline outputs, and debug UI. The next refactor should make
future rendering algorithms fit into the renderer without turning
`RenderPipeline.cpp` or `PostProcessPass.cpp` into a catch-all file.

The target algorithms are Bloom, TAA, RSM, SSGI, VXGI, and DDGI. The
project still has one concrete graphics backend: OpenGL 4.6. This design
must not introduce an RHI or a general render graph.

## Current Problem

The renderer now has explicit pass outputs, but it does not yet have a
clear place for optional algorithms. If Phase 4 starts by adding Bloom
directly into `RenderPipeline` and then TAA directly into
`PostProcessPass`, the later GI phases will repeat the same coupling.

The missing boundary is a small "technique" layer:

- A technique owns one algorithm's GPU resources.
- A technique reads explicit frame and pipeline inputs.
- A technique writes explicit output handles.
- Settings live in app state and are edited by `DebugUI`.
- The fixed pipeline remains easy to read.

## Alternatives Considered

### A. Keep Adding Passes Directly to RenderPipeline

This is the smallest immediate change, but it makes `RenderPipeline` the
owner of every algorithm, setting, intermediate texture, and debug mode.
It is acceptable for one more pass, but it scales poorly to TAA, RSM,
SSGI, VXGI, and DDGI.

### B. Add a Full Render Graph

A render graph would model resource lifetimes and pass dependencies well,
but it is too much infrastructure for this project right now. It would
also hide the OpenGL learning value behind graph scheduling code.

### C. Add Concrete Technique Modules

This is the recommended path. Each algorithm becomes a concrete class
with a small, consistent shape. `RenderPipeline` still calls the steps in
an explicit order, but algorithm resources and settings are not spread
across unrelated passes.

## Decision

Use concrete technique modules, not a polymorphic plugin system at first.
The names should stay direct:

```cpp
class BloomTechnique;
class TaaTechnique;
class RsmTechnique;
class SsgiTechnique;
class VxgiTechnique;
class DdgiTechnique;
```

Do not add a shared virtual `IRenderTechnique` until two or three
implemented techniques prove that common dynamic dispatch is useful.
Simple concrete classes are easier to debug and better match the current
renderer.

## Target Module Layout

```text
src/pipeline/
  RenderPipeline.h/cpp
  PipelineOutputs.h
  techniques/
    BloomTechnique.h/cpp
    TaaTechnique.h/cpp
    RsmTechnique.h/cpp
    SsgiTechnique.h/cpp
    VxgiTechnique.h/cpp
    DdgiTechnique.h/cpp
```

Only create technique files when the corresponding algorithm starts.
The architecture refactor should introduce the folder and the first real
technique through Bloom.

## Technique Shape

Each technique should follow this concrete pattern:

```cpp
struct BloomSettings {
    bool enabled = true;
    float threshold = 1.0f;
    float intensity = 0.08f;
    int radius = 5;
};

struct BloomOutputs {
    std::shared_ptr<Texture> bloom;
};

class BloomTechnique {
public:
    void Init(int width, int height);
    void Resize(int width, int height);

    BloomOutputs Execute(const FrameContext& frame,
                         const PipelineOutputs& inputs,
                         const BloomSettings& settings);

    BloomOutputs GetOutputs() const;
};
```

This is a convention, not a mandatory base class. Each technique may
adapt the shape where the algorithm needs it.

## Settings Ownership

Settings belong to app state, not to render passes.

`ApplicationState` should continue to own one `RenderSettings` instance.
`RenderSettings` can grow nested settings as algorithms appear:

```cpp
struct RenderSettings {
    float ambientStrength = 0.03f;
    ToneMapMode toneMapMode = ToneMapMode::ACES;
    BloomSettings bloom;
    TaaSettings taa;
};
```

`DebugUI` edits these settings. `RenderPipeline` receives settings only
through `FrameContext`. No UI code should hold references to pass or
technique objects.

## Output Ownership

Passes and techniques keep owning their GL resources. `PipelineOutputs`
is only the named handoff point for resource handles.

The output structure should remain explicit and typed:

```cpp
struct PipelineOutputs {
    ShadowOutputs shadow;
    GBufferOutputs gbuffer;
    LightingOutputs lighting;
    BloomOutputs bloom;
    TaaOutputs taa;
};
```

Optional outputs may contain null texture handles when the corresponding
technique is disabled. Consumers must treat null outputs as disabled or
unavailable, not as an error.

## RenderPipeline Role

`RenderPipeline` remains the frame orchestrator. It should be readable as
the high-level rendering order:

```cpp
outputs_.shadow = shadowPass_.Render(scene, frame);
outputs_.gbuffer = gbufferPass_.Render(scene, frame);
outputs_.lighting = lightingPass_.Render(scene, outputs_.gbuffer,
                                         outputs_.shadow, frame);

outputs_.bloom = bloomTechnique_.Execute(frame, outputs_,
                                         frame.renderSettings.bloom);

postProcessPass_.Render(outputs_, frame);
```

This keeps the order explicit while preventing each algorithm's internal
framebuffers, shaders, and temporary textures from leaking into the
orchestrator.

## PostProcessPass Role

`PostProcessPass` should become the final compositor and display pass.
Its job is:

- choose the display source,
- combine final optional outputs where appropriate,
- apply exposure, tone mapping, and gamma,
- show debug visualizations.

It should not own multi-pass algorithms such as Bloom blur chains, TAA
history, SSGI ray marching, or voxel cone tracing.

## Debug UI

`DebugUI` should expose algorithm settings by editing
`ApplicationState::renderSettings`.

The UI may have collapsible sections:

- Render
- Lighting
- Post Process
- Techniques
- Scene
- Stats

The "Techniques" section can contain Bloom, TAA, RSM, SSGI, VXGI, and
DDGI controls as they are implemented. The UI should not call methods on
technique instances.

## Error Handling

Technique initialization should fail loudly for missing shaders or
invalid framebuffer setup, matching existing pass behavior. Runtime
disabling should be handled through settings, not by leaving partially
initialized objects around.

On resize, each technique receives `Resize(width, height)` and rebuilds
only the resources it owns. For history-based techniques such as TAA and
SSGI temporal accumulation, resize must also invalidate history.

On scene switch, history-based techniques must invalidate history. The
first implementation can expose this as:

```cpp
void RenderPipeline::InvalidateHistory();
```

`InputController` or `SceneRegistry` should not know about individual
techniques.

## Rollout Plan

1. Add `src/pipeline/techniques/`.
2. Move Bloom into `BloomTechnique` as the first concrete technique.
3. Extend `RenderSettings`, `FrameContext`, and `PipelineOutputs` with
   Bloom settings and outputs.
4. Keep `PostProcessPass` responsible for final composition and display.
5. Add `RenderPipeline::InvalidateHistory()` before implementing TAA.
6. Implement TAA as the second technique and reuse the same boundaries.
7. Revisit whether a shared base class is useful only after Bloom and TAA
   are both implemented.

## Non-Goals

- No RHI layer.
- No generic render graph.
- No data-driven pass scheduler.
- No ECS migration.
- No editor serialization, asset browser, undo/redo, or ImGuizmo work in
  this refactor.

## Verification

For the architecture refactor itself:

- Configure and build with the standard Windows commands.
- Confirm `RenderPipeline` remains the only frame-order orchestrator.
- Confirm `DebugUI` edits settings and world data, not pass internals.
- Confirm pass and technique resources are exchanged only through typed
  output structs.

For the first implementation slice:

- Bloom disabled should preserve the current HDR-to-postprocess output.
- Bloom enabled should affect only bright regions before tone mapping.
- Resizing the window should rebuild Bloom resources without stale
  texture handles.
- Debug UI changes should take effect without recreating the app.
