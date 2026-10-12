#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require

#include "gltf_types.glsl"

layout(set = 0, binding = 0, scalar) uniform scene_buffer {
    gltf_scene scene;
};
layout(set = 0, binding = 8) uniform sampler2D probe_offsets;

layout(push_constant, scalar) uniform probe_parameters {
    vec3 grid_position;
    ivec3 counts;
    vec3 spacing;
    float radius;
    int irradiance_side_length;
    uint offsetting_enabled;
};

layout(location = 0) out vec3 probe_normal;
layout(location = 1) flat out int probe_index;

void main() {
    const ivec2 corners[6] = ivec2[6](ivec2(0, 0), ivec2(1, 0), ivec2(1, 1),
        ivec2(0, 0), ivec2(1, 1), ivec2(0, 1));
    const float pi = 3.14159265359;
    int cell = gl_VertexIndex / 6;
    ivec2 coordinate = ivec2(cell % 12, cell / 12) + corners[gl_VertexIndex % 6];
    float longitude = float(coordinate.x) * (2 * pi / 12);
    float latitude = float(coordinate.y) * (pi / 8);
    probe_normal = vec3(sin(latitude) * cos(longitude), cos(latitude), sin(latitude) * sin(longitude));
    probe_index = gl_InstanceIndex;

    int row_count = counts.x * counts.y;
    ivec3 grid = ivec3(probe_index % counts.x, (probe_index % row_count) / counts.x, probe_index / row_count);
    vec3 offset = offsetting_enabled != 0
        ? texelFetch(probe_offsets, ivec2(probe_index % row_count, probe_index / row_count), 0).xyz
        : vec3(0);
    vec3 position = grid_position + vec3(grid) * spacing + offset + probe_normal * radius;
    gl_Position = scene.view_projection * vec4(position, 1);
}
