#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_ray_tracing : require

#include "shared.glsl"
#include "surface.glsl"

struct light_data {
    vec4 position_range;
    vec4 direction_type;
    vec4 radiance;
    vec4 cone;
};

layout(set = 2, binding = 0, std430) readonly buffer Lights {
    light_data lights[];
};

layout(location = 0) rayPayloadInEXT ray_payload payload;

void main() {
    gltf_material material = materials[instance_data().z];
    payload.distance = gl_HitTEXT;
    payload.hit_distance = gl_HitTEXT;

    uvec3 indices = triangle_vertices();
    vec3 a = vec3(gl_ObjectToWorldEXT * vec4(read_vertex3(indices.x, 0), 1));
    vec3 b = vec3(gl_ObjectToWorldEXT * vec4(read_vertex3(indices.y, 0), 1));
    vec3 c = vec3(gl_ObjectToWorldEXT * vec4(read_vertex3(indices.z, 0), 1));
    payload.geometric_normal = normalize(cross(b - a, c - a));

    // Two-sided surfaces have no interior; use the normal facing the probe.
    if (material.flags.y != 0 && dot(payload.geometric_normal, gl_WorldRayDirectionEXT) > 0)
        payload.geometric_normal = -payload.geometric_normal;

    if (gl_HitKindEXT == gl_HitKindBackFacingTriangleEXT && material.flags.y == 0) {
        payload.radiance = vec3(0);
        payload.distance *= -0.2;
        return;
    }

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

    float f0 = pow((material.optical.x - 1) / (material.optical.x + 1), 2);
    vec3 specular_f0 = min(vec3(f0) * material.specular.rgb * surface_sample(material, 8).rgb, vec3(1));
    float specular_weight = material.specular.w * surface_sample(material, 7).a;
    vec3 view_direction = -gl_WorldRayDirectionEXT;

    for (uint i = 0; i < num_lights; ++i) {
        light_data light = lights[i];
        bool directional = light.direction_type.w == 0;
        vec3 delta = directional ? -light.direction_type.xyz : light.position_range.xyz - position;
        float distance_squared = max(dot(delta, delta), 1e-8);
        vec3 direction = normalize(delta);
        float attenuation = directional ? 1 : 1 / distance_squared;

        if (!directional && light.position_range.w > 0) {
            float ratio = distance_squared / (light.position_range.w * light.position_range.w);
            attenuation *= max(1 - ratio * ratio, 0);
        }

        if (light.direction_type.w == 2) {
            float spot = clamp((dot(-direction, light.direction_type.xyz) - light.cone.y) / max(light.cone.x - light.cone.y, 1e-7), 0, 1);
            attenuation *= spot * spot;
        }

        vec3 half_vector = direction + view_direction;
        float view_half = dot(half_vector, half_vector) > 1e-8 ? max(dot(view_direction, normalize(half_vector)), 0) : 0;
        vec3 fresnel = specular_f0 + (vec3(1) - specular_f0) * pow(1 - view_half, 5);
        result += light.radiance.rgb * attenuation * max(dot(normal, direction), 0) * diffuse_color * (vec3(1) - fresnel * specular_weight) / PI;
    }

    // Keep the indirect-light normalization used by the reference DDGI passes.
    if (use_infinite_bounces())
        result += diffuse_color * sample_irradiance(position, normal, camera_position) * infinite_bounces_multiplier;

    payload.radiance = max(result, vec3(0));
}
