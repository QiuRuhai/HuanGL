#pragma once
#include <memory>
#include <array>
#include <glm/glm.hpp>
#include "../../renderer/Schema.h"
#include "../../renderer/UniformBuffer.h"

namespace HuanGL {

class Shader;
class Framebuffer;
class Scene;

struct CascadeData {
    glm::mat4 viewProj;
    float     farPlane = 0.f;
    float     pad[3]   = {};
};

class ShadowPass {
public:
    void Init(int resolution = 2048);
    ~ShadowPass();
    void Render(const Scene& scene, const CameraData& camera,
                const DirectionalLight& light);

    GLuint GetShadowMapArray() const { return shadowArrayID_; }
    const std::array<CascadeData, 4>& GetCascades() const { return cascades_; }

private:
    std::unique_ptr<Shader>      shader_;
    std::unique_ptr<Framebuffer> fbo_;
    GLuint                       shadowArrayID_ = 0;
    std::array<CascadeData, 4>   cascades_;
    int resolution_ = 2048;
};

} // namespace HuanGL
