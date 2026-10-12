#extension GL_EXT_nonuniform_qualifier : require
#ifndef VKZ_QUERY_SET
#define VKZ_QUERY_SET 1
#endif

layout(set = VKZ_QUERY_SET, binding = 0, std430) readonly buffer vertices_buffer { float vertex_data[]; };
layout(set = VKZ_QUERY_SET, binding = 1, std430) readonly buffer indices_buffer { uint index_data[]; };
layout(set = VKZ_QUERY_SET, binding = 2, std430) readonly buffer instances_buffer { uvec4 instances[]; };
layout(set = VKZ_QUERY_SET, binding = 3, std430) readonly buffer materials_buffer { gltf_material materials[]; };
layout(constant_id = 0) const uint material_texture_count = 1;
layout(set = VKZ_QUERY_SET, binding = 5) uniform sampler2D material_textures[material_texture_count];


#include "ray_visibility.glsl"
