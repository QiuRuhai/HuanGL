#pragma once
#include "../../renderer/Buffer.h"
#include "../../renderer/Framebuffer.h"
#include "../../renderer/FrameContext.h"
#include "../../renderer/Shader.h"
#include "../../renderer/Texture.h"
#include "../PipelineOutputs.h"
#include <memory>

namespace HuanGL {

class BloomTechnique {
public:
    void Init(int width, int height);
    void Resize(int width, int height);

    BloomOutputs Execute(const FrameContext& frame,
                         const PipelineOutputs& inputs,
                         const BloomSettings& settings);

    BloomOutputs GetOutputs() const { return outputs_; }

private:
    void CreateResources(int width, int height);
    void DrawFullscreen() const;

    int width_ = 0;
    int height_ = 0;

    std::unique_ptr<Shader> extractShader_;
    std::unique_ptr<Shader> blurShader_;
    std::unique_ptr<VertexArray> dummyVAO_;

    std::unique_ptr<Framebuffer> brightFBO_;
    std::unique_ptr<Framebuffer> pingFBO_;
    std::unique_ptr<Framebuffer> pongFBO_;

    std::shared_ptr<Texture> brightTexture_;
    std::shared_ptr<Texture> pingTexture_;
    std::shared_ptr<Texture> pongTexture_;

    BloomOutputs outputs_;
};

} // namespace HuanGL
