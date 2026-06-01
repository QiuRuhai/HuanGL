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

    // Normals transform by the inverse-transpose of the model matrix, which
    // stays correct under non-uniform scale (plain mat3(model) skews normals
    // when scale is anisotropic). Computed per-vertex here for learning
    // clarity; production code precomputes a normalMatrix on the CPU and
    // uploads it once per draw instead of inverting per vertex.
    mat3 normalMat = transpose(inverse(mat3(model)));

    vWorldNormal = normalMat * aNormal;
    vWorldTangent = normalMat * aTangent;
    // Do NOT normalize or orthogonalize here — interpolation breaks both.
    // Fragment shader handles normalization and re-orthogonalization.

    vTexCoord = aTexCoord;
}
