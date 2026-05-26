#include "GBufferStage.h"
#include "../PipelineResources.h"
#include "../../renderer/Shader.h"
#include "../../renderer/Framebuffer.h"
#include "../../renderer/Renderer.h"
#include "../../renderer/Schema.h"
#include "../../renderer/FrameContext.h"
#include "../../renderer/RenderSceneView.h"
#include <stdexcept>

namespace HuanGL {

void GBufferStage::Init(int w, int h) {
    width_ = w; height_ = h;
    shader_ = std::make_unique<Shader>("gbuffer/gbuffer.vert",
                                       "gbuffer/gbuffer.frag");
    fbo_ = std::make_unique<Framebuffer>(w, h);

    auto rt0 = Texture::Create2D(w, h, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
    auto rt1 = Texture::Create2D(w, h, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    auto dtx = Texture::Create2D(w, h, GL_DEPTH_COMPONENT24,
                                 GL_DEPTH_COMPONENT, GL_FLOAT);
    fbo_->AttachColor(rt0, 0);
    fbo_->AttachColor(rt1, 1);
    fbo_->AttachDepth(dtx);
    fbo_->SetDrawBuffers({GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1});
    if (!fbo_->IsComplete())
        throw std::runtime_error("[GBufferStage] FBO incomplete");
}

void GBufferStage::Resize(int w, int h) { Init(w, h); }

void GBufferStage::Execute(PipelineResources& resources, const FrameContext& frame) {
    const auto& scene = resources.Get<RenderSceneView>();

    fbo_->Bind();
    Renderer::SetViewport(0, 0, width_, height_);
    Renderer::Clear(true, true, false);
    Renderer::EnableDepthTest(true);
    Renderer::EnableDepthWrite(true);
    Renderer::SetDepthFunc(GL_LESS);

    shader_->Use();
    shader_->SetMat4("viewProj", frame.camera.viewProj);

    for (const auto& renderable : scene.renderables) {
        if (!renderable.mesh || !renderable.materials) continue;

        const Mesh* mesh = renderable.mesh;
        shader_->SetMat4("model", renderable.modelMatrix);

        mesh->vao->Bind();
        for (const auto& sub : mesh->subMeshes) {
            const Material& mat = (*renderable.materials)[sub.materialIndex];

            shader_->SetInt("uHasAlbedoTex",    mat.albedoMap    ? 1 : 0);
            shader_->SetInt("uHasRoughnessTex", mat.roughnessMap ? 1 : 0);
            shader_->SetInt("uHasMetallicTex",  mat.metallicMap  ? 1 : 0);
            shader_->SetInt("uHasNormalTex",    mat.normalMap    ? 1 : 0);
            shader_->SetInt("uPackedMetallicRoughness", mat.packedMetallicRoughness ? 1 : 0);
            shader_->SetVec4("uBaseColor",  mat.baseColorFactor);
            shader_->SetFloat("uRoughness", mat.roughnessFactor);
            shader_->SetFloat("uMetallic",  mat.metallicFactor);

            if (mat.albedoMap)    mat.albedoMap->Bind(0);
            if (mat.roughnessMap) mat.roughnessMap->Bind(1);
            if (mat.metallicMap)  mat.metallicMap->Bind(2);
            if (mat.normalMap)    mat.normalMap->Bind(3);

            glDrawElements(GL_TRIANGLES, sub.indexCount, GL_UNSIGNED_INT,
                           (void*)(uintptr_t)(sub.indexOffset * sizeof(uint32_t)));
        }
        mesh->vao->Unbind();
    }
    Framebuffer::BindDefault();

    GBufferOutputs outputs;
    outputs.albedoMetallic = fbo_->GetColor(0);
    outputs.normalRoughness = fbo_->GetColor(1);
    outputs.depth = fbo_->GetDepth();
    resources.Set(outputs);
}

} // namespace HuanGL
