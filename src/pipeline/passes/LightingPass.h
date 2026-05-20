#pragma once
#include <memory>
#include <string>
#include <glm/glm.hpp>
#include "../../renderer/Texture.h"
#include "../../renderer/FrameContext.h"
#include "../../renderer/RenderSceneView.h"
#include "../PipelineOutputs.h"

namespace HuanGL {

class Shader;
class VertexArray;
class Buffer;
class Framebuffer;

class LightingPass {
public:
    void Init(int width, int height, const std::string& hdrPath);
    void Resize(int width, int height);
    LightingOutputs Render(const GBufferOutputs& gbuffer,
                           const ShadowOutputs& shadow,
                           const RenderSceneView& scene,
                           const FrameContext& frame);
    LightingOutputs GetOutputs() const;

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
