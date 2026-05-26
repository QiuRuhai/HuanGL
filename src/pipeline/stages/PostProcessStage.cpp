#include "PostProcessStage.h"
#include "ShadowStage.h"
#include "GBufferStage.h"
#include "LightingStage.h"
#include "TAAStage.h"
#include "BloomStage.h"
#include "../PipelineResources.h"
#include "../../renderer/Shader.h"
#include "../../renderer/Renderer.h"
#include "../../renderer/Framebuffer.h"
#include "../../renderer/FrameContext.h"

namespace HuanGL {

void PostProcessStage::Init(int /*width*/, int /*height*/) {
    shader_ = std::make_unique<Shader>("lighting/fullscreen.vert",
                                       "postprocess/postprocess.frag");
    dummyVAO_ = std::make_unique<VertexArray>();
}

void PostProcessStage::Resize(int /*width*/, int /*height*/) {}

void PostProcessStage::Execute(PipelineResources& resources, const FrameContext& frame) {
    const auto& lighting = resources.Get<LightingOutputs>();
    const auto& taa = resources.Get<TAAOutputs>();
    const std::shared_ptr<Texture>& hdrInput = taa.resolvedHdr ? taa.resolvedHdr : lighting.hdrColor;
    const auto& gbuffer = resources.Get<GBufferOutputs>();
    const auto& shadow = resources.Get<ShadowOutputs>();
    const auto& bloom = resources.Get<BloomOutputs>();

    Framebuffer::BindDefault();
    Renderer::SetViewport(0, 0, frame.width, frame.height);
    Renderer::EnableDepthTest(false);
    Renderer::EnableCullFace(false);

    shader_->Use();
    shader_->SetInt("uToneMapMode", ToShaderToneMapMode(frame.renderSettings.toneMapMode));
    shader_->SetInt("uDebugMode", ToShaderDebugView(frame.debugSettings.view));
    const bool bloomAvailable = frame.renderSettings.bloom.enabled &&
                                bloom.bloom != nullptr;
    shader_->SetBool("uBloomEnabled", bloomAvailable);
    shader_->SetFloat("uBloomIntensity", frame.renderSettings.bloom.intensity);
    shader_->SetFloat("uExposure", frame.renderSettings.exposure);

    shader_->SetMat4("uView", frame.camera.view);
    shader_->SetMat4("uInvViewProj", glm::inverse(frame.camera.viewProj));
    shader_->SetFloat("uNearPlane", frame.camera.near_);
    shader_->SetFloat("uFarPlane", frame.camera.far_);

    auto& cascades = shadow.cascades;
    for (int c = 0; c < 4; ++c) {
        shader_->SetFloat("uCascadeFar[" + std::to_string(c) + "]", cascades[c].farPlane);
    }

    hdrInput->Bind(0);
    gbuffer.albedoMetallic->Bind(1);
    gbuffer.normalRoughness->Bind(2);
    gbuffer.depth->Bind(3);
    if (bloomAvailable) {
        bloom.bloom->Bind(4);
    }

    dummyVAO_->Bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);
    dummyVAO_->Unbind();

    Renderer::EnableCullFace(true);
    Renderer::EnableDepthTest(true);
}

} // namespace HuanGL
