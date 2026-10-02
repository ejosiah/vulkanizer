#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#include "gltf_ray_surface.glsl"
layout(location = 0) rayPayloadInEXT gltf_ray_payload payload;
void main() {
    uint seed = gltf_hash(gl_LaunchIDEXT.x + gl_LaunchSizeEXT.x * gl_LaunchIDEXT.y) ^ gltf_hash(reference.sampling.x) ^ gltf_hash(payload.sample_index) ^ gltf_hash(payload.bounce + 1u);
    gltf_surface surface = evaluate_triangle(instances.data[gl_InstanceCustomIndexEXT], uint(gl_PrimitiveID), hit_barycentrics,
        gl_ObjectToWorldEXT, gl_WorldToObjectEXT, gl_HitKindEXT == gl_HitKindFrontFacingTriangleEXT, gl_WorldRayDirectionEXT, seed, payload.volume, uint(gl_InstanceCustomIndexEXT), gl_HitTEXT);
    payload.radiance = vec4(surface.radiance.rgb, 1);
    payload.weight = surface.weight;
    payload.direction = surface.direction;
    payload.origin = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;
}
