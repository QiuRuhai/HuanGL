#include "BloomStage.h"
#include "LightingStage.h"
#include "TAAStage.h"
#include "../PipelineResources.h"
#include "../../renderer/Renderer.h"
#include "../../renderer/FrameContext.h"
#include <algorithm>
#include <glm/vec2.hpp>
#include <stdexcept>
#include <string>

namespace HuanGL {

namespace {
constexpr int kMaxBloomMips = 6;
}

void BloomStage::Init(int width, int height) {
    extractShader_ = std::make_unique<Shader>("lighting/fullscreen.vert",
                                              "bloom/bright_extract.frag");
    downsampleShader_ = std::make_unique<Shader>("lighting/fullscreen.vert",
                                                 "bloom/downsample.frag");
    upsampleShader_ = std::make_unique<Shader>("lighting/fullscreen.vert",
                                               "bloom/upsample.frag");
    dummyVAO_ = std::make_unique<VertexArray>();
    CreateResources(width, height);
}

void BloomStage::Resize(int width, int height) {
    CreateResources(width, height);
}

BloomStage::BloomMip BloomStage::CreateMip(int width, int height,
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
        throw std::runtime_error(std::string("[BloomStage] ") +
                                 label + " framebuffer incomplete");
    }
    return mip;
}

void BloomStage::CreateResources(int width, int height) {
    downMips_.clear();
    upMips_.clear();

    int mipWidth = std::max(1, width / 2);
    int mipHeight = std::max(1, height / 2);

    for (int i = 0; i < kMaxBloomMips; ++i) {
        downMips_.push_back(CreateMip(mipWidth, mipHeight, "downsample"));
        upMips_.push_back(CreateMip(mipWidth, mipHeight, "upsample"));

        if (mipWidth == 1 && mipHeight == 1) break;

        mipWidth = std::max(1, mipWidth / 2);
        mipHeight = std::max(1, mipHeight / 2);
    }
}

void BloomStage::DrawFullscreen() const {
    dummyVAO_->Bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);
    dummyVAO_->Unbind();
}

void BloomStage::Execute(PipelineResources& resources, const FrameContext& frame) {
    const auto& settings = frame.renderSettings.bloom;
    const auto& lighting = resources.Get<LightingOutputs>();
    const auto& taa = resources.Get<TAAOutputs>();
    const std::shared_ptr<Texture>& hdrInput = taa.resolvedHdr ? taa.resolvedHdr : lighting.hdrColor;

    if (!settings.enabled || !hdrInput || downMips_.empty()) {
        resources.Set(BloomOutputs{});
        return;
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
    hdrInput->Bind(0);
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

    BloomOutputs outputs;

    if (activeCount == 1) {
        Framebuffer::BindDefault();
        Renderer::SetViewport(0, 0, frame.width, frame.height);
        Renderer::EnableCullFace(true);
        Renderer::EnableDepthTest(true);
        outputs.bloom = downMips_[0].texture;
        resources.Set(outputs);
        return;
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

    Framebuffer::BindDefault();
    Renderer::SetViewport(0, 0, frame.width, frame.height);
    Renderer::EnableCullFace(true);
    Renderer::EnableDepthTest(true);

    outputs.bloom = upMips_[0].texture;
    resources.Set(outputs);
}

} // namespace HuanGL
