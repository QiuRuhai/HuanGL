#pragma once
#include "../scene/Scene.h"
#include <memory>
#include <string>
#include <vector>

namespace HuanGL {

class ResourceManager;

class SceneRegistry {
public:
    void RegisterRequired(std::unique_ptr<Scene> scene,
                          std::string name,
                          ResourceManager& resources);
    void RegisterOptional(std::unique_ptr<Scene> scene,
                          std::string name,
                          ResourceManager& resources);

    void Cycle();

    bool Empty() const { return scenes_.empty(); }
    size_t GetActiveIndex() const { return activeSceneIdx_; }
    const std::string& GetActiveName() const;

    Scene* GetActiveScene();
    const Scene* GetActiveScene() const;
    World* GetActiveWorld();
    const World* GetActiveWorld() const;

private:
    std::vector<std::unique_ptr<Scene>> scenes_;
    std::vector<std::string> sceneNames_;
    size_t activeSceneIdx_ = 0;
};

} // namespace HuanGL
