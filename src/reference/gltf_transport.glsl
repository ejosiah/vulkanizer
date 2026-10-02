#ifndef VKZ_GLTF_TRANSPORT_GLSL
#define VKZ_GLTF_TRANSPORT_GLSL
#include "gltf_material.glsl"
uint gltf_hash(uint value) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}
struct gltf_surface {
    vec4 radiance;
    vec3 weight;
    vec3 direction;
};
struct gltf_medium {
    uint instance_id;
    float ior;
    vec4 attenuation;
};
struct gltf_volume_state {
    gltf_medium media[9];
    int count;
    int channel;
};
vec3 gltf_attenuation(vec4 attenuation, float distance) {
    if (distance <= 0.0 || isinf(attenuation.w)) return vec3(1);
    float exponent = distance / attenuation.w;
    vec3 result;
    for (int i = 0; i < 3; ++i)
        result[i] = attenuation[i] >= 1.0 ? 1.0 : attenuation[i] <= 0.0 ? 0.0 : exp(log(attenuation[i]) * exponent);
    return result;
}
vec3 gltf_medium_attenuation(gltf_volume_state volume, float distance) {
    return volume.count == 0 ? vec3(1) : gltf_attenuation(volume.media[volume.count - 1].attenuation, distance);
}
gltf_surface gltf_evaluate(gltf_scene scene, gltf_material material, vec4 color, vec4 samples[17], gltf_geometry geometry, vec3 direction, bool front, uint seed,
    inout gltf_volume_state volume, uint instance_id, float distance) {
    vec3 n = geometry.normal;
    float ior = material.optical.x;
    if (material.optical.w > 0.0 && material.optical.z > 0.0 && ior > 0.0)
        ior = max(1.0, ior + float(volume.channel - 1) * (ior - 1.0) * 0.025 * material.optical.w);
    bool solid = material.flags.z == 0 && material.optical.z > 0.0;
    int medium_index = -1;
    for (int i = 0; i < volume.count; ++i) if (volume.media[i].instance_id == instance_id) medium_index = i;
    vec3 attenuation = gltf_medium_attenuation(volume, distance);
    float incident_ior = volume.count == 0 ? 1.0 : volume.media[volume.count - 1].ior;
    if (solid && !front && volume.count == 0) {
        incident_ior = ior;
        attenuation = gltf_attenuation(material.attenuation, distance);
    }
    float outgoing_ior = ior;
    if (solid && !front) {
        outgoing_ior = medium_index >= 0 && medium_index < volume.count - 1 ? incident_ior : volume.count > 1 ? volume.media[volume.count - 2].ior : 1.0;
    }
    gltf_material surface_material = material;
    if (solid && material.optical.x > 0.0 && incident_ior > 0.0) surface_material.optical.x = outgoing_ior / incident_ior;
    gltf_surface result;
    result.radiance = solid && !front ? vec4(0, 0, 0, 1) : gltf_shade(scene, surface_material, color, samples, geometry, -direction);
    result.radiance.rgb *= attenuation;
    result.weight = vec3(0);
    result.direction = direction;
    if (material.flags.z != 0) return result;
    float transmission = clamp(material.optical.y * samples[5].r, 0.0, 1.0);
    float metallic = clamp(material.emissive_metallic.w * samples[1].b, 0.0, 1.0);
    if (ior == 0.0 || ((!solid || front) && (transmission <= 0.0 || metallic >= 1.0))) return result;
    float roughness = clamp(material.roughness_normal_occlusion_cutoff.x * samples[1].g, 0.0, 1.0) * clamp(2.0 * ior - 2.0, 0.0, 1.0);
    vec2 u = vec2(gltf_hash(seed), gltf_hash(seed ^ 0x9e3779b9u)) * (1.0 / 4294967296.0);
    float a = roughness * roughness;
    float cosine = sqrt((1.0 - u.y) / max(1.0 + (a * a - 1.0) * u.y, 1e-7));
    float sine = sqrt(max(0.0, 1.0 - cosine * cosine));
    vec3 t = normalize(cross(abs(n.z) < 0.999 ? vec3(0, 0, 1) : vec3(0, 1, 0), n));
    vec3 h = normalize(t * (sine * cos(2.0 * GLTF_PI * u.x)) + cross(n, t) * (sine * sin(2.0 * GLTF_PI * u.x)) + n * cosine);
    if (dot(h, -direction) <= 0.0) h = n;
    float eta = solid ? incident_ior / outgoing_ior : 1.0 / ior;
    vec3 refracted = refract(direction, h, eta);
    if (solid && dot(refracted, refracted) < 1e-10) {
        result.direction = reflect(direction, h);
        result.weight = attenuation;
        return result;
    }
    float fresnel_cosine = solid && incident_ior > outgoing_ior ? abs(dot(refracted, h)) : dot(h, -direction);
    gltf_material boundary_material = material;
    boundary_material.optical.x = solid ? outgoing_ior / incident_ior : ior;
    vec3 fresnel = gltf_boundary_fresnel(boundary_material, samples, fresnel_cosine);
    if (solid) {
        result.direction = normalize(refracted);
        if (front && medium_index < 0 && volume.count < 9) {
            volume.media[volume.count].instance_id = instance_id;
            volume.media[volume.count].ior = ior;
            volume.media[volume.count].attenuation = material.attenuation;
            ++volume.count;
        } else if (!front && medium_index >= 0) {
            for (int i = medium_index; i + 1 < volume.count; ++i) volume.media[i] = volume.media[i + 1];
            --volume.count;
        }
    } else {
        vec3 outgoing = refract(refracted, n, ior);
        result.direction = dot(outgoing, outgoing) > 1e-10 ? normalize(outgoing) : direction;
    }
    vec3 boundary = solid && !front ? vec3(1) : gltf_base_color(material, color, samples[0]).rgb * transmission * (1.0 - metallic);
    float layer_transmission = 1.0 - material.clearcoat.x * samples[9].r * gltf_schlick(0.04, abs(dot(geometry.coat_normal, direction)));
    vec3 sheen = material.sheen.rgb * samples[12].rgb;
    if (gltf_max(sheen) > 0.0) layer_transmission *= 1.0 - gltf_max(sheen) * gltf_sheen_energy(abs(dot(n,direction)), material.sheen.a*samples[13].a);
    result.weight = attenuation * boundary * (1.0 - fresnel) * layer_transmission;
    return result;
}
gltf_surface gltf_evaluate(gltf_scene scene, gltf_material material, vec4 color, vec4 samples[17], vec3 n, vec3 direction, bool front, uint seed,
    inout gltf_volume_state volume, uint instance_id, float distance) {
    gltf_geometry geometry = gltf_geometry(vec3(0), n, vec3(1,0,0), vec3(0,1,0), n);
    return gltf_evaluate(scene, material, color, samples, geometry, direction, front, seed, volume, instance_id, distance);
}

gltf_surface gltf_evaluate(gltf_scene scene, gltf_material material, vec4 color, vec4 samples[17], vec3 n, vec3 direction, bool front, uint seed) {
    gltf_volume_state volume;
    volume.count = 0;
    volume.channel = 1;
    return gltf_evaluate(scene, material, color, samples, n, direction, front, seed, volume, 0u, 0.0);
}
#endif
