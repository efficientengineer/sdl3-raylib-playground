#version 450

layout(location = 0) in vec3 v_normal;
layout(location = 0) out vec4 color;

void main() {
    vec3 n = normalize(v_normal);
    vec3 light = normalize(vec3(1.0, 1.0, 1.0));
    float d = max(dot(n, light), 0.15);
    color = vec4(vec3(0.2, 0.5, 0.9) * d, 1.0);
}
