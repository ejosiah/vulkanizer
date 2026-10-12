#ifndef DDGI_SHARED_GLSL
#define DDGI_SHARED_GLSL

#extension GL_EXT_scalar_block_layout : enable

#ifndef PI
#define PI 3.14159265358979323846
#define TWO_PI (PI * 2.0)
#endif // PI

#include "octahedral.glsl"

#define PROBE_STATUS_OFF 0
#define PROBE_STATUS_SLEEP 1
#define PROBE_STATUS_ACTIVE 4
#define PROBE_STATUS_UNINITIALIZED 6

#define EPSILON 0.0001f

struct ray_payload {
    vec3 radiance;
    float distance;
    vec3 geometric_normal;
    float hit_distance;
};

layout(set = 0, binding = 0, scalar) uniform constants_buffer {
    mat4  random_rotation;
    vec3  probe_grid_position;
    vec3  probe_spacing;
    vec3  reciprocal_probe_spacing;
    ivec3 probe_counts;
    int   probe_update_offset;
    int   probe_update_count;
    int   probe_rays;
    int   irradiance_side_length;
    int   visibility_side_length;
    float hysteresis;
    float self_shadow_bias;
    float infinite_bounces_multiplier;
    uint output_resolution_half;

    uint num_lights;
    uint ddgi_debug_options;
    uint vertex_stride;
    uint frame_index;
    float max_ray_distance;
    mat4 inverse_view_projection;
    vec3 camera_position;
    float max_offset;
    float sphere_scale;
};

#include "debug_opts.glsl"

layout(set = 0, binding = 1, scalar) buffer probe_status_buffer {
    uint probe_status[];
};

layout(set = 1, binding = 0) uniform sampler2D depth_texture;
layout(set = 1, binding = 1) uniform sampler2D normal_texture;
layout(set = 1, binding = 2) uniform sampler2D radiance_texture;
layout(set = 1, binding = 3) uniform sampler2D irradiance_texture;
layout(set = 1, binding = 4) uniform sampler2D visibility_texture;
layout(set = 1, binding = 5) uniform sampler2D probe_offset_texture;

int irradiance_texture_width = textureSize(irradiance_texture, 0).x;
int irradiance_texture_height = textureSize(irradiance_texture, 0).y;

int visibility_texture_width = textureSize(visibility_texture, 0).x;
int visibility_texture_height = textureSize(visibility_texture, 0).y;

vec3 spherical_fibonacci(float i, float n) {
    const float pi = 3.14159265358;
    const float PHI = sqrt(5.0f) * 0.5 + 0.5;
    #define madfrac(A, B) ((A) * (B)-floor((A) * (B)))
    float phi       = 2.0 * pi * madfrac(i, PHI - 1);
    float cos_theta = 1.0 - (2.0 * i + 1.0) * (1.0 / n);
    float sin_theta = sqrt(clamp(1.0 - cos_theta * cos_theta, 0.0f, 1.0f));

    return vec3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);

    #undef madfrac
}

ivec3 probe_index_to_grid_indices(int probe_index) {
    const int probe_x = probe_index % probe_counts.x;
    const int probe_counts_xy = probe_counts.x * probe_counts.y;

    const int probe_y = (probe_index % probe_counts_xy) / probe_counts.x;
    const int probe_z = probe_index / probe_counts_xy;

    return ivec3(probe_x, probe_y, probe_z);
}

int probe_indices_to_index(in ivec3 probe_coords) {
    return int(probe_coords.x + probe_coords.y * probe_counts.x + probe_coords.z * probe_counts.x * probe_counts.y);
}

vec3 grid_indices_to_world_no_offsets(ivec3 grid_indices) {
    return grid_indices * probe_spacing + probe_grid_position;
}

vec3 grid_indices_to_world(ivec3 grid_indices, int probe_index) {
    const int probe_counts_xy = probe_counts.x * probe_counts.y;
    ivec2 probe_offset_sampling_coordinates = ivec2(probe_index % probe_counts_xy, probe_index / probe_counts_xy);
    vec3 probe_offset = use_probe_offsetting() ? texelFetch(probe_offset_texture, probe_offset_sampling_coordinates, 0).rgb : vec3(0);

    return grid_indices_to_world_no_offsets(grid_indices) + probe_offset;
}

