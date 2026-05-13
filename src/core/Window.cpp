#include "Window.h"
#include <stdexcept>
#include <iostream>

namespace HuanGL {

Window::Window(int width, int height, const std::string& title)
    : width_(width), height_(height)
{
    if (!glfwInit())
        throw std::runtime_error("Failed to initialize GLFW");
    glfw_owned_ = true;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 1);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

    handle_ = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!handle_) {
        glfwTerminate();
        glfw_owned_ = false;
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(handle_);
    glfwSetWindowUserPointer(handle_, this);
    glfwSetFramebufferSizeCallback(handle_, FramebufferSizeCallback);

    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {
        glfwDestroyWindow(handle_);
        handle_ = nullptr;
        glfwTerminate();
        glfw_owned_ = false;
        throw std::runtime_error("Failed to initialize GLAD");
    }

    std::cout << "[HuanGL] OpenGL " << reinterpret_cast<const char*>(glGetString(GL_VERSION))
              << " | " << reinterpret_cast<const char*>(glGetString(GL_RENDERER)) << "\n";
}

Window::~Window() {
    if (handle_) glfwDestroyWindow(handle_);
    if (glfw_owned_) glfwTerminate();
}

bool Window::ShouldClose() const { return glfwWindowShouldClose(handle_); }
void Window::SwapBuffers() const { glfwSwapBuffers(handle_); }
void Window::PollEvents() { glfwPollEvents(); }

void Window::SetResizeCallback(std::function<void(int, int)> cb) {
    resizeCb_ = std::move(cb);
}

void Window::FramebufferSizeCallback(GLFWwindow* w, int width, int height) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
    self->width_  = width;
    self->height_ = height;
    if (self->resizeCb_) self->resizeCb_(width, height);
}

} // namespace HuanGL
