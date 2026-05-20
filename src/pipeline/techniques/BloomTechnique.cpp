#include "BloomTechnique.h"
#include "../../renderer/Renderer.h"
#include <algorithm>
#include <glm/vec2.hpp>
#include <stdexcept>

namespace HuanGL {

void BloomTechnique::Init(int width, int height) {
    extractShader_ = std::make_unique<Shader>("../shader/lighting/fullscreen.vert",
                                              "../shader/bloom/bright_extract.frag");
    blurShader_ = std::make_unique<Shader>("../shader/lighting/fullscreen.vert",
                                           "../shader/bloom/blur.frag");
    dummyVAO_ = std::make_unique<VertexArray>();
    CreateResources(width, height);
}

void BloomTechnique::Resize(int width, int height) {
    CreateResources(width, height);
}

void BloomTechnique::CreateResources(int width, int height) {
    width_ = std::max(1, width / 2);
    height_ = std::max(1, height / 2);

    auto makeTarget = [this]() {
        auto texture = Texture::Create2D(width_, height_, GL_RGBA16F, GL_RGBA, GL_FLOAT);
        texture->SetFilter(GL_LINEAR, GL_LINEAR);
        texture->SetWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
        return texture;
    };

    brightTexture_ = makeTarget();
    pingTexture_ = makeTarget();
    pongTexture_ = makeTarget();

    brightFBO_ = std::make_unique<Framebuffer>(width_, height_);
    brightFBO_->AttachColor(brightTexture_);
    brightFBO_->SetDrawBuffers({GL_COLOR_ATTACHMENT0});
    if (!brightFBO_->IsComplete()) {
        throw std::runtime_error("[BloomTechnique] bright framebuffer incomplete");
    }

    pingFBO_ = std::make_unique<Framebuffer>(width_, height_);
    pingFBO_->AttachColor(pingTexture_);
    pingFBO_->SetDrawBuffers({GL_COLOR_ATTACHMENT0});
    if (!pingFBO_->IsComplete()) {
        throw std::runtime_error("[BloomTechnique] ping framebuffer incomplete");
    }

    pongFBO_ = std::make_unique<Framebuffer>(width_, height_);
    pongFBO_->AttachColor(pongTexture_);
    pongFBO_->SetDrawBuffers({GL_COLOR_ATTACHMENT0});
    if (!pongFBO_->IsComplete()) {
        throw std::runtime_error("[BloomTechnique] pong framebuffer incomplete");
    }

    outputs_ = {};
}

void BloomTechnique::DrawFullscreen() const {
    dummyVAO_->Bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);
    dummyVAO_->Unbind();
}

BloomOutputs BloomTechnique::Execute(const FrameContext& frame,
                                     const PipelineOutputs& inputs,
                                     const BloomSettings& settings) {
    outputs_ = {};
    if (!settings.enabled || !inputs.lighting.hdrColor) {
        return outputs_;
    }

    const int radius = std::clamp(settings.radius, 1, 16);

    Renderer::SetViewport(0, 0, width_, height_);
    Renderer::EnableDepthTest(false);
    Renderer::EnableCullFace(false);

    brightFBO_->Bind();
    Renderer::Clear(true, false, false);
    extractShader_->Use();
    extractShader_->SetFloat("uThreshold", settings.threshold);
    inputs.lighting.hdrColor->Bind(0);
    DrawFullscreen();

    blurShader_->Use();
    blurShader_->SetInt("uRadius", radius);
    blurShader_->SetVec2("uTexelSize", glm::vec2(1.0f / width_, 1.0f / height_));

    pingFBO_->Bind();
    Renderer::Clear(true, false, false);
    blurShader_->SetBool("uHorizontal", true);
    brightTexture_->Bind(0);
    DrawFullscreen();

    pongFBO_->Bind();
    Renderer::Clear(true, false, false);
    blurShader_->SetBool("uHorizontal", false);
    pingTexture_->Bind(0);
    DrawFullscreen();

    Framebuffer::BindDefault();
    Renderer::SetViewport(0, 0, frame.width, frame.height);
    Renderer::EnableCullFace(true);
    Renderer::EnableDepthTest(true);

    outputs_.bloom = pongTexture_;
    return outputs_;
}

} // namespace HuanGL
