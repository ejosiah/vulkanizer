#version 450
#extension GL_GOOGLE_include_directive : require
#include "floor_common.glsl"
#include "vulkanizer/shaders/svsm.glsl"
layout(location=0) out vec4 color;
void main() {
    vec3 world=floorPosition();
    color=floorColor(world,svsmSample(world,0.00008));
}
