#pragma once
#include "../IPipelineStage.h"
#include "../../renderer/Texture.h"
#include <memory>
#include <string>

namespace HuanGL {

class Shader;
class VertexArray;
class Buffer;
class Framebuffer;

struct LightingOutputs {
    std::shared_ptr<Texture> hdrColor;
};

class LightingStage : public IPipelineStage {
public:
    explicit LightingStage(std::string hdrPath);

    const char* GetName() const override { return "LightingStage"; }
    void Init(int width, int height) override;
    void Resize(int width, int height) override;
    void Execute(PipelineResources& resources, const FrameContext& frame) override;

private:
    void GenerateIBL(const std::string& hdrPath);
    void CreateHDRFBO(int w, int h);

    std::string hdrPath_;
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
