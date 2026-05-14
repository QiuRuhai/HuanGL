#version 460 core
in vec3 vWorldNormal;
in vec2 vTexCoord;

layout(location = 0) out vec4 OutAlbedoMetallic;
layout(location = 1) out vec4 OutNormalRoughness;

uniform sampler2D uAlbedoMap;
uniform sampler2D uRoughnessMap;
uniform sampler2D uMetallicMap;
uniform vec4  uBaseColor;
uniform float uRoughness;
uniform float uMetallic;
uniform int   uHasAlbedoTex;
uniform int   uHasRoughnessTex;
uniform int   uHasMetallicTex;

void main() {
    vec4 albedo = uHasAlbedoTex > 0 ? texture(uAlbedoMap, vTexCoord) : vec4(1.0);
    albedo *= uBaseColor;

    float roughness = uHasRoughnessTex > 0
        ? texture(uRoughnessMap, vTexCoord).r : uRoughness;
    float metallic  = uHasMetallicTex > 0
        ? texture(uMetallicMap, vTexCoord).r  : uMetallic;

    vec3 N = normalize(vWorldNormal);

    OutAlbedoMetallic   = vec4(albedo.rgb, metallic);
    OutNormalRoughness  = vec4(N, roughness);
}
