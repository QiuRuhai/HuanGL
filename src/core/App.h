#pragma once
#include <memory>
#include "../renderer/UniformBuffer.h"

namespace HuanGL {

class Window;
class RenderPipeline;
class Scene;
class Camera;
class ResourceManager;

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

    std::unique_ptr<Window>          window_;
    std::unique_ptr<Camera>          camera_;
    std::unique_ptr<Scene>           scene_;
    std::unique_ptr<RenderPipeline>  pipeline_;
    std::unique_ptr<ResourceManager> resourceManager_;

    std::unique_ptr<CameraUBO> cameraUBO_;
    std::unique_ptr<LightsUBO> lightsUBO_;
    std::unique_ptr<TimeUBO>   timeUBO_;

    float lastTime_ = 0.0f;
    bool  running_  = true;
};

} // namespace HuanGL
