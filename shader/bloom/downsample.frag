#version 460 core
in vec2 vUV;
out vec4 FragColor;

layout(binding = 0) uniform sampler2D uInput;
uniform vec2 uTexelSize;

void main() {
    vec3 center = texture(uInput, vUV).rgb * 0.50;

    vec3 axis = vec3(0.0);
    axis += texture(uInput, vUV + vec2( uTexelSize.x, 0.0)).rgb;
    axis += texture(uInput, vUV + vec2(-uTexelSize.x, 0.0)).rgb;
    axis += texture(uInput, vUV + vec2(0.0,  uTexelSize.y)).rgb;
    axis += texture(uInput, vUV + vec2(0.0, -uTexelSize.y)).rgb;
    axis *= 0.0833333;

    vec3 diagonal = vec3(0.0);
    diagonal += texture(uInput, vUV + vec2( uTexelSize.x,  uTexelSize.y)).rgb;
    diagonal += texture(uInput, vUV + vec2(-uTexelSize.x,  uTexelSize.y)).rgb;
    diagonal += texture(uInput, vUV + vec2( uTexelSize.x, -uTexelSize.y)).rgb;
    diagonal += texture(uInput, vUV + vec2(-uTexelSize.x, -uTexelSize.y)).rgb;
    diagonal *= 0.0416667;

    FragColor = vec4(center + axis + diagonal, 1.0);
}
