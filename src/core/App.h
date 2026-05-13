#pragma once
#include <memory>

namespace HuanGL {

class Window;

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

    std::unique_ptr<Window> window_;
    float lastTime_ = 0.0f;
    bool  running_  = true;
};

} // namespace HuanGL
