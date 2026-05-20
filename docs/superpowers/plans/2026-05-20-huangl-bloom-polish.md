# HuanGL Bloom Polish Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Upgrade Bloom from a single blur pass into a multi-mip HDR bloom chain with soft-knee extraction and UI controls.

**Architecture:** Keep Bloom entirely inside `BloomTechnique`; `RenderPipeline` still sees one `BloomOutputs` value, and `PostProcessPass` still only composites the final bloom texture before tone mapping. Settings stay in `RenderSettings` and flow through `FrameContext`; `DebugUI` edits state only.

**Tech Stack:** C++17, OpenGL 4.6 DSA, GLAD2, GLSL 460, Dear ImGui, CMake/vcpkg on Windows.

---

## File Structure

- Modify `src/renderer/FrameContext.h`: extend `BloomSettings` with `softKnee` and `mipCount`.
- Modify `src/ui/DebugUI.cpp`: expose soft knee and mip count controls.
- Modify `shader/bloom/bright_extract.frag`: replace hard threshold with soft-knee threshold.
- Create `shader/bloom/downsample.frag`: filtered downsample shader.
- Create `shader/bloom/upsample.frag`: upsample-and-combine shader.
- Delete `shader/bloom/blur.frag`: the old single separable blur shader becomes stale.
- Modify `src/pipeline/techniques/BloomTechnique.h/cpp`: replace fixed bright/ping/pong resources with downsample and upsample mip vectors.
- Modify `AGENTS.md` and `docs/architecture.md`: describe multi-mip Bloom instead of single blur.

## Task 1: Extend Bloom Settings and UI

**Files:**
- Modify: `src/renderer/FrameContext.h`
- Modify: `src/ui/DebugUI.cpp`

- [ ] **Step 1: Extend `BloomSettings`**

In `src/renderer/FrameContext.h`, replace the current `BloomSettings` block with:

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

- [ ] **Step 2: Add controls to `DebugUI`**

In `src/ui/DebugUI.cpp`, replace the current Techniques section with:

```cpp
    if (ImGui::CollapsingHeader("Techniques")) {
        ImGui::Checkbox("Bloom", &state.renderSettings.bloom.enabled);
        ImGui::DragFloat("Bloom Threshold", &state.renderSettings.bloom.threshold,
                         0.05f, 0.0f, 20.0f);
        ImGui::DragFloat("Bloom Soft Knee", &state.renderSettings.bloom.softKnee,
                         0.01f, 0.0f, 2.0f);
        ImGui::DragFloat("Bloom Intensity", &state.renderSettings.bloom.intensity,
                         0.01f, 0.0f, 5.0f);
        ImGui::SliderInt("Bloom Radius", &state.renderSettings.bloom.radius,
                         1, 16);
        ImGui::SliderInt("Bloom Mips", &state.renderSettings.bloom.mipCount,
                         1, 6);
        ImGui::DragFloat("Exposure", &state.renderSettings.exposure,
                         0.01f, 0.0f, 10.0f);
    }
```

- [ ] **Step 3: Build check**

Run:

```powershell
cmake --build build --config Debug
```

Expected: build succeeds. Existing MSVC `C4819` warnings may remain.

- [ ] **Step 4: Commit**

```powershell
git add src/renderer/FrameContext.h src/ui/DebugUI.cpp
git commit -m "feat: add bloom polish settings"
```

## Task 2: Replace Bloom Shaders

**Files:**
- Modify: `shader/bloom/bright_extract.frag`
- Create: `shader/bloom/downsample.frag`
- Create: `shader/bloom/upsample.frag`
- Delete: `shader/bloom/blur.frag`

- [ ] **Step 1: Replace bright extract shader**

Replace all of `shader/bloom/bright_extract.frag` with:

```glsl
#version 460 core
in vec2 vUV;
out vec4 FragColor;

layout(binding = 0) uniform sampler2D uHDRInput;
uniform float uThreshold;
uniform float uSoftKnee;

void main() {
    vec3 hdr = texture(uHDRInput, vUV).rgb;
    float luma = dot(hdr, vec3(0.2126, 0.7152, 0.0722));

    float knee = max(uThreshold * uSoftKnee, 1e-5);
    float soft = clamp(luma - uThreshold + knee, 0.0, 2.0 * knee);
    soft = (soft * soft) / (4.0 * knee);

    float contribution = max(luma - uThreshold, soft);
    float weight = contribution / max(luma, 1e-5);
    FragColor = vec4(hdr * weight, 1.0);
}
```

- [ ] **Step 2: Add filtered downsample shader**

Create `shader/bloom/downsample.frag`:

