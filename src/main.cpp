#include "core/Window.h"
#include <iostream>

int main() {
    try {
        HuanGL::Window window(1280, 720, "HuanGL");
        while (!window.ShouldClose()) {
            glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            window.SwapBuffers();
            window.PollEvents();
        }
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return -1;
    }
    return 0;
}
