#pragma once
#include <memory>
#include "../../renderer/FrameContext.h"
#include "../PipelineOutputs.h"

namespace HuanGL {

class Shader;
class VertexArray;

class PostProcessPass {
public:
    void Init();
    void Render(const LightingOutputs& lighting,
                const GBufferOutputs& gbuffer,
                const ShadowOutputs& shadow,
                const FrameContext& frame);

private:
    std::unique_ptr<Shader>      shader_;
    std::unique_ptr<VertexArray>  dummyVAO_;
};

} // namespace HuanGL
