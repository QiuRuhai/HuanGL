#pragma once
#include "../IPipelineStage.h"
#include "../CascadeData.h"
#include <array>
#include <memory>
#include <glad/glad.h>

namespace HuanGL {

class Shader;
class Framebuffer;

struct ShadowOutputs {
    GLuint shadowArray = 0;
    std::array<CascadeData, 4> cascades {};
};

class ShadowStage : public IPipelineStage {
public:
    explicit ShadowStage(int resolution = 2048);
    ~ShadowStage() override;

    const char* GetName() const override { return "ShadowStage"; }
    void Init(int width, int height) override;
    void Resize(int width, int height) override;
    void Execute(PipelineResources& resources, const FrameContext& frame) override;

private:
    std::unique_ptr<Shader>      shader_;
    std::unique_ptr<Framebuffer> fbo_;
    GLuint                       shadowArrayID_ = 0;
    std::array<CascadeData, 4>   cascades_;
    int resolution_;
};

} // namespace HuanGL
