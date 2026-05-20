#pragma once
#include <memory>
#include "../app/ApplicationState.h"
#include "../app/InputController.h"
#include "../renderer/FrameContext.h"

namespace HuanGL {

class Window;
class RenderPipeline;
class ResourceManager;
class ImGuiLayer;
class DebugUI;

class App {
public:
    App();
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    void Run();

private:
    void Init();
    void Shutdown();
    void Update(float dt);
    void Render(float dt);
    void RegisterScenes();
    FrameContext BuildFrameContext(float dt) const;

    std::unique_ptr<Window>          window_;
    std::unique_ptr<RenderPipeline>  pipeline_;
    std::unique_ptr<ResourceManager> resourceManager_;
    std::unique_ptr<ImGuiLayer>      imguiLayer_;
    std::unique_ptr<DebugUI>         debugUI_;

    ApplicationState state_;
    InputController inputController_;

    float lastTime_ = 0.0f;
};

} // namespace HuanGL
