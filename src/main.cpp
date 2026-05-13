#include "core/Window.h"
#include "core/Input.h"
#include "renderer/Renderer.h"
#include "renderer/Shader.h"
#include <iostream>

int main() {
    try {
        HuanGL::Window window(1280, 720, "HuanGL");
        HuanGL::Input::Init(window.GetHandle());
        HuanGL::Renderer::Init();
        HuanGL::Renderer::SetViewport(0, 0, window.GetWidth(), window.GetHeight());

        HuanGL::Shader testShader("../shader/test/test.vert", "../shader/test/test.frag");
        std::cout << "[Test] Shader compiled OK, id=" << testShader.GetID() << "\n";

        while (!window.ShouldClose()) {
            HuanGL::Input::Update();
            window.PollEvents();
            if (HuanGL::Input::IsKeyPressed(GLFW_KEY_ESCAPE)) break;
            HuanGL::Renderer::SetClearColor(0.1f, 0.1f, 0.15f, 1.0f);
            HuanGL::Renderer::Clear();
            window.SwapBuffers();
        }
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return -1;
    }
    return 0;
}