```glsl
#version 460 core
in vec2 vUV;
out vec4 FragColor;

layout(binding = 0) uniform sampler2D uInput;
uniform vec2 uTexelSize;

void main() {
    vec3 center = texture(uInput, vUV).rgb * 0.50;
    vec3 axis = vec3(0.0);
    axis += texture(uInput, vUV + vec2( uTexelSize.x, 0.0)).rgb;
    axis += texture(uInput, vUV + vec2(-uTexelSize.x, 0.0)).rgb;
    axis += texture(uInput, vUV + vec2(0.0,  uTexelSize.y)).rgb;
    axis += texture(uInput, vUV + vec2(0.0, -uTexelSize.y)).rgb;
    axis *= 0.0833333;

    vec3 diagonal = vec3(0.0);
    diagonal += texture(uInput, vUV + vec2( uTexelSize.x,  uTexelSize.y)).rgb;
    diagonal += texture(uInput, vUV + vec2(-uTexelSize.x,  uTexelSize.y)).rgb;
    diagonal += texture(uInput, vUV + vec2( uTexelSize.x, -uTexelSize.y)).rgb;
    diagonal += texture(uInput, vUV + vec2(-uTexelSize.x, -uTexelSize.y)).rgb;
    diagonal *= 0.0416667;

    FragColor = vec4(center + axis + diagonal, 1.0);
}
```

- [ ] **Step 3: Add upsample-combine shader**

Create `shader/bloom/upsample.frag`:

```glsl
#version 460 core
in vec2 vUV;
out vec4 FragColor;

layout(binding = 0) uniform sampler2D uLowInput;
layout(binding = 1) uniform sampler2D uHighInput;
uniform vec2 uLowTexelSize;
uniform float uRadius;

void main() {
    vec2 radius = uLowTexelSize * max(uRadius, 0.25);

    vec3 low = texture(uLowInput, vUV).rgb * 4.0;
    low += texture(uLowInput, vUV + vec2( radius.x, 0.0)).rgb * 2.0;
    low += texture(uLowInput, vUV + vec2(-radius.x, 0.0)).rgb * 2.0;
    low += texture(uLowInput, vUV + vec2(0.0,  radius.y)).rgb * 2.0;
    low += texture(uLowInput, vUV + vec2(0.0, -radius.y)).rgb * 2.0;
    low += texture(uLowInput, vUV + vec2( radius.x,  radius.y)).rgb;
    low += texture(uLowInput, vUV + vec2(-radius.x,  radius.y)).rgb;
    low += texture(uLowInput, vUV + vec2( radius.x, -radius.y)).rgb;
    low += texture(uLowInput, vUV + vec2(-radius.x, -radius.y)).rgb;
    low /= 16.0;

    vec3 high = texture(uHighInput, vUV).rgb;
    FragColor = vec4(high + low, 1.0);
}
```

- [ ] **Step 4: Delete the old blur shader**

Delete `shader/bloom/blur.frag`.

- [ ] **Step 5: Build check**

Run:

```powershell
cmake --build build --config Debug
```

Expected: build succeeds. Shader path issues will be caught by the runtime smoke test after `BloomTechnique` is rewired.

- [ ] **Step 6: Commit**

```powershell
git add shader/bloom/bright_extract.frag shader/bloom/downsample.frag shader/bloom/upsample.frag
git rm shader/bloom/blur.frag
git commit -m "feat: add multi-mip bloom shaders"
```

## Task 3: Refactor BloomTechnique to Multi-Mip

**Files:**
- Modify: `src/pipeline/techniques/BloomTechnique.h`
- Modify: `src/pipeline/techniques/BloomTechnique.cpp`

- [ ] **Step 1: Replace `BloomTechnique.h`**

Replace all of `src/pipeline/techniques/BloomTechnique.h` with:

```cpp
#pragma once
#include "../../renderer/Buffer.h"
#include "../../renderer/Framebuffer.h"
#include "../../renderer/FrameContext.h"
#include "../../renderer/Shader.h"
#include "../../renderer/Texture.h"
#include "../PipelineOutputs.h"
#include <memory>
#include <vector>

namespace HuanGL {

class BloomTechnique {
public:
    void Init(int width, int height);
    void Resize(int width, int height);

    BloomOutputs Execute(const FrameContext& frame,
                         const PipelineOutputs& inputs,
                         const BloomSettings& settings);

    BloomOutputs GetOutputs() const { return outputs_; }

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
    void RestoreFrameState(const FrameContext& frame) const;

    std::unique_ptr<Shader> extractShader_;
    std::unique_ptr<Shader> downsampleShader_;
    std::unique_ptr<Shader> upsampleShader_;
    std::unique_ptr<VertexArray> dummyVAO_;

    std::vector<BloomMip> downMips_;
    std::vector<BloomMip> upMips_;

    BloomOutputs outputs_;
};

} // namespace HuanGL
```

