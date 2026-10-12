#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_ray_query : require

#include "shared.glsl"
#include "surface.glsl"
#include "../reference/ray_visibility.glsl"
#include "ddgi_sampling.glsl"

layout(set = 1, binding = 4) uniform accelerationStructureEXT tlas;

layout(location = 0) rayPayloadInEXT reflection_payload payload;

void main() {
    payload.hit_distance = gl_HitTEXT;
    gltf_material material = materials[instance_data().z];

    if (gl_HitKindEXT == gl_HitKindBackFacingTriangleEXT && material.flags.y == 0) {
        payload.radiance = vec3(0);
        return;
    }

    uvec3 indices = triangle_vertices();
    vec3 weights = barycentric_weights();
    vec3 object_normal = vec3(0);
    vec4 object_tangent = vec4(0);

    for (int i = 0; i < 3; ++i) {
        object_normal += weights[i] * read_vertex3(indices[i], 3);
        object_tangent += weights[i] * vec4(read_vertex3(indices[i], 6), vertex_data[indices[i] * vertex_stride + 9]);
    }

    vec3 normal = normalize(transpose(mat3(gl_WorldToObjectEXT)) * object_normal);

    if (material.textures[2].y != 0 && dot(object_tangent.xyz, object_tangent.xyz) > 1e-8) {
        vec3 tangent = normalize(mat3(gl_ObjectToWorldEXT) * object_tangent.xyz);
        tangent = normalize(tangent - normal * dot(normal, tangent));
        vec3 bitangent = cross(normal, tangent) * sign(object_tangent.w) * sign(determinant(mat3(gl_ObjectToWorldEXT)));
        vec3 sampled_normal = surface_sample(material, 2).xyz * 2 - 1;
        sampled_normal.xy *= material.roughness_normal_occlusion_cutoff.y;
        normal = normalize(mat3(tangent, bitangent, normal) * sampled_normal);
    }

    if (gl_HitKindEXT == gl_HitKindBackFacingTriangleEXT)
        normal = -normal;

    vec3 position = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    vec3 base = material.base_color.rgb * vertex_color().rgb * surface_sample(material, 0).rgb;
    vec3 mr = surface_sample(material, 1).rgb;
    float metallic = clamp(material.emissive_metallic.w * mr.b, 0, 1);
    vec3 diffuse_color = base * (1 - metallic);
    vec3 result = material.emissive_metallic.rgb * surface_sample(material, 4).rgb;

    if (material.flags.z != 0) {
        payload.radiance = base;
        return;
    }

    // One importance-sampled light, with a guarded normalization and PDF compensation.
    float total_importance = 0;

    for (uint i = 0; i < num_lights; ++i) {
        light_data source = lights[i];
        bool directional = source.direction_type.w == 0;
        vec3 delta = directional ? -source.direction_type.xyz : source.position_range.xyz - position;
        float distance_squared = max(dot(delta, delta), 1e-6);
        float weight = max(dot(normal, normalize(delta)), 0) * max(luminance(source.radiance.rgb), 0);

        if (!directional) {
            float ratio = source.position_range.w > 0 ? distance_squared / (source.position_range.w * source.position_range.w) : 0;
            weight *= max(1 - ratio * ratio, 0) / distance_squared;
        }

        total_importance += weight;
    }

    uint random_bits = gl_LaunchIDEXT.x * 747796405u + gl_LaunchIDEXT.y * 277803737u + frame_index * 2891336453u;
    random_bits = ((random_bits >> ((random_bits >> 28u) + 4u)) ^ random_bits) * 277803737u;
    float target = float((random_bits >> 22u) ^ random_bits) / 4294967296.0 * total_importance;
    float accumulated = 0;

    if (total_importance > 0) {
        for (uint i = 0; i < num_lights; ++i) {
            light_data source = lights[i];
            bool directional = source.direction_type.w == 0;
            vec3 delta = directional ? -source.direction_type.xyz : source.position_range.xyz - position;
            float distance_squared = max(dot(delta, delta), 1e-6);
            vec3 direction = normalize(delta);
            float attenuation = directional ? 1 : 1 / distance_squared;

            if (!directional && source.position_range.w > 0) {
                float ratio = distance_squared / (source.position_range.w * source.position_range.w);
                attenuation *= max(1 - ratio * ratio, 0);
            }

            float cosine = max(dot(normal, direction), 0);
            float weight = cosine * attenuation * max(luminance(source.radiance.rgb), 0);
            accumulated += weight;

            if (weight <= 0 || accumulated <= target)
                continue;

            if (source.direction_type.w == 2) {
                float spot = clamp((dot(-direction, source.direction_type.xyz) - source.cone.y)
                    / max(source.cone.x - source.cone.y, 1e-6), 0, 1);
                attenuation *= spot * spot;
            }

            float visibility = trace_surface_visibility(tlas, position + normal * 0.005,
                direction, directional ? 100 : max(sqrt(distance_squared) - source.cone.z - 0.01, 0.011), vertex_stride).x;

            if (visibility > 0) {
                vec3 view = -gl_WorldRayDirectionEXT;
                vec3 half_vector = normalize(view + direction);
                float n_v = max(dot(normal, view), 0.001);
                float n_h = max(dot(normal, half_vector), 0.001);
                float v_h = max(dot(view, half_vector), 0);
                float roughness = max(material.roughness_normal_occlusion_cutoff.x * mr.g, 0.04);
                float alpha_squared = pow(roughness, 4);
                float denominator = n_h * n_h * (alpha_squared - 1) + 1;
                float distribution = alpha_squared / (PI * denominator * denominator);
                float geometry = 2 * n_v * cosine / max(cosine * sqrt(alpha_squared + (1 - alpha_squared) * n_v * n_v)
                    + n_v * sqrt(alpha_squared + (1 - alpha_squared) * cosine * cosine), 1e-6);
                float dielectric = pow((material.optical.x - 1) / (material.optical.x + 1), 2);
                vec3 f0 = mix(vec3(dielectric), base, metallic);
                vec3 fresnel = f0 + (1 - f0) * pow(1 - v_h, 5);
                vec3 brdf = diffuse_color * (1 - fresnel) / PI
                    + distribution * geometry * fresnel / max(4 * n_v * cosine, 1e-6);
                result += source.radiance.rgb * attenuation * visibility * cosine * brdf * total_importance / weight;
            }

            break;
        }
    }

    if (enabled == 1u)
        result += diffuse_color * sample_irradiance(position, normal, camera_position);
    payload.radiance = max(result, vec3(0));
}
