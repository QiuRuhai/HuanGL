#pragma once
#include "../renderer/UniformBuffer.h"
#include <glm/glm.hpp>

namespace HuanGL {

class Camera {
public:
    Camera(float fovDeg = 60.f, float nearP = 0.1f, float farP = 100.f);

    void Look(float yawDelta, float pitchDelta);
    void Move(glm::vec3 localDelta, float dt);

    CameraData GetData(float aspect) const;

    glm::vec3 GetPosition() const { return pos_; }
    float GetFov() const;
    void SetFov(float deg);

private:
    glm::vec3 pos_ = {0, 3, 10};
    glm::vec3 front_ = {0, 0, -1};
    glm::vec3 worldUp_ = {0, 1, 0};
    float yaw_ = -90.f;
    float pitch_ = 0.f;
    float fov_;
    float near_;
    float far_;
    float moveSpeed_ = 5.f;
};

} // namespace HuanGL
