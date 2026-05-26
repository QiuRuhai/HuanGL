// Shared UBO definitions — include in every shader that needs camera/lights/time.
// Binding points are fixed; do not change them.

layout(std140, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    mat4 invView;
    mat4 invProj;
    mat4 invViewProj;
    mat4 unjitteredProj;
    mat4 unjitteredViewProj;
    mat4 prevViewProj;
    vec4 jitter; // xy = current jitter, zw = previous jitter
    vec3 camPos;
    float near_;
    float far_;
    float _pad[3];
};

layout(std140, binding = 1) uniform LightsUBO {
    vec3  dirLightDir;
    float _pad0;
    vec3  dirLightColor;
    float dirLightIntensity;
};

layout(std140, binding = 2) uniform TimeUBO {
    float time;
    float deltaTime;
    float _pad[2];
};
