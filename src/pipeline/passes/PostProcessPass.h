#pragma once
#include <memory>
#include "../../renderer/UniformBuffer.h"

namespace HuanGL {

class Shader;
class VertexArray;
class GBufferPass;
class LightingPass;
class ShadowPass;

class PostProcessPass {
public:
    void Init();
    void Render(const LightingPass& lighting, const GBufferPass& gbuffer,
                const ShadowPass& shadow, const CameraData& camera);

    void SetToneMapMode(int mode) { toneMapMode_ = mode; }
    void SetDebugMode(int mode)   { debugMode_ = mode; }
    void CycleToneMap()  { toneMapMode_ = (toneMapMode_ + 1) % 3; }
    void CycleDebugMode() { debugMode_ = (debugMode_ + 1) % 7; }

    int GetToneMapMode() const { return toneMapMode_; }
    int GetDebugMode() const   { return debugMode_; }

private:
    std::unique_ptr<Shader>      shader_;
    std::unique_ptr<VertexArray>  dummyVAO_;
    int toneMapMode_ = 0; // 0=ACES, 1=Reinhard, 2=None
    int debugMode_   = 0; // 0=Final, 1-6=debug views
};

} // namespace HuanGL
