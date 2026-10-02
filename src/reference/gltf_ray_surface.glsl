#extension GL_EXT_nonuniform_qualifier : require
#include "gltf_ray_scene.glsl"
#include "gltf_material.glsl"
layout(set = 0, binding = 1, std430) readonly buffer Vertices { float data[]; } vertices;
layout(set = 0, binding = 2, std430) readonly buffer Indices { uint data[]; } indices;
layout(set = 0, binding = 3, std430) readonly buffer Instances { uvec4 data[]; } instances;
layout(set = 0, binding = 4, std430) readonly buffer Materials { gltf_material data[]; } materials;
layout(constant_id = 0) const uint texture_count = 1;
layout(set = 1, binding = 0) uniform sampler2D textures[texture_count];
hitAttributeEXT vec2 hit_barycentrics;

#include "gltf_surface.glsl"
vec3 barycentrics() {
    return vec3(1.0 - hit_barycentrics.x - hit_barycentrics.y, hit_barycentrics);
}
