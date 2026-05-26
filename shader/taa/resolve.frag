#version 460 core
in vec2 vUV;
out vec4 FragColor;

layout(binding = 0) uniform sampler2D uCurrentHdr;
layout(binding = 1) uniform sampler2D uDepth;
layout(binding = 2) uniform sampler2D uHistory;

uniform mat4 uInvViewProj;
uniform mat4 uPrevViewProj;
uniform bool uHistoryValid;
uniform float uFeedback;

vec3 WorldPosFromDepth(vec2 uv, float depth, mat4 invVP) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 world = invVP * clip;
    return world.xyz / world.w;
}

void NeighborhoodBounds(vec2 uv, out vec3 mn, out vec3 mx) {
    ivec2 size = textureSize(uCurrentHdr, 0);
    vec2 texel = 1.0 / vec2(size);
    mn = vec3(1e20);
    mx = vec3(-1e20);
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec3 c = texture(uCurrentHdr, uv + vec2(x, y) * texel).rgb;
            mn = min(mn, c);
            mx = max(mx, c);
        }
    }
}

void main() {
    vec3 current = texture(uCurrentHdr, vUV).rgb;

    if (!uHistoryValid) {
        FragColor = vec4(current, 1.0);
        return;
    }

    float depth = texture(uDepth, vUV).r;
    if (depth >= 1.0 - 1e-6) {
        FragColor = vec4(current, 1.0);
        return;
    }

    vec3 worldPos = WorldPosFromDepth(vUV, depth, uInvViewProj);
    vec4 prevClip = uPrevViewProj * vec4(worldPos, 1.0);
    if (prevClip.w <= 0.0) {
        FragColor = vec4(current, 1.0);
        return;
    }

    vec2 historyUV = prevClip.xy / prevClip.w * 0.5 + 0.5;
    if (any(lessThan(historyUV, vec2(0.0))) || any(greaterThan(historyUV, vec2(1.0)))) {
        FragColor = vec4(current, 1.0);
        return;
    }

    vec3 history = texture(uHistory, historyUV).rgb;
    vec3 mn, mx;
    NeighborhoodBounds(vUV, mn, mx);
    history = clamp(history, mn, mx);

    vec3 resolved = mix(current, history, clamp(uFeedback, 0.0, 0.98));
    FragColor = vec4(resolved, 1.0);
}
