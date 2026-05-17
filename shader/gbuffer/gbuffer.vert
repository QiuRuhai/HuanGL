#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aTangent;

uniform mat4 model;
uniform mat4 viewProj;

out vec3 vWorldNormal;
out vec3 vWorldTangent;
out vec2 vTexCoord;

void main() {
    vec4 worldPos = model * vec4(aPos, 1.0);
    gl_Position   = viewProj * worldPos;

    // Note: mat3(model) is only correct for orthogonal transforms (rotation +
    // uniform scale). For non-uniform scale, transpose(inverse(mat3(model)))
    // would be needed. Pre-existing limitation across the codebase.
    mat3 normalMat = mat3(model);

    vWorldNormal = normalMat * aNormal;
    vWorldTangent = normalMat * aTangent;
    // Do NOT normalize or orthogonalize here — interpolation breaks both.
    // Fragment shader handles normalization and re-orthogonalization.

    vTexCoord = aTexCoord;
}
