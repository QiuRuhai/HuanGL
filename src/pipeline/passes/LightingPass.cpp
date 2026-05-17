#include "LightingPass.h"
#include "GBufferPass.h"
#include "ShadowPass.h"
#include "../../renderer/Shader.h"
#include "../../renderer/Framebuffer.h"
#include "../../renderer/Buffer.h"
#include "../../renderer/Renderer.h"
#include "../../scene/Scene.h"
#include <glm/gtc/matrix_transform.hpp>

namespace HuanGL {

static const glm::mat4 kCaptureViews[6] = {
    glm::lookAt(glm::vec3(0,0,0), glm::vec3( 1, 0, 0), glm::vec3(0,-1, 0)),
    glm::lookAt(glm::vec3(0,0,0), glm::vec3(-1, 0, 0), glm::vec3(0,-1, 0)),
    glm::lookAt(glm::vec3(0,0,0), glm::vec3( 0, 1, 0), glm::vec3(0, 0, 1)),
    glm::lookAt(glm::vec3(0,0,0), glm::vec3( 0,-1, 0), glm::vec3(0, 0,-1)),
    glm::lookAt(glm::vec3(0,0,0), glm::vec3( 0, 0, 1), glm::vec3(0,-1, 0)),
    glm::lookAt(glm::vec3(0,0,0), glm::vec3( 0, 0,-1), glm::vec3(0,-1, 0)),
};
// Cube faces are at +/-1, so a far plane of 10 is plenty.
// Shader writes gl_Position.xyww anyway, pushing depth to far — projection range
// only needs to avoid clipping the unit cube.
static const glm::mat4 kCaptureProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

void LightingPass::CreateHDRFBO(int w, int h) {
    hdrFBO_ = std::make_unique<Framebuffer>(w, h);
    auto hdrTex = Texture::Create2D(w, h, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    hdrFBO_->AttachColor(hdrTex, 0);
    hdrFBO_->AttachDepthRenderbuffer();
}

void LightingPass::Resize(int w, int h) {
    width_ = w; height_ = h;
    CreateHDRFBO(w, h);
}

std::shared_ptr<Texture> LightingPass::GetHDROutput() const {
    return hdrFBO_->GetColor(0);
}

void LightingPass::Init(int width, int height, const std::string& hdrPath) {
    width_ = width; height_ = height;
    CreateHDRFBO(width, height);

    pbrShader_ = std::make_unique<Shader>("../shader/lighting/fullscreen.vert",
                                          "../shader/lighting/pbr_ibl.frag");
    irrShader_ = std::make_unique<Shader>("../shader/lighting/cube.vert",
                                          "../shader/lighting/irradiance.frag");
    pfShader_ = std::make_unique<Shader>("../shader/lighting/cube.vert",
                                         "../shader/lighting/prefilter.frag");
    brdfShader_ = std::make_unique<Shader>("../shader/lighting/fullscreen.vert",
                                           "../shader/lighting/brdf_lut.frag");

    captureFBO_ = std::make_unique<Framebuffer>(512, 512);
    captureFBO_->AttachDepthRenderbuffer();

    // Unit cube for cubemap rendering
    static const float kCube[108] = {
        -1,-1,-1, -1,-1, 1, -1, 1, 1, -1,-1,-1, -1, 1, 1, -1, 1,-1,
         1,-1, 1,  1,-1,-1,  1, 1,-1,  1,-1, 1,  1, 1,-1,  1, 1, 1,
        -1,-1, 1,  1,-1, 1,  1, 1, 1, -1,-1, 1,  1, 1, 1, -1, 1, 1,
         1,-1,-1, -1,-1,-1, -1, 1,-1,  1,-1,-1, -1, 1,-1,  1, 1,-1,
        -1, 1, 1,  1, 1, 1,  1, 1,-1, -1, 1, 1,  1, 1,-1, -1, 1,-1,
        -1,-1,-1,  1,-1,-1,  1,-1, 1, -1,-1,-1,  1,-1, 1, -1,-1, 1,
    };
    cubeVAO_ = std::make_unique<VertexArray>();
    cubeVBO_ = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
    cubeVBO_->Upload(kCube, sizeof(kCube));
    cubeVAO_->Bind();
    cubeVBO_->Bind();
    cubeVAO_->BindVertexBuffer(0, cubeVBO_->GetID(), 3 * sizeof(float), 0);
    cubeVAO_->AddAttribute(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);
    cubeVAO_->Unbind();

    dummyVAO_ = std::make_unique<VertexArray>();
    GenerateIBL(hdrPath);
}

void LightingPass::GenerateIBL(const std::string& hdrPath) {
    Renderer::EnableDepthTest(false);
    Renderer::EnableCullFace(false);

    cubeVAO_->Bind();

    // Load HDR
    auto hdrTex = Texture::LoadHDR(hdrPath);

    // Equirect to cubemap (512^2)
    auto envCubemap = Texture::CreateCubemap(512, GL_RGB16F, true);
    Shader eqShader("../shader/lighting/cube.vert",
                    "../shader/lighting/equirect_to_cubemap.frag");
    eqShader.Use();
    hdrTex->Bind(0);

    for (int i = 0; i < 6; ++i) {
        eqShader.SetMat4("uViewProj", kCaptureProj * kCaptureViews[i]);
        glNamedFramebufferTextureLayer(captureFBO_->GetID(), GL_COLOR_ATTACHMENT0,
                                       envCubemap->GetID(), 0, i);
        captureFBO_->Bind();
        Renderer::SetViewport(0, 0, 512, 512);
        Renderer::Clear(true, false, false);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glGenerateTextureMipmap(envCubemap->GetID());

    // Irradiance map (32^2 cubemap)
    irradianceMap_ = Texture::CreateCubemap(32, GL_RGB16F, false);
    irrShader_->Use();
    envCubemap->Bind(0);

    for (int i = 0; i < 6; ++i) {
        irrShader_->SetMat4("uViewProj", kCaptureProj * kCaptureViews[i]);
        glNamedFramebufferTextureLayer(captureFBO_->GetID(), GL_COLOR_ATTACHMENT0,
                                       irradianceMap_->GetID(), 0, i);
        captureFBO_->Bind();
        Renderer::SetViewport(0, 0, 32, 32);
        Renderer::Clear(true, false, false);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // Prefilter map (128^2 cubemap, 5 mips)
    prefilterMap_ = Texture::CreateCubemap(128, GL_RGB16F, true);
    glGenerateTextureMipmap(prefilterMap_->GetID());
    pfShader_->Use();
    envCubemap->Bind(0);

    const int maxMips = 5;
    for (int mip = 0; mip < maxMips; ++mip) {
        int mipSize = 128 >> mip;
        auto pfFBO = std::make_unique<Framebuffer>(mipSize, mipSize);
        pfFBO->AttachDepthRenderbuffer();

        float roughness = (float)mip / (float)(maxMips - 1);
        pfShader_->SetFloat("uRoughness", roughness);

        for (int i = 0; i < 6; ++i) {
            pfShader_->SetMat4("uViewProj", kCaptureProj * kCaptureViews[i]);
            glNamedFramebufferTextureLayer(pfFBO->GetID(), GL_COLOR_ATTACHMENT0,
                                           prefilterMap_->GetID(), mip, i);
            pfFBO->Bind();
            Renderer::SetViewport(0, 0, mipSize, mipSize);
            Renderer::Clear(true, false, false);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
    }

    // BRDF LUT (512^2, RG16F)
    brdfLUT_ = Texture::Create2D(512, 512, GL_RG16F, GL_RG, GL_FLOAT);
    auto brdfFBO = std::make_unique<Framebuffer>(512, 512);
    brdfFBO->AttachColor(brdfLUT_, 0);
    brdfFBO->AttachDepthRenderbuffer();
    brdfShader_->Use();
    brdfFBO->Bind();
    Renderer::SetViewport(0, 0, 512, 512);
    Renderer::Clear(true, false, false);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    cubeVAO_->Unbind();
    Framebuffer::BindDefault();
    Renderer::EnableCullFace(true);
    Renderer::EnableDepthTest(true);
}

void LightingPass::Render(const GBufferPass& gbuffer, const ShadowPass& shadow,
                           const Scene& scene, const CameraData& camera) {
    hdrFBO_->Bind();
    Renderer::SetViewport(0, 0, width_, height_);
    Renderer::Clear(true, true, false);
    Renderer::EnableDepthTest(false);
    Renderer::EnableCullFace(false);

    pbrShader_->Use();

    // GBuffer inputs
    gbuffer.GetAlbedoMetallic()->Bind(0);
    gbuffer.GetNormalRoughness()->Bind(1);
    gbuffer.GetDepth()->Bind(2);

    // Shadow
    glBindTextureUnit(3, shadow.GetShadowMapArray());
    auto& cascades = shadow.GetCascades();
    for (int c = 0; c < 4; ++c) {
        pbrShader_->SetMat4("uCascadeViewProj[" + std::to_string(c) + "]", cascades[c].viewProj);
        pbrShader_->SetFloat("uCascadeFar[" + std::to_string(c) + "]", cascades[c].farPlane);
    }
    auto& light = scene.GetSunLight();
    pbrShader_->SetVec3("uLightDir", light.direction);
    pbrShader_->SetVec3("uLightColor", light.color * light.intensity);

    // IBL
    irradianceMap_->Bind(4);
    prefilterMap_->Bind(5);
    brdfLUT_->Bind(6);

    // Camera
    pbrShader_->SetMat4("uView", camera.view);
    pbrShader_->SetMat4("uInvViewProj", glm::inverse(camera.viewProj));
    pbrShader_->SetVec3("uCamPos", camera.camPos);
    pbrShader_->SetFloat("uAmbientStrength", 1.0f);

    dummyVAO_->Bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);
    dummyVAO_->Unbind();

    Framebuffer::BindDefault();
    Renderer::EnableCullFace(true);
    Renderer::EnableDepthTest(true);
}

} // namespace HuanGL
