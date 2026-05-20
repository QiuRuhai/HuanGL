#pragma once
#include "../../renderer/Buffer.h"
#include "../../renderer/Framebuffer.h"
#include "../../renderer/FrameContext.h"
#include "../../renderer/Shader.h"
#include "../../renderer/Texture.h"
#include "../PipelineOutputs.h"
#include <memory>
#include <vector>

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
    struct BloomMip {
        int width = 1;
        int height = 1;
        std::shared_ptr<Texture> texture;
        std::unique_ptr<Framebuffer> fbo;
    };

    void CreateResources(int width, int height);
    BloomMip CreateMip(int width, int height, const char* label) const;
    void DrawFullscreen() const;
    void RestoreFrameState(const FrameContext& frame) const;

    std::unique_ptr<Shader> extractShader_;
    std::unique_ptr<Shader> downsampleShader_;
    std::unique_ptr<Shader> upsampleShader_;
    std::unique_ptr<VertexArray> dummyVAO_;

    std::vector<BloomMip> downMips_;
    std::vector<BloomMip> upMips_;

    BloomOutputs outputs_;
};

} // namespace HuanGL
