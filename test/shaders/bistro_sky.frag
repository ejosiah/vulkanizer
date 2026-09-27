#version 450
#extension GL_GOOGLE_include_directive : require
#include "bistro_common.glsl"
layout(set=1,binding=1) uniform samplerCube skyTexture;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
void main() {
    vec4 world=scene.matrix*vec4(uv*2.0-1.0,1,1);
    vec3 ray=normalize(world.xyz/world.w-scene.diffuse.xyz);
    vec3 sky=texture(skyTexture,ray).rgb;
    color=vec4(sky/(1.0+sky),1);
}
