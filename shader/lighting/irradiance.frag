#version 460 core
in vec3 vWorldPos;
out vec4 FragColor;
layout(binding = 0) uniform samplerCube uEnvMap;

void main() {
    vec3 N = normalize(vWorldPos);
    vec3 up = abs(N.y) < 0.999 ? vec3(0,1,0) : vec3(1,0,0);
    vec3 right = normalize(cross(up, N));
    vec3 up2   = cross(N, right);

    vec3 color = vec3(0);
    float samples = 0;
    float step = 0.05;
    for (float phi = 0; phi < 6.2832; phi += step) {
        for (float theta = 0; theta < 1.5708; theta += step) {
            vec3 tangent = cos(phi) * right + sin(phi) * up2;
            vec3 sampleDir = cos(theta) * N + sin(theta) * tangent;
            color += texture(uEnvMap, sampleDir).rgb * cos(theta) * sin(theta);
            samples++;
        }
    }
    FragColor = vec4(3.14159 * color / samples, 1.0);
}
