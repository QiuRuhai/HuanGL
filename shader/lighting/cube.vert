#version 460 core
layout(location = 0) in vec3 aPos;
out vec3 vWorldPos;
uniform mat4 uViewProj;
void main() {
    vWorldPos = aPos;
    gl_Position = (uViewProj * vec4(aPos, 1.0)).xyww;
}
