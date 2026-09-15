#version 450

layout(location = 0) in vec3 pos;
layout(location = 0) out vec3 v_normal;

layout(binding = 0) uniform UBO {
    mat4 mvp;
};

void main() {
    v_normal = pos;
    gl_Position = mvp * vec4(pos, 1.0);
}
