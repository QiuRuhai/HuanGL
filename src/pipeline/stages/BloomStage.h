#pragma once
#include "../IPipelineStage.h"
#include "../../renderer/Buffer.h"
#include "../../renderer/Framebuffer.h"
#include "../../renderer/Shader.h"
#include "../../renderer/Texture.h"
#include <memory>
#include <vector>

namespace HuanGL {

struct BloomOutputs {
    std::shared_ptr<Texture> bloom;
};

class BloomStage : public IPipelineStage {
public:
    const char* GetName() const override { return "BloomStage"; }
    void Init(int width, int height) override;
    void Resize(int width, int height) override;
    void Execute(PipelineResources& resources, const FrameContext& frame) override;

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

    std::unique_ptr<Shader> extractShader_;
    std::unique_ptr<Shader> downsampleShader_;
    std::unique_ptr<Shader> upsampleShader_;
    std::unique_ptr<VertexArray> dummyVAO_;

    std::vector<BloomMip> downMips_;
    std::vector<BloomMip> upMips_;
};

} // namespace HuanGL
