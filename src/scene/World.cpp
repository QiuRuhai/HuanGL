#include "World.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <utility>

namespace HuanGL {

glm::mat4 Transform::ToMatrix() const {
    glm::mat4 t = glm::translate(glm::mat4(1.0f), translation);
    glm::mat4 r = glm::toMat4(rotation);
    glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);
    return t * r * s;
}

void World::Clear() {
    entities_.clear();
    materials_.clear();
    sunLight_ = DirectionalLight {};
    ambient_ = {0.03f, 0.03f, 0.05f};
    nextEntityId_ = 1;
}

void World::Update(float dt) {
    (void)dt;
}

Entity& World::CreateEntity(std::string name) {
    Entity entity;
    entity.id = nextEntityId_++;
    entity.name = std::move(name);
    entities_.push_back(std::move(entity));
    return entities_.back();
}

RenderSceneView World::BuildRenderSceneView() const {
    RenderSceneView view;
    view.sunLight = sunLight_;
    view.ambient = ambient_;
    for (const auto& entity : entities_) {
        if (!entity.meshRenderer || !entity.meshRenderer->mesh) {
            continue;
        }

        Renderable renderable;
        renderable.mesh = entity.meshRenderer->mesh.get();
        renderable.materials = &materials_;
        renderable.modelMatrix = entity.transform.ToMatrix();
        view.renderables.push_back(renderable);
    }
    return view;
}

} // namespace HuanGL
