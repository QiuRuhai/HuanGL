#include "RenderPipeline.h"
#include "stages/ShadowStage.h"
#include "stages/GBufferStage.h"
#include "stages/LightingStage.h"
#include "stages/TAAStage.h"
#include "stages/BloomStage.h"
#include "stages/PostProcessStage.h"
#include "stages/PathTracerStage.h"
#include "stages/ComparisonStage.h"
#include "../renderer/Renderer.h"

namespace HuanGL {

void RenderPipeline::BuildStages(const std::string& hdrPath) {
    stages_.push_back(std::make_unique<ShadowStage>(2048));
    stages_.push_back(std::make_unique<GBufferStage>());
    stages_.push_back(std::make_unique<LightingStage>(hdrPath));
    stages_.push_back(std::make_unique<BloomStage>());
    stages_.push_back(std::make_unique<TAAStage>());
    stages_.push_back(std::make_unique<PostProcessStage>());
    stages_.push_back(std::make_unique<PathTracerStage>(hdrPath));

    // ComparisonStage must run after PathTracerStage so ReferenceOutputs are ready.
    auto compStage = std::make_unique<ComparisonStage>();
    comparisonStage_ = compStage.get(); // non-owning raw pointer for GetComparisonReadout
    stages_.push_back(std::move(compStage));
}

const ComparisonReadout& RenderPipeline::GetComparisonReadout() const {
    return comparisonStage_->Readout();
}

void RenderPipeline::Init(int w, int h, const std::string& hdrPath) {
    cameraUBO_ = std::make_unique<CameraUBO>();
    lightsUBO_ = std::make_unique<LightsUBO>();
    timeUBO_   = std::make_unique<TimeUBO>();

    BuildStages(hdrPath);
    for (auto& stage : stages_)
        stage->Init(w, h);
}

void RenderPipeline::Resize(int w, int h) {
    for (auto& stage : stages_)
        stage->Resize(w, h);
}

void RenderPipeline::InvalidateHistory() {
    for (auto& stage : stages_)
        stage->InvalidateHistory();
}

void RenderPipeline::UpdateUniformBuffers(const RenderSceneView& scene,
                                          const FrameContext& frame) {
    cameraUBO_->Update(frame.camera);

    LightsData lightData;
    lightData.dirLightDir       = scene.sunLight.direction;
    lightData.dirLightColor     = scene.sunLight.color;
    lightData.dirLightIntensity = scene.sunLight.intensity;
    lightsUBO_->Update(lightData);

    TimeData timeData;
    timeData.time      = frame.time;
    timeData.deltaTime = frame.deltaTime;
    timeUBO_->Update(timeData);
}

void RenderPipeline::Execute(const RenderSceneView& scene,
                              const FrameContext& frame) {
    resources_.Clear();
    resources_.Set(scene);
    UpdateUniformBuffers(scene, frame);

    profiler_.BeginFrame();
    for (auto& stage : stages_) {
        Renderer::PushDebugGroup(stage->GetName());
        profiler_.BeginStage(stage->GetName());
        stage->Execute(resources_, frame);
        profiler_.EndStage();
        Renderer::PopDebugGroup();
    }
    profiler_.EndFrame();
}

} // namespace HuanGL
