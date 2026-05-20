#version 460 core
in vec2 vUV;
out vec4 FragColor;

layout(binding = 0) uniform sampler2D uLowInput;
layout(binding = 1) uniform sampler2D uHighInput;
uniform vec2 uLowTexelSize;
uniform float uRadius;

void main() {
    vec2 radius = uLowTexelSize * max(uRadius, 0.25);

    vec3 low = texture(uLowInput, vUV).rgb * 4.0;
    low += texture(uLowInput, vUV + vec2( radius.x, 0.0)).rgb * 2.0;
    low += texture(uLowInput, vUV + vec2(-radius.x, 0.0)).rgb * 2.0;
    low += texture(uLowInput, vUV + vec2(0.0,  radius.y)).rgb * 2.0;
    low += texture(uLowInput, vUV + vec2(0.0, -radius.y)).rgb * 2.0;
    low += texture(uLowInput, vUV + vec2( radius.x,  radius.y)).rgb;
    low += texture(uLowInput, vUV + vec2(-radius.x,  radius.y)).rgb;
    low += texture(uLowInput, vUV + vec2( radius.x, -radius.y)).rgb;
    low += texture(uLowInput, vUV + vec2(-radius.x, -radius.y)).rgb;
    low /= 16.0;

    vec3 high = texture(uHighInput, vUV).rgb;
    FragColor = vec4(high + low, 1.0);
}
