#extension GL_EXT_scalar_block_layout : require

const float pi = 3.141592653589793;

struct reflection_payload {
    vec3 radiance;
    float hit_distance;
};

layout(set = 0, binding = 0, scalar) uniform reflection_constants {
    mat4 inverse_view_projection;
    mat4 previous_view_projection;
    vec3 camera_position;
    uint frame_index;
    uvec2 resolution;
    uint num_lights;
    uint vertex_stride;
    uint history_valid;
    uint enabled;
};

struct light_data {
    vec4 position_range;
    vec4 direction_type;
    vec4 radiance;
    vec4 cone;
};

layout(set = 0, binding = 1, scalar) readonly buffer light_buffer { light_data lights[]; };
layout(set = 0, binding = 2) uniform sampler2D scene_depth;
layout(set = 0, binding = 3) uniform sampler2D scene_normal;
layout(set = 0, binding = 4) uniform sampler2D scene_surface;
layout(set = 0, binding = 5) uniform usampler2D scene_mesh_id;
layout(set = 0, binding = 6, rgba16f) uniform image2D raw_color;
layout(set = 0, binding = 7, rgba16f) uniform image2D integrated_color;
layout(set = 0, binding = 8, rgba16f) uniform image2D filtered_0;
layout(set = 0, binding = 9, rgba16f) uniform image2D filtered_1;
layout(set = 0, binding = 10, rgba16f) uniform image2D history_color_0;
layout(set = 0, binding = 11, rgba16f) uniform image2D history_color_1;
layout(set = 0, binding = 12, rgba32f) uniform image2D history_moments_0;
layout(set = 0, binding = 13, rgba32f) uniform image2D history_moments_1;
layout(set = 0, binding = 14, rgba32f) uniform image2D history_world_0;
layout(set = 0, binding = 15, rgba32f) uniform image2D history_world_1;
layout(set = 0, binding = 16, rgba16f) uniform image2D history_normal_0;
layout(set = 0, binding = 17, rgba16f) uniform image2D history_normal_1;
layout(set = 0, binding = 18, rgba32f) uniform image2D integrated_moments;

#include "../ddgi/octahedral.glsl"

ivec2 source_pixel(ivec2 p) {
    ivec2 size = textureSize(scene_depth, 0);
    ivec2 scale = max((size + ivec2(resolution) - 1) / ivec2(resolution), ivec2(1));
    ivec2 closest = min(p * scale, size - 1);
    float depth = texelFetch(scene_depth, closest, 0).r;

    for (int y = 0; y < scale.y; ++y) {
        for (int x = 0; x < scale.x; ++x) {
            ivec2 q = min(p * scale + ivec2(x, y), size - 1);
            float candidate = texelFetch(scene_depth, q, 0).r;

            if (candidate < depth) {
                closest = q;
                depth = candidate;
            }
        }
    }

    return closest;
}

vec3 world_at(ivec2 p) {
    ivec2 q = source_pixel(p);
    vec2 uv = (vec2(q) + 0.5) / vec2(textureSize(scene_depth, 0));
    vec4 position = inverse_view_projection * vec4(uv * 2 - 1, texelFetch(scene_depth, q, 0).r, 1);
    return position.xyz / position.w;
}

vec3 normal_at(ivec2 p) {
    return oct_decode(texelFetch(scene_normal, source_pixel(p), 0).xy);
}

float luminance(vec3 color) { return dot(color, vec3(0.2126, 0.7152, 0.0722)); }
bool even_frame() { return (frame_index & 1u) == 0u; }

vec3 current_world(ivec2 p) {
    return even_frame() ? imageLoad(history_world_0, p).xyz : imageLoad(history_world_1, p).xyz;
}

vec3 current_normal(ivec2 p) {
    return even_frame() ? imageLoad(history_normal_0, p).xyz : imageLoad(history_normal_1, p).xyz;
}

bool reflective_pixel(ivec2 p) {
    vec4 normal = even_frame() ? imageLoad(history_normal_0, p) : imageLoad(history_normal_1, p);
    vec4 world = even_frame() ? imageLoad(history_world_0, p) : imageLoad(history_world_1, p);
    return normal.w <= 0.3 && world.w > 0;
}
