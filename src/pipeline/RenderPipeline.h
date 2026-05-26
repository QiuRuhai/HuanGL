#pragma once
#include "IPipelineStage.h"
#include "PipelineResources.h"
#include "../renderer/FrameContext.h"
#include "../renderer/RenderSceneView.h"
#include "../renderer/UniformBuffer.h"
#include <memory>
#include <string>
#include <vector>

namespace HuanGL {

class RenderPipeline {
public:
    void Init(int width, int height, const std::string& hdrPath);
    void Resize(int width, int height);
    void InvalidateHistory();
    void Execute(const RenderSceneView& scene, const FrameContext& frame);

private:
    void BuildStages(const std::string& hdrPath);
    void UpdateUniformBuffers(const RenderSceneView& scene, const FrameContext& frame);

    std::vector<std::unique_ptr<IPipelineStage>> stages_;
    PipelineResources resources_;

    std::unique_ptr<CameraUBO> cameraUBO_;
    std::unique_ptr<LightsUBO> lightsUBO_;
    std::unique_ptr<TimeUBO>   timeUBO_;
};

} // namespace HuanGL
