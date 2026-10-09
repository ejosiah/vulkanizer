#version 450
#extension GL_GOOGLE_include_directive : require
#include "floor_common.glsl"
#include "bistro_csm_sample.glsl"
layout(location=0) out vec4 color;
void main() {
    vec3 world=floorPosition();
    float cosine=max(dot(vec3(0,1,0),-scene.lightDirection.xyz),0);
    color=floorColor(world,csmSample(world,cosine));
}
