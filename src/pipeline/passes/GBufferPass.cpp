#include "GBufferPass.h"
#include "../../renderer/Shader.h"
#include "../../renderer/Framebuffer.h"
#include "../../renderer/Renderer.h"
#include "../../renderer/Schema.h"
#include <stdexcept>
#include "../../scene/Scene.h"
#include <glm/gtc/type_ptr.hpp>

namespace HuanGL {

void GBufferPass::Init(int w, int h) {
    width_ = w; height_ = h;
    shader_ = std::make_unique<Shader>("../shader/gbuffer/gbuffer.vert",
                                       "../shader/gbuffer/gbuffer.frag");
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
        throw std::runtime_error("[GBufferPass] FBO incomplete");
}

void GBufferPass::Resize(int w, int h) { Init(w, h); }

void GBufferPass::Render(const Scene& scene, const CameraData& camera) {
    fbo_->Bind();
    Renderer::SetViewport(0, 0, width_, height_);
    Renderer::Clear(true, true, false);
    Renderer::EnableDepthTest(true);
    Renderer::EnableDepthWrite(true);
    Renderer::SetDepthFunc(GL_LESS);

    shader_->Use();
    shader_->SetMat4("viewProj", camera.viewProj);

    for (size_t i = 0; i < scene.GetMeshCount(); ++i) {
        Mesh* mesh = scene.GetMesh(i);
        glm::mat4 model = scene.GetModelMatrix(i);
        shader_->SetMat4("model", model);

        mesh->vao->Bind();
        for (auto& sub : mesh->subMeshes) {
            const Material& mat = scene.GetMaterials()[sub.materialIndex];

            shader_->SetInt("uHasAlbedoTex",    mat.albedoMap    ? 1 : 0);
            shader_->SetInt("uHasRoughnessTex", mat.roughnessMap ? 1 : 0);
            shader_->SetInt("uHasMetallicTex",  mat.metallicMap  ? 1 : 0);
            shader_->SetVec4("uBaseColor",  mat.baseColorFactor);
            shader_->SetFloat("uRoughness", mat.roughnessFactor);
            shader_->SetFloat("uMetallic",  mat.metallicFactor);

            if (mat.albedoMap)    mat.albedoMap->Bind(0);
            if (mat.roughnessMap) mat.roughnessMap->Bind(1);
            if (mat.metallicMap)  mat.metallicMap->Bind(2);

            glDrawElements(GL_TRIANGLES, sub.indexCount, GL_UNSIGNED_INT,
                           (void*)(uintptr_t)(sub.indexOffset * sizeof(uint32_t)));
        }
        mesh->vao->Unbind();
    }
    Framebuffer::BindDefault();
}

std::shared_ptr<Texture> GBufferPass::GetAlbedoMetallic()  const { return fbo_->GetColor(0); }
std::shared_ptr<Texture> GBufferPass::GetNormalRoughness() const { return fbo_->GetColor(1); }
std::shared_ptr<Texture> GBufferPass::GetDepth()           const { return fbo_->GetDepth(); }

} // namespace HuanGL
