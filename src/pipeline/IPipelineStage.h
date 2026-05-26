#pragma once

namespace HuanGL {

class PipelineResources;
struct FrameContext;

class IPipelineStage {
public:
    virtual ~IPipelineStage() = default;
    virtual const char* GetName() const = 0;
    virtual void Init(int width, int height) = 0;
    virtual void Resize(int width, int height) = 0;
    virtual void InvalidateHistory() {}
    virtual void Execute(PipelineResources& resources, const FrameContext& frame) = 0;
};

} // namespace HuanGL
