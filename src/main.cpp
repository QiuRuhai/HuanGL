#include "core/Window.h"
#include "core/Input.h"
#include <iostream>

int main() {
    try {
        HuanGL::Window window(1280, 720, "HuanGL");
        HuanGL::Input::Init(window.GetHandle());

        while (!window.ShouldClose()) {
            HuanGL::Input::Update();
            window.PollEvents();

            if (HuanGL::Input::IsKeyPressed(GLFW_KEY_ESCAPE))
                break;

            glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            window.SwapBuffers();
        }
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return -1;
    }
    return 0;
}
