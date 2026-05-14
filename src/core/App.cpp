#include "../pipeline/RenderPipeline.h"
#include "App.h"
#include "Window.h"
#include "Input.h"
#include "Camera.h"
#include "../renderer/Renderer.h"
#include "../scene/Scene.h"
#include "../scene/TestScene.h"
#include "../resource/ResourceManager.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace HuanGL {

App::App()  { Init(); }
App::~App() { Shutdown(); }

void App::Init() {
    window_ = std::make_unique<Window>(1280, 720, "HuanGL");
    Input::Init(window_->GetHandle());
    Renderer::Init();
    Renderer::SetViewport(0, 0, window_->GetWidth(), window_->GetHeight());

    window_->SetResizeCallback([](int w, int h) {
        Renderer::SetViewport(0, 0, w, h);
    });

    cameraUBO_ = std::make_unique<CameraUBO>();
    lightsUBO_ = std::make_unique<LightsUBO>();
    timeUBO_   = std::make_unique<TimeUBO>();

    camera_ = std::make_unique<Camera>(60.f, 0.1f, 100.f);
    Input::SetCursorCaptured(true);

    resourceManager_ = std::make_unique<ResourceManager>();
    scene_ = std::make_unique<TestScene>();
    scene_->Init(*resourceManager_);

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

        Update(dt);
        Render();
        window_->SwapBuffers();
    }
}

void App::Update(float dt) {
    camera_->Update(dt, window_->GetHandle(), true);
    scene_->Update(dt);

    TimeData timeData;
    timeData.time      = static_cast<float>(glfwGetTime());
    timeData.deltaTime = dt;
    timeUBO_->Update(timeData);
}

void App::Render() {
    int w = window_->GetWidth();
    int h = window_->GetHeight();
    float aspect = w > 0 && h > 0 ? static_cast<float>(w) / static_cast<float>(h) : 1.f;

    CameraData camData = camera_->GetData(aspect);
    cameraUBO_->Update(camData);

    LightsData lightData;
    auto& sun = scene_->GetSunLight();
    lightData.dirLightDir       = sun.direction;
    lightData.dirLightColor     = sun.color;
    lightData.dirLightIntensity = sun.intensity;
    lightsUBO_->Update(lightData);

    Renderer::SetClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    Renderer::Clear();

    pipeline_->Execute(*scene_, camData);
}

} // namespace HuanGL
