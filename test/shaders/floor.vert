#version 450
#extension GL_GOOGLE_include_directive : require
#include "bistro_common.glsl"
layout(location=0) out vec3 nearPoint;
layout(location=1) out vec3 farPoint;
void main() {
    vec2 uv=vec2((gl_VertexIndex<<1)&2,gl_VertexIndex&2);
    vec2 ndc=uv*2-1;
    mat4 inverseVP=inverse(scene.matrix);
    vec4 a=inverseVP*vec4(ndc,0,1), b=inverseVP*vec4(ndc,1,1);
    nearPoint=a.xyz/a.w; farPoint=b.xyz/b.w;
    gl_Position=vec4(ndc,0,1);
}
