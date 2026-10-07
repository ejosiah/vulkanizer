#version 460

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 tangent;
layout(location = 3) in vec4 color;
layout(location = 4) in vec2 uv;

layout(push_constant) uniform Constants {
    mat4 model;
    vec4 base_color;
};

layout(location = 0) out vec4 o_color;
layout(location = 1) out vec3 world_position;
layout(location = 2) out vec3 world_normal;

layout(std430, set = 0, binding = 0) readonly buffer Scene {
    mat4 view_projection;
    vec4 light_position;
    vec4 eye;
};

void main() {
    o_color = color * base_color;
    vec4 world = model * vec4(position, 1);
    world_position = world.xyz;
    world_normal = transpose(inverse(mat3(model))) * normal;
    gl_Position = view_projection * world;
}
