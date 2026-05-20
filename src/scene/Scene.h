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

    size_t GetMeshCount() const { return legacyView_.renderables.size(); }
    Mesh* GetMesh(size_t i) const {
        return const_cast<Mesh*>(legacyView_.renderables[i].mesh);
    }
    glm::mat4 GetModelMatrix(size_t i) const {
        return legacyView_.renderables[i].modelMatrix;
    }

    const std::vector<Material>& GetMaterials() const {
        return world_.GetMaterials();
    }
    const DirectionalLight& GetSunLight() const {
        return world_.GetSunLight();
    }
    DirectionalLight& GetMutableSunLight() {
        return world_.GetSunLight();
    }
    const glm::vec3& GetAmbient() const {
        return world_.GetAmbient();
    }

protected:
    World world_;
    RenderSceneView legacyView_;

    void SyncPtrs() {
        legacyView_ = world_.BuildRenderSceneView();
    }
};

} // namespace HuanGL
