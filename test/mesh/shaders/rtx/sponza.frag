#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_ray_query : require

#include "sponza_uniforms.glsl"
#define VKZ_LIGHT_BINDING 2
#include "gltf_material.glsl"

layout(set = 1, binding = 1) uniform sampler2D base_color_map;
layout(set = 1, binding = 2) uniform sampler2D metallic_roughness_map;
layout(set = 1, binding = 3) uniform sampler2D normal_map;
layout(set = 1, binding = 4) uniform sampler2D occlusion_map;
layout(set = 1, binding = 5) uniform sampler2D emissive_map;
layout(set = 1, binding = 6) uniform sampler2D transmission_map;
layout(set = 1, binding = 7) uniform sampler2D thickness_map;
layout(set = 1, binding = 8) uniform sampler2D specular_map;
layout(set = 1, binding = 9) uniform sampler2D specular_color_map;
layout(set = 1, binding = 10) uniform sampler2D clearcoat_map;
layout(set = 1, binding = 11) uniform sampler2D clearcoat_roughness_map;
layout(set = 1, binding = 12) uniform sampler2D clearcoat_normal_map;
layout(set = 1, binding = 13) uniform sampler2D sheen_color_map;
layout(set = 1, binding = 14) uniform sampler2D sheen_roughness_map;
layout(set = 1, binding = 15) uniform sampler2D anisotropy_map;
layout(set = 1, binding = 16) uniform sampler2D iridescence_map;
layout(set = 1, binding = 17) uniform sampler2D iridescence_thickness_map;

layout(location = 21) flat in uint instance_id;
layout(location = 0) in vec3 world_position;
layout(location = 1) in vec3 world_normal;
layout(location = 2) in vec4 world_tangent;
layout(location = 3) in vec4 vertex_color;
layout(location = 4) in vec2 uv[17];

#ifdef GBUFFER_PASS
layout(location = 0) out vec2 encoded_normal;
layout(location = 1) out vec4 surface_parameters;
layout(location = 2) out uint mesh_id;

vec2 oct_encode(vec3 normal) {
    normal /= abs(normal.x) + abs(normal.y) + abs(normal.z);

    if (normal.z < 0)
        normal.xy = (1 - abs(normal.yx)) * vec2(normal.x >= 0 ? 1 : -1, normal.y >= 0 ? 1 : -1);

    return normal.xy;
}
#else
layout(set = 0, binding = 3) uniform sampler2D indirect_light;
layout(set = 0, binding = 6) uniform sampler2D reflection_color;
layout(location = 0) out vec4 frag_color;
#endif

#ifndef GBUFFER_PASS
#include "tv_lighting.glsl"
#endif