- [ ] **Step 2: Replace `BloomTechnique.cpp`**

Replace all of `src/pipeline/techniques/BloomTechnique.cpp` with:

```cpp
#include "BloomTechnique.h"
#include "../../renderer/Renderer.h"
#include <algorithm>
#include <glm/vec2.hpp>
#include <stdexcept>
#include <string>

namespace HuanGL {

namespace {
constexpr int kMaxBloomMips = 6;
}

void BloomTechnique::Init(int width, int height) {
    extractShader_ = std::make_unique<Shader>("../shader/lighting/fullscreen.vert",
                                              "../shader/bloom/bright_extract.frag");
    downsampleShader_ = std::make_unique<Shader>("../shader/lighting/fullscreen.vert",
                                                 "../shader/bloom/downsample.frag");
    upsampleShader_ = std::make_unique<Shader>("../shader/lighting/fullscreen.vert",
                                               "../shader/bloom/upsample.frag");
    dummyVAO_ = std::make_unique<VertexArray>();
    CreateResources(width, height);
}

void BloomTechnique::Resize(int width, int height) {
    CreateResources(width, height);
}

BloomTechnique::BloomMip BloomTechnique::CreateMip(int width, int height,
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
        throw std::runtime_error(std::string("[BloomTechnique] ") +
                                 label + " framebuffer incomplete");
    }
    return mip;
}

void BloomTechnique::CreateResources(int width, int height) {
    downMips_.clear();
    upMips_.clear();
    outputs_ = {};

    int mipWidth = std::max(1, width / 2);
    int mipHeight = std::max(1, height / 2);

    for (int i = 0; i < kMaxBloomMips; ++i) {
        downMips_.push_back(CreateMip(mipWidth, mipHeight, "downsample"));
        upMips_.push_back(CreateMip(mipWidth, mipHeight, "upsample"));

        if (mipWidth == 1 && mipHeight == 1) {
            break;
        }

        mipWidth = std::max(1, mipWidth / 2);
        mipHeight = std::max(1, mipHeight / 2);
    }
}

void BloomTechnique::DrawFullscreen() const {
    dummyVAO_->Bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);
    dummyVAO_->Unbind();
}

void BloomTechnique::RestoreFrameState(const FrameContext& frame) const {
    Framebuffer::BindDefault();
    Renderer::SetViewport(0, 0, frame.width, frame.height);
    Renderer::EnableCullFace(true);
    Renderer::EnableDepthTest(true);
}

BloomOutputs BloomTechnique::Execute(const FrameContext& frame,
                                     const PipelineOutputs& inputs,
                                     const BloomSettings& settings) {
    outputs_ = {};
    if (!settings.enabled || !inputs.lighting.hdrColor || downMips_.empty()) {
        return outputs_;
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
    inputs.lighting.hdrColor->Bind(0);
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

    if (activeCount == 1) {
        RestoreFrameState(frame);
        outputs_.bloom = downMips_[0].texture;
        return outputs_;
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

    RestoreFrameState(frame);

    outputs_.bloom = upMips_[0].texture;
    return outputs_;
}

} // namespace HuanGL
```

- [ ] **Step 3: Reconfigure and build**

This task only modifies existing `.cpp` files, but run configure anyway after deleting a shader file and adding two new shader files so the generated project remains current.

Run:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

Expected: build succeeds. Existing MSVC `C4819` warnings may remain.

- [ ] **Step 4: Runtime shader path smoke**

Run:

```powershell
$build=(Resolve-Path 'build').Path
$out=Join-Path $build 'bloom-polish-smoke.out'
$err=Join-Path $build 'bloom-polish-smoke.err'
Remove-Item -LiteralPath $out,$err -ErrorAction SilentlyContinue
$p=Start-Process -FilePath (Join-Path $build 'Debug\HuanGL.exe') -WorkingDirectory $build -PassThru -WindowStyle Hidden -RedirectStandardOutput $out -RedirectStandardError $err
Start-Sleep -Seconds 5
if ($p.HasExited) { Write-Output "EXITED:$($p.ExitCode)" } else { Stop-Process -Id $p.Id; Write-Output 'STARTED_AND_STOPPED' }
if (Test-Path $out) { Get-Content -Raw $out }
if (Test-Path $err) { Get-Content -Raw $err }
```

Expected: output includes OpenGL version and no shader file load failure.

- [ ] **Step 5: Commit**

