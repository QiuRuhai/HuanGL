#pragma once
#include <memory>
#include <string>
#include "passes/ShadowPass.h"
#include "passes/GBufferPass.h"
#include "passes/LightingPass.h"
#include "../renderer/UniformBuffer.h"

namespace HuanGL {

class Scene;

class RenderPipeline {
public:
    void Init(int width, int height, const std::string& hdrPath);
    void Resize(int width, int height);
    void Execute(const Scene& scene, const CameraData& camera);

private:
    ShadowPass   shadowPass_;
    GBufferPass  gbufferPass_;
    LightingPass lightingPass_;
};

} // namespace HuanGL
