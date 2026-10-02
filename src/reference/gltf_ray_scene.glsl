#define VKZ_LIGHT_BINDING 7
#include "gltf_types.glsl"
#include "gltf_transport.glsl"
layout(set = 0, binding = 0, std140) uniform RayScene {
    gltf_scene scene;
    mat4 inverse_view_projection;
    vec4 background;
    vec4 ray_parameters;
    uvec4 sampling;
} reference;

struct gltf_ray_payload {
    vec4 radiance;
    uint sample_index;
    vec3 weight;
    vec3 origin;
    vec3 direction;
    uint bounce;
    gltf_volume_state volume;
};
