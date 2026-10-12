#ifndef VKZ_GLTF_MATERIAL_GLSL
#define VKZ_GLTF_MATERIAL_GLSL
#include "gltf_types.glsl"
#include "gltf_core.glsl"

struct gltf_geometry {
    vec3 position;
    vec3 normal;
    vec3 tangent;
    vec3 bitangent;
    vec3 coat_normal;
};
#ifdef VKZ_LIGHT_BINDING
struct gltf_light {
    vec4 position_range;
    vec4 direction_type;
    vec4 radiance;
    vec4 cone;
};
layout(set = 0, binding = VKZ_LIGHT_BINDING, std430) readonly buffer Lights { gltf_light data[]; } gltf_lights;
#endif

vec4 gltf_base_color(gltf_material material, vec4 vertex_color, vec4 base_sample) {
    return material.base_color * vertex_color * base_sample;
}

bool gltf_masked(gltf_material material, float alpha) {
    return material.flags.x == 1 && alpha < material.roughness_normal_occlusion_cutoff.w;
}

mat3 gltf_tangent_frame(vec3 n, vec4 tangent, vec3 p1, vec3 p2, vec2 uv1, vec2 uv2) {
    vec3 t, b;
    if (dot(tangent.xyz, tangent.xyz) > 1e-10 && abs(tangent.w) > 0.5) {
        t = tangent.xyz - n * dot(n, tangent.xyz);
        if (dot(t, t) < 1e-10) return mat3(0);
        t = normalize(t);
        b = cross(n, t) * sign(tangent.w);
    } else {
        float determinant = uv1.x * uv2.y - uv1.y * uv2.x;
        if (abs(determinant) < 1e-10) return mat3(0);
        t = (p1 * uv2.y - p2 * uv1.y) / determinant;
        b = (p2 * uv1.x - p1 * uv2.x) / determinant;
        t -= n * dot(n, t);
        if (dot(t, t) < 1e-10) return mat3(0);
        t = normalize(t);
        b = cross(n, t) * (dot(cross(n, t), b) < 0.0 ? -1.0 : 1.0);
    }
    return mat3(t, b, n);
}

vec3 gltf_normal(gltf_material material, vec3 normal, mat3 frame, vec4 normal_sample, bool front_facing) {
    vec3 n = normalize(normal);
    if (material.textures[2].y != 0 && dot(frame[0], frame[0]) > 1e-10) {
        vec3 sampled = normal_sample.xyz * 2.0 - 1.0;
        sampled.xy *= material.roughness_normal_occlusion_cutoff.y;
        if (dot(sampled, sampled) > 1e-10) n = normalize(frame * normalize(sampled));
    }
    return front_facing ? n : -n;
}

#include "gltf_layers.glsl"
#ifndef GBUFFER_PASS
layout(set = 0, binding = 5) uniform sampler3D shadow_visibility;
#define VKZ_QUERY_SET 2
#include "../../../../src/reference/query_surface.glsl"
layout(set = 2, binding = 4) uniform accelerationStructureEXT scene_acceleration;
#endif

vec4 gltf_shade(gltf_scene scene, gltf_material material, vec4 vertex_color, vec4 samples[17], gltf_geometry geometry, vec3 v) {
    vec4 base = gltf_base_color(material, vertex_color, samples[0]);
    float alpha = material.flags.x == 2 ? base.a : 1.0;
    if (material.flags.z != 0) return vec4(base.rgb, alpha);
    vec2 mr = clamp(vec2(material.emissive_metallic.w, material.roughness_normal_occlusion_cutoff.x) * samples[1].bg, 0.0, 1.0);
    float transmission = clamp(material.optical.y * samples[5].r, 0.0, 1.0);
    vec3 direct = vec3(0);
#ifdef VKZ_LIGHT_BINDING
    for (uint i = 0; i < scene.num_lights; ++i) {
        gltf_light light = gltf_lights.data[i];
        vec3 delta = light.position_range.xyz - geometry.position;
        float distance_squared = dot(delta, delta);
        bool directional = light.direction_type.w == 0.0;
        vec3 l = directional ? -light.direction_type.xyz : delta * inversesqrt(max(distance_squared,1e-10));
        float attenuation = directional ? 1.0 : 1.0 / max(distance_squared,1e-10);
        if (!directional && light.position_range.w > 0.0) {
            float ratio = distance_squared / (light.position_range.w * light.position_range.w);
            attenuation *= max(1.0 - ratio * ratio, 0.0);
        }
        if (light.direction_type.w == 2.0) {
            float cosine = dot(-l, light.direction_type.xyz);
            float spot = clamp((cosine-light.cone.y)/max(light.cone.x-light.cone.y,1e-7),0.0,1.0);
            attenuation *= spot*spot;
        }
#ifndef GBUFFER_PASS
        if (scene.enable_shadows != 0u && material.flags.x == 2) {
            float distance = directional ? 100 : max(sqrt(distance_squared) - 0.01, 0.011);
            attenuation *= trace_surface_visibility(scene_acceleration, geometry.position + geometry.normal * 0.005,
                l, distance, uint(scene.vertex_layout.x)).x;
        } else if (scene.enable_shadows != 0u) {
            vec2 screen_uv = gl_FragCoord.xy / scene.render_resolution;
            float layer = (float(i) + 0.5) / float(max(scene.num_lights, 1u));
            attenuation *= texture(shadow_visibility, vec3(screen_uv, layer)).r;
        }
#endif

        if (attenuation > 0.0)
            direct += gltf_layered_direct(material,samples,geometry,base.rgb,mr,transmission,v,l)*light.radiance.rgb*attenuation;
    }
#endif
    float coat_transmission = 1.0 - material.clearcoat.x*samples[9].r*gltf_schlick(0.04,abs(dot(geometry.coat_normal,v)));
    vec3 emissive = material.emissive_metallic.rgb * samples[4].rgb * coat_transmission;
    return vec4(direct + emissive, alpha);
}
#endif
