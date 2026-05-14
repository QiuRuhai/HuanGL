#include "ShadowPass.h"
#include "../../renderer/Shader.h"
#include "../../renderer/Framebuffer.h"
#include "../../renderer/Renderer.h"
#include "../../scene/Scene.h"
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>

namespace HuanGL {

static std::array<float, 4> ComputeCascadeSplits(float nearP, float farP, float lambda = 0.75f) {
    std::array<float, 4> splits;
    for (int i = 0; i < 4; ++i) {
        float p = (i + 1) / 4.f;
        float logSplit  = nearP * pow(farP / nearP, p);
        float linSplit  = nearP + (farP - nearP) * p;
        splits[i] = glm::mix(linSplit, logSplit, lambda);
    }
    return splits;
}

void ShadowPass::Init(int resolution) {
    resolution_ = resolution;
    shader_ = std::make_unique<Shader>("../shader/shadow/csm.vert",
                                       "../shader/shadow/csm.frag");

    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &shadowArrayID_);
    glTextureStorage3D(shadowArrayID_, 1, GL_DEPTH_COMPONENT24,
                       resolution, resolution, 4);
    glTextureParameteri(shadowArrayID_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(shadowArrayID_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(shadowArrayID_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTextureParameteri(shadowArrayID_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border[] = {1,1,1,1};
    glTextureParameterfv(shadowArrayID_, GL_TEXTURE_BORDER_COLOR, border);
    // Required for sampler2DArrayShadow + texture(...) PCF sampling.
    glTextureParameteri(shadowArrayID_, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTextureParameteri(shadowArrayID_, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    fbo_ = std::make_unique<Framebuffer>(resolution, resolution);
    glNamedFramebufferTexture(fbo_->GetID(), GL_DEPTH_ATTACHMENT,
                              shadowArrayID_, 0);
    glNamedFramebufferDrawBuffer(fbo_->GetID(), GL_NONE);
    glNamedFramebufferReadBuffer(fbo_->GetID(), GL_NONE);
    if (!fbo_->IsComplete())
        throw std::runtime_error("[ShadowPass] FBO incomplete");
}

ShadowPass::~ShadowPass() {
    if (shadowArrayID_) glDeleteTextures(1, &shadowArrayID_);
}

static glm::mat4 LightViewProj(const DirectionalLight& light,
                                const std::array<glm::vec3, 8>& frustumCorners) {
    glm::vec3 center(0);
    for (auto& c : frustumCorners) center += c;
    center /= (float)frustumCorners.size();

    glm::vec3 lightPos = center - light.direction * 50.f;
    glm::mat4 lightView = glm::lookAt(lightPos, center, {0, 1, 0});

    glm::vec3 mn(1e9f), mx(-1e9f);
    for (auto& c : frustumCorners) {
        glm::vec3 ls = glm::vec3(lightView * glm::vec4(c, 1));
        mn = glm::min(mn, ls); mx = glm::max(mx, ls);
    }
    mn.z -= 50.f; mx.z += 50.f;

    glm::mat4 lightProj = glm::ortho(mn.x, mx.x, mn.y, mx.y, mn.z, mx.z);
    return lightProj * lightView;
}

// Returns 8 world-space corners of the camera sub-frustum defined by
// [nearRatio, farRatio] along the near→far axis (0 = near plane, 1 = far plane).
// Linear interpolation along each corner ray yields a constant view-Z slice.
static std::array<glm::vec3, 8> SubFrustumCorners(const glm::mat4& invViewProj,
                                                   float nearRatio, float farRatio) {
    std::array<glm::vec3, 8> full;
    int idx = 0;
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                glm::vec4 ndc(x * 2.f - 1.f, y * 2.f - 1.f,
                              z == 0 ? -1.f : 1.f, 1.f);
                glm::vec4 world = invViewProj * ndc;
                full[idx++] = glm::vec3(world) / world.w;
            }
        }
    }
    std::array<glm::vec3, 8> sub;
    for (int i = 0; i < 4; ++i) {
        sub[i]     = glm::mix(full[i], full[i + 4], nearRatio);
        sub[i + 4] = glm::mix(full[i], full[i + 4], farRatio);
    }
    return sub;
}

void ShadowPass::Render(const Scene& scene, const CameraData& camera,
                         const DirectionalLight& light) {
    auto splits = ComputeCascadeSplits(camera.near_, camera.far_);
    auto invVP  = glm::inverse(camera.viewProj);

    Renderer::EnableCullFace(true);
    Renderer::EnableDepthTest(true);
    Renderer::EnableDepthWrite(true);
    // Polygon offset handles peter-panning without breaking open meshes
    // (front-face culling drops the floor plane entirely).
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 4.0f);

    shader_->Use();

    float prevSplit = camera.near_;
    float range = camera.far_ - camera.near_;
    for (int c = 0; c < 4; ++c) {
        float nearRatio = (prevSplit - camera.near_) / range;
        float farRatio  = (splits[c]  - camera.near_) / range;
        auto corners = SubFrustumCorners(invVP, nearRatio, farRatio);
        glm::mat4 lightVP = LightViewProj(light, corners);

        cascades_[c].viewProj = lightVP;
        cascades_[c].farPlane = splits[c];

        glNamedFramebufferTextureLayer(fbo_->GetID(), GL_DEPTH_ATTACHMENT,
                                       shadowArrayID_, 0, c);
        fbo_->Bind();
        Renderer::SetViewport(0, 0, resolution_, resolution_);
        Renderer::Clear(false, true, false);

        shader_->SetMat4("lightViewProj", lightVP);

        for (size_t i = 0; i < scene.GetMeshCount(); ++i) {
            shader_->SetMat4("model", scene.GetModelMatrix(i));
            Mesh* mesh = scene.GetMesh(i);
            mesh->vao->Bind();
            for (auto& sub : mesh->subMeshes)
                glDrawElements(GL_TRIANGLES, sub.indexCount, GL_UNSIGNED_INT,
                               (void*)(uintptr_t)(sub.indexOffset * sizeof(uint32_t)));
            mesh->vao->Unbind();
        }
        prevSplit = splits[c];
    }

    glDisable(GL_POLYGON_OFFSET_FILL);
    Framebuffer::BindDefault();
}

} // namespace HuanGL