```powershell
git add src/pipeline/techniques/BloomTechnique.h src/pipeline/techniques/BloomTechnique.cpp
git commit -m "feat: implement multi-mip bloom"
```

## Task 4: Update Documentation

**Files:**
- Modify: `AGENTS.md`
- Modify: `docs/architecture.md`

- [ ] **Step 1: Update `AGENTS.md` Bloom inventory**

In `AGENTS.md`, replace the Phase 3.5 rows for Bloom shaders with:

```markdown
| `src/pipeline/techniques/BloomTechnique.h/cpp` | Multi-mip Bloom technique with soft-knee bright extract, downsample chain, and upsample combine |
| `shader/bloom/bright_extract.frag` | Extracts bright HDR radiance with soft-knee thresholding |
| `shader/bloom/downsample.frag` | Filtered downsample pass for the Bloom mip chain |
| `shader/bloom/upsample.frag` | Upsample-and-combine pass for reconstructing broad Bloom |
| `shader/postprocess/postprocess.frag` | Composites optional Bloom before tone mapping and exposes Bloom debug view |
```

- [ ] **Step 2: Update `docs/architecture.md` pipeline description**

In the Render Pipeline diagram section, replace the Bloom lines with:

```text
        BloomTechnique     -> BloomOutputs
             |               soft-knee extract + multi-mip downsample/upsample chain
             v
        PostProcessPass    <- reads Lighting/GBuffer/Shadow/Bloom outputs
             |               composite + tone map + gamma + debug overlay
```

Add this sentence after the paragraph explaining `PipelineOutputs`:

```markdown
Bloom is intentionally hidden behind one `BloomOutputs::bloom` texture even
though the technique owns several internal mip-sized framebuffers. This keeps
the postprocess boundary stable for TAA and later GI techniques.
```

- [ ] **Step 3: Build check**

Run:

```powershell
cmake --build build --config Debug
```

Expected: build succeeds.

- [ ] **Step 4: Commit**

```powershell
git add AGENTS.md docs/architecture.md
git commit -m "docs: document multi-mip bloom"
```

## Task 5: Final Verification

**Files:**
- No code edits unless verification exposes a defect.

- [ ] **Step 1: Full configure and build**

Run:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

Expected: build succeeds. Existing MSVC `C4819` warnings may remain.

- [ ] **Step 2: Boundary searches**

Run:

```powershell
rg -n "GetPostProcess|GetLighting|GetHDROutput|GetAlbedoMetallic|GetNormalRoughness|GetShadowMapArray|GetCascades" src
rg -n "ImGui::|<imgui.h>" src/pipeline src/renderer src/scene src/app
rg -n "RenderGraph|class IRenderTechnique|RHI" src
rg -n "blur.frag" src shader AGENTS.md docs/architecture.md
```

Expected:
- First command has no matches.
- Second command has no matches.
- Third command has no matches.
- Fourth command has no matches, proving the stale single-blur shader path is gone.

- [ ] **Step 3: Runtime smoke test**

Run:

```powershell
$build=(Resolve-Path 'build').Path
$out=Join-Path $build 'bloom-polish-final.out'
$err=Join-Path $build 'bloom-polish-final.err'
Remove-Item -LiteralPath $out,$err -ErrorAction SilentlyContinue
$p=Start-Process -FilePath (Join-Path $build 'Debug\HuanGL.exe') -WorkingDirectory $build -PassThru -WindowStyle Hidden -RedirectStandardOutput $out -RedirectStandardError $err
Start-Sleep -Seconds 5
if ($p.HasExited) { Write-Output "EXITED:$($p.ExitCode)" } else { Stop-Process -Id $p.Id; Write-Output 'STARTED_AND_STOPPED' }
if (Test-Path $out) { Get-Content -Raw $out }
if (Test-Path $err) { Get-Content -Raw $err }
```

Expected:
- App starts and remains alive for 5 seconds.
- Output includes OpenGL version.
- Output does not include shader compile, shader link, or shader file load failures.
- Optional model missing logs are acceptable because scene registration is soft-failure by design.

- [ ] **Step 4: Manual visual check if a visible OpenGL window is available**

Run:

```powershell
.\build\Debug\HuanGL.exe
```

Expected:
- Final view renders.
- Bloom debug view shows only bloom contribution.
- Bloom disabled returns to the non-bloom path.
- Soft knee changes transition smoothness near the threshold.
- Mip count changes the apparent spread of the glow.
- Window resize does not crash.

- [ ] **Step 5: Final status**

Run:

```powershell
git status --short --branch --untracked-files=all
```

Expected: clean worktree on `codex/bloom-polish`.
