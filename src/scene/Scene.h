#pragma once
#include "World.h"

namespace HuanGL {

class ResourceManager;

class Scene {
public:
    virtual ~Scene() = default;
    virtual void Init(ResourceManager& rm) = 0;
    virtual void Update(float dt) { world_.Update(dt); }

    World& GetWorld() { return world_; }
    const World& GetWorld() const { return world_; }

    RenderSceneView BuildRenderSceneView() const {
        return world_.BuildRenderSceneView();
    }

    const DirectionalLight& GetSunLight() const {
        return world_.GetSunLight();
    }
    const glm::vec3& GetAmbient() const {
        return world_.GetAmbient();
    }

protected:
    World world_;
};

} // namespace HuanGL
