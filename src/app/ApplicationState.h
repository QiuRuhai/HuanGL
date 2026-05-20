#pragma once
#include "SceneRegistry.h"
#include "../core/Camera.h"
#include "../renderer/FrameContext.h"

namespace HuanGL {

struct FrameStats {
    float deltaTime = 0.0f;
    float frameTimeMs = 0.0f;
    float fps = 0.0f;
};

struct ApplicationState {
    bool running = true;
    SceneRegistry sceneRegistry;
    Camera camera {60.0f, 0.1f, 100.0f};
    RenderSettings renderSettings;
    DebugSettings debugSettings;
    FrameStats frameStats;
};

} // namespace HuanGL
