#pragma once
#include <memory>
#include <string>
// MSVC eagerly instantiates unique_ptr deleters — pass headers need complete types below
#include "../renderer/Shader.h"
#include "../renderer/Framebuffer.h"
#include "../renderer/FrameContext.h"
#include "../renderer/RenderSceneView.h"
#include "../renderer/UniformBuffer.h"
#include "PipelineOutputs.h"
#include "passes/ShadowPass.h"
#include "passes/GBufferPass.h"
#include "passes/LightingPass.h"
#include "passes/PostProcessPass.h"
#include "techniques/BloomTechnique.h"

namespace HuanGL {

class RenderPipeline {
public:
    void Init(int width, int height, const std::string& hdrPath);
    void Resize(int width, int height);
    void InvalidateHistory();
    const PipelineOutputs& Execute(const RenderSceneView& scene,
                                   const FrameContext& frame);
    const PipelineOutputs& GetOutputs() const { return outputs_; }

private:
    void UpdateUniformBuffers(const RenderSceneView& scene, const FrameContext& frame);

    ShadowPass      shadowPass_;
    GBufferPass     gbufferPass_;
    LightingPass    lightingPass_;
    PostProcessPass postProcessPass_;
    BloomTechnique  bloomTechnique_;

    std::unique_ptr<CameraUBO> cameraUBO_;
    std::unique_ptr<LightsUBO> lightsUBO_;
    std::unique_ptr<TimeUBO>   timeUBO_;
    PipelineOutputs outputs_;
};

} // namespace HuanGL
