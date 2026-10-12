#ifndef DDGI_SURFACE_GLSL
#define DDGI_SURFACE_GLSL
#extension GL_EXT_nonuniform_qualifier : require

#include "../reference/gltf_types.glsl"

layout(set = 1, binding = 0, std430) readonly buffer vertices_buffer {
    float vertex_data[];
};

layout(set = 1, binding = 1, std430) readonly buffer indices_buffer {
    uint index_data[];
};

layout(set = 1, binding = 2, std430) readonly buffer instances_buffer {
    uvec4 instances[];
};

layout(set = 1, binding = 3, std430) readonly buffer materials_buffer {
    gltf_material materials[];
};

layout(constant_id = 0) const uint material_texture_count = 1;
layout(set = 1, binding = 5) uniform sampler2D material_textures[material_texture_count];
hitAttributeEXT vec2 barycentrics;
uvec4 instance_data() {
    return instances[gl_InstanceCustomIndexEXT];
}

uvec3 triangle_vertices() {
    uvec4 instance = instance_data();
    uint start = instance.y + 3 * gl_PrimitiveID;

    return uvec3(index_data[start], index_data[start + 1], index_data[start + 2]) + instance.x;
}

vec3 barycentric_weights() {
    return vec3(1 - barycentrics.x - barycentrics.y, barycentrics);
}

vec3 read_vertex3(uint vertex, uint offset) {
    uint start = vertex * vertex_stride + offset;

    return vec3(vertex_data[start], vertex_data[start + 1], vertex_data[start + 2]);
}

vec4 vertex_color() {
    uvec3 indices = triangle_vertices();
    vec3 weights = barycentric_weights();
    vec4 value = vec4(0);

    for (int i = 0; i < 3; ++i) {
        uint start = indices[i] * vertex_stride + 10;
        value += weights[i] * vec4(read_vertex3(indices[i], 10), vertex_data[start + 3]);
    }

    return value;
}

vec2 surface_uv(gltf_material material, int slot) {
    uvec3 indices = triangle_vertices();
    vec3 weights = barycentric_weights();
    vec2 uv = vec2(0);

    for (int i = 0; i < 3; ++i) {
        uint start = indices[i] * vertex_stride + uint(material.textures[slot].x);
        uv += weights[i] * vec2(vertex_data[start], vertex_data[start + 1]);
    }

    return gltf_uv(material, slot, uv);
}

vec4 surface_sample(gltf_material material, int slot) {
    if (material.textures[slot].y == 0)
        return vec4(1);

    return textureLod(material_textures[nonuniformEXT(material.textures[slot].z)], surface_uv(material, slot), 0);
}

#endif
