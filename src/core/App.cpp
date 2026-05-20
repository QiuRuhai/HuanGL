#include "../pipeline/RenderPipeline.h"
#include "App.h"
#include "Window.h"
#include "Input.h"
#include "Camera.h"
#include "../renderer/Renderer.h"
#include "../scene/Scene.h"
#include "../scene/TestScene.h"
#include "../scene/ModelScene.h"
#include "../resource/ResourceManager.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <exception>
#include <filesystem>
#include "../ui/ImGuiLayer.h"
#include <imgui.h>

namespace HuanGL {

App::App()  { Init(); }
App::~App() { Shutdown(); }

void App::RegisterScene(std::unique_ptr<Scene> scene, std::string name) {
    scene->Init(*resourceManager_);
    scenes_.push_back(std::move(scene));
    sceneNames_.push_back(std::move(name));
    std::printf("[App] Registered scene '%s' (index %zu)\n",
                sceneNames_.back().c_str(), scenes_.size() - 1);
}

void App::CycleScene() {
    if (scenes_.empty()) return;
    activeSceneIdx_ = (activeSceneIdx_ + 1) % scenes_.size();
    std::printf("[App] Switched to scene '%s' (index %zu)\n",
                sceneNames_[activeSceneIdx_].c_str(), activeSceneIdx_);
}

void App::Init() {
    window_ = std::make_unique<Window>(1280, 720, "HuanGL");
    Input::Init(window_->GetHandle());
    imguiLayer_ = std::make_unique<ImGuiLayer>();
    imguiLayer_->Init(window_->GetHandle());
    Renderer::Init();
    Renderer::SetViewport(0, 0, window_->GetWidth(), window_->GetHeight());

    window_->SetResizeCallback([this](int w, int h) {
        Renderer::SetViewport(0, 0, w, h);
        if (w > 0 && h > 0 && pipeline_)
            pipeline_->Resize(w, h);
    });

    camera_ = std::make_unique<Camera>(60.f, 0.1f, 100.f);
    Input::SetCursorCaptured(true);

    resourceManager_ = std::make_unique<ResourceManager>();

    std::printf("[App] CWD: %s\n", std::filesystem::current_path().string().c_str());
    std::fflush(stdout);

    // Always register TestScene as a known-good baseline.
    RegisterScene(std::make_unique<TestScene>(), "TestScene");

    // Try to register optional model scenes. Tolerate missing files so the
    // app still runs if the user hasn't downloaded a model yet.
    {
        const char* path = "../resources/models/DamagedHelmet.glb";
        std::printf("[App] Attempting DamagedHelmet from '%s' (exists=%d)\n",
                    path, std::filesystem::exists(path) ? 1 : 0);
        std::fflush(stdout);
        try {
            RegisterScene(
                std::make_unique<ModelScene>(path, "DamagedHelmet",
                    /*addFloor=*/true, /*modelScale=*/1.0f),
                "DamagedHelmet");
        } catch (const std::exception& e) {
            std::printf("[App] Skipped DamagedHelmet: %s\n", e.what());
        } catch (...) {
            std::printf("[App] Skipped DamagedHelmet: unknown exception\n");
        }
        std::fflush(stdout);
    }

    {
        const char* path = "../resources/models/Sponza/glTF/Sponza.gltf";
        std::printf("[App] Attempting Sponza from '%s' (exists=%d)\n",
                    path, std::filesystem::exists(path) ? 1 : 0);
        std::fflush(stdout);
        try {
            RegisterScene(
                std::make_unique<ModelScene>(path, "Sponza",
                    /*addFloor=*/false, /*modelScale=*/0.02f),
                "Sponza");
        } catch (const std::exception& e) {
            std::printf("[App] Skipped Sponza: %s\n", e.what());
        } catch (...) {
            std::printf("[App] Skipped Sponza: unknown exception\n");
        }
        std::fflush(stdout);
    }

    pipeline_ = std::make_unique<RenderPipeline>();
    pipeline_->Init(window_->GetWidth(), window_->GetHeight(),
                    "../resources/texture/hdr/brown_photostudio_02_2k.hdr");
}

void App::Shutdown() {
    if (imguiLayer_) imguiLayer_->Shutdown();
    ResourceManager::Shutdown();
}

void App::HandleHotkeys() {
    if (Input::IsKeyPressed(GLFW_KEY_ESCAPE))
        running_ = false;

    if (Input::IsKeyJustPressed(GLFW_KEY_N))
        CycleScene();

    if (Input::IsKeyJustPressed(GLFW_KEY_T))
        renderSettings_.CycleToneMap();
    if (Input::IsKeyJustPressed(GLFW_KEY_0) || Input::IsKeyJustPressed(GLFW_KEY_KP_0))
        debugSettings_.view = DebugView::Final;
    if (Input::IsKeyJustPressed(GLFW_KEY_1) || Input::IsKeyJustPressed(GLFW_KEY_KP_1))
        debugSettings_.view = DebugView::Albedo;
    if (Input::IsKeyJustPressed(GLFW_KEY_2) || Input::IsKeyJustPressed(GLFW_KEY_KP_2))
        debugSettings_.view = DebugView::Normal;
    if (Input::IsKeyJustPressed(GLFW_KEY_3) || Input::IsKeyJustPressed(GLFW_KEY_KP_3))
        debugSettings_.view = DebugView::Roughness;
    if (Input::IsKeyJustPressed(GLFW_KEY_4) || Input::IsKeyJustPressed(GLFW_KEY_KP_4))
        debugSettings_.view = DebugView::Metallic;
    if (Input::IsKeyJustPressed(GLFW_KEY_5) || Input::IsKeyJustPressed(GLFW_KEY_KP_5))
        debugSettings_.view = DebugView::Depth;
    if (Input::IsKeyJustPressed(GLFW_KEY_6) || Input::IsKeyJustPressed(GLFW_KEY_KP_6))
        debugSettings_.view = DebugView::Cascades;
}

void App::Run() {
    while (!window_->ShouldClose() && running_) {
        float now = static_cast<float>(glfwGetTime());
        float dt  = now - lastTime_;
        lastTime_ = now;

        Input::Update();
        window_->PollEvents();
        HandleHotkeys();

        Update(dt);
        Render(dt);

        imguiLayer_->BeginFrame();
        BuildDebugPanel();
        imguiLayer_->EndFrame();

        window_->SwapBuffers();
    }
}

void App::BuildDebugPanel() {
    ImGui::Begin("HuanGL Debug");

    if (ImGui::CollapsingHeader("Render")) {
        static const char* toneModes[] = { "ACES", "Reinhard", "None" };
        int toneMode = ToShaderToneMapMode(renderSettings_.toneMapMode);
        if (ImGui::Combo("Tone Map", &toneMode, toneModes, 3))
            renderSettings_.toneMapMode = static_cast<ToneMapMode>(toneMode);

        static const char* debugModes[] = {
            "Final", "Albedo", "Normal", "Roughness", "Metallic", "Depth", "Cascades"
        };
        int debugMode = ToShaderDebugView(debugSettings_.view);
        if (ImGui::Combo("Debug Mode", &debugMode, debugModes, 7))
            debugSettings_.view = static_cast<DebugView>(debugMode);

        ImGui::DragFloat("Ambient Strength", &renderSettings_.ambientStrength,
                         0.01f, 0.0f, 2.0f);
    }

    if (ImGui::CollapsingHeader("Lighting")) {
        if (!scenes_.empty()) {
            auto& sun = scenes_[activeSceneIdx_]->GetMutableSunLight();
            ImGui::DragFloat3("Direction", &sun.direction.x, 0.01f, -1.f, 1.f);
            if (glm::length(sun.direction) > 0.0f)
                sun.direction = glm::normalize(sun.direction);
            ImGui::ColorEdit3("Color", &sun.color.r);
            ImGui::DragFloat("Intensity", &sun.intensity, 0.05f, 0.f, 20.f);

        }
    }

    if (ImGui::CollapsingHeader("Camera")) {
        float fov = camera_->GetFov();
        if (ImGui::SliderFloat("FOV", &fov, 30.f, 120.f))
            camera_->SetFov(fov);
    }

    if (ImGui::CollapsingHeader("Scene")) {
        if (!scenes_.empty()) {
            ImGui::Text("Active: %s", sceneNames_[activeSceneIdx_].c_str());
            if (ImGui::Button("Next"))
                CycleScene();
        }
    }

    if (ImGui::CollapsingHeader("Stats")) {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("FPS: %.1f", 1.0f / io.DeltaTime);
        ImGui::Text("Frame: %.2f ms", io.DeltaTime * 1000.0f);
    }

    ImGui::End();
}

void App::Update(float dt) {
    camera_->Update(dt, window_->GetHandle(), true);
    if (!scenes_.empty())
        scenes_[activeSceneIdx_]->Update(dt);

}

void App::Render(float dt) {
    int w = window_->GetWidth();
    int h = window_->GetHeight();
    if (w <= 0 || h <= 0) return; // minimized
    if (scenes_.empty()) return;
    float aspect = static_cast<float>(w) / static_cast<float>(h);

    Scene& activeScene = *scenes_[activeSceneIdx_];
    RenderSceneView sceneView = activeScene.BuildRenderSceneView();

    FrameContext frame;
    frame.width = w;
    frame.height = h;
    frame.time = static_cast<float>(glfwGetTime());
    frame.deltaTime = dt;
    frame.camera = camera_->GetData(aspect);
    frame.renderSettings = renderSettings_;
    frame.debugSettings = debugSettings_;

    Renderer::SetClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    Renderer::Clear();

    pipeline_->Execute(sceneView, frame);
}

} // namespace HuanGL
