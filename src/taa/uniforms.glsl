layout(set = 0, binding = 0, std140) uniform Constants {
    mat4 current_view_projection;
    mat4 inverse_current_view_projection;
    mat4 previous_view_projection;
    vec2 jitter_xy;
    vec2 resolution;
    uvec4 filters;
    uvec4 state; // history valid, depth channel
    vec4 tuning; // maximum history weight, depth rejection threshold
}

uniforms;
layout(set = 0, binding = 1) uniform sampler2D color_buffer;
layout(set = 0, binding = 2) uniform sampler2D depth_buffer;
layout(set = 0, binding = 3) uniform sampler2D velocity_buffer;
layout(set = 0, binding = 4) uniform sampler2D history_buffer;
layout(set = 0, binding = 5, rgba16f) uniform writeonly image2D velocity_out;
#ifdef TAA_RGBA16F
layout(set = 0, binding = 6, rgba16f) uniform writeonly image2D resolve_image;
#else
layout(set = 0, binding = 6, rgba32f) uniform writeonly image2D resolve_image;
#endif
layout(set = 0, binding = 7) uniform sampler2D history_depth;
layout(set = 0, binding = 8, r32f) uniform writeonly image2D next_depth;

float read_depth(ivec2 pixel) {
    return texelFetch(depth_buffer, pixel, 0)[uniforms.state.y];
}
