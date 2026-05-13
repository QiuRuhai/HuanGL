#pragma once
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

namespace HuanGL {

class Input {
public:
    static void Init(GLFWwindow* window);
    static void Update(); // call at start of each frame to reset per-frame deltas

    static bool IsKeyPressed(int glfwKey);
    static bool IsMouseButtonPressed(int glfwButton);
    static glm::vec2 GetMousePosition();
    static glm::vec2 GetMouseDelta();   // per-frame mouse displacement
    static float GetScrollDelta();      // per-frame scroll amount
    static void SetCursorCaptured(bool captured);

private:
    static void MouseCallback(GLFWwindow*, double x, double y);
    static void ScrollCallback(GLFWwindow*, double, double y);

    static GLFWwindow* window_;
    static glm::vec2 mousePos_;
    static glm::vec2 mouseDelta_;
    static float scrollDelta_;
    static bool firstMouse_;
};

} // namespace HuanGL
