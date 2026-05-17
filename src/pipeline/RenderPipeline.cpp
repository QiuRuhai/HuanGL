#include "RenderPipeline.h"
#include "../scene/Scene.h"

namespace HuanGL {

void RenderPipeline::Init(int w, int h, const std::string& hdrPath) {
    shadowPass_.Init(2048);
    gbufferPass_.Init(w, h);
    lightingPass_.Init(w, h, hdrPath);
    postProcessPass_.Init();
}

void RenderPipeline::Resize(int w, int h) {
    gbufferPass_.Resize(w, h);
    lightingPass_.Resize(w, h);
}

void RenderPipeline::Execute(const Scene& scene, const CameraData& camera) {
    shadowPass_.Render(scene, camera, scene.GetSunLight());
    gbufferPass_.Render(scene, camera);
    lightingPass_.Render(gbufferPass_, shadowPass_, scene, camera);
    postProcessPass_.Render(lightingPass_, gbufferPass_, shadowPass_, camera);
}

} // namespace HuanGL
