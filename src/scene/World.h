#pragma once
#include "../renderer/RenderSceneView.h"
#include "../renderer/Schema.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace HuanGL {

using EntityId = uint32_t;

struct Transform {
    glm::vec3 translation = {0.0f, 0.0f, 0.0f};
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale = {1.0f, 1.0f, 1.0f};

    glm::mat4 ToMatrix() const;
};

struct MeshRenderer {
    std::shared_ptr<Mesh> mesh;
};

struct Entity {
    EntityId id = 0;
    std::string name;
    Transform transform;
    std::optional<MeshRenderer> meshRenderer;
};

class World {
public:
    void Clear();
    void Update(float dt);

    Entity& CreateEntity(std::string name);

    std::vector<Entity>& GetEntities() { return entities_; }
    const std::vector<Entity>& GetEntities() const { return entities_; }

    std::vector<Material>& GetMaterials() { return materials_; }
    const std::vector<Material>& GetMaterials() const { return materials_; }

    DirectionalLight& GetSunLight() { return sunLight_; }
    const DirectionalLight& GetSunLight() const { return sunLight_; }

    glm::vec3& GetAmbient() { return ambient_; }
    const glm::vec3& GetAmbient() const { return ambient_; }

    RenderSceneView BuildRenderSceneView() const;

private:
    std::vector<Entity> entities_;
    std::vector<Material> materials_;
    DirectionalLight sunLight_;
    glm::vec3 ambient_ = {0.03f, 0.03f, 0.05f};
    EntityId nextEntityId_ = 1;
};

} // namespace HuanGL
