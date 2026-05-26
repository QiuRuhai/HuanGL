#include "RenderPipeline.h"
#include "stages/ShadowStage.h"
#include "stages/GBufferStage.h"
#include "stages/LightingStage.h"
#include "stages/BloomStage.h"
#include "stages/PostProcessStage.h"
#include "../renderer/Renderer.h"

namespace HuanGL {

void RenderPipeline::BuildStages(const std::string& hdrPath) {
    stages_.push_back(std::make_unique<ShadowStage>(2048));
    stages_.push_back(std::make_unique<GBufferStage>());
    stages_.push_back(std::make_unique<LightingStage>(hdrPath));
    stages_.push_back(std::make_unique<BloomStage>());
    stages_.push_back(std::make_unique<PostProcessStage>());
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

    for (auto& stage : stages_) {
        Renderer::PushDebugGroup(stage->GetName());
        stage->Execute(resources_, frame);
        Renderer::PopDebugGroup();
    }
}

} // namespace HuanGL
