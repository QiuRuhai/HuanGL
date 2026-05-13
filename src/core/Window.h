#pragma once
#include <functional>
#include <string>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace HuanGL {

class Window {
public:
    Window(int width, int height, const std::string& title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    Window(Window&& other) noexcept
        : handle_(other.handle_), width_(other.width_), height_(other.height_),
          glfw_owned_(other.glfw_owned_), resizeCb_(std::move(other.resizeCb_))
    {
        other.handle_ = nullptr;
        other.glfw_owned_ = false;
        if (handle_) glfwSetWindowUserPointer(handle_, this);
    }
    Window& operator=(Window&&) = delete; // single window per app

    bool ShouldClose() const;
    void SwapBuffers() const;
    void PollEvents();

    int GetWidth() const  { return width_; }
    int GetHeight() const { return height_; }
    GLFWwindow* GetHandle() const { return handle_; }

    void SetResizeCallback(std::function<void(int, int)> cb);

private:
    GLFWwindow* handle_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    bool glfw_owned_ = false;
    std::function<void(int, int)> resizeCb_;

    static void FramebufferSizeCallback(GLFWwindow* w, int width, int height);
};

} // namespace HuanGL
