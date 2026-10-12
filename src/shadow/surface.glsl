#extension GL_EXT_nonuniform_qualifier : require
#include "../reference/gltf_types.glsl"

layout(set = 1, binding = 0, std430) readonly buffer vertices_buffer { float vertex_data[]; };
layout(set = 1, binding = 1, std430) readonly buffer indices_buffer { uint index_data[]; };
layout(set = 1, binding = 2, std430) readonly buffer instances_buffer { uvec4 instances[]; };
layout(set = 1, binding = 3, std430) readonly buffer materials_buffer { gltf_material materials[]; };
layout(constant_id = 0) const uint material_texture_count = 1;
layout(set = 1, binding = 5) uniform sampler2D material_textures[material_texture_count];

#include "../reference/ray_visibility.glsl"

vec2 trace_shadow_visibility(vec3 origin, vec3 direction, float distance) {
    if (constants.alpha_test != 0u)
        return trace_surface_visibility(scene_acceleration, origin, direction, distance, constants.vertex_stride);

    rayQueryEXT query;
    rayQueryInitializeEXT(query, scene_acceleration, gl_RayFlagsOpaqueEXT,
        0xff, origin, 0.01, direction, distance);
    while (rayQueryProceedEXT(query)) {}

    if (rayQueryGetIntersectionTypeEXT(query, true) != gl_RayQueryCommittedIntersectionNoneEXT)
        return vec2(0, rayQueryGetIntersectionTEXT(query, true));
    return vec2(1, distance);
}
