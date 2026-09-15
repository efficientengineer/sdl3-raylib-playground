#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 baked_color;
layout(location = 2) in vec2 texcoord;

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

layout(location = 0) out vec3 v_baked;
layout(location = 1) out vec2 v_uv;
layout(location = 2) out float v_fog;
layout(location = 3) out float v_height_fog;
layout(location = 4) out vec3 v_world_pos;

void main() {
    vec4 world = model * vec4(pos, 1.0);
    v_world_pos = world.xyz;
    v_baked = baked_color;
    v_uv = texcoord;

    float dist = length(world.xyz - camera_pos.xyz);
    v_fog = clamp((dist - fog_start) / (fog_end - fog_start), 0.0, 1.0);

    float h = clamp((world.y - height_fog_floor) / (height_fog_ceiling - height_fog_floor), 0.0, 1.0);
    v_height_fog = 1.0 - h;

    gl_Position = mvp * vec4(pos, 1.0);
}
