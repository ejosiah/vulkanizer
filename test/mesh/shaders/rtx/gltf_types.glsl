#ifndef VKZ_GLTF_TYPES_GLSL
#define VKZ_GLTF_TYPES_GLSL

struct gltf_scene {
    mat4 view_projection;
    vec4 eye;
    ivec4 vertex_layout;
    uint num_lights;
    uint enable_ddgi;
    uint indirect_only;
    float exposure;
    float gi_multiplier;
    uint enable_shadows;
    vec2 render_resolution;
    uint enable_reflection;
    uint reflection_only;
};

struct gltf_material {
    vec4 base_color;
    vec4 emissive_metallic;
    vec4 roughness_normal_occlusion_cutoff;
    ivec4 flags;
    vec4 optical;
    vec4 attenuation;
    vec4 specular;
    vec4 clearcoat;
    vec4 sheen;
    vec4 anisotropy;
    vec4 iridescence;
    ivec4 textures[17];
    vec4 uv_transform[17];
    vec4 uv_offset[17];
};

vec2 gltf_uv(gltf_material material, int slot, vec2 uv) {
    if (material.textures[slot].w == 0) return uv;
    vec4 m = material.uv_transform[slot];
    return vec2(dot(m.xy, uv), dot(m.zw, uv)) + material.uv_offset[slot].xy;
}

#endif
