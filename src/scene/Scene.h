#pragma once
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "../renderer/Schema.h"

namespace HuanGL {

class ResourceManager;

class Scene {
public:
    virtual ~Scene() = default;
    virtual void Init(ResourceManager& rm) = 0;
    virtual void Update(float dt) { (void)dt; }

    size_t GetMeshCount() const { return meshPtrs_.size(); }
    Mesh*  GetMesh(size_t i) const { return meshPtrs_[i]; }
    glm::mat4 GetModelMatrix(size_t i) const { return modelMatrices_[i]; }

    const std::vector<Material>&    GetMaterials() const { return materials_; }
    const DirectionalLight&         GetSunLight()  const { return sunLight_; }
    const glm::vec3&                GetAmbient()   const { return ambient_; }

protected:
    std::vector<std::shared_ptr<Mesh>> meshesOwned_;
    std::vector<Mesh*>                 meshPtrs_;
    std::vector<glm::mat4>             modelMatrices_;
    std::vector<Material>              materials_;
    DirectionalLight                   sunLight_;
    glm::vec3                          ambient_ = {0.03f, 0.03f, 0.05f};

    void SyncPtrs() {
        meshPtrs_.clear();
        for (auto& m : meshesOwned_) meshPtrs_.push_back(m.get());
    }
};

} // namespace HuanGL
