#include "ComparisonStage.h"
#include "PathTracerStage.h"
#include "TAAStage.h"
#include "LightingStage.h"
#include "../PipelineResources.h"
#include "../../renderer/Shader.h"
#include "../../renderer/Renderer.h"
#include "../../renderer/Framebuffer.h"
#include "../../renderer/FrameContext.h"
#include <glad/glad.h>
#include <cmath>
#include <vector>
#include <algorithm>

namespace HuanGL {

void ComparisonStage::Init(int width, int height) {
    width_  = width;
    height_ = height;

    // Compute shader: per-pixel error into RGBA32F texture
    errorShader_     = std::make_unique<Shader>("comparison/error.comp");
    // Fragment shader: composite view modes to default framebuffer
    compositeShader_ = std::make_unique<Shader>("lighting/fullscreen.vert",
                                                "comparison/composite.frag");

    errorTex_ = Texture::Create2D(width_, height_, GL_RGBA32F, GL_RGBA, GL_FLOAT);
    dummyVAO_ = std::make_unique<VertexArray>();
}

void ComparisonStage::Resize(int width, int height) {
    width_  = width;
    height_ = height;
    errorTex_ = Texture::Create2D(width_, height_, GL_RGBA32F, GL_RGBA, GL_FLOAT);
}

void ComparisonStage::Execute(PipelineResources& resources, const FrameContext& frame) {
    // PT disabled — leave PostProcessStage's image on the backbuffer unchanged.
    if (!resources.Has<ReferenceOutputs>()) {
        readout_.valid = false;
        return;
    }

    const auto& ref = resources.Get<ReferenceOutputs>();

    // Resolve realtime HDR: prefer the TAA-resolved output, fall back to raw
    // lighting. Guard on the POINTER, not Has<TAAOutputs>(): TAAStage always
    // publishes a TAAOutputs, but its resolvedHdr is null when TAA is disabled,
    // so Has<>() is true yet the texture is null. Mirrors PostProcessStage.
    const auto& taa = resources.Get<TAAOutputs>();
    const auto& lighting = resources.Get<LightingOutputs>();
    const std::shared_ptr<Texture>& realtime =
        taa.resolvedHdr ? taa.resolvedHdr : lighting.hdrColor;

    const float invSampleCount = 1.0f / static_cast<float>(std::max(ref.sampleCount, 1u));

    // ---- Error compute pass ------------------------------------------------
    // Bind textures as sampler inputs.
    realtime->Bind(0);
    ref.hdr->Bind(1);
    // Bind error texture as write-only image (unit 2).
    errorTex_->BindImage(2, GL_WRITE_ONLY, GL_RGBA32F);

    // Set uniforms via DSA (glProgramUniform — no bind needed for uniforms).
    errorShader_->SetVec2("uResolution",
        glm::vec2(static_cast<float>(width_), static_cast<float>(height_)));
    errorShader_->SetFloat("uInvSampleCount", invSampleCount);

    // CRITICAL: Use() must precede Dispatch(). Shader::Dispatch only issues
    // glDispatchCompute + a memory barrier; it does NOT bind the program.
    // Without Use() the dispatch raises GL_INVALID_OPERATION "No active compute shader"
    // (same trap fixed in PathTracerStage).
    errorShader_->Use();
    errorShader_->Dispatch((width_ + 7) / 8, (height_ + 7) / 8);

    // ---- Throttled CPU readback for RMSE/MAPE ------------------------------
    // Only read back on the first sample and on power-of-two sample counts to
    // avoid a per-frame GPU stall. Between readbacks, readout_ keeps its last values.
    const bool doReadback = (ref.sampleCount == 1) ||
                            (ref.sampleCount != 0 &&
                             (ref.sampleCount & (ref.sampleCount - 1)) == 0);
    if (doReadback) {
        const int pixelCount = width_ * height_;
        const GLsizei byteSize = static_cast<GLsizei>(pixelCount * 4 * sizeof(float));
        std::vector<float> cpuBuf(static_cast<size_t>(pixelCount) * 4);

        glGetTextureImage(errorTex_->GetID(), 0, GL_RGBA, GL_FLOAT,
                          byteSize, cpuBuf.data());

        double sumSq  = 0.0;
        double sumRel = 0.0;
        for (int i = 0; i < pixelCount; ++i) {
            sumSq  += static_cast<double>(cpuBuf[i * 4 + 1]); // g = squared luma error
            sumRel += static_cast<double>(cpuBuf[i * 4 + 2]); // b = relative luma error
        }

        readout_.rmse        = std::sqrt(sumSq / pixelCount);
        readout_.mape        = sumRel / pixelCount;
        readout_.sampleCount = ref.sampleCount;
        readout_.valid       = true;
    }

    // ---- Composite to default framebuffer ----------------------------------
    Framebuffer::BindDefault();
    Renderer::SetViewport(0, 0, width_, height_);
    Renderer::EnableDepthTest(false);
    Renderer::EnableCullFace(false);

    compositeShader_->Use();

    // Bind realtime→unit 0, reference→unit 1, error→unit 2.
    realtime->Bind(0);
    ref.hdr->Bind(1);
    errorTex_->Bind(2);

    compositeShader_->SetInt("uRealtime",      0);
    compositeShader_->SetInt("uReference",     1);
    compositeShader_->SetInt("uError",         2);
    compositeShader_->SetVec2("uResolution",
        glm::vec2(static_cast<float>(width_), static_cast<float>(height_)));
    compositeShader_->SetFloat("uInvSampleCount", invSampleCount);
    compositeShader_->SetInt("uView",
        static_cast<int>(frame.debugSettings.compareView));
    compositeShader_->SetFloat("uErrorScale", frame.debugSettings.errorScale);

    dummyVAO_->Bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);
    dummyVAO_->Unbind();

    Renderer::EnableCullFace(true);
    Renderer::EnableDepthTest(true);
}

} // namespace HuanGL
