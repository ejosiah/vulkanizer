#version 450
#extension GL_GOOGLE_include_directive : require
#define SVSM_WRITE
#include "vulkanizer/shaders/svsm.glsl"
layout(location=0) in vec2 virtualUV;
layout(push_constant) uniform Draw {
    mat4 model; uint clipmap; uint caster; uint useVisibility; float bias;
} draw;
void main() { svsmStore(draw.clipmap,virtualUV,gl_FragCoord.z+draw.bias); }