void main() {
    vec4 samples[17];
    samples[0] = material.textures[0].y != 0 ? texture(base_color_map, uv[0]) : vec4(1);
    samples[1] = material.textures[1].y != 0 ? texture(metallic_roughness_map, uv[1]) : vec4(1);
    samples[2] = material.textures[2].y != 0 ? texture(normal_map, uv[2]) : vec4(1);
    samples[3] = material.textures[3].y != 0 ? texture(occlusion_map, uv[3]) : vec4(1);
    samples[4] = material.textures[4].y != 0 ? texture(emissive_map, uv[4]) : vec4(1);
    samples[5] = material.textures[5].y != 0 ? texture(transmission_map, uv[5]) : vec4(1);
    samples[6] = vec4(1);
    samples[7] = material.textures[7].y != 0 ? texture(specular_map, uv[7]) : vec4(1);
    samples[8] = material.textures[8].y != 0 ? texture(specular_color_map, uv[8]) : vec4(1);
    samples[9] = material.textures[9].y != 0 ? texture(clearcoat_map, uv[9]) : vec4(1);
    samples[10] = material.textures[10].y != 0 ? texture(clearcoat_roughness_map, uv[10]) : vec4(1);
    samples[11] = material.textures[11].y != 0 ? texture(clearcoat_normal_map, uv[11]) : vec4(1);
    samples[12] = material.textures[12].y != 0 ? texture(sheen_color_map, uv[12]) : vec4(1);
    samples[13] = material.textures[13].y != 0 ? texture(sheen_roughness_map, uv[13]) : vec4(1);
    samples[14] = material.textures[14].y != 0 ? texture(anisotropy_map, uv[14]) : vec4(1);
    samples[15] = material.textures[15].y != 0 ? texture(iridescence_map, uv[15]) : vec4(1);
    samples[16] = material.textures[16].y != 0 ? texture(iridescence_thickness_map, uv[16]) : vec4(1);

    mat3 frame = gltf_tangent_frame(normalize(world_normal), material.textures[2].w != 0 ? vec4(0) : world_tangent, dFdx(world_position), dFdy(world_position), dFdx(uv[2]), dFdy(uv[2]));
    vec3 n = gltf_normal(material, world_normal, frame, samples[2], gl_FrontFacing);
    vec3 coat_normal = normalize(world_normal)*(gl_FrontFacing?1.0:-1.0);

    if (material.textures[11].y != 0) {
        mat3 coat_frame = gltf_tangent_frame(normalize(world_normal), material.textures[11].w == 0 && (material.textures[2].y == 0 || material.textures[11].x == material.textures[2].x) ? world_tangent : vec4(0),dFdx(world_position),dFdy(world_position),dFdx(uv[11]),dFdy(uv[11]));
        vec3 sampled = samples[11].xyz*2.0-1.0;
        sampled.xy *= material.clearcoat.z;
        if (dot(coat_frame[0], coat_frame[0]) > 1e-10 && dot(sampled, sampled) > 1e-10)
            coat_normal = normalize(coat_frame * sampled) * (gl_FrontFacing ? 1.0 : -1.0);
    }

#ifndef GBUFFER_PASS
    gltf_geometry geometry = gltf_geometry(world_position,n,frame[0],frame[1],coat_normal);
    vec4 color = gltf_shade(scene, material, vertex_color, samples, geometry, normalize(scene.eye.xyz - world_position));
#endif

    if (material.flags.y == 0 && material.optical.z == 0.0 && !gl_FrontFacing)
        discard;

    if (gltf_masked(material, gltf_base_color(material, vertex_color, samples[0]).a))
        discard;

#ifdef GBUFFER_PASS
    encoded_normal = oct_encode(n);
    vec3 base = gltf_base_color(material, vertex_color, samples[0]).rgb;
    float metallic = clamp(material.emissive_metallic.w * samples[1].b, 0, 1);
    float roughness = clamp(material.roughness_normal_occlusion_cutoff.x * samples[1].g, 0, 1);
    float dielectric = pow((material.optical.x - 1) / (material.optical.x + 1), 2);
    vec3 specular = min(vec3(dielectric) * material.specular.rgb * samples[8].rgb, vec3(1));
    specular *= material.specular.w * samples[7].a;
    surface_parameters = vec4(mix(specular, base, metallic), material.flags.z != 0 ? 1 : roughness);
    mesh_id = instance_id + 1u;
#else
    if (scene.reflection_only != 0u) {
        vec3 reflected = scene.enable_reflection != 0u
            ? texture(reflection_color, gl_FragCoord.xy / scene.render_resolution).rgb : vec3(0);
        frag_color = vec4(reflected, 1);
        return;
    }

    // Blended surfaces have no opaque G-buffer entry. Screen-space lighting
    // belongs to the surface behind them and must not be shaded onto the shell.
    vec3 irradiance = scene.enable_ddgi != 0u && material.flags.x != 2
        ? texture(indirect_light, gl_FragCoord.xy / scene.render_resolution).rgb * scene.gi_multiplier
        : vec3(0);

    if (scene.indirect_only != 0u) {
        frag_color = vec4(irradiance, 1);
        return;
    }

    vec3 indirect = vec3(0);

    if (material.flags.z == 0 && material.flags.x != 2 && scene.enable_ddgi != 0u) {
        vec3 base = gltf_base_color(material, vertex_color, samples[0]).rgb;
        float metallic = clamp(material.emissive_metallic.w * samples[1].b, 0, 1);
        float transmission = clamp(material.optical.y * samples[5].r, 0, 1);
        float occlusion = mix(1, samples[3].r, material.roughness_normal_occlusion_cutoff.z);

        indirect = base * (1 - metallic) * (1 - transmission) * occlusion * irradiance;
    }

    if (material.flags.z == 0) {
        vec3 base = gltf_base_color(material, vertex_color, samples[0]).rgb;
        float metallic = clamp(material.emissive_metallic.w * samples[1].b, 0, 1);
        float roughness = clamp(material.roughness_normal_occlusion_cutoff.x * samples[1].g, 0.01, 1);
        color.rgb += tv_lighting(world_position, n, normalize(scene.eye.xyz - world_position), base, metallic, roughness);
    }

    color.rgb += indirect;

    if (scene.enable_reflection != 0u && material.flags.z == 0 && material.flags.x != 2
        && material.roughness_normal_occlusion_cutoff.x * samples[1].g <= 0.3)
        color.rgb += texture(reflection_color, gl_FragCoord.xy / scene.render_resolution).rgb;

    frag_color = color;
#endif
}


