#version 450
#extension GL_GOOGLE_include_directive : require
#include "bistro_common.glsl"
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texcoord;
layout(location=0) out vec3 world;
layout(location=1) out vec3 worldNormal;
layout(location=2) out vec2 uv;
void main() { world=position; worldNormal=normal; uv=texcoord; gl_Position=scene.matrix*vec4(position,1); }
