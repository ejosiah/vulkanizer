#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_ray_tracing : require

#include "shared.glsl"

layout(location = 0) rayPayloadInEXT reflection_payload payload;

void main() {
    payload.radiance = vec3(0);
}
