#version 450
#extension GL_GOOGLE_include_directive : require
#include "bistro_common.glsl"
#include "vulkanizer/shaders/svsm.glsl"
layout(set=1,binding=0) uniform sampler2D diffuseTexture;
layout(set=1,binding=2) uniform samplerCube irradianceTexture;
layout(location=0) in vec3 world;
layout(location=1) in vec3 worldNormal;
layout(location=2) in vec2 uv;
layout(location=0) out vec4 color;
void main() {
    vec4 albedo=texture(diffuseTexture,uv)*scene.diffuse;
    if (albedo.a<0.5) discard;
    vec3 n=normalize(worldNormal); if (!gl_FrontFacing) n=-n;
    float cosine=max(dot(n,-scene.lightDirection.xyz),0.0);
    vec2 shadow=svsmSample(world,0.00008+0.00015*(1-cosine));
    vec3 ambient=texture(irradianceTexture,n).rgb*0.2+vec3(0.07);
    vec3 lit=albedo.rgb*(ambient+vec3(1.4,1.3,1.1)*cosine*shadow.x);
    if (scene.info.z==1u) lit=vec3(shadow.x);
    if (scene.info.z==2u) lit=shadow.y>0.5?vec3(0.1,0.8,0.2):vec3(1,0,0.6);
    color=vec4(lit/(1.0+lit),1);
}
