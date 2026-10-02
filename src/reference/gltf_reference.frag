#version 460
#extension GL_GOOGLE_include_directive : require
#ifdef VKZ_REFERENCE_RAY_QUERY
#extension GL_EXT_ray_query : require
#endif
#include "gltf_reference_uniforms.glsl"
#define VKZ_LIGHT_BINDING 2
#include "gltf_material.glsl"
#ifdef VKZ_REFERENCE_RAY_QUERY
#include "gltf_ray_query.glsl"
#endif
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
layout(location = 0) out vec4 fragColor;

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
    if(material.textures[11].y != 0) {
        mat3 coat_frame = gltf_tangent_frame(normalize(world_normal), material.textures[11].w == 0 && (material.textures[2].y == 0 || material.textures[11].x == material.textures[2].x) ? world_tangent : vec4(0),dFdx(world_position),dFdy(world_position),dFdx(uv[11]),dFdy(uv[11]));
        vec3 sampled = samples[11].xyz*2.0-1.0;
        sampled.xy *= material.clearcoat.z;
        if(dot(coat_frame[0],coat_frame[0])>1e-10 && dot(sampled,sampled)>1e-10) coat_normal=normalize(coat_frame*sampled)*(gl_FrontFacing?1.0:-1.0);
    }
    gltf_geometry geometry = gltf_geometry(world_position,n,frame[0],frame[1],coat_normal);
    vec4 color = gltf_shade(scene, material, vertex_color, samples, geometry, normalize(scene.eye.xyz - world_position));
    if (material.flags.y == 0 && material.optical.z == 0.0 && !gl_FrontFacing) discard;
    if (gltf_masked(material, gltf_base_color(material, vertex_color, samples[0]).a)) discard;
#ifdef VKZ_REFERENCE_RAY_QUERY
    uint seed = gltf_hash(uint(gl_FragCoord.x) ^ gltf_hash(uint(gl_FragCoord.y))) ^ gltf_hash(reference.sampling.x);
    int channel_count = scene.vertex_layout.z != 0 ? 3 : 1;
    for (int channel = 0; channel < channel_count; ++channel) {
        gltf_volume_state volume;
        volume.count = 0;
        volume.channel = channel_count == 3 ? channel : 1;
        gltf_surface surface = gltf_evaluate(scene, material, vertex_color, samples, geometry, normalize(world_position - scene.eye.xyz), gl_FrontFacing, seed, volume, instance_id, length(world_position - scene.eye.xyz));
        vec3 radiance = gltf_query_transmission(world_position, surface, seed, volume);
        if (channel_count == 3) color[channel] = radiance[channel];
        else color.rgb = radiance;
    }
#endif
    fragColor = color;
}
