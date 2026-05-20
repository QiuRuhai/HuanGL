#version 460 core
in vec2 vUV;
out vec4 FragColor;

layout(binding = 0) uniform sampler2D uInput;
uniform bool uHorizontal;
uniform int uRadius;
uniform vec2 uTexelSize;

void main() {
    int radius = clamp(uRadius, 1, 16);
    vec2 direction = uHorizontal ? vec2(uTexelSize.x, 0.0)
                                 : vec2(0.0, uTexelSize.y);

    vec3 color = texture(uInput, vUV).rgb;
    float totalWeight = 1.0;

    for (int i = 1; i <= radius; ++i) {
        float x = float(i);
        float weight = exp(-(x * x) / 32.0);
        vec2 offset = direction * x;
        color += texture(uInput, vUV + offset).rgb * weight;
        color += texture(uInput, vUV - offset).rgb * weight;
        totalWeight += weight * 2.0;
    }

    FragColor = vec4(color / totalWeight, 1.0);
}
