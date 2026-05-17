#version 460 core
in vec2 vUV;
out vec4 FragColor;

layout(binding = 0) uniform sampler2D uHDRInput;
layout(binding = 1) uniform sampler2D uAlbedoMetallic;
layout(binding = 2) uniform sampler2D uNormalRoughness;
layout(binding = 3) uniform sampler2D uDepth;

uniform int uToneMapMode;  // 0=ACES, 1=Reinhard, 2=None
uniform int uDebugMode;    // 0=Final, 1=Albedo, 2=Normal, 3=Roughness, 4=Metallic, 5=Depth, 6=Cascades

uniform mat4 uView;
uniform mat4 uInvViewProj;
uniform float uCascadeFar[4];
uniform float uNearPlane;
uniform float uFarPlane;

// ACES Filmic curve, fit by Krzysztof Narkowicz.
// Input is linear HDR radiance; output is in [0,1] approximately film-mapped.
vec3 ACESFilmic(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 Reinhard(vec3 x) {
    return x / (x + vec3(1.0));
}

// Convert sampled depth-buffer value to view-space distance.
// Assumes a standard perspective projection with NDC depth in [-1, 1].
float LinearizeDepth(float d, float zNear, float zFar) {
    float ndc = d * 2.0 - 1.0;
    return (2.0 * zNear * zFar) / (zFar + zNear - ndc * (zFar - zNear));
}

// Reconstruct world-space position from screen UV + depth-buffer value.
vec3 WorldPosFromDepth(vec2 uv, float depth, mat4 invVP) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 world = invVP * clip;
    return world.xyz / world.w;
}

void main() {
    if (uDebugMode == 0) {
        vec3 hdr = texture(uHDRInput, vUV).rgb;
        vec3 color;
        if (uToneMapMode == 0)      color = ACESFilmic(hdr);
        else if (uToneMapMode == 1) color = Reinhard(hdr);
        else                        color = clamp(hdr, 0.0, 1.0);
        // sRGB-ish gamma. (Could use a precise sRGB transfer instead.)
        color = pow(color, vec3(1.0 / 2.2));
        FragColor = vec4(color, 1.0);
        return;
    }

    if (uDebugMode == 1) {
        FragColor = vec4(texture(uAlbedoMetallic, vUV).rgb, 1.0);
        return;
    }
    if (uDebugMode == 2) {
        vec3 N = texture(uNormalRoughness, vUV).rgb;
        FragColor = vec4(N * 0.5 + 0.5, 1.0);
        return;
    }
    if (uDebugMode == 3) {
        float r = texture(uNormalRoughness, vUV).a;
        FragColor = vec4(vec3(r), 1.0);
        return;
    }
    if (uDebugMode == 4) {
        float m = texture(uAlbedoMetallic, vUV).a;
        FragColor = vec4(vec3(m), 1.0);
        return;
    }
    if (uDebugMode == 5) {
        // Linearized depth, then a sqrt curve so close objects are visible
        // and far objects aren't crushed to white. Background (depth=1) → 1.
        float d = texture(uDepth, vUV).r;
        float lin = LinearizeDepth(d, uNearPlane, uFarPlane);
        float vis = sqrt(clamp(lin / uFarPlane, 0.0, 1.0));
        FragColor = vec4(vec3(vis), 1.0);
        return;
    }
    if (uDebugMode == 6) {
        float d = texture(uDepth, vUV).r;
        // Background pixels (depth = 1.0) have no cascade — show plain albedo.
        if (d >= 1.0 - 1e-6) {
            FragColor = vec4(texture(uAlbedoMetallic, vUV).rgb, 1.0);
            return;
        }
        vec3 worldPos = WorldPosFromDepth(vUV, d, uInvViewProj);
        float viewZ = -(uView * vec4(worldPos, 1.0)).z;
        vec3 cascadeColors[4] = vec3[4](
            vec3(1.0, 0.3, 0.3),  // cascade 0 (nearest)
            vec3(0.3, 1.0, 0.3),  // cascade 1
            vec3(0.3, 0.3, 1.0),  // cascade 2
            vec3(1.0, 1.0, 0.3)   // cascade 3 (farthest)
        );
        int cascade = 3;
        for (int c = 0; c < 4; ++c) {
            if (viewZ < uCascadeFar[c]) { cascade = c; break; }
        }
        vec3 base = texture(uAlbedoMetallic, vUV).rgb;
        FragColor = vec4(mix(base, cascadeColors[cascade], 0.5), 1.0);
        return;
    }

    // Unknown mode — emit magenta so it's obvious.
    FragColor = vec4(1.0, 0.0, 1.0, 1.0);
}
