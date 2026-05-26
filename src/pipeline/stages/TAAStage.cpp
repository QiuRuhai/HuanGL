#include "TAAStage.h"
#include "GBufferStage.h"
#include "LightingStage.h"
#include "../PipelineResources.h"
#include "../../renderer/Buffer.h"
#include "../../renderer/FrameContext.h"
#include "../../renderer/Renderer.h"
#include <algorithm>
#include <stdexcept>

namespace HuanGL {

void TAAStage::Init(int width, int height) {
    shader_ = std::make_unique<Shader>("lighting/fullscreen.vert", "taa/resolve.frag");
    dummyVAO_ = std::make_unique<VertexArray>();
    CreateResources(width, height);
}

void TAAStage::Resize(int width, int height) {
    CreateResources(width, height);
}

void TAAStage::InvalidateHistory() {
    historyValid_ = false;
    wasEnabled_ = false;
}

void TAAStage::CreateResources(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);

    resolved_ = Texture::Create2D(width_, height_, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    resolved_->SetFilter(GL_LINEAR, GL_LINEAR);
    resolved_->SetWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

    resolvedFBO_ = std::make_unique<Framebuffer>(width_, height_);
    resolvedFBO_->AttachColor(resolved_, 0);
    resolvedFBO_->SetDrawBuffers({GL_COLOR_ATTACHMENT0});
    if (!resolvedFBO_->IsComplete()) {
        throw std::runtime_error("[TAAStage] resolved framebuffer incomplete");
    }

    for (auto& historyTexture : history_) {
        historyTexture = Texture::Create2D(width_, height_, GL_RGBA16F, GL_RGBA, GL_FLOAT);
        historyTexture->SetFilter(GL_LINEAR, GL_LINEAR);
        historyTexture->SetWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
    }

    historyReadIndex_ = 0;
    InvalidateHistory();
}

void TAAStage::DrawFullscreen() const {
    dummyVAO_->Bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);
    dummyVAO_->Unbind();
}

void TAAStage::Execute(PipelineResources& resources, const FrameContext& frame) {
    const auto& lighting = resources.Get<LightingOutputs>();
    const auto& gbuffer = resources.Get<GBufferOutputs>();

    TAAOutputs outputs;
    if (!frame.renderSettings.taa.enabled || !lighting.hdrColor || !gbuffer.depth) {
        InvalidateHistory();
        resources.Set(outputs);
        return;
    }

    if (!wasEnabled_) {
        historyValid_ = false;
    }

    const int historyWriteIndex = 1 - historyReadIndex_;

    resolvedFBO_->Bind();
    Renderer::SetViewport(0, 0, width_, height_);
    Renderer::Clear(true, false, false);
    Renderer::EnableDepthTest(false);
    Renderer::EnableCullFace(false);

    shader_->Use();
    lighting.hdrColor->Bind(0);
    gbuffer.depth->Bind(1);
    history_[historyReadIndex_]->Bind(2);
    shader_->SetMat4("uInvViewProj", frame.camera.invViewProj);
    shader_->SetMat4("uPrevViewProj", frame.camera.prevViewProj);
    shader_->SetBool("uHistoryValid", historyValid_);
    shader_->SetFloat("uFeedback", std::clamp(frame.renderSettings.taa.feedback, 0.0f, 0.98f));
    DrawFullscreen();

    glCopyImageSubData(resolved_->GetID(), GL_TEXTURE_2D, 0, 0, 0, 0,
                       history_[historyWriteIndex]->GetID(), GL_TEXTURE_2D, 0, 0, 0, 0,
                       width_, height_, 1);

    Framebuffer::BindDefault();
    Renderer::SetViewport(0, 0, frame.width, frame.height);
    Renderer::EnableCullFace(true);
    Renderer::EnableDepthTest(true);

    historyReadIndex_ = historyWriteIndex;
    historyValid_ = true;
    wasEnabled_ = true;
    outputs.resolvedHdr = resolved_;
    resources.Set(outputs);
}

} // namespace HuanGL
