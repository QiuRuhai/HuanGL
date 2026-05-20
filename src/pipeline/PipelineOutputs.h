#pragma once
#include "CascadeData.h"
#include "../renderer/Texture.h"
#include <array>
#include <glad/glad.h>
#include <memory>

namespace HuanGL {

struct ShadowOutputs {
    GLuint shadowArray = 0;
    std::array<CascadeData, 4> cascades {};
};

struct GBufferOutputs {
    std::shared_ptr<Texture> albedoMetallic;
    std::shared_ptr<Texture> normalRoughness;
    std::shared_ptr<Texture> depth;
};

struct LightingOutputs {
    std::shared_ptr<Texture> hdrColor;
};

struct BloomOutputs {
    std::shared_ptr<Texture> bloom;
};

struct PipelineOutputs {
    ShadowOutputs shadow;
    GBufferOutputs gbuffer;
    LightingOutputs lighting;
    BloomOutputs bloom;
};

} // namespace HuanGL
