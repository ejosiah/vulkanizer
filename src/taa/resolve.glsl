#ifndef GLSL_TAA_SIMPLE
#define GLSL_TAA_SIMPLE

#define HISTORY_SAMPLING_FILTER_SINGLE 0
#define HISTORY_SAMPLING_FILTER_CATMULL_ROM 1

#define SUB_SAMPLE_FILTER_NONE 0
#define SUB_SAMPLE_FILTER_MICHELL 1
#define SUB_SAMPLE_FILTER_BLACKMAN_HARRIS 2
#define SUB_SAMPLE_FILTER_CATMULL_ROM 3

#define HISTORY_CONSTRIANT_NONE 0
#define HISTORY_CONSTRIANT_CLAMP 1
#define HISTORY_CONSTRIANT_CLIP 2
#define HISTORY_CONSTRIANT_VARIANCE_CLIP 3
#define HISTORY_CONSTRIANT_VARIANCE_CLIP_CLAMP 4

#include "filters.glsl"
#include "uniforms.glsl"
#define history_filter_type uniforms.filters.x
#define subsample_filter_type uniforms.filters.y
#define history_constraint_type uniforms.filters.z
#define temporal_filtering ((uniforms.filters.w & 1u) != 0u ? 1 : 0)
#define inverse_luminance_filtering ((uniforms.filters.w & 2u) != 0u ? 1 : 0)
#define luminance_difference_filtering ((uniforms.filters.w & 4u) != 0u ? 1 : 0)
#define taa_simple ((uniforms.filters.w & 8u) != 0u ? 1 : 0)

struct ColorSample {
    vec3 value;
    vec3 min;
    vec3 max;

    // moments
    vec3 m1;
    vec3 m2;
};

float luminance(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

void find_closest_fragment3x3(ivec2 pos, out ivec2 closest_pos, out float closest_depth) {
    closest_pos = pos;
    closest_depth = read_depth(pos);

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            ivec2 candidate_pos = clamp(pos + ivec2(x, y), ivec2(0), ivec2(uniforms.resolution - 1));
            float candidate_depth = read_depth(candidate_pos);

            if (candidate_depth < closest_depth) {
                closest_pos = candidate_pos;
                closest_depth = candidate_depth;
            }
        }
    }
}

vec3 sample_history_color(vec2 uv) {
    switch (history_filter_type) {
    case HISTORY_SAMPLING_FILTER_SINGLE:
        return texture(history_buffer, uv).rgb;
    case HISTORY_SAMPLING_FILTER_CATMULL_ROM:
        return sample_texture_catmull_rom(uv, history_buffer, uniforms.resolution);
    default:
        return vec3(1, 0, 0);
    }
}

// Choose between different filters.
float subsample_filter(float value) {
    switch (subsample_filter_type) {
    case SUB_SAMPLE_FILTER_NONE:
        return value < 0.5 ? 1.0 : 0.0;
    case SUB_SAMPLE_FILTER_MICHELL:
        return filter_mitchell(value);
    case SUB_SAMPLE_FILTER_BLACKMAN_HARRIS:
        return filter_blackman_harris(value);
    case SUB_SAMPLE_FILTER_CATMULL_ROM:
        return filter_catmull_rom(value);
    }
    return value;
}

ColorSample sample_color(ivec2 pos) {
    ColorSample color_sample;
    color_sample.min = vec3(10000);
    color_sample.max = vec3(-1000);
    color_sample.m1 = vec3(0);
    color_sample.m2 = vec3(0);

    vec3 total = vec3(0);
    float weight = 0;

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            ivec2 sample_pos = clamp(pos + ivec2(x, y), ivec2(0), ivec2(uniforms.resolution - 1));

            vec3 current_sample = texelFetch(color_buffer, sample_pos, 0).rgb;
            vec2 subsample_position = vec2(x, y) - uniforms.jitter_xy;
            float subsample_distance = length(subsample_position);
            float subsample_weight = subsample_filter(subsample_distance);

            total += current_sample * subsample_weight;
            weight += subsample_weight;

            color_sample.min = min(color_sample.min, current_sample);
            color_sample.max = max(color_sample.max, current_sample);

            color_sample.m1 += current_sample;
            color_sample.m2 += current_sample * current_sample;
        }
    }

    color_sample.value = subsample_filter_type == SUB_SAMPLE_FILTER_NONE ? texelFetch(color_buffer, pos, 0).rgb : max(total / max(weight, 1e-6), vec3(0.0));
    return color_sample;
}

// Optimized clip aabb function from Inside game.
vec4 clip_aabb(vec3 aabb_min, vec3 aabb_max, vec4 previous_sample, float average_alpha) {
    // note: only clips towards aabb center (but fast!)
    vec3 p_clip = 0.5 * (aabb_max + aabb_min);
    vec3 e_clip = 0.5 * (aabb_max - aabb_min) + 0.000000001f;

    vec4 v_clip = previous_sample - vec4(p_clip, average_alpha);
    vec3 v_unit = v_clip.xyz / e_clip;
    vec3 a_unit = abs(v_unit);
    float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));

    if (ma_unit > 1.0) {
        return vec4(p_clip, average_alpha) + v_clip / ma_unit;
    } else {
        // point inside aabb
        return previous_sample;
    }
}

