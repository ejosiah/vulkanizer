#version 450
#extension GL_GOOGLE_include_directive : require
#include "bistro_common.glsl"
layout(set=1,binding=0) uniform sampler2D diffuseTexture;
layout(location=2) in vec2 uv;
void main() { if (texture(diffuseTexture,uv).a*scene.diffuse.a<0.5) discard; }
