#pragma once
#include "../IPipelineStage.h"
#include "../../renderer/Buffer.h"
#include "../../renderer/Framebuffer.h"
#include "../../renderer/Shader.h"
#include "../../renderer/Texture.h"
#include <array>
#include <memory>

namespace HuanGL {

struct TAAOutputs {
    std::shared_ptr<Texture> resolvedHdr;
};

class TAAStage : public IPipelineStage {
public:
    const char* GetName() const override { return "TAAStage"; }
    void Init(int width, int height) override;
    void Resize(int width, int height) override;
    void InvalidateHistory() override;
    void Execute(PipelineResources& resources, const FrameContext& frame) override;

private:
    void CreateResources(int width, int height);
    void DrawFullscreen() const;

    std::unique_ptr<Shader> shader_;
    std::unique_ptr<VertexArray> dummyVAO_;
    std::shared_ptr<Texture> resolved_;
    std::unique_ptr<Framebuffer> resolvedFBO_;
    std::array<std::shared_ptr<Texture>, 2> history_;

    int width_ = 0;
    int height_ = 0;
    int historyReadIndex_ = 0;
    bool historyValid_ = false;
    bool wasEnabled_ = false;
};

} // namespace HuanGL
