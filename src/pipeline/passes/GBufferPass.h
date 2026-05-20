#pragma once
#include <memory>
#include "../../renderer/Texture.h"
#include "../../renderer/FrameContext.h"
#include "../../renderer/RenderSceneView.h"
#include "../PipelineOutputs.h"

namespace HuanGL {

class Shader;
class Framebuffer;

class GBufferPass {
public:
    void Init(int width, int height);
    void Resize(int width, int height);
    GBufferOutputs Render(const RenderSceneView& scene, const FrameContext& frame);
    GBufferOutputs GetOutputs() const;

    std::shared_ptr<Texture> GetAlbedoMetallic()  const;
    std::shared_ptr<Texture> GetNormalRoughness() const;
    std::shared_ptr<Texture> GetDepth()           const;

private:
    std::unique_ptr<Framebuffer> fbo_;
    std::unique_ptr<Shader>      shader_;
    int width_ = 0, height_ = 0;
};

} // namespace HuanGL
