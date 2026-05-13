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

    bool ShouldClose() const;
    void SwapBuffers() const;
    void PollEvents() const;

    int GetWidth() const  { return width_; }
    int GetHeight() const { return height_; }
    GLFWwindow* GetHandle() const { return handle_; }

    void SetResizeCallback(std::function<void(int, int)> cb);

private:
    GLFWwindow* handle_ = nullptr;
    int width_, height_;
    std::function<void(int, int)> resizeCb_;

    static void FramebufferSizeCallback(GLFWwindow* w, int width, int height);
};

} // namespace HuanGL
