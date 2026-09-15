#version 450

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D tex;

layout(binding = 1) uniform TextUBO {
    vec3 textColor;
};

void main() {
    float a = texture(tex, uv).r;
    fragColor = vec4(textColor, a);
}
