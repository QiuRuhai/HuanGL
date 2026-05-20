#pragma once
#include "UniformBuffer.h"

namespace HuanGL {

enum class ToneMapMode {
    ACES = 0,
    Reinhard = 1,
    None = 2,
};

enum class DebugView {
    Final = 0,
    Albedo = 1,
    Normal = 2,
    Roughness = 3,
    Metallic = 4,
    Depth = 5,
    Cascades = 6,
    Bloom = 7,
};

inline int ToShaderToneMapMode(ToneMapMode mode) {
    return static_cast<int>(mode);
}

inline int ToShaderDebugView(DebugView view) {
    return static_cast<int>(view);
}

struct BloomSettings {
    bool enabled = true;
    float threshold = 1.0f;
    float intensity = 0.08f;
    int radius = 5;
};

struct RenderSettings {
    ToneMapMode toneMapMode = ToneMapMode::ACES;
    float ambientStrength = 1.0f;
    int shadowResolution = 2048;
    float exposure = 1.0f;
    BloomSettings bloom;

    void CycleToneMap() {
        int next = (ToShaderToneMapMode(toneMapMode) + 1) % 3;
        toneMapMode = static_cast<ToneMapMode>(next);
    }
};

struct DebugSettings {
    DebugView view = DebugView::Final;
    bool showImGui = true;
    bool freezeCamera = false;
};

struct FrameContext {
    int width = 0;
    int height = 0;
    float time = 0.0f;
    float deltaTime = 0.0f;

    CameraData camera;
    RenderSettings renderSettings;
    DebugSettings debugSettings;
};

} // namespace HuanGL
