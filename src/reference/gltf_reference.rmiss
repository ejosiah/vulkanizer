#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#include "gltf_ray_scene.glsl"
#ifdef VKZ_REFERENCE_IBL
#include "gltf_ibl.glsl"
#endif
layout(location = 0) rayPayloadInEXT gltf_ray_payload payload;
void main() {
    payload.weight = vec3(0);
#ifdef VKZ_REFERENCE_IBL
    payload.radiance = vec4(payload.bounce == 0u ? gltf_background(gl_WorldRayDirectionEXT) : gltf_environment(gl_WorldRayDirectionEXT), 1.0);
#else
    payload.radiance = reference.background;
#endif
    payload.radiance.rgb *= gltf_medium_attenuation(payload.volume, reference.ray_parameters.y);
}
