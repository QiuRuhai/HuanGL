#version 460 core
in vec3 vWorldPos;
out vec4 FragColor;
layout(binding = 0) uniform sampler2D uEquirect;

const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleSphericalMap(vec3 dir) {
    vec2 uv = vec2(atan(dir.z, dir.x), asin(dir.y));
    uv *= invAtan; uv += 0.5; return uv;
}

void main() {
    vec2 uv = SampleSphericalMap(normalize(vWorldPos));
    FragColor = vec4(texture(uEquirect, uv).rgb, 1.0);
}
