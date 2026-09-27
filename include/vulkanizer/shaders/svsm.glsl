#ifndef VKZ_SVSM_GLSL
#define VKZ_SVSM_GLSL
#ifndef SVSM_SET
#define SVSM_SET 0
#endif
const uint SVSM_REQUESTED = 1u, SVSM_DIRTY = 2u, SVSM_VALID = 4u;
const uint SVSM_SCHEDULED = 8u, SVSM_RESIDENT = 16u, SVSM_NONE = 0xffffffffu;
const uint SVSM_RENDERED = 32u;
struct SvsmPage { ivec2 address; uint physical; uint flags; };
layout(std140, set=SVSM_SET, binding=0) uniform SvsmUniforms {
    mat4 inverseViewProjection;
    mat4 light[16];
    ivec4 origin[16];
    uvec4 config; // virtual resolution, page size, levels, physical pool side in pages
    uvec4 work;   // caster count, invalidation count, reset, cache enabled
    uvec4 limits; // page budget, physical capacity, hierarchy stride, reserved
    vec4 receiver; // background depth, reserved
    uvec4 options; // depth channel, PCF radius, request all, indexed templates
} svsm;
#ifdef SVSM_DEBUG
/** @brief Physical slot to virtual page mapping; SVSM_NONE denotes a free slot. */
layout(std430, set=SVSM_SET, binding=10) readonly buffer SvsmOwners { uint svsmOwners[]; };
#endif
#ifdef SVSM_COMPUTE
layout(std430, set=SVSM_SET, binding=1) buffer SvsmPages { SvsmPage svsmPages[]; };
#else
layout(std430, set=SVSM_SET, binding=1) readonly buffer SvsmPages { SvsmPage svsmPages[]; };
#endif
#if defined(SVSM_WRITE) || defined(SVSM_COMPUTE)
layout(r32ui, set=SVSM_SET, binding=2) uniform uimage2D svsmPool;
#else
layout(r32ui, set=SVSM_SET, binding=2) readonly uniform uimage2D svsmPool;
#endif
#ifdef SVSM_COMPUTE
layout(std430, set=SVSM_SET, binding=7) buffer SvsmVisibility { uint svsmVisibility[]; };
#else
layout(std430, set=SVSM_SET, binding=7) readonly buffer SvsmVisibility { uint svsmVisibility[]; };
#endif
uint svsmPageCount() { return svsm.config.x / svsm.config.y; }
uint svsmPageIndex(uint level, ivec2 address) {
    uint n = svsmPageCount();
    uvec2 wrapped = uvec2(address) & uvec2(n-1u);
    return level*n*n + wrapped.y*n + wrapped.x;
}
bool svsmInside(uint level, ivec2 address) {
    ivec2 p = address-svsm.origin[level].xy;
    return all(greaterThanEqual(p, ivec2(0))) && all(lessThan(p, ivec2(svsmPageCount())));
}
ivec2 svsmPhysicalTexel(uint physical, ivec2 offset) {
    return ivec2(physical % svsm.config.w, physical / svsm.config.w)*int(svsm.config.y)+offset;
}
#ifdef SVSM_WRITE
// Call AFTER application alpha testing. Depth must use this clipmap's Vulkan [0,1] range.
void svsmStore(uint level, vec2 uv, float depth) {
    if (any(lessThan(uv,vec2(0))) || any(greaterThanEqual(uv,vec2(1))) || isnan(depth) || isinf(depth)) return;
    ivec2 texel = ivec2(floor(uv*float(svsm.config.x)));
    ivec2 address = svsm.origin[level].xy + texel/int(svsm.config.y);
    SvsmPage page = svsmPages[svsmPageIndex(level,address)];
    if ((page.flags & SVSM_SCHEDULED)==0u || any(notEqual(page.address,address))) return;
    // Integer atomics only. IEEE positive float bit ordering equals unsigned integer ordering.
    uint bits=floatBitsToUint(clamp(depth,0.0,1.0)) & 0x7fffffffu; // Canonicalize negative zero, too.
    imageAtomicMin(svsmPool,svsmPhysicalTexel(page.physical,texel%int(svsm.config.y)),bits);
}
#endif
#ifndef SVSM_COMPUTE
// Returns (visibility, coverage). coverage=0 means no valid fallback exists; visibility defaults lit.
// All taps use ONE valid level, avoiding seams from mixing fine and coarse depths in a kernel.
vec2 svsmSample(vec3 worldPosition, float receiverBias) {
    int radius = int(svsm.options.y);
    for (uint level=0u; level<svsm.config.z; ++level) {
        vec3 projected=(svsm.light[level]*vec4(worldPosition,1)).xyz;
        vec2 uv=projected.xy*0.5+0.5;
        if (projected.z<0.0 || projected.z>1.0 || any(lessThan(uv,vec2(0))) || any(greaterThanEqual(uv,vec2(1)))) continue;
        ivec2 texel=ivec2(floor(uv*float(svsm.config.x)));
        float sum=0; bool complete=true;
        for (int y=-radius;y<=radius;++y) for (int x=-radius;x<=radius;++x) {
            ivec2 t=texel+ivec2(x,y);
            if (any(lessThan(t,ivec2(0))) || any(greaterThanEqual(t,ivec2(svsm.config.x)))) { complete=false; continue; }
            ivec2 address=svsm.origin[level].xy+t/int(svsm.config.y);
            SvsmPage page=svsmPages[svsmPageIndex(level,address)];
            if ((page.flags & SVSM_VALID)==0u || any(notEqual(page.address,address))) { complete=false; continue; }
            float depth=uintBitsToFloat(imageLoad(svsmPool,svsmPhysicalTexel(page.physical,t%int(svsm.config.y))).r);
            sum += projected.z-receiverBias<=depth ? 1.0 : 0.0;
        }
        if (complete) return vec2(sum/float((2*radius+1)*(2*radius+1)),1);
    }
    return vec2(1,0);
}
#endif
#endif
