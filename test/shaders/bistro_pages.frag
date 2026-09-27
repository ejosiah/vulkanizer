#version 450
#extension GL_GOOGLE_include_directive : require
#define SVSM_DEBUG
#include "bistro_common.glsl"
#include "vulkanizer/shaders/svsm.glsl"
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
void main() {
    // The overlay follows physical atlas order, not the virtual clipmap arrangement.
    vec2 local=(gl_FragCoord.xy-scene.diffuse.xy)/scene.diffuse.z;
    if (any(lessThan(local,vec2(0))) || any(greaterThanEqual(local,vec2(1)))) discard;
    uint side=svsm.config.w;
    vec2 grid=local*float(side);
    uvec2 cell=uvec2(grid);
    uint physical=cell.y*side+cell.x;
    vec3 tint=vec3(0.025);
    if (physical>=svsm.limits.y) {
        tint=vec3(0.18,0.02,0.2); // Square atlas padding, outside the allocator's capacity.
    } else {
        uint owner=svsmOwners[physical];
        if (owner!=SVSM_NONE) {
            SvsmPage page=svsmPages[owner];
            bool valid=(page.flags & SVSM_VALID)!=0u;
            bool rendered=(page.flags & SVSM_RENDERED)!=0u;
            bool requested=(page.flags & SVSM_REQUESTED)!=0u;
            tint=!valid ? vec3(0.9,0.08,0.15) : rendered ? vec3(1,0.48,0.05)
                 : requested ? vec3(0.12,0.8,0.3) : vec3(0.15,0.35,0.8);
            if (scene.info.z==1u && valid) {
                ivec2 offset=ivec2(fract(grid)*float(svsm.config.y));
                float depth=uintBitsToFloat(imageLoad(svsmPool,svsmPhysicalTexel(physical,offset)).r);
                // Keep state visible in the border while displaying the actual stored depth.
                if (all(greaterThan(fract(grid),vec2(0.12))) && all(lessThan(fract(grid),vec2(0.88))))
                    tint=vec3(depth);
            }
        }
    }
    vec2 edge=min(fract(grid),1.0-fract(grid))*scene.diffuse.z/float(side);
    if (min(edge.x,edge.y)<0.55) tint*=0.3;
    color=vec4(tint,1);
}
