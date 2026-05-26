#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>

namespace HuanGL {

Camera::Camera(float fovDeg, float nearP, float farP)
    : fov_(glm::radians(fovDeg)), near_(nearP), far_(farP) {}

void Camera::Look(float yawDelta, float pitchDelta) {
    yaw_ += yawDelta;
    pitch_ += pitchDelta;
    pitch_ = glm::clamp(pitch_, -89.f, 89.f);

    front_.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    front_.y = sin(glm::radians(pitch_));
    front_.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    front_ = glm::normalize(front_);
}

void Camera::Move(glm::vec3 localDelta, float dt) {
    glm::vec3 right = glm::normalize(glm::cross(front_, worldUp_));
    glm::vec3 up = glm::cross(right, front_);
    float spd = moveSpeed_ * dt;
    pos_ += front_ * localDelta.z * spd;
    pos_ += right * localDelta.x * spd;
    pos_ += up * localDelta.y * spd;
}

CameraData Camera::GetData(float aspect, glm::vec2 jitter) const {
    CameraData d;
    d.view = glm::lookAt(pos_, pos_ + front_, worldUp_);
    d.unjitteredProj = glm::perspective(fov_, aspect, near_, far_);
    d.proj = d.unjitteredProj;
    d.proj[2][0] += jitter.x;
    d.proj[2][1] += jitter.y;
    d.viewProj = d.proj * d.view;
    d.unjitteredViewProj = d.unjitteredProj * d.view;
    d.invView = glm::inverse(d.view);
    d.invProj = glm::inverse(d.proj);
    d.invViewProj = glm::inverse(d.viewProj);
    d.prevViewProj = d.viewProj;
    d.jitter = glm::vec4(jitter, 0.0f, 0.0f);
    d.camPos = pos_;
    d.near_ = near_;
    d.far_ = far_;
    return d;
}

float Camera::GetFov() const { return glm::degrees(fov_); }
void Camera::SetFov(float deg) { fov_ = glm::radians(deg); }

} // namespace HuanGL
