#include "LightingStage.h"
#include "ShadowStage.h"
#include "GBufferStage.h"
#include "../PipelineResources.h"
#include "../../renderer/Shader.h"
#include "../../renderer/Framebuffer.h"
#include "../../renderer/Buffer.h"
#include "../../renderer/Renderer.h"
#include "../../renderer/FrameContext.h"
#include "../../renderer/RenderSceneView.h"
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
static const glm::mat4 kCaptureProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

LightingStage::LightingStage(std::string hdrPath)
    : hdrPath_(std::move(hdrPath)) {}

void LightingStage::CreateHDRFBO(int w, int h) {
    hdrFBO_ = std::make_unique<Framebuffer>(w, h);
    auto hdrTex = Texture::Create2D(w, h, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    hdrFBO_->AttachColor(hdrTex, 0);
    hdrFBO_->AttachDepthRenderbuffer();
}

void LightingStage::Init(int width, int height) {
    width_ = width; height_ = height;
    CreateHDRFBO(width, height);

    pbrShader_ = std::make_unique<Shader>("lighting/fullscreen.vert",
                                          "lighting/pbr_ibl.frag");
    irrShader_ = std::make_unique<Shader>("lighting/cube.vert",
                                          "lighting/irradiance.frag");
    pfShader_ = std::make_unique<Shader>("lighting/cube.vert",
                                         "lighting/prefilter.frag");
    brdfShader_ = std::make_unique<Shader>("lighting/fullscreen.vert",
                                           "lighting/brdf_lut.frag");

    captureFBO_ = std::make_unique<Framebuffer>(512, 512);
    captureFBO_->AttachDepthRenderbuffer();

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
    GenerateIBL(hdrPath_);
}

void LightingStage::Resize(int w, int h) {
    width_ = w; height_ = h;
    CreateHDRFBO(w, h);
}

void LightingStage::GenerateIBL(const std::string& hdrPath) {
    Renderer::EnableDepthTest(false);
    Renderer::EnableCullFace(false);

    cubeVAO_->Bind();

    auto hdrTex = Texture::LoadHDR(hdrPath);

    auto envCubemap = Texture::CreateCubemap(512, GL_RGB16F, true);
    Shader eqShader("lighting/cube.vert",
                    "lighting/equirect_to_cubemap.frag");
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

void LightingStage::Execute(PipelineResources& resources, const FrameContext& frame) {
    const auto& gbuffer = resources.Get<GBufferOutputs>();
    const auto& shadow = resources.Get<ShadowOutputs>();
    const auto& scene = resources.Get<RenderSceneView>();

    hdrFBO_->Bind();
    Renderer::SetViewport(0, 0, width_, height_);
    Renderer::Clear(true, true, false);
    Renderer::EnableDepthTest(false);
    Renderer::EnableCullFace(false);

    pbrShader_->Use();

    gbuffer.albedoMetallic->Bind(0);
    gbuffer.normalRoughness->Bind(1);
    gbuffer.depth->Bind(2);

    glBindTextureUnit(3, shadow.shadowArray);
    auto& cascades = shadow.cascades;
    for (int c = 0; c < 4; ++c) {
        pbrShader_->SetMat4("uCascadeViewProj[" + std::to_string(c) + "]", cascades[c].viewProj);
        pbrShader_->SetFloat("uCascadeFar[" + std::to_string(c) + "]", cascades[c].farPlane);
    }
    auto& light = scene.sunLight;
    pbrShader_->SetVec3("uLightDir", light.direction);
    pbrShader_->SetVec3("uLightColor", light.color * light.intensity);

    irradianceMap_->Bind(4);
    prefilterMap_->Bind(5);
    brdfLUT_->Bind(6);

    pbrShader_->SetMat4("uView", frame.camera.view);
    pbrShader_->SetMat4("uInvViewProj", glm::inverse(frame.camera.viewProj));
    pbrShader_->SetVec3("uCamPos", frame.camera.camPos);
    pbrShader_->SetFloat("uAmbientStrength", frame.renderSettings.ambientStrength);

    dummyVAO_->Bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);
    dummyVAO_->Unbind();

    Framebuffer::BindDefault();
    Renderer::EnableCullFace(true);
    Renderer::EnableDepthTest(true);

    LightingOutputs outputs;
    outputs.hdrColor = hdrFBO_->GetColor(0);
    resources.Set(outputs);
}

} // namespace HuanGL
