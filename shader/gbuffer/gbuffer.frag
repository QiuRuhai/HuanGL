#version 460 core
in vec3 vWorldNormal;
in vec3 vWorldTangent;
in vec2 vTexCoord;

layout(location = 0) out vec4 OutAlbedoMetallic;
layout(location = 1) out vec4 OutNormalRoughness;

layout(binding = 0) uniform sampler2D uAlbedoMap;
layout(binding = 1) uniform sampler2D uRoughnessMap;
layout(binding = 2) uniform sampler2D uMetallicMap;
layout(binding = 3) uniform sampler2D uNormalMap;
uniform vec4  uBaseColor;
uniform float uRoughness;
uniform float uMetallic;
uniform int   uHasAlbedoTex;
uniform int   uHasRoughnessTex;
uniform int   uHasMetallicTex;
uniform int   uHasNormalTex;
uniform int   uPackedMetallicRoughness;

void main() {
    vec4 albedo = uHasAlbedoTex > 0 ? texture(uAlbedoMap, vTexCoord) : vec4(1.0);
    albedo *= uBaseColor;

    float roughness, metallic;
    if (uPackedMetallicRoughness > 0) {
        // glTF spec: B=metallic, G=roughness in metallicRoughness texture.
        // NOTE: 'packed' is a reserved word in GLSL — use 'mrSample' instead.
        vec4 mrSample = texture(uRoughnessMap, vTexCoord);
        roughness = mrSample.g * uRoughness;
        metallic  = mrSample.b * uMetallic;
    } else {
        roughness = uHasRoughnessTex > 0
            ? texture(uRoughnessMap, vTexCoord).r : uRoughness;
        metallic  = uHasMetallicTex > 0
            ? texture(uMetallicMap, vTexCoord).r  : uMetallic;
    }

    // Always normalize the interpolated normal first.
    vec3 N = normalize(vWorldNormal);

    // Apply normal map only if texture present AND tangent is valid (non-zero).
    // Procedural meshes (e.g. spheres in TestScene) can have zero tangents
    // which would produce NaN if normalized.
    if (uHasNormalTex > 0 && dot(vWorldTangent, vWorldTangent) > 1e-6) {
        // Re-orthogonalize T against N (Gram-Schmidt) in fragment shader,
        // because vertex interpolation does not preserve orthogonality.
        vec3 T = normalize(vWorldTangent - dot(vWorldTangent, N) * N);
        vec3 B = cross(N, T);
        mat3 TBN = mat3(T, B, N);

        vec3 tangentNormal = texture(uNormalMap, vTexCoord).rgb * 2.0 - 1.0;
        N = normalize(TBN * tangentNormal);
    }

    OutAlbedoMetallic   = vec4(albedo.rgb, metallic);
    OutNormalRoughness  = vec4(N, roughness);
}
