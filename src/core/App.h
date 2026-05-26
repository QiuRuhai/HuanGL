#pragma once
#include <cstdint>
#include <memory>
#include <glm/glm.hpp>
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
    glm::vec2 ComputeTAAJitter(int width, int height) const;
    void StorePreviousCameraState(const CameraData& camera);
    void InvalidateTemporalHistory();
    FrameContext BuildFrameContext(float dt);

    std::unique_ptr<Window>          window_;
    std::unique_ptr<RenderPipeline>  pipeline_;
    std::unique_ptr<ResourceManager> resourceManager_;
    std::unique_ptr<ImGuiLayer>      imguiLayer_;
    std::unique_ptr<DebugUI>         debugUI_;

    ApplicationState state_;
    InputController inputController_;

    float lastTime_ = 0.0f;
    glm::mat4 previousStableViewProj_ = glm::mat4(1.0f);
    glm::vec2 previousJitter_ = glm::vec2(0.0f);
    uint32_t taaFrameIndex_ = 0;
    bool hasPreviousCamera_ = false;
};

} // namespace HuanGL
