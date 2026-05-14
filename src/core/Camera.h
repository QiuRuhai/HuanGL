#pragma once
#include "../renderer/UniformBuffer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>

namespace HuanGL {

class Camera {
public:
    Camera(float fovDeg = 60.f, float nearP = 0.1f, float farP = 100.f)
        : fov_(glm::radians(fovDeg)), near_(nearP), far_(farP) {}

    void Update(float dt, GLFWwindow* window, bool capture = true) {
        if (capture) {
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);
            float x = (float)mx, y = (float)my;
            if (first_) { lastX_ = x; lastY_ = y; first_ = false; }
            float dx = x - lastX_, dy = lastY_ - y;
            lastX_ = x; lastY_ = y;
            yaw_ += dx * 0.1f; pitch_ += dy * 0.1f;
            pitch_ = glm::clamp(pitch_, -89.f, 89.f);
        }
        front_.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
        front_.y = sin(glm::radians(pitch_));
        front_.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));
        front_ = glm::normalize(front_);
        glm::vec3 right = glm::normalize(glm::cross(front_, worldUp_));
        glm::vec3 up    = glm::cross(right, front_);
        float spd = moveSpeed_ * dt;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) pos_ += front_ * spd;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) pos_ -= front_ * spd;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) pos_ -= right * spd;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) pos_ += right * spd;
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) pos_ += up * spd;
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) pos_ -= up * spd;
    }

    CameraData GetData(float aspect) const {
        CameraData d;
        d.view     = glm::lookAt(pos_, pos_ + front_, worldUp_);
        d.proj     = glm::perspective(fov_, aspect, near_, far_);
        d.viewProj = d.proj * d.view;
        d.invView  = glm::inverse(d.view);
        d.invProj  = glm::inverse(d.proj);
        d.camPos   = pos_;
        d.near_    = near_;
        d.far_     = far_;
        return d;
    }

    glm::vec3 GetPos() const { return pos_; }

private:
    glm::vec3 pos_ = {0, 3, 10}, front_ = {0, 0, -1}, worldUp_ = {0, 1, 0};
    float yaw_ = -90, pitch_ = 0, fov_, near_, far_, moveSpeed_ = 5;
    float lastX_ = 0, lastY_ = 0;
    bool first_ = true;
};

} // namespace HuanGL
