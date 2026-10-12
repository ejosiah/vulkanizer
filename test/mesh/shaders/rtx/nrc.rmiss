#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_ray_tracing : require

#include "nrc_payload.glsl"
layout(location = 0) rayPayloadInEXT nrc_hit payload;

void main() {
    payload.valid = 0;
}