ivec3 world_to_grid_indices(vec3 world_position) {
    return clamp(ivec3((world_position - probe_grid_position) * reciprocal_probe_spacing), ivec3(0), probe_counts - ivec3(1));
}

int get_probe_index_from_pixels(ivec2 pixels, int probe_with_border_side, int full_texture_width) {
    int probes_per_side = full_texture_width / probe_with_border_side;

    return int(pixels.x / probe_with_border_side) + probes_per_side * int(pixels.y / probe_with_border_side);
}

vec2 normalized_oct_coord(ivec2 frag_coord, int probe_side_length) {
    int probe_with_border_side = probe_side_length + 2;
    vec2 octahedral_texel_coordinates = ivec2((frag_coord.x - 1) % probe_with_border_side, (frag_coord.y - 1) % probe_with_border_side);

    octahedral_texel_coordinates += vec2(0.5f);
    octahedral_texel_coordinates *= (2.0f / float(probe_side_length));
    octahedral_texel_coordinates -= vec2(1.0f);

    return octahedral_texel_coordinates;
}

vec2 get_probe_uv(vec3 direction, int probe_index, int full_texture_width, int full_texture_height, int probe_side_length) {
    // Get octahedral coordinates (-1,1)
    const vec2 octahedral_coordinates = oct_encode(normalize(direction));
    // TODO: use probe index for this.
    const float probe_with_border_side = float(probe_side_length) + 2.0f;
    const int probes_per_row = (full_texture_width) / int(probe_with_border_side);
    // Get probe indices in the atlas
    ivec2 probe_indices = ivec2((probe_index % probes_per_row),
                               (probe_index / probes_per_row));

    // Get top left atlas texels
    vec2 atlas_texels = vec2( probe_indices.x * probe_with_border_side, probe_indices.y * probe_with_border_side );
    // Account for 1 pixel border
    atlas_texels += vec2(1.0f);
    // Move to center of the probe area
    atlas_texels += vec2(probe_side_length * 0.5f);
    // Use octahedral coordinates (-1,1) to move between internal pixels, no border
    atlas_texels += octahedral_coordinates * (probe_side_length * 0.5f);
    // Calculate final uvs
    const vec2 uv = atlas_texels / vec2(float(full_texture_width), float(full_texture_height));

    return uv;
}

