#extension GL_EXT_nonuniform_qualifier : require
#include "gltf_transport.glsl"
#ifdef VKZ_REFERENCE_IBL
#define VKZ_QUERY_SET 3
#else
#define VKZ_QUERY_SET 2
#endif
layout(set = VKZ_QUERY_SET, binding = 0, std140) uniform QueryScene {
    gltf_scene scene;
    mat4 inverse_view_projection;
    vec4 background;
    vec4 ray_parameters;
    uvec4 sampling;
} reference;
layout(set = VKZ_QUERY_SET, binding = 1, std430) readonly buffer Vertices { float data[]; } vertices;
layout(set = VKZ_QUERY_SET, binding = 2, std430) readonly buffer Indices { uint data[]; } indices;
layout(set = VKZ_QUERY_SET, binding = 3, std430) readonly buffer Instances { uvec4 data[]; } instances;
layout(set = VKZ_QUERY_SET, binding = 4, std430) readonly buffer Materials { gltf_material data[]; } materials;
layout(set = VKZ_QUERY_SET, binding = 5) uniform accelerationStructureEXT scene_bvh;
layout(constant_id = 0) const uint texture_count = 1;
layout(set = VKZ_QUERY_SET, binding = 6) uniform sampler2D textures[texture_count];
#include "gltf_surface.glsl"

bool accept_candidate(rayQueryEXT query, uint seed) {
    uint id = rayQueryGetIntersectionInstanceCustomIndexEXT(query, false);
    uint primitive = rayQueryGetIntersectionPrimitiveIndexEXT(query, false);
    uvec4 instance_data = instances.data[id];
    gltf_material hit_material = materials.data[instance_data.z];
    bool front = rayQueryGetIntersectionFrontFaceEXT(query, false);
    if (!front && hit_material.flags.y == 0 && hit_material.optical.z == 0.0) return false;
    uvec3 triangle = triangle_vertices(instance_data, primitive);
    vec2 barycentric = rayQueryGetIntersectionBarycentricsEXT(query, false);
    vec3 weights = vec3(1.0 - barycentric.x - barycentric.y, barycentric);
    float alpha = gltf_base_color(hit_material, fetch_vertex_color(triangle, weights), sample_texture(hit_material, 0, triangle, weights)).a;
    if (gltf_masked(hit_material, alpha)) return false;
    if (hit_material.flags.x == 2) {
        float value = float(gltf_hash(seed ^ gltf_hash(id) ^ gltf_hash(primitive + 1u)) >> 8) * (1.0 / 16777216.0);
        return value < clamp(alpha, 0.0, 1.0);
    }
    return true;
}

vec3 gltf_query_transmission(vec3 origin, gltf_surface surface, uint seed, inout gltf_volume_state volume) {
    vec3 radiance = surface.radiance.rgb;
    vec3 weight = surface.weight;
    vec3 direction = surface.direction;
    for (uint bounce = 0; bounce < min(reference.sampling.w, 8u); ++bounce) {
        if (max(weight.r, max(weight.g, weight.b)) < 1e-5) break;
        rayQueryEXT query;
        rayQueryInitializeEXT(query, scene_bvh, gl_RayFlagsNoOpaqueEXT, reference.sampling.y,
            origin, reference.ray_parameters.x, direction, reference.ray_parameters.y);
        uint bounce_seed = gltf_hash(seed ^ gltf_hash(bounce + 1u));
        while (rayQueryProceedEXT(query)) {
            if (rayQueryGetIntersectionTypeEXT(query, false) == gl_RayQueryCandidateIntersectionTriangleEXT && accept_candidate(query, bounce_seed))
                rayQueryConfirmIntersectionEXT(query);
        }
        if (rayQueryGetIntersectionTypeEXT(query, true) == gl_RayQueryCommittedIntersectionNoneEXT) {
#ifdef VKZ_REFERENCE_IBL
            radiance += weight * gltf_medium_attenuation(volume, reference.ray_parameters.y) * gltf_environment(direction);
#else
            radiance += weight * gltf_medium_attenuation(volume, reference.ray_parameters.y) * reference.background.rgb;
#endif
            break;
        }
        surface = evaluate_triangle(instances.data[rayQueryGetIntersectionInstanceCustomIndexEXT(query, true)],
            rayQueryGetIntersectionPrimitiveIndexEXT(query, true), rayQueryGetIntersectionBarycentricsEXT(query, true),
            rayQueryGetIntersectionObjectToWorldEXT(query, true), rayQueryGetIntersectionWorldToObjectEXT(query, true),
            rayQueryGetIntersectionFrontFaceEXT(query, true), direction, bounce_seed, volume, rayQueryGetIntersectionInstanceCustomIndexEXT(query, true), rayQueryGetIntersectionTEXT(query, true));
        radiance += weight * surface.radiance.rgb;
        weight *= surface.weight;
        origin += direction * rayQueryGetIntersectionTEXT(query, true);
        direction = surface.direction;
    }
    return radiance;
}
