#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

uniform mat4 model;
uniform mat4 viewProj;

out vec3 vWorldNormal;
out vec2 vTexCoord;

void main() {
    vec4 worldPos = model * vec4(aPos, 1.0);
    gl_Position   = viewProj * worldPos;
    vWorldNormal  = mat3(model) * aNormal;
    vTexCoord     = aTexCoord;
}
