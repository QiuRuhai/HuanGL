#include "DebugUI.h"
#include "../app/ApplicationState.h"
#include "../scene/World.h"
#include <glm/geometric.hpp>
#include <imgui.h>

namespace HuanGL {

static int ToneMapIndex(ToneMapMode mode) {
    return ToShaderToneMapMode(mode);
}

static ToneMapMode ToneMapFromIndex(int mode) {
    return static_cast<ToneMapMode>(mode);
}

static int DebugViewIndex(DebugView view) {
    return ToShaderDebugView(view);
}

static DebugView DebugViewFromIndex(int view) {
    return static_cast<DebugView>(view);
}

void DebugUI::Draw(ApplicationState& state) {
    if (!state.debugSettings.showImGui) {
        return;
    }

    ImGui::Begin("HuanGL Debug");

    if (ImGui::CollapsingHeader("Render")) {
        static const char* toneModes[] = { "ACES", "Reinhard", "None" };
        int toneMode = ToneMapIndex(state.renderSettings.toneMapMode);
        if (ImGui::Combo("Tone Map", &toneMode, toneModes, 3)) {
            state.renderSettings.toneMapMode = ToneMapFromIndex(toneMode);
        }

        static const char* debugModes[] = {
            "Final", "Albedo", "Normal", "Roughness", "Metallic", "Depth", "Cascades"
        };
        int debugMode = DebugViewIndex(state.debugSettings.view);
        if (ImGui::Combo("Debug Mode", &debugMode, debugModes, 7)) {
            state.debugSettings.view = DebugViewFromIndex(debugMode);
        }

        ImGui::DragFloat("Ambient Strength", &state.renderSettings.ambientStrength,
                         0.01f, 0.0f, 2.0f);
    }

    if (ImGui::CollapsingHeader("Lighting")) {
        World* world = state.sceneRegistry.GetActiveWorld();
        if (world) {
            auto& sun = world->GetSunLight();
            ImGui::DragFloat3("Direction", &sun.direction.x, 0.01f, -1.0f, 1.0f);
            if (glm::length(sun.direction) > 0.0f) {
                sun.direction = glm::normalize(sun.direction);
            }
            ImGui::ColorEdit3("Color", &sun.color.r);
            ImGui::DragFloat("Intensity", &sun.intensity, 0.05f, 0.0f, 20.0f);
        }
    }

    if (ImGui::CollapsingHeader("Camera")) {
        float fov = state.camera.GetFov();
        if (ImGui::SliderFloat("FOV", &fov, 30.0f, 120.0f)) {
            state.camera.SetFov(fov);
        }
        ImGui::Checkbox("Freeze Camera", &state.debugSettings.freezeCamera);
    }

    if (ImGui::CollapsingHeader("Scene")) {
        if (!state.sceneRegistry.Empty()) {
            ImGui::Text("Active: %s", state.sceneRegistry.GetActiveName().c_str());
            if (ImGui::Button("Next")) {
                state.sceneRegistry.Cycle();
            }
        }

        World* world = state.sceneRegistry.GetActiveWorld();
        if (world) {
            for (auto& entity : world->GetEntities()) {
                ImGui::PushID(static_cast<int>(entity.id));
                if (ImGui::TreeNode(entity.name.c_str())) {
                    ImGui::DragFloat3("Translation", &entity.transform.translation.x, 0.05f);
                    ImGui::DragFloat3("Scale", &entity.transform.scale.x, 0.01f, 0.01f, 100.0f);
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }
    }

    if (ImGui::CollapsingHeader("Stats")) {
        ImGui::Text("FPS: %.1f", state.frameStats.fps);
        ImGui::Text("Frame: %.2f ms", state.frameStats.frameTimeMs);
    }

    ImGui::End();
}

} // namespace HuanGL
