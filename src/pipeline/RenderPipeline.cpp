#include "RenderPipeline.h"

namespace HuanGL {

void RenderPipeline::Init(int w, int h, const std::string& hdrPath) {
    cameraUBO_ = std::make_unique<CameraUBO>();
    lightsUBO_ = std::make_unique<LightsUBO>();
    timeUBO_   = std::make_unique<TimeUBO>();

    shadowPass_.Init(2048);
    gbufferPass_.Init(w, h);
    lightingPass_.Init(w, h, hdrPath);
    bloomTechnique_.Init(w, h);
    postProcessPass_.Init();
}

void RenderPipeline::Resize(int w, int h) {
    gbufferPass_.Resize(w, h);
    lightingPass_.Resize(w, h);
    bloomTechnique_.Resize(w, h);
}

void RenderPipeline::InvalidateHistory() {
    // History-based techniques will reset their temporal resources here.
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

const PipelineOutputs& RenderPipeline::Execute(const RenderSceneView& scene,
                                               const FrameContext& frame) {
    UpdateUniformBuffers(scene, frame);
    outputs_.shadow = shadowPass_.Render(scene, frame);
    outputs_.gbuffer = gbufferPass_.Render(scene, frame);
    outputs_.lighting = lightingPass_.Render(outputs_.gbuffer, outputs_.shadow,
                                             scene, frame);
    outputs_.bloom = bloomTechnique_.Execute(frame, outputs_,
                                             frame.renderSettings.bloom);
    postProcessPass_.Render(outputs_, frame);
    return outputs_;
}

} // namespace HuanGL
