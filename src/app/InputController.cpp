#include "InputController.h"
#include "ApplicationState.h"
#include "../core/Input.h"
#include <GLFW/glfw3.h>

namespace HuanGL {

void InputController::Update(ApplicationState& state, float deltaTime) {
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
    if (Input::IsKeyJustPressed(GLFW_KEY_7) || Input::IsKeyJustPressed(GLFW_KEY_KP_7)) {
        state.debugSettings.view = DebugView::Bloom;
    }

    // Right-click camera mode
    bool rmb = Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
    state.cameraActive = rmb;

    if (rmb && !wasCameraActive_) {
        Input::SetCursorCaptured(true);
    }
    if (!rmb && wasCameraActive_) {
        Input::SetCursorCaptured(false);
    }
    wasCameraActive_ = rmb;

    // Camera movement (only while RMB held and camera not frozen)
    if (state.cameraActive && !state.debugSettings.freezeCamera) {
        glm::vec2 delta = Input::GetMouseDelta();
        state.camera.Look(delta.x * 0.1f, delta.y * 0.1f);

        glm::vec3 move{0};
        if (Input::IsKeyPressed(GLFW_KEY_W)) move.z += 1.f;
        if (Input::IsKeyPressed(GLFW_KEY_S)) move.z -= 1.f;
        if (Input::IsKeyPressed(GLFW_KEY_A)) move.x -= 1.f;
        if (Input::IsKeyPressed(GLFW_KEY_D)) move.x += 1.f;
        if (Input::IsKeyPressed(GLFW_KEY_E)) move.y += 1.f;
        if (Input::IsKeyPressed(GLFW_KEY_Q)) move.y -= 1.f;
        state.camera.Move(move, deltaTime);
    }
}

} // namespace HuanGL
