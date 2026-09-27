#version 450
#extension GL_GOOGLE_include_directive : require
#include "vulkanizer/shaders/svsm.glsl"
layout(location=0) in vec3 position;
layout(location=0) out vec2 virtualUV;
layout(push_constant) uniform Draw {
    mat4 model; uint clipmap; uint caster; uint useVisibility; float bias;
} draw;
void main() {
    gl_Position=svsm.light[draw.clipmap]*draw.model*vec4(position,1);
    virtualUV=gl_Position.xy*0.5+0.5;
    if (draw.useVisibility!=0u && (draw.caster>=svsm.work.x || svsmVisibility[draw.clipmap*svsm.work.x+draw.caster]==0u))
        gl_Position=vec4(2,2,2,1);
}