vec3 sample_irradiance( vec3 world_position, vec3 normal, vec3 camera_position ) {
    const vec3 Wo = normalize(camera_position.xyz - world_position);
    // Bias vector to offset probe sampling based on normal and view vector.
    float minimum_distance_between_probes = min(probe_spacing.x, min(probe_spacing.y, probe_spacing.z));
    vec3 bias_vector = (normal * 0.2f + Wo * 0.8f) * (0.75f * minimum_distance_between_probes) * self_shadow_bias;

    vec3 biased_world_position = world_position + bias_vector;

    // Sample at world position + probe offset reduces shadow leaking.
    ivec3 base_grid_indices = world_to_grid_indices(biased_world_position);
    vec3 base_probe_world_position = grid_indices_to_world_no_offsets( base_grid_indices );

    // alpha is how far from the floor(currentVertex) position. on [0, 1] for each axis.
    vec3 alpha = clamp((biased_world_position - base_probe_world_position) * reciprocal_probe_spacing, vec3(0.0f), vec3(1.0f));

    vec3  sum_irradiance = vec3(0.0f);
    float sum_weight = 0.0f;

    // Iterate over adjacent probe cage

    for (int i = 0; i < 8; ++i) {
        // Compute the offset grid coord and clamp to the probe grid boundary
        // Offset = 0 or 1 along each axis
        ivec3  offset = ivec3(i, i >> 1, i >> 2) & ivec3(1);
        ivec3  probe_grid_coord = clamp(base_grid_indices + offset, ivec3(0), probe_counts - ivec3(1));
        int probe_index = probe_indices_to_index(probe_grid_coord);

        if (use_probe_status() && probe_status[probe_index] == PROBE_STATUS_OFF)
            continue;

        // Make cosine falloff in tangent plane with respect to the angle from the surface to the probe so that we never
        // test a probe that is *behind* the surface.
        // It doesn't have to be cosine, but that is efficient to compute and we must clip to the tangent plane.
        vec3 probe_pos = grid_indices_to_world(probe_grid_coord, probe_index);

        // Compute the trilinear weights based on the grid cell vertex to smoothly
        // transition between probes. Avoid ever going entirely to zero because that
        // will cause problems at the border probes. This isn't really a lerp.
        // We're using 1-a when offset = 0 and a when offset = 1.
        vec3 trilinear = mix(1.0 - alpha, alpha, offset);
        float weight = 1.0;

        if ( use_smooth_backface() ) {
            // Computed without the biasing applied to the "dir" variable.
            // This test can cause reflection-map looking errors in the image
            // (stuff looks shiny) if the transition is poor.
            vec3 direction_to_probe = normalize(probe_pos - world_position);

            // The naive soft backface weight would ignore a probe when
            // it is behind the surface. That's good for walls. But for small details inside of a
            // room, the normals on the details might rule out all of the probes that have mutual
            // visibility to the point. So, we instead use a "wrap shading" test below inspired by
            // NPR work.

            // The small offset at the end reduces the "going to zero" impact
            // where this is really close to exactly opposite
            const float dir_dot_n = (dot(direction_to_probe, normal) + 1.0) * 0.5f;
            weight *= (dir_dot_n * dir_dot_n) + 0.2;
        }

        // Bias the position at which visibility is computed; this avoids performing a shadow
        // test *at* a surface, which is a dangerous location because that is exactly the line
        // between shadowed and unshadowed. If the normal bias is too small, there will be
        // light and dark leaks. If it is too large, then samples can pass through thin occluders to
        // the other side (this can only happen if there are MULTIPLE occluders near each other, a wall surface
        // won't pass through itself.)
        vec3 probe_to_biased_point_direction = biased_world_position - probe_pos;
        float distance_to_biased_point = length(probe_to_biased_point_direction);
        probe_to_biased_point_direction *= 1.0 / max(distance_to_biased_point, EPSILON);

        // Visibility

        if ( use_visibility() ) {
            vec2 uv = get_probe_uv(probe_to_biased_point_direction, probe_index, visibility_texture_width, visibility_texture_height, visibility_side_length );

            vec2 visibility = textureLod(visibility_texture, uv, 0).rg;

            float mean_distance_to_occluder = visibility.x;

            float chebyshev_weight = 1.0;

            if (distance_to_biased_point > mean_distance_to_occluder) {
                // In "shadow"
                float variance = abs((visibility.x * visibility.x) - visibility.y);
                // http://www.punkuser.net/vsm/vsm_paper.pdf; equation 5
                // Need the max in the denominator because biasing can cause a negative displacement
                const float distance_diff = distance_to_biased_point - mean_distance_to_occluder;
                chebyshev_weight = variance / max(variance + (distance_diff * distance_diff), EPSILON);

                // Increase contrast in the weight
                chebyshev_weight = max((chebyshev_weight * chebyshev_weight * chebyshev_weight), 0.0f);
            }

            // Avoid visibility weights ever going all of the way to zero because when *no* probe has
            // visibility we need some fallback value.
            chebyshev_weight = max(0.05f, chebyshev_weight);
            weight *= chebyshev_weight;
        }

        // Avoid zero weight
        weight = max(0.000001, weight);

        // A small amount of light is visible due to logarithmic perception, so
        // crush tiny weights but keep the curve continuous
        const float crush_threshold = 0.2f;

        if (weight < crush_threshold) {
            weight *= (weight * weight) * (1.f / (crush_threshold * crush_threshold));
        }

        vec2 uv = get_probe_uv(normal, probe_index, irradiance_texture_width, irradiance_texture_height, irradiance_side_length );

        vec3 probe_irradiance = textureLod(irradiance_texture, uv, 0).rgb;

        if ( use_perceptual_encoding() ) {
            probe_irradiance = pow(probe_irradiance, vec3(0.5f * 5.0f));
        }

        // Trilinear weights
        weight *= trilinear.x * trilinear.y * trilinear.z + 0.001f;

        sum_irradiance += weight * probe_irradiance;
        sum_weight += weight;
    }

    vec3 net_irradiance = sum_irradiance / max(sum_weight, EPSILON);

    if ( use_perceptual_encoding() ) {
        net_irradiance = net_irradiance * net_irradiance;
    }

    vec3 irradiance = 0.5f * PI * net_irradiance * 0.95f;

    return irradiance;
}

#endif// DDGI_SHARED_GLSL
