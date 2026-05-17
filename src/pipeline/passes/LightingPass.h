#pragma once
#include <memory>
#include <string>
#include <glm/glm.hpp>
#include "../../renderer/Texture.h"
#include "../../renderer/UniformBuffer.h"

namespace HuanGL {

class Shader;
class VertexArray;
class Buffer;
class Framebuffer;
class GBufferPass;
class ShadowPass;
class Scene;

class LightingPass {
public:
    void Init(int width, int height, const std::string& hdrPath);
    void Resize(int width, int height);
    void Render(const GBufferPass& gbuffer, const ShadowPass& shadow,
                const Scene& scene, const CameraData& camera);

    std::shared_ptr<Texture> GetHDROutput() const;

private:
    void GenerateIBL(const std::string& hdrPath);
    void CreateHDRFBO(int w, int h);

    std::unique_ptr<Shader> pbrShader_;
    std::unique_ptr<Shader> irrShader_;
    std::unique_ptr<Shader> pfShader_;
    std::unique_ptr<Shader> brdfShader_;
    std::shared_ptr<Texture> irradianceMap_;
    std::shared_ptr<Texture> prefilterMap_;
    std::shared_ptr<Texture> brdfLUT_;
    std::unique_ptr<Framebuffer> captureFBO_;
    std::unique_ptr<Framebuffer> hdrFBO_;
    std::unique_ptr<VertexArray> cubeVAO_;
    std::unique_ptr<Buffer>      cubeVBO_;
    std::unique_ptr<VertexArray> dummyVAO_;
    int width_ = 0, height_ = 0;
};

} // namespace HuanGL
