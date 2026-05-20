# HuanGL Bloom Polish Design

Date: 2026-05-20
Status: Draft for review

## Purpose

The current Bloom implementation proved the technique architecture, but
it is still a minimal bright-pass plus one horizontal and one vertical
blur. That shape is useful for wiring, but it does not produce the broad,
stable glow expected from a modern HDR renderer.

This phase upgrades Bloom into a multi-mip HDR bloom chain while keeping
the new technique boundary intact. The work should improve visual
quality without starting TAA, exposure automation, lens dirt, or another
architecture refactor.

## Current Behavior

`BloomTechnique` currently:

- extracts bright HDR pixels to one half-resolution texture,
- runs one separable blur over that texture,
- exposes the final blurred texture through `BloomOutputs`,
- lets `PostProcessPass` add the result before tone mapping.

The main limitation is scale. A single blur radius cannot give both a
tight local halo and a wide soft glow without either looking too sharp or
turning the whole image hazy.

## Target Behavior

Bloom should become a multi-mip chain:

```text
Lighting HDR
    ↓
Bright extract with soft knee
    ↓
level 0: 1/2 resolution bright texture
    ↓ downsample
level 1: 1/4 resolution
    ↓ downsample
level 2: 1/8 resolution
    ↓ downsample
level 3: 1/16 resolution
    ↓ downsample
level 4: 1/32 resolution
    ↓
upsample and combine from smallest to largest
    ↓
BloomOutputs.bloom = level 0 combined bloom
    ↓
PostProcessPass adds bloom to HDR before tone mapping
```

The final output remains a single texture handle in `BloomOutputs`, so
downstream code does not need to know how many internal levels exist.

## Algorithm

### Bright Extract

Use luminance thresholding with a soft knee. The threshold decides where
bloom begins; the soft knee makes the transition gradual so highlights
do not pop harshly.

The extraction shader should output HDR radiance, not tone-mapped color.
For a pixel luminance `luma`, threshold `t`, and knee width `k`, the
weight should ramp smoothly near `t` and preserve bright color ratios.

### Downsample Chain

Allocate `N` levels, clamped by viewport size and the user setting.
Level 0 starts at half resolution. Each following level halves width and
height, with a minimum dimension of 1.

Each downsample pass reads the previous level and writes the next level.
Use linear filtering and a small weighted filter instead of a raw single
tap. This reduces sparkle and makes the chain stable during camera
movement.

### Upsample Combine

Upsample from the smallest level back to level 0. Each upsample pass
reads:

- the current lower-resolution bloom level,
- the next higher-resolution level,
- an upsample radius in texels.

It writes a combined result back to the higher-resolution level. This
keeps broad glow from low levels while retaining tighter halos from high
levels.

### Composite

`PostProcessPass` keeps its current role:

```glsl
hdr += bloom * bloomIntensity;
hdr *= exposure;
toneMap(hdr);
gammaCorrect(hdr);
```

Bloom remains disabled by returning a null `BloomOutputs::bloom` handle.

## Settings

Extend `BloomSettings` conservatively:

```cpp
struct BloomSettings {
    bool enabled = true;
    float threshold = 1.0f;
    float softKnee = 0.5f;
    float intensity = 0.08f;
    int radius = 5;
    int mipCount = 5;
};
```

`radius` controls upsample spread. `mipCount` controls how many levels
participate. The implementation should clamp `mipCount` to the number of
levels possible for the current viewport.

`DebugUI` should edit these fields and continue to edit only
`ApplicationState::renderSettings`.

## Files

Expected implementation scope:

- `src/renderer/FrameContext.h`: add `softKnee` and `mipCount`.
- `src/pipeline/techniques/BloomTechnique.h/cpp`: replace fixed
  bright/ping/pong resources with a vector of mip levels.
- `shader/bloom/bright_extract.frag`: add soft-knee thresholding.
- `shader/bloom/downsample.frag`: new downsample shader.
- `shader/bloom/upsample.frag`: new upsample-combine shader.
- `src/ui/DebugUI.cpp`: expose soft knee and mip count.
- `AGENTS.md` and `docs/architecture.md`: update Bloom description.

`PostProcessPass` should not gain Bloom internals. It should still only
consume `PipelineOutputs`.

## Non-Goals

- No TAA.
- No auto exposure.
- No lens dirt texture.
- No anamorphic streaks.
- No color grading pass.
- No render graph.
- No RHI.

## Verification

Required checks:

- Configure and build with the standard Windows commands.
- Run the old boundary searches: no pass getter regression and no ImGui
  includes outside `src/ui`.
- Run a short runtime smoke test from the build directory.
- Verify `Bloom` debug view shows the bloom contribution rather than the
  full scene color.
- Verify toggling Bloom off returns to the non-bloom path.
- Verify window resize recreates the bloom levels without crashing.

Expected visual result:

- Small highlights retain a local halo.
- Very bright areas produce a wider soft glow.
- Threshold changes affect which pixels bloom.
- Soft knee changes highlight transition smoothness.
- The image should not become uniformly gray at default settings.
