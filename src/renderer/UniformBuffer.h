#pragma once
#include "Buffer.h"
#include <glm/glm.hpp>

namespace HuanGL {

// CPU-side mirror of shader/common/uniforms.glsl — must stay in sync.

struct CameraData {
    glm::mat4 view {};
    glm::mat4 proj {};
    glm::mat4 viewProj {};
    glm::mat4 invView {};
    glm::mat4 invProj {};
    glm::mat4 invViewProj {};
    glm::mat4 unjitteredProj {};
    glm::mat4 unjitteredViewProj {};
    glm::mat4 prevViewProj {}; // previous stable view-projection for TAA history
    glm::vec4 jitter {}; // xy = current jitter, zw = previous jitter
    glm::vec3 camPos {};
    float near_ = 0.1f;
    float far_ = 100.f;
    float pad[3] = {};
};

struct LightsData {
    glm::vec3 dirLightDir       = {0.f, -1.f, 0.f};
    float     pad0              = 0.f;
    glm::vec3 dirLightColor     = {1.f,  1.f, 1.f};
    float     dirLightIntensity = 1.f;
};

struct TimeData {
    float time      = 0.f;
    float deltaTime = 0.f;
    float pad[2]    = {};
};

// Thin wrapper: uploads T to a UBO at a fixed binding point.
template<typename T, GLuint BindingPoint>
class UniformBuffer {
public:
    UniformBuffer() : buffer_(GL_UNIFORM_BUFFER, GL_DYNAMIC_DRAW) {
        buffer_.Upload(nullptr, sizeof(T));
        buffer_.BindBase(BindingPoint);
    }

    void Update(const T& data) {
        buffer_.UpdateSubData(&data, sizeof(T));
    }

private:
    Buffer buffer_;
};

using CameraUBO = UniformBuffer<CameraData, 0>;
using LightsUBO = UniformBuffer<LightsData, 1>;
using TimeUBO   = UniformBuffer<TimeData,   2>;

} // namespace HuanGL
