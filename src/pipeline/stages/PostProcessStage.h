#pragma once
#include "../IPipelineStage.h"
#include <memory>

namespace HuanGL {

class Shader;
class VertexArray;

class PostProcessStage : public IPipelineStage {
public:
    const char* GetName() const override { return "PostProcessStage"; }
    void Init(int width, int height) override;
    void Resize(int width, int height) override;
    void Execute(PipelineResources& resources, const FrameContext& frame) override;

private:
    std::unique_ptr<Shader>      shader_;
    std::unique_ptr<VertexArray>  dummyVAO_;
};

} // namespace HuanGL
