#pragma once
#include <string>
// MSVC eagerly instantiates unique_ptr deleters — pass headers need complete types below
#include "../renderer/Shader.h"
#include "../renderer/Framebuffer.h"
#include "passes/ShadowPass.h"
#include "passes/GBufferPass.h"
#include "passes/LightingPass.h"
#include "passes/PostProcessPass.h"
#include "../renderer/UniformBuffer.h"

namespace HuanGL {

class Scene;

class RenderPipeline {
public:
    void Init(int width, int height, const std::string& hdrPath);
    void Resize(int width, int height);
    void Execute(const Scene& scene, const CameraData& camera);

    PostProcessPass& GetPostProcess() { return postProcessPass_; }

private:
    ShadowPass      shadowPass_;
    GBufferPass     gbufferPass_;
    LightingPass    lightingPass_;
    PostProcessPass postProcessPass_;
};

} // namespace HuanGL
