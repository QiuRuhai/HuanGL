#include "InputController.h"
#include "ApplicationState.h"
#include "../core/Input.h"
#include <GLFW/glfw3.h>

namespace HuanGL {

void InputController::Update(ApplicationState& state) {
    if (Input::IsKeyPressed(GLFW_KEY_ESCAPE)) {
        state.running = false;
    }

    if (Input::IsKeyJustPressed(GLFW_KEY_N)) {
        state.sceneRegistry.Cycle();
    }

    if (Input::IsKeyJustPressed(GLFW_KEY_T)) {
        state.renderSettings.CycleToneMap();
    }

    if (Input::IsKeyJustPressed(GLFW_KEY_0) || Input::IsKeyJustPressed(GLFW_KEY_KP_0)) {
        state.debugSettings.view = DebugView::Final;
    }
    if (Input::IsKeyJustPressed(GLFW_KEY_1) || Input::IsKeyJustPressed(GLFW_KEY_KP_1)) {
        state.debugSettings.view = DebugView::Albedo;
    }
    if (Input::IsKeyJustPressed(GLFW_KEY_2) || Input::IsKeyJustPressed(GLFW_KEY_KP_2)) {
        state.debugSettings.view = DebugView::Normal;
    }
    if (Input::IsKeyJustPressed(GLFW_KEY_3) || Input::IsKeyJustPressed(GLFW_KEY_KP_3)) {
        state.debugSettings.view = DebugView::Roughness;
    }
    if (Input::IsKeyJustPressed(GLFW_KEY_4) || Input::IsKeyJustPressed(GLFW_KEY_KP_4)) {
        state.debugSettings.view = DebugView::Metallic;
    }
    if (Input::IsKeyJustPressed(GLFW_KEY_5) || Input::IsKeyJustPressed(GLFW_KEY_KP_5)) {
        state.debugSettings.view = DebugView::Depth;
    }
    if (Input::IsKeyJustPressed(GLFW_KEY_6) || Input::IsKeyJustPressed(GLFW_KEY_KP_6)) {
        state.debugSettings.view = DebugView::Cascades;
    }
}

} // namespace HuanGL
