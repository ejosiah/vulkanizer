#version 450
#extension GL_GOOGLE_include_directive : require
#include "bistro_common.glsl"
#include "vulkanizer/shaders/svsm.glsl"
layout(location=0) in vec3 position;
layout(location=2) in vec2 texcoord;
layout(location=0) out vec2 virtualUV;
layout(location=2) out vec2 uv;
void main() {
    gl_Position=svsm.light[scene.info.x]*vec4(position,1);
    virtualUV=gl_Position.xy*0.5+0.5; uv=texcoord;
    if (svsmVisibility[scene.info.x*svsm.work.x+scene.info.y]==0u) gl_Position=vec4(2,2,2,1);
}
