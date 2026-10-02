#version 460
layout(set = 0, binding = 0) uniform sampler2D hdr_image;
layout(push_constant) uniform ToneMapping {
    float exposure;
} tone_mapping;
layout(location = 0) in vec2 clip_position;
layout(location = 0) out vec4 frag_color;
void main() {
    vec4 hdr = textureLod(hdr_image, clip_position * 0.5 + 0.5, 0.0);
    vec3 x = clamp(hdr.rgb * exp2(clamp(tone_mapping.exposure, -16.0, 16.0)), 0.0, 16.0);
    vec3 mapped = (x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14);
    frag_color = vec4(clamp(mapped, 0.0, 1.0), hdr.a);
}
