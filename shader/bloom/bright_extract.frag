#version 460 core
in vec2 vUV;
out vec4 FragColor;

layout(binding = 0) uniform sampler2D uHDRInput;
uniform float uThreshold;
uniform float uSoftKnee;

void main() {
    vec3 hdr = texture(uHDRInput, vUV).rgb;
    float luma = dot(hdr, vec3(0.2126, 0.7152, 0.0722));

    float knee = max(uThreshold * uSoftKnee, 1e-5);
    float soft = clamp(luma - uThreshold + knee, 0.0, 2.0 * knee);
    soft = (soft * soft) / (4.0 * knee);

    float contribution = max(luma - uThreshold, soft);
    float weight = contribution / max(luma, 1e-5);
    FragColor = vec4(hdr * weight, 1.0);
}
