# HuanGL Post-Process Polish Design

## Summary

This phase completes the most visible part of Phase 4 by improving final
image stability and tone response without starting the GI roadmap. The
work adds a Temporal Anti-Aliasing stage, one additional filmic tone-map
operator, and two small quality fixes that reduce friction for later
history-based techniques.

The goal is a better portfolio-facing default image: less shimmering,
more stable edges, and a more flexible final color transform. The design
intentionally avoids motion vectors, automatic exposure, a full
post-process graph, and any RHI-style abstraction.

## Goals

- Add a `TAAStage` that runs after lighting and before Bloom.
- Keep `LightingStage` responsible only for raw HDR radiance.
- Let Bloom and final PostProcess consume the TAA-resolved HDR texture
  when TAA is enabled.
- Add an AgX tone-map option alongside ACES, Reinhard, and linear output.
- Fix camera movement so it uses the current frame delta time.
- Improve `PipelineResources` missing-resource diagnostics.
- Add clear history invalidation paths for resize, scene switch, and TAA
  toggles.

## Non-Goals

- No motion-vector GBuffer attachment or velocity pass.
- No automatic exposure or luminance histogram.
- No lens dirt, chromatic aberration, vignette, sharpening pass, or
  general post-process stack.
- No render graph. The fixed `IPipelineStage` vector remains the
  orchestration model.
- No dynamic-object-perfect TAA. Dynamic transform editing may still
  show limited ghosting in this first version.

## Pipeline Placement

The stage order becomes:

```text
ShadowStage
GBufferStage
LightingStage
TAAStage
BloomStage
PostProcessStage
```

`LightingStage` continues to write a raw RGBA16F HDR target. `TAAStage`
reads that target and the GBuffer depth, resolves against a history
texture, and writes a stable RGBA16F HDR target. `BloomStage` and
`PostProcessStage` use the resolved target when available; if TAA is
disabled or invalid for the current frame, they fall back to the raw
lighting target.

This keeps tone mapping in `PostProcessStage` and keeps Bloom operating
on HDR values before tone mapping.

## Resource Contracts

Add a new output struct in `TAAStage.h`:

```cpp
struct TAAOutputs {
    std::shared_ptr<Texture> resolvedHdr;
};
```

Stage reads and writes:

| Stage | Reads | Writes |
|-------|-------|--------|
| `LightingStage` | `GBufferOutputs`, `ShadowOutputs`, `RenderSceneView` | `LightingOutputs` |
| `TAAStage` | `LightingOutputs`, `GBufferOutputs` | `TAAOutputs` |
| `BloomStage` | `TAAOutputs` if present, otherwise `LightingOutputs` | `BloomOutputs` |
| `PostProcessStage` | `TAAOutputs` if present, otherwise `LightingOutputs`; plus debug inputs | Backbuffer |

`TAAOutputs::resolvedHdr == nullptr` means downstream stages should use
`LightingOutputs::hdrColor`.

## Camera and Jitter Data

TAA needs jittered main rasterization, but shadow cascades should remain
stable. Extend `CameraData` so consumers can choose the correct matrix:

```cpp
glm::mat4 proj;                 // jittered main projection
glm::mat4 viewProj;             // jittered main view-projection
glm::mat4 unjitteredProj;
glm::mat4 unjitteredViewProj;
glm::mat4 prevViewProj;         // previous jittered view-projection
glm::vec2 jitter;
glm::vec2 prevJitter;
```

The GBuffer pass uses `viewProj` so rasterization receives sub-pixel
offsets. Shadow cascade fitting and cascade debug selection use
`unjitteredViewProj` to avoid visible shadow swimming.

`App` or the camera/frame builder owns the previous-frame camera data.
The previous matrix is updated once per rendered frame, after the
pipeline executes. Minimized windows do not advance history.

## TAA Algorithm

The first version uses a practical depth-reprojection resolve:

1. Generate an 8-frame Halton jitter sequence.
2. Scale jitter by inverse viewport dimensions and apply it to the main
   projection matrix.
3. Render lighting into the current HDR texture.
4. Reconstruct world position from current depth and current inverse
   jittered view-projection.
5. Project world position by previous jittered view-projection to get
   history UV.
6. Reject history if UV is outside `[0, 1]`, history is invalid, or TAA
   was just enabled.
