#version 460 core
in vec2 vUV;
out vec4 FragColor;

layout(binding = 0) uniform sampler2D uAlbedoMetallic;
layout(binding = 1) uniform sampler2D uNormalRoughness;
layout(binding = 2) uniform sampler2D uDepth;

layout(binding = 3) uniform sampler2DArrayShadow uShadowMap;
uniform mat4 uCascadeViewProj[4];
uniform float uCascadeFar[4];
uniform vec3 uLightDir;
uniform vec3 uLightColor;

layout(binding = 4) uniform samplerCube uIrradianceMap;
layout(binding = 5) uniform samplerCube uPrefilterMap;
layout(binding = 6) uniform sampler2D uBRDFLUT;

uniform mat4 uView;
uniform mat4 uInvViewProj;
uniform vec3 uCamPos;
uniform float uAmbientStrength;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness, a2 = a * a;
    float NdotH = max(dot(N, H), 0.0), denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0, k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness) *
           GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// 3x3 PCF over the chosen cascade. (Plan called this PCSS but no blocker
// search / penumbra estimation is implemented — true PCSS is a TODO.)
float PCF(sampler2DArrayShadow shadowMap, vec3 worldPos) {
    // Cascade selection by view-space Z so screen-edge points don't get
    // pushed into a higher cascade than necessary.
    float viewZ = -(uView * vec4(worldPos, 1.0)).z;
    int cascade = 3;
    for (int c = 0; c < 4; ++c) { if (viewZ < uCascadeFar[c]) { cascade = c; break; } }
    vec4 lightSpace = uCascadeViewProj[cascade] * vec4(worldPos, 1);
    vec3 proj = lightSpace.xyz / lightSpace.w;
    proj = proj * 0.5 + 0.5;
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0 || proj.z > 1.0) return 1.0;
    float shadow = 0;
    vec2 ts = 1.0 / vec2(textureSize(uShadowMap, 0));
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
            shadow += texture(uShadowMap, vec4(proj.xy + vec2(x,y) * ts, cascade, proj.z));
    return shadow / 9.0;
}

vec3 WorldPosFromDepth(vec2 uv, float depth, mat4 invVP) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 world = invVP * clip;
    return world.xyz / world.w;
}

void main() {
    vec4 albedoMetallic  = texture(uAlbedoMetallic, vUV);
    vec4 normalRoughness = texture(uNormalRoughness, vUV);
    float depth          = texture(uDepth, vUV).r;

    vec3 albedo    = albedoMetallic.rgb;
    float metallic = albedoMetallic.a;
    vec3 N         = normalize(normalRoughness.xyz);
    float roughness = max(normalRoughness.a, 0.04);

    vec3 worldPos = WorldPosFromDepth(vUV, depth, uInvViewProj);
    vec3 V = normalize(uCamPos - worldPos);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 L = normalize(-uLightDir);
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    vec3 radiance = uLightColor;
    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
    vec3 specular = numerator / denominator;
    vec3 kD = (1.0 - F) * (1.0 - metallic);
    vec3 direct = (kD * albedo / PI + specular) * radiance * NdotL;
    float shadow = PCF(uShadowMap, worldPos);
    direct *= shadow;

    vec3 kS = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    vec3 kDIbl = (1.0 - kS) * (1.0 - metallic);
    vec3 irradiance = texture(uIrradianceMap, N).rgb;
    vec3 diffuseIBL = kDIbl * irradiance * albedo;
    vec3 R = reflect(-V, N);
    float maxReflectionLod = 4.0;
    vec3 prefiltered = textureLod(uPrefilterMap, R, roughness * maxReflectionLod).rgb;
    vec2 envBRDF = texture(uBRDFLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 specularIBL = prefiltered * (kS * envBRDF.x + envBRDF.y);
    vec3 ambient = (diffuseIBL + specularIBL) * uAmbientStrength;
    vec3 color = direct + ambient;
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));
    FragColor = vec4(color, 1.0);
}
