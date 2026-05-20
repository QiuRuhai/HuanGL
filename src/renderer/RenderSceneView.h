#pragma once
#include "Schema.h"
#include <glm/glm.hpp>
#include <vector>

namespace HuanGL {

struct Renderable {
    const Mesh* mesh = nullptr;
    const std::vector<Material>* materials = nullptr;
    glm::mat4 modelMatrix = glm::mat4(1.0f);
};

struct RenderSceneView {
    std::vector<Renderable> renderables;
    DirectionalLight sunLight;
    glm::vec3 ambient = {0.03f, 0.03f, 0.05f};
};

} // namespace HuanGL
