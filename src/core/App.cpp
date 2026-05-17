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
    Renderer::Init();
    Renderer::SetViewport(0, 0, window_->GetWidth(), window_->GetHeight());

    window_->SetResizeCallback([this](int w, int h) {
        Renderer::SetViewport(0, 0, w, h);
        if (w > 0 && h > 0 && pipeline_)
            pipeline_->Resize(w, h);
    });

    cameraUBO_ = std::make_unique<CameraUBO>();
    lightsUBO_ = std::make_unique<LightsUBO>();
    timeUBO_   = std::make_unique<TimeUBO>();

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
    ResourceManager::Shutdown();
}

void App::Run() {
    while (!window_->ShouldClose() && running_) {
        float now = static_cast<float>(glfwGetTime());
        float dt  = now - lastTime_;
        lastTime_ = now;

        Input::Update();
        window_->PollEvents();

        if (Input::IsKeyPressed(GLFW_KEY_ESCAPE))
            running_ = false;

        // Scene switching
        if (Input::IsKeyJustPressed(GLFW_KEY_N))
            CycleScene();

        // Debug visualization toggles
        auto& pp = pipeline_->GetPostProcess();
        if (Input::IsKeyJustPressed(GLFW_KEY_T))
            pp.CycleToneMap();
        if (Input::IsKeyJustPressed(GLFW_KEY_0) || Input::IsKeyJustPressed(GLFW_KEY_KP_0))
            pp.SetDebugMode(0);
        if (Input::IsKeyJustPressed(GLFW_KEY_1) || Input::IsKeyJustPressed(GLFW_KEY_KP_1))
            pp.SetDebugMode(1);
        if (Input::IsKeyJustPressed(GLFW_KEY_2) || Input::IsKeyJustPressed(GLFW_KEY_KP_2))
            pp.SetDebugMode(2);
        if (Input::IsKeyJustPressed(GLFW_KEY_3) || Input::IsKeyJustPressed(GLFW_KEY_KP_3))
            pp.SetDebugMode(3);
        if (Input::IsKeyJustPressed(GLFW_KEY_4) || Input::IsKeyJustPressed(GLFW_KEY_KP_4))
            pp.SetDebugMode(4);
        if (Input::IsKeyJustPressed(GLFW_KEY_5) || Input::IsKeyJustPressed(GLFW_KEY_KP_5))
            pp.SetDebugMode(5);
        if (Input::IsKeyJustPressed(GLFW_KEY_6) || Input::IsKeyJustPressed(GLFW_KEY_KP_6))
            pp.SetDebugMode(6);

        Update(dt);
        Render();
        window_->SwapBuffers();
    }
}

void App::Update(float dt) {
    camera_->Update(dt, window_->GetHandle(), true);
    if (!scenes_.empty())
        scenes_[activeSceneIdx_]->Update(dt);

    TimeData timeData;
    timeData.time      = static_cast<float>(glfwGetTime());
    timeData.deltaTime = dt;
    timeUBO_->Update(timeData);
}

void App::Render() {
    int w = window_->GetWidth();
    int h = window_->GetHeight();
    if (w <= 0 || h <= 0) return; // minimized
    if (scenes_.empty()) return;
    float aspect = static_cast<float>(w) / static_cast<float>(h);

    CameraData camData = camera_->GetData(aspect);
    cameraUBO_->Update(camData);

    Scene& activeScene = *scenes_[activeSceneIdx_];

    LightsData lightData;
    auto& sun = activeScene.GetSunLight();
    lightData.dirLightDir       = sun.direction;
    lightData.dirLightColor     = sun.color;
    lightData.dirLightIntensity = sun.intensity;
    lightsUBO_->Update(lightData);

    Renderer::SetClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    Renderer::Clear();

    pipeline_->Execute(activeScene, camData);
}

} // namespace HuanGL
