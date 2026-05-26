#include "../pipeline/RenderPipeline.h"
#include "App.h"
#include "Window.h"
#include "Input.h"
#include "../renderer/Renderer.h"
#include "../renderer/Shader.h"
#include "../scene/Scene.h"
#include "../scene/TestScene.h"
#include "../scene/ModelScene.h"
#include "../resource/ResourceManager.h"
#include "../ui/DebugUI.h"
#include "../ui/ImGuiLayer.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <filesystem>

namespace HuanGL {

App::App()  { Init(); }
App::~App() { Shutdown(); }

void App::RegisterScenes() {
    state_.sceneRegistry.RegisterRequired(
        std::make_unique<TestScene>(), "TestScene", *resourceManager_);

    const char* helmetPath = "../resources/models/DamagedHelmet.glb";
    std::printf("[App] Attempting DamagedHelmet from '%s' (exists=%d)\n",
                helmetPath, std::filesystem::exists(helmetPath) ? 1 : 0);
    state_.sceneRegistry.RegisterOptional(
        std::make_unique<ModelScene>(helmetPath, "DamagedHelmet", true, 1.0f),
        "DamagedHelmet", *resourceManager_);

    const char* sponzaPath = "../resources/models/Sponza/glTF/Sponza.gltf";
    std::printf("[App] Attempting Sponza from '%s' (exists=%d)\n",
                sponzaPath, std::filesystem::exists(sponzaPath) ? 1 : 0);
    state_.sceneRegistry.RegisterOptional(
        std::make_unique<ModelScene>(sponzaPath, "Sponza", false, 0.02f),
        "Sponza", *resourceManager_);
}

void App::Init() {
    window_ = std::make_unique<Window>(1280, 720, "HuanGL");
    Input::Init(window_->GetHandle());
    imguiLayer_ = std::make_unique<ImGuiLayer>();
    imguiLayer_->Init(window_->GetHandle());
    debugUI_ = std::make_unique<DebugUI>();
    Renderer::Init();
    Shader::SetBasePath("../shader/");
    Renderer::SetViewport(0, 0, window_->GetWidth(), window_->GetHeight());

    window_->SetResizeCallback([this](int w, int h) {
        Renderer::SetViewport(0, 0, w, h);
        if (w > 0 && h > 0 && pipeline_)
            pipeline_->Resize(w, h);
    });

    Input::SetCursorCaptured(true);

    resourceManager_ = std::make_unique<ResourceManager>();

    std::printf("[App] CWD: %s\n", std::filesystem::current_path().string().c_str());
    std::fflush(stdout);

    RegisterScenes();

    pipeline_ = std::make_unique<RenderPipeline>();
    pipeline_->Init(window_->GetWidth(), window_->GetHeight(),
                    "../resources/texture/hdr/brown_photostudio_02_2k.hdr");
}

void App::Shutdown() {
    if (imguiLayer_) imguiLayer_->Shutdown();
    ResourceManager::Shutdown();
}

void App::Run() {
    while (!window_->ShouldClose() && state_.running) {
        float now = static_cast<float>(glfwGetTime());
        float dt  = now - lastTime_;
        lastTime_ = now;

        Input::Update();
        window_->PollEvents();
        inputController_.Update(state_);

        Update(dt);
        Render(dt);

        imguiLayer_->BeginFrame();
        debugUI_->Draw(state_);
        imguiLayer_->EndFrame();

        window_->SwapBuffers();
    }
}

void App::Update(float dt) {
    state_.camera.Update(dt, window_->GetHandle(), !state_.debugSettings.freezeCamera);
    if (Scene* scene = state_.sceneRegistry.GetActiveScene()) {
        scene->Update(dt);
    }

    state_.frameStats.deltaTime = dt;
    state_.frameStats.frameTimeMs = dt * 1000.0f;
    state_.frameStats.fps = dt > 0.0f ? 1.0f / dt : 0.0f;
}

FrameContext App::BuildFrameContext(float dt) const {
    FrameContext frame;
    frame.width = window_->GetWidth();
    frame.height = window_->GetHeight();
    frame.time = static_cast<float>(glfwGetTime());
    frame.deltaTime = dt;
    frame.renderSettings = state_.renderSettings;
    frame.debugSettings = state_.debugSettings;

    float aspect = frame.height > 0
        ? static_cast<float>(frame.width) / static_cast<float>(frame.height)
        : 1.0f;
    frame.camera = state_.camera.GetData(aspect);
    return frame;
}

void App::Render(float dt) {
    int w = window_->GetWidth();
    int h = window_->GetHeight();
    if (w <= 0 || h <= 0) return; // minimized

    Scene* activeScene = state_.sceneRegistry.GetActiveScene();
    if (!activeScene) return;

    RenderSceneView sceneView = activeScene->BuildRenderSceneView();
    FrameContext frame = BuildFrameContext(dt);

    Renderer::SetClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    Renderer::Clear();

    pipeline_->Execute(sceneView, frame);
}

} // namespace HuanGL