vec3 constrainHistory(ColorSample current_sample, vec3 history_color) {
    switch (history_constraint_type) {
    case HISTORY_CONSTRIANT_NONE:
        return history_color;

    case HISTORY_CONSTRIANT_CLAMP:
        return clamp(history_color, current_sample.min, current_sample.max);

    case HISTORY_CONSTRIANT_CLIP:
        return clip_aabb(current_sample.min, current_sample.max, vec4(history_color, 1.0f), 1.0f).rgb;

    case HISTORY_CONSTRIANT_VARIANCE_CLIP: {
        float rcp_sample_count = 1.0f / 9.0f;
        float gamma = 1.0f;
        vec3 mu = current_sample.m1 * rcp_sample_count;
        vec3 sigma = sqrt(abs((current_sample.m2 * rcp_sample_count) - (mu * mu)));
        vec3 minc = mu - gamma * sigma;
        vec3 maxc = mu + gamma * sigma;

        return clip_aabb(minc, maxc, vec4(history_color, 1), 1.0f).rgb;
    }
    case HISTORY_CONSTRIANT_VARIANCE_CLIP_CLAMP:
        float rcp_sample_count = 1.0f / 9.0f;
        float gamma = 1.0f;
        vec3 mu = current_sample.m1 * rcp_sample_count;
        vec3 sigma = sqrt(abs((current_sample.m2 * rcp_sample_count) - (mu * mu)));
        vec3 minc = mu - gamma * sigma;
        vec3 maxc = mu + gamma * sigma;

        vec3 clamped_history_color = clamp(history_color.rgb, current_sample.min, current_sample.max);
        return clip_aabb(minc, maxc, vec4(clamped_history_color, 1), 1.0f).rgb;
    }
    return history_color;
}

bool valid_history(vec2 uv, vec4 motion) {
    if (uniforms.state.x == 0u || motion.w == 0.0 || any(lessThan(uv, vec2(0.0))) || any(greaterThanEqual(uv, vec2(1.0)))) {
        return false;
    }
    float previous_depth = textureLod(history_depth, uv, 0).r;
    return abs(previous_depth - motion.z) <= uniforms.tuning.y;
}

vec3 taa_resolve_simple(ivec2 pos) {
    vec4 motion = texelFetch(velocity_buffer, pos, 0);
    vec2 screen_uv = (vec2(pos) + 0.5) / uniforms.resolution;
    vec2 reprojected_uv = screen_uv - motion.xy;
    vec3 current_color = texelFetch(color_buffer, pos, 0).rgb;
    if (!valid_history(reprojected_uv, motion)) {
        return current_color;
    }
    return mix(current_color, textureLod(history_buffer, reprojected_uv, 0).rgb, uniforms.tuning.x);
}

void filter_blend_weights(ColorSample current_sample, vec3 history_color, out vec3 current_weight, out vec3 history_weight) {
    current_weight = vec3(1.0 - uniforms.tuning.x);
    history_weight = vec3(1.0 - current_weight);

    // Temporal filtering
    if (temporal_filtering == 1) {
        vec3 temporal_weight = clamp(abs(current_sample.max - current_sample.min) / max(abs(current_sample.value), vec3(1e-6)), vec3(0), vec3(1));
        history_weight = clamp(mix(vec3(0.25), vec3(0.85), temporal_weight), vec3(0), vec3(1));
        current_weight = 1.0f - history_weight;
    }

    history_weight = clamp(history_weight, vec3(0.0), vec3(uniforms.tuning.x));
    current_weight = 1.0 - history_weight;

    // Inverse luminance filtering
    if (inverse_luminance_filtering == 1 || luminance_difference_filtering == 1) {
        // Calculate compressed colors and luminances
        vec3 compressed_source = current_sample.value / (max(max(current_sample.value.r, current_sample.value.g), current_sample.value.b) + 1.0f);
        vec3 compressed_history = history_color / (max(max(history_color.r, history_color.g), history_color.b) + 1.0f);
        float luminance_source = luminance(compressed_source);
        float luminance_history = luminance(compressed_history);

        if (luminance_difference_filtering == 1) {
            float unbiased_diff = abs(luminance_source - luminance_history) / max(luminance_source, max(luminance_history, 0.2));
            float unbiased_weight = 1.0 - unbiased_diff;
            float unbiased_weight_sqr = unbiased_weight * unbiased_weight;
            float k_feedback = mix(0.0f, 1.0f, unbiased_weight_sqr);

            history_weight = vec3(k_feedback);
            current_weight = vec3(1.0 - k_feedback);
        }

        history_weight = clamp(history_weight, vec3(0.0), vec3(uniforms.tuning.x));
        current_weight = 1.0 - history_weight;
        current_weight *= 1.0 / (1.0 + luminance_source);
        history_weight *= 1.0 / (1.0 + luminance_history);
    }
}

vec3 taa_resolve(ivec2 pos) {

    ivec2 cpos;
    float cdepth;
    find_closest_fragment3x3(pos, cpos, cdepth);
    vec4 motion = texelFetch(velocity_buffer, cpos, 0);
    vec2 velocity = motion.xy;
    vec2 screen_uv = (vec2(pos) + 0.5) / uniforms.resolution;
    vec2 reprojected_uv = screen_uv - velocity;

    ColorSample current_sample = sample_color(pos);

    // Guard for outside sampling
    if (!valid_history(reprojected_uv, motion)) {
        return current_sample.value;
    }

    vec3 historic_color = constrainHistory(current_sample, max(sample_history_color(reprojected_uv), vec3(0.0)));
    vec3 current_weight;
    vec3 history_weight;
    filter_blend_weights(current_sample, historic_color, current_weight, history_weight);

    vec3 result = (current_sample.value * current_weight + historic_color * history_weight) / max(current_weight + history_weight, 0.00001);
    return result;
}

vec3 resolve(ivec2 pos) {
    if (uniforms.state.x == 0u) {
        return texelFetch(color_buffer, pos, 0).rgb;
    }
    return taa_simple == 1 ? taa_resolve_simple(pos) : taa_resolve(pos);
}

#endif // GLSL_TAA_SIMPLE
