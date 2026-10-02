#version 460
#extension GL_GOOGLE_include_directive : require
#define VKZ_IBL_SET 0
#include "gltf_ibl.glsl"
layout(location = 0) in vec2 clip_position;
layout(location = 0) out vec4 frag_color;
void main() {
    vec4 world = ibl.inverse_view_projection * vec4(clip_position, 0.5, 1.0);
    frag_color = vec4(gltf_background(normalize(world.xyz / world.w - ibl.eye.xyz)), 1.0);
}
