#include "SceneRegistry.h"
#include "../resource/ResourceManager.h"
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <utility>

namespace HuanGL {

void SceneRegistry::RegisterRequired(std::unique_ptr<Scene> scene,
                                     std::string name,
                                     ResourceManager& resources) {
    scene->Init(resources);
    scenes_.push_back(std::move(scene));
    sceneNames_.push_back(std::move(name));
    std::printf("[App] Registered scene '%s' (index %zu)\n",
                sceneNames_.back().c_str(), scenes_.size() - 1);
}

void SceneRegistry::RegisterOptional(std::unique_ptr<Scene> scene,
                                     std::string name,
                                     ResourceManager& resources) {
    try {
        RegisterRequired(std::move(scene), std::move(name), resources);
    } catch (const std::exception& e) {
        std::printf("[App] Skipped optional scene: %s\n", e.what());
    } catch (...) {
        std::printf("[App] Skipped optional scene: unknown exception\n");
    }
}

void SceneRegistry::Cycle() {
    if (scenes_.empty()) {
        return;
    }
    activeSceneIdx_ = (activeSceneIdx_ + 1) % scenes_.size();
    std::printf("[App] Switched to scene '%s' (index %zu)\n",
                sceneNames_[activeSceneIdx_].c_str(), activeSceneIdx_);
}

const std::string& SceneRegistry::GetActiveName() const {
    if (scenes_.empty()) {
        throw std::runtime_error("[SceneRegistry] no active scene");
    }
    return sceneNames_[activeSceneIdx_];
}

Scene* SceneRegistry::GetActiveScene() {
    return scenes_.empty() ? nullptr : scenes_[activeSceneIdx_].get();
}

const Scene* SceneRegistry::GetActiveScene() const {
    return scenes_.empty() ? nullptr : scenes_[activeSceneIdx_].get();
}

World* SceneRegistry::GetActiveWorld() {
    Scene* scene = GetActiveScene();
    return scene ? &scene->GetWorld() : nullptr;
}

const World* SceneRegistry::GetActiveWorld() const {
    const Scene* scene = GetActiveScene();
    return scene ? &scene->GetWorld() : nullptr;
}

} // namespace HuanGL
