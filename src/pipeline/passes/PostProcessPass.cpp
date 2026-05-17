#include "PostProcessPass.h"
#include "GBufferPass.h"
#include "ShadowPass.h"
#include "LightingPass.h"
#include "../../renderer/Shader.h"
#include "../../renderer/Renderer.h"
#include "../../renderer/Framebuffer.h"
#include <glm/gtc/matrix_transform.hpp>

namespace HuanGL {

void PostProcessPass::Init() {
    shader_ = std::make_unique<Shader>("../shader/lighting/fullscreen.vert",
                                       "../shader/postprocess/postprocess.frag");
    dummyVAO_ = std::make_unique<VertexArray>();
}

void PostProcessPass::Render(const LightingPass& lighting, const GBufferPass& gbuffer,
                             const ShadowPass& shadow, const CameraData& camera) {
    Framebuffer::BindDefault();
    Renderer::EnableDepthTest(false);
    Renderer::EnableCullFace(false);

    shader_->Use();
    shader_->SetInt("uToneMapMode", toneMapMode_);
    shader_->SetInt("uDebugMode", debugMode_);

    // Camera data for debug views
    shader_->SetMat4("uView", camera.view);
    shader_->SetMat4("uInvViewProj", glm::inverse(camera.viewProj));
    shader_->SetFloat("uNearPlane", camera.near_);
    shader_->SetFloat("uFarPlane", camera.far_);

    // Cascade data for debug view 6
    auto& cascades = shadow.GetCascades();
    for (int c = 0; c < 4; ++c) {
        shader_->SetFloat("uCascadeFar[" + std::to_string(c) + "]", cascades[c].farPlane);
    }

    // Bind textures
    lighting.GetHDROutput()->Bind(0);
    gbuffer.GetAlbedoMetallic()->Bind(1);
    gbuffer.GetNormalRoughness()->Bind(2);
    gbuffer.GetDepth()->Bind(3);

    dummyVAO_->Bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);
    dummyVAO_->Unbind();

    Renderer::EnableCullFace(true);
    Renderer::EnableDepthTest(true);
}

} // namespace HuanGL
