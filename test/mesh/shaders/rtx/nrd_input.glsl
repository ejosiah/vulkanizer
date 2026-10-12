#extension GL_EXT_scalar_block_layout : require

layout(set = 1, binding = 0) uniform sampler2D scene_depth;
layout(set = 1, binding = 1) uniform sampler2D scene_normal;
layout(set = 1, binding = 2) uniform sampler2D scene_surface;
layout(set = 1, binding = 3) uniform sampler2D diffuse_input;
layout(set = 1, binding = 4) uniform sampler2D specular_input;
layout(set = 1, binding = 5) uniform sampler3D penumbra_input;
layout(set = 1, binding = 6, rgba32f) uniform image2D diffuse_output;
layout(set = 1, binding = 7, rgba32f) uniform image2D specular_output;
layout(set = 1, binding = 8, r16f) uniform image3D shadow_output;
layout(set = 1, binding = 9, scalar) uniform frame_buffer {
    mat4 inverse_view_projection;
    mat4 view;
    vec3 camera_position;
    int shadow_light;
};

// Each denoiser uses the resolution of its own signal.
ivec2 signal_size() {
    if (shadow_light >= 0) return imageSize(shadow_output).xy;
    if (shadow_light == -2) return imageSize(specular_output);
    return imageSize(diffuse_output);
}

ivec2 source_pixel(ivec2 p) {
    ivec2 size = textureSize(scene_depth, 0);
    ivec2 scale = max((size + signal_size() - 1) / signal_size(), ivec2(1));
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

vec3 oct_decode(vec2 e) {
    vec3 n = vec3(e, 1 - abs(e.x) - abs(e.y));
    n.xy += vec2(n.x >= 0 ? -1 : 1, n.y >= 0 ? -1 : 1) * max(-n.z, 0);
    return normalize(n);
}
