layout(set=0,binding=0,std430) readonly buffer Cascades { mat4 cascadeMatrices[]; };
layout(set=0,binding=1) uniform sampler2DArray shadowMap;
layout(set=0,binding=2,std140) uniform CsmCamera { mat4 view; vec4 splits; } csm;
vec2 csmSample(vec3 world, float cosine) {
    float z=(csm.view*vec4(world,1)).z;
    int level=0;
    for(int i=0;i<3;++i) if(z<csm.splits[i]) level=i+1;
    vec4 clip=cascadeMatrices[level]*vec4(world,1);
    vec3 p=clip.xyz/clip.w; p.xy=p.xy*0.5+0.5;
    bool covered=z>=csm.splits.w && all(greaterThanEqual(p,vec3(0))) && all(lessThanEqual(p,vec3(1)));
    float visibility=1;
    if(covered) {
        visibility=0;
        ivec2 size=textureSize(shadowMap,0).xy;
        ivec2 center=ivec2(p.xy*vec2(size));
        for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x) {
            float depth=texelFetch(shadowMap,ivec3(clamp(center+ivec2(x,y),ivec2(0),size-1),level),0).r;
            visibility+=p.z-(0.00008+0.00015*(1-cosine))<=depth?1.0:0.0;
        }
        visibility/=9;
    }
    return vec2(visibility,covered?1:0);
}
