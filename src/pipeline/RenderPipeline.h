#pragma once
#include "IPipelineStage.h"
#include "PipelineResources.h"
#include "ComparisonReadout.h"
#include "../renderer/FrameContext.h"
#include "../renderer/RenderSceneView.h"
#include "../renderer/UniformBuffer.h"
#include "../renderer/GpuProfiler.h"
#include <memory>
#include <string>
#include <vector>

namespace HuanGL {

class ComparisonStage;

class RenderPipeline {
public:
    void Init(int width, int height, const std::string& hdrPath);
    void Resize(int width, int height);
    void InvalidateHistory();
    void Execute(const RenderSceneView& scene, const FrameContext& frame);

    // Per-stage GPU timings from the most recently resolved frame.
    const std::vector<StageTiming>& GetStageTimings() const {
        return profiler_.GetResults();
    }

    // Comparison metrics (valid only when path tracer is enabled).
    const ComparisonReadout& GetComparisonReadout() const;

private:
    void BuildStages(const std::string& hdrPath);
    void UpdateUniformBuffers(const RenderSceneView& scene, const FrameContext& frame);

    std::vector<std::unique_ptr<IPipelineStage>> stages_;
    PipelineResources resources_;

    std::unique_ptr<CameraUBO> cameraUBO_;
    std::unique_ptr<LightsUBO> lightsUBO_;
    std::unique_ptr<TimeUBO>   timeUBO_;

    GpuProfiler profiler_;

    // Raw (non-owning) pointer to the ComparisonStage inside stages_.
    // Set during BuildStages; valid for the lifetime of this pipeline.
    ComparisonStage* comparisonStage_ = nullptr;
};

} // namespace HuanGL
