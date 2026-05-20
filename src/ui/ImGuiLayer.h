#pragma once
struct GLFWwindow;

namespace HuanGL {

class ImGuiLayer {
public:
    ImGuiLayer() = default;
    ~ImGuiLayer();
    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;

    void Init(GLFWwindow* window);
    void Shutdown();

    void BeginFrame();
    void EndFrame();

private:
    bool initialized_ = false;
};

} // namespace HuanGL
