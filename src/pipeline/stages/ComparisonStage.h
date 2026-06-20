#pragma once
#include "../IPipelineStage.h"
#include "../ComparisonReadout.h"
#include "../../renderer/Buffer.h"
#include "../../renderer/Texture.h"
#include <memory>

namespace HuanGL {

class Shader;

class ComparisonStage : public IPipelineStage {
public:
    const char* GetName() const override { return "ComparisonStage"; }
    void Init(int width, int height) override;
    void Resize(int width, int height) override;
    void Execute(PipelineResources& resources, const FrameContext& frame) override;
    const ComparisonReadout& Readout() const { return readout_; }

private:
    int width_  = 0;
    int height_ = 0;

    std::unique_ptr<Shader>      errorShader_;
    std::unique_ptr<Shader>      compositeShader_;
    std::shared_ptr<Texture>     errorTex_;
    std::unique_ptr<VertexArray> dummyVAO_;

    ComparisonReadout readout_;
};

} // namespace HuanGL
