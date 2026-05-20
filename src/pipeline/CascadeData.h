#pragma once
#include <glm/glm.hpp>

namespace HuanGL {

struct CascadeData {
    glm::mat4 viewProj;
    float farPlane = 0.0f;
    float pad[3] = {};
};

} // namespace HuanGL
