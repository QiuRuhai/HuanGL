#version 460 core
in vec2 vUV;
out vec4 FragColor;

layout(binding = 0) uniform sampler2D uHDRInput;
uniform float uThreshold;

void main() {
    vec3 hdr = texture(uHDRInput, vUV).rgb;
    float luma = dot(hdr, vec3(0.2126, 0.7152, 0.0722));
    float contribution = max(luma - uThreshold, 0.0);
    vec3 bright = luma > 0.0 ? hdr * (contribution / luma) : vec3(0.0);
    FragColor = vec4(bright, 1.0);
}
