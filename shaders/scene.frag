#version 450

layout(location = 0) in vec3 v_baked;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in float v_fog;
layout(location = 3) in float v_height_fog;
layout(location = 4) in vec3 v_world_pos;

layout(binding = 0) uniform UBO {
    mat4 mvp;
    mat4 model;
    vec4 camera_pos;
    vec4 fog_color;
    float fog_start;
    float fog_end;
    float height_fog_floor;
    float height_fog_ceiling;
};

layout(binding = 1) uniform sampler2D tex;

layout(location = 0) out vec4 frag_color;

void main() {
    vec3 tex_color = texture(tex, v_uv).rgb;

    vec3 lit = v_baked * tex_color;

    // height fog darkens and desaturates low areas
    vec3 height_fog_tint = fog_color.rgb * 0.6;
    lit = mix(lit, height_fog_tint, v_height_fog * 0.5);

    // distance fog fades to fog color
    lit = mix(lit, fog_color.rgb, v_fog);

    frag_color = vec4(lit, 1.0);
}
