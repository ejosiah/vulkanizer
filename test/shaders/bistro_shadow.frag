#version 450
#extension GL_GOOGLE_include_directive : require
#include "bistro_common.glsl"
#define SVSM_WRITE
#include "vulkanizer/shaders/svsm.glsl"
layout(set=1,binding=0) uniform sampler2D diffuseTexture;
layout(location=0) in vec2 virtualUV;
layout(location=2) in vec2 uv;
void main() {
    if (texture(diffuseTexture,uv).a*scene.diffuse.a<0.5) discard;
    svsmStore(scene.info.x,virtualUV,gl_FragCoord.z+0.00002);
}
