#version 450
#extension GL_EXT_multiview : require
#extension GL_GOOGLE_include_directive : require
#include "bistro_common.glsl"
layout(set=0,binding=0,std430) readonly buffer Cascades { mat4 cascadeMatrices[]; };
layout(location=0) in vec3 position;
layout(location=2) in vec2 texcoord;
layout(location=2) out vec2 uv;
void main() { uv=texcoord; gl_Position=cascadeMatrices[gl_ViewIndex]*vec4(position,1); }
