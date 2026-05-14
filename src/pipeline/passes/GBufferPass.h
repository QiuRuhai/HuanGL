#pragma once
#include <memory>
#include "../../renderer/Texture.h"
#include "../../renderer/UniformBuffer.h"

namespace HuanGL {

class Shader;
class Framebuffer;
class Scene;

class GBufferPass {
public:
    void Init(int width, int height);
    void Resize(int width, int height);
    void Render(const Scene& scene, const CameraData& camera);

    std::shared_ptr<Texture> GetAlbedoMetallic()  const;
    std::shared_ptr<Texture> GetNormalRoughness() const;
    std::shared_ptr<Texture> GetDepth()           const;

private:
    std::unique_ptr<Framebuffer> fbo_;
    std::unique_ptr<Shader>      shader_;
    int width_ = 0, height_ = 0;
};

} // namespace HuanGL
