#pragma once
#include <memory>
#include <string>
#include <vector>
#include "../renderer/UniformBuffer.h"

namespace HuanGL {

class Window;
class RenderPipeline;
class Scene;
class Camera;
class ResourceManager;
class ImGuiLayer;

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
    void Render();
    void RegisterScene(std::unique_ptr<Scene> scene, std::string name);
    void CycleScene();
    void HandleHotkeys();
    void BuildDebugPanel();

    std::unique_ptr<Window>          window_;
    std::unique_ptr<Camera>          camera_;
    std::unique_ptr<RenderPipeline>  pipeline_;
    std::unique_ptr<ResourceManager> resourceManager_;
    std::unique_ptr<ImGuiLayer>      imguiLayer_;

    // All registered scenes; index `activeSceneIdx_` is the live one.
    std::vector<std::unique_ptr<Scene>> scenes_;
    std::vector<std::string>            sceneNames_;
    size_t                              activeSceneIdx_ = 0;

    std::unique_ptr<CameraUBO> cameraUBO_;
    std::unique_ptr<LightsUBO> lightsUBO_;
    std::unique_ptr<TimeUBO>   timeUBO_;

    float lastTime_ = 0.0f;
    bool  running_  = true;
};

} // namespace HuanGL
