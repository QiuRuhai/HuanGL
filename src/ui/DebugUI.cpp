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
        static const char* toneModes[] = { "ACES", "Reinhard", "AgX", "None" };
        int toneMode = ToneMapIndex(state.renderSettings.toneMapMode);
        if (ImGui::Combo("Tone Map", &toneMode, toneModes, 4)) {
            state.renderSettings.toneMapMode = ToneMapFromIndex(toneMode);
        }

        static const char* debugModes[] = {
            "Final", "Albedo", "Normal", "Roughness",
            "Metallic", "Depth", "Cascades", "Bloom"
        };
        int debugMode = DebugViewIndex(state.debugSettings.view);
        if (ImGui::Combo("Debug Mode", &debugMode, debugModes, 8)) {
            state.debugSettings.view = DebugViewFromIndex(debugMode);
        }

        ImGui::DragFloat("Ambient Strength", &state.renderSettings.ambientStrength,
                         0.01f, 0.0f, 2.0f);
    }

    if (ImGui::CollapsingHeader("Techniques")) {
        ImGui::Checkbox("TAA", &state.renderSettings.taa.enabled);
        ImGui::SliderFloat("TAA Feedback", &state.renderSettings.taa.feedback,
                           0.0f, 0.98f, "%.2f");
        ImGui::Checkbox("Bloom", &state.renderSettings.bloom.enabled);
        ImGui::DragFloat("Bloom Threshold", &state.renderSettings.bloom.threshold,
                         0.05f, 0.0f, 20.0f);
        ImGui::DragFloat("Bloom Soft Knee", &state.renderSettings.bloom.softKnee,
                         0.01f, 0.0f, 2.0f);
        ImGui::DragFloat("Bloom Intensity", &state.renderSettings.bloom.intensity,
                         0.01f, 0.0f, 5.0f);
        ImGui::SliderInt("Bloom Radius", &state.renderSettings.bloom.radius,
                         1, 16);
        ImGui::SliderInt("Bloom Mips", &state.renderSettings.bloom.mipCount,
                         1, 6);
        ImGui::DragFloat("Exposure", &state.renderSettings.exposure,
                         0.01f, 0.0f, 10.0f);
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

    if (ImGui::CollapsingHeader("GI Comparison")) {
        auto& rs = state.renderSettings;
        auto& ds = state.debugSettings;
        ImGui::Checkbox("Path tracer (reference)", &rs.pathTracerEnabled);
        ImGui::SliderInt("SPP / frame", &rs.pathTracerSpp, 1, 8);
        ImGui::SliderInt("Max bounces", &rs.pathTracerMaxBounces, 1, 8);
        const char* views[] = {"Realtime","Reference","Split","Error heatmap"};
        int v = (int)ds.compareView;
        if (ImGui::Combo("View", &v, views, 4)) ds.compareView = (DebugSettings::CompareView)v;
        ImGui::SliderFloat("Error scale", &ds.errorScale, 0.01f, 10.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
        const auto& r = state.comparisonReadout;
        if (r.valid) {
            ImGui::Text("Samples: %u", r.sampleCount);
            ImGui::Text("RMSE: %.5f", r.rmse);
            ImGui::Text("MAPE: %.2f%%", r.mape * 100.0);
        } else {
            ImGui::TextDisabled("enable path tracer to measure");
        }
    }

    if (ImGui::CollapsingHeader("Stats")) {
        ImGui::Text("FPS: %.1f", state.frameStats.fps);
        ImGui::Text("Frame: %.2f ms", state.frameStats.frameTimeMs);
    }

    if (ImGui::CollapsingHeader("GPU Timing")) {
        if (state.stageTimings.empty()) {
            ImGui::TextDisabled("measuring...");
        } else {
            double total = 0.0;
            if (ImGui::BeginTable("gpu_timing", 2,
                                  ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Stage");
                ImGui::TableSetupColumn("ms");
                ImGui::TableHeadersRow();
                for (const auto& t : state.stageTimings) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(t.name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%.3f", t.ms);
                    total += t.ms;
                }
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Total GPU");
                ImGui::TableNextColumn();
                ImGui::Text("%.3f", total);
                ImGui::EndTable();
            }
        }
    }

    ImGui::End();
}

} // namespace HuanGL
