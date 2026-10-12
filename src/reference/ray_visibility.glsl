#ifndef VKZ_RAY_VISIBILITY_GLSL
#define VKZ_RAY_VISIBILITY_GLSL

// Caller supplies ordinary mesh/material descriptors and the vertex stride.
vec2 trace_surface_visibility(accelerationStructureEXT acceleration, vec3 origin,
    vec3 direction, float distance, uint stride) {
    rayQueryEXT query;
    rayQueryInitializeEXT(query, acceleration, gl_RayFlagsNoneEXT,
        0xff, origin, 0.01, direction, distance);
    float visibility = 1;
    float nearest_blocker = distance;

    while (rayQueryProceedEXT(query)) {
        if (rayQueryGetIntersectionTypeEXT(query, false) != gl_RayQueryCandidateIntersectionTriangleEXT)
            continue;

        uvec4 instance = instances[rayQueryGetIntersectionInstanceCustomIndexEXT(query, false)];
        gltf_material material = materials[instance.z];
        vec2 barycentrics = rayQueryGetIntersectionBarycentricsEXT(query, false);
        vec3 weights = vec3(1 - barycentrics.x - barycentrics.y, barycentrics);
        uint start = instance.y + 3 * rayQueryGetIntersectionPrimitiveIndexEXT(query, false);
        vec2 uv = vec2(0);
        float alpha = 0;

        for (uint i = 0; i < 3; ++i) {
            uint vertex = (index_data[start + i] + instance.x) * stride;
            uint offset = vertex + uint(material.textures[0].x);
            uv += weights[i] * vec2(vertex_data[offset], vertex_data[offset + 1]);
            alpha += weights[i] * vertex_data[vertex + 13];
        }

        alpha *= material.base_color.a;
        if (material.textures[0].y != 0)
            alpha *= textureLod(material_textures[nonuniformEXT(material.textures[0].z)], gltf_uv(material, 0, uv), 0).a;
        alpha = clamp(alpha, 0, 1);

        if (material.flags.x == 1 && alpha < material.roughness_normal_occlusion_cutoff.w)
            continue;

        if (material.flags.x == 2) {
            visibility *= 1 - alpha;
            if (alpha > 0)
                nearest_blocker = min(nearest_blocker, rayQueryGetIntersectionTEXT(query, false));
        } else {
            rayQueryConfirmIntersectionEXT(query);
        }
    }

    if (rayQueryGetIntersectionTypeEXT(query, true) != gl_RayQueryCommittedIntersectionNoneEXT) {
        visibility = 0;
        nearest_blocker = min(nearest_blocker, rayQueryGetIntersectionTEXT(query, true));
    }

    return vec2(visibility, nearest_blocker);
}

#endif
