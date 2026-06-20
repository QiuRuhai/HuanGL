#pragma once
#include "../IPipelineStage.h"
#include "../PathTracerScene.h"
#include "../../renderer/Shader.h"
#include "../../renderer/Texture.h"
#include <glm/glm.hpp>
#include <memory>
#include <string>

namespace HuanGL {

struct ReferenceOutputs {
    std::shared_ptr<Texture> hdr;
    uint32_t sampleCount = 0;
};

class PathTracerStage : public IPipelineStage {
public:
    explicit PathTracerStage(std::string hdrPath);

    const char* GetName() const override { return "PathTracerStage"; }
    void Init(int width, int height) override;
    void Resize(int width, int height) override;
    void InvalidateHistory() override;
    void Execute(PipelineResources& resources, const FrameContext& frame) override;

private:
    std::string              hdrPath_;
    std::unique_ptr<Shader>  shader_;
    std::shared_ptr<Texture> accum_;
    std::shared_ptr<Texture> env_;
    PathTracerScene          scene_;
    uint32_t                 sampleCount_  = 0;
    bool                     sceneDirty_   = true;
    glm::mat4                lastViewProj_ = glm::mat4(0.0f);
    int                      width_        = 0;
    int                      height_       = 0;
};

} // namespace HuanGL