7. Clamp sampled history to a current-frame 3x3 neighborhood min/max.
8. Blend current HDR and clamped history using a feedback value.
9. Write the resolved HDR target and copy/flip it into the next history
   texture.

The default feedback should be conservative, around `0.90`. UI exposes
`enabled` and `feedback` only. Additional clamp strength or sharpening
settings can be added later if the first implementation needs them.

## History Resources

`TAAStage` owns:

- two RGBA16F history textures for ping-pong accumulation,
- one RGBA16F output texture exposed as `TAAOutputs::resolvedHdr`,
- frame validity state,
- previous TAA-enabled state.

On resize, `TAAStage::Resize()` recreates all textures and marks history
invalid. The first frame after invalidation writes current HDR directly
to the output/history, avoiding stale samples.

## History Invalidation

`RenderPipeline` should regain a small explicit invalidation hook:

```cpp
void InvalidateHistory();
```

The hook iterates stages and invalidates stages that support history.
Add a virtual no-op method to `IPipelineStage` so history-aware stages
can override it without adding another interface:

```cpp
virtual void InvalidateHistory() {}
```

Required invalidation triggers:

- window resize,
- scene switch via `N` or the Debug UI scene button,
- TAA changing from disabled to enabled,
- pipeline initialization.

DebugUI transform or light edits do not invalidate history in this
phase. If ghosting is obvious during editing, a later `World` dirty flag
can trigger invalidation.

## Tone Mapping

Add `AgX` to `ToneMapMode`:

```cpp
enum class ToneMapMode {
    ACES = 0,
    Reinhard = 1,
    AgX = 2,
    None = 3,
};
```

`PostProcessStage` remains the only tone-map owner. The GLSL
implementation should be a compact analytic AgX approximation rather
than a LUT-based implementation. ACES remains the default operator for
compatibility with existing scene tuning; AgX is added as a selectable
option.

`CycleToneMap()` and DebugUI combo entries must be updated together so
keyboard and UI behavior stay aligned.

## Input and UI

Extend render settings:

```cpp
struct TAASettings {
    bool enabled = true;
    float feedback = 0.90f;
};
```

Add `TAASettings taa;` to `RenderSettings`.

The existing DebugUI `Techniques` section should expose:

```text
TAA enabled
TAA feedback
Bloom settings
Exposure
```

Tone-map selection remains in the `Render` section, now with AgX.

Fix `InputController::Update` so it receives current `dt` directly:

```cpp
void Update(ApplicationState& state, float deltaTime);
```

Camera movement should not read `state.frameStats.deltaTime`, because
frame stats are written after input processing.

## PipelineResources Diagnostics

Improve missing-resource errors from:

```text
PipelineResources: missing resource
```

to include the requested C++ type when possible:

```text
PipelineResources: missing resource: HuanGL::TAAOutputs
```

This keeps the typed registry lightweight while making stage-order bugs
faster to diagnose.

## Debug View Behavior

Final output uses:

```text
Lighting HDR -> TAA -> Bloom -> Tone map -> Gamma
```

Existing debug views keep their current meaning:

- Albedo, normal, roughness, metallic, and depth read GBuffer data.
- Cascades reads unjittered cascade information.
- Bloom shows Bloom contribution generated from the same HDR source used
  by final composition.

No separate TAA debug view is required in this phase. Avoid adding debug
UI that is not needed to verify the feature.

## Testing and Verification

Required verification:

- MSVC Debug configure and build pass.
- clang-cl Debug configure and build pass.
- TAA on/off does not crash or produce a black frame.
- Bloom on/off still composites correctly with TAA enabled and disabled.
- Window resize invalidates history and does not sample old dimensions.
- Scene switch invalidates history and does not retain the previous
  scene in the first new frame.
- Tone-map cycling reaches ACES, Reinhard, AgX, and None in both
  keyboard and DebugUI paths.
- Debug views retain their current semantic meaning.

Runtime visual checks:

- Static scene edges shimmer less with TAA enabled.
- Camera movement does not leave large persistent trails.
- Bloom contribution remains HDR and is composited before tone mapping.

## Documentation Updates

Update:

- `AGENTS.md` current progress and roadmap rows for Phase 4,
- `docs/architecture.md` render pipeline diagram and Phase 4 section.

The documentation should state that this is a first TAA implementation
without motion vectors, so future SSGI/RSM denoising work can decide
whether to extend it or add velocity data.
