#pragma once
#include "../IPipelineStage.h"
#include "../../renderer/Texture.h"
#include <memory>

namespace HuanGL {

class Shader;
class Framebuffer;

struct GBufferOutputs {
    std::shared_ptr<Texture> albedoMetallic;
    std::shared_ptr<Texture> normalRoughness;
    std::shared_ptr<Texture> depth;
};

class GBufferStage : public IPipelineStage {
public:
    const char* GetName() const override { return "GBufferStage"; }
    void Init(int width, int height) override;
    void Resize(int width, int height) override;
    void Execute(PipelineResources& resources, const FrameContext& frame) override;

private:
    std::unique_ptr<Framebuffer> fbo_;
    std::unique_ptr<Shader>      shader_;
    int width_ = 0, height_ = 0;
};

} // namespace HuanGL
