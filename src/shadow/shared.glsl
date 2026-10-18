#extension GL_EXT_scalar_block_layout : require

layout(set = 0, binding = 0, scalar) uniform shadow_constants {
    mat4 inverse_view_projection;
    mat4 previous_view_projection;
    vec3 camera_position;
    uint frame_index;
    uvec2 resolution;
    uint num_lights;
    uint history_count;
    uint vertex_stride;
    uint alpha_test;
    uint world_positions;
    float directional_distance;
}

constants;

struct light {
    vec4 position_range;
    vec4 direction_type;
    vec4 radiance;
    vec4 cone;
};

layout(set = 0, binding = 1, scalar) readonly buffer light_buffer {
    light lights[];
};

layout(set = 0, binding = 2) uniform sampler2D scene_depth;
layout(set = 0, binding = 3) uniform sampler2D scene_normal;
layout(set = 0, binding = 18) uniform sampler2D scene_world_positions;
layout(set = 0, binding = 5, rg16f) uniform image2D motion_vector;
layout(set = 0, binding = 6, rgba16f) uniform image3D visibility_cache;
layout(set = 0, binding = 7, r16f) uniform image3D variation;
layout(set = 0, binding = 8, r16f) uniform image3D max_variation;
layout(set = 0, binding = 9, r16f) uniform image3D filtered_variation;
layout(set = 0, binding = 10, r16f) uniform image3D filtered_visibility;
layout(set = 0, binding = 11, rgba8ui) uniform uimage3D sample_count_cache;

ivec2 source_pixel(ivec2 p) {
    ivec2 size = textureSize(scene_depth, 0);
    ivec2 scale = max((size + ivec2(constants.resolution) - 1) / ivec2(constants.resolution), ivec2(1));
    ivec2 closest = min(p * scale, size - 1);
    float nearest_depth = texelFetch(scene_depth, closest, 0).r;

    for (int y = 0; y < scale.y; ++y) {
        for (int x = 0; x < scale.x; ++x) {
            ivec2 candidate = min(p * scale + ivec2(x, y), size - 1);
            float depth = texelFetch(scene_depth, candidate, 0).r;

            if (depth < nearest_depth) {
                closest = candidate;
                nearest_depth = depth;
            }
        }
    }

    return closest;
}

vec3 decode_normal(vec2 e) {
    vec3 n = vec3(e, 1 - abs(e.x) - abs(e.y));
    n.xy += vec2(n.x >= 0 ? -1 : 1, n.y >= 0 ? -1 : 1) * max(-n.z, 0);
    return normalize(n);
}

vec3 normal_at(ivec2 p) {
    return decode_normal(texelFetch(scene_normal, source_pixel(p), 0).xy);
}

float depth_at(ivec2 p) {
    return texelFetch(scene_depth, source_pixel(p), 0).r;
}

vec3 world_at(ivec2 p) {
    if (constants.world_positions != 0u) {
        return texelFetch(scene_world_positions, source_pixel(p), 0).xyz;
    }
    vec2 uv = (vec2(source_pixel(p)) + 0.5) / vec2(textureSize(scene_depth, 0));
    vec4 world = constants.inverse_view_projection * vec4(uv * 2 - 1, depth_at(p), 1);
    return world.xyz / world.w;
}

ivec3 bounded(ivec3 p) {
    return ivec3(clamp(p.xy, ivec2(0), ivec2(constants.resolution) - 1), p.z);
}

bool valid_pixel(ivec3 p) {
    return all(lessThan(p.xy, ivec2(constants.resolution))) && p.z < int(constants.num_lights);
}

layout(set = 0, binding = 12, rgba16f) uniform image3D alternate_visibility_cache;
layout(set = 0, binding = 13, rgba32f) uniform image2D world_history_0;
layout(set = 0, binding = 14, rgba32f) uniform image2D world_history_1;
layout(set = 0, binding = 15, rgba16f) uniform image2D normal_history_0;
layout(set = 0, binding = 16, rgba16f) uniform image2D normal_history_1;

bool even_frame() {
    return (constants.frame_index & 1u) == 0u;
}

vec4 previous_visibility(ivec3 p) {
    return even_frame() ? imageLoad(alternate_visibility_cache, bounded(p)) : imageLoad(visibility_cache, bounded(p));
}

vec4 current_visibility(ivec3 p) {
    return even_frame() ? imageLoad(visibility_cache, bounded(p)) : imageLoad(alternate_visibility_cache, bounded(p));
}

void store_visibility(ivec3 p, vec4 value) {
    if (even_frame())
        imageStore(visibility_cache, p, value);
    else
        imageStore(alternate_visibility_cache, p, value);
}

ivec2 previous_pixel(ivec2 p) {
    vec2 uv = (vec2(source_pixel(p)) + 0.5) / vec2(textureSize(scene_depth, 0));
    return clamp(ivec2((uv - imageLoad(motion_vector, p).xy) * vec2(constants.resolution)), ivec2(0), ivec2(constants.resolution) - 1);
}

bool valid_history(ivec2 p) {
    return constants.history_count > 0 && imageLoad(motion_vector, p).x > -0.5;
}
