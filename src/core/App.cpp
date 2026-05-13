#include "App.h"
#include "Window.h"
#include "Input.h"
#include "../renderer/Renderer.h"
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
}

void App::Shutdown() {}

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

void App::Update(float /*dt*/) {
    // Phase 3: SceneManager::ActiveScene->OnUpdate(dt)
}

void App::Render() {
    Renderer::SetClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    Renderer::Clear();
    // Phase 3: SceneManager::ActiveScene->OnRender()
}

} // namespace HuanGL
