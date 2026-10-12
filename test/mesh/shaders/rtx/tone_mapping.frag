#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require

#include "gltf_types.glsl"

layout(set = 0, binding = 0, scalar) uniform scene_buffer {
    gltf_scene scene;
};

layout(set = 0, binding = 4) uniform sampler2D hdr_image;

layout(push_constant) uniform output_format {
    uint srgb_attachment;
};

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 frag_color;

// Uncharted 2 curve and white point from the reference tone_mapping.glsl.
vec3 uncharted2_curve(vec3 x) {
    const float a = 0.15;
    const float b = 0.50;
    const float c = 0.10;
    const float d = 0.20;
    const float e = 0.02;
    const float f = 0.30;

    return ((x * (a * x + c * b) + d * e) / (x * (a * x + b) + d * f)) - e / f;
}

vec3 srgb_to_linear(vec3 color) {
    return mix(pow((color + 0.055) / 1.055, vec3(2.4)),
               color / 12.92, lessThanEqual(color, vec3(0.04045)));
}

void main() {
    vec3 hdr = max(texture(hdr_image, uv).rgb * scene.exposure, vec3(0));
    vec3 mapped = uncharted2_curve(hdr) / uncharted2_curve(vec3(11.2));
    vec3 display_color = pow(max(mapped, vec3(0)), vec3(0.454));

    // The reference explicitly applies gamma; undo the attachment's sRGB
    // encoding here so the displayed result receives that gamma exactly once.
    if (srgb_attachment != 0u)
        display_color = srgb_to_linear(display_color);

    frag_color = vec4(display_color, 1);
}
