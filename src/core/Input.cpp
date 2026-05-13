#include "Input.h"

namespace HuanGL {

GLFWwindow* Input::window_      = nullptr;
glm::vec2   Input::mousePos_    = {0, 0};
glm::vec2   Input::mouseDelta_  = {0, 0};
float       Input::scrollDelta_ = 0.0f;
bool        Input::firstMouse_  = true;

void Input::Init(GLFWwindow* window) {
    window_ = window;
    glfwSetCursorPosCallback(window, MouseCallback);
    glfwSetScrollCallback(window, ScrollCallback);
}

void Input::Update() {
    mouseDelta_  = {0.0f, 0.0f};
    scrollDelta_ = 0.0f;
}

bool Input::IsKeyPressed(int key) {
    return glfwGetKey(window_, key) == GLFW_PRESS;
}

bool Input::IsMouseButtonPressed(int button) {
    return glfwGetMouseButton(window_, button) == GLFW_PRESS;
}

glm::vec2 Input::GetMousePosition() { return mousePos_; }
glm::vec2 Input::GetMouseDelta()    { return mouseDelta_; }
float     Input::GetScrollDelta()   { return scrollDelta_; }

void Input::SetCursorCaptured(bool captured) {
    glfwSetInputMode(window_, GLFW_CURSOR,
        captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    firstMouse_ = true; // prevent jump on re-capture
}

void Input::MouseCallback(GLFWwindow*, double x, double y) {
    glm::vec2 pos = {static_cast<float>(x), static_cast<float>(y)};
    if (firstMouse_) {
        mousePos_   = pos;
        firstMouse_ = false;
    }
    mouseDelta_ = pos - mousePos_;
    mouseDelta_.y = -mouseDelta_.y; // flip Y so positive = up
    mousePos_   = pos;
}

void Input::ScrollCallback(GLFWwindow*, double, double y) {
    scrollDelta_ = static_cast<float>(y);
}

} // namespace HuanGL
