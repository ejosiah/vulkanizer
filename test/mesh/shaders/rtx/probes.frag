#version 460

#extension GL_EXT_scalar_block_layout : require

layout(set = 0, binding = 7) uniform sampler2D probe_irradiance;
layout(push_constant, scalar) uniform probe_parameters {
    vec3 grid_position;
    ivec3 counts;
    vec3 spacing;
    float radius;
    int irradiance_side_length;
    uint offsetting_enabled;
};

layout(location = 0) in vec3 probe_normal;
layout(location = 1) flat in int probe_index;
layout(location = 0) out vec4 frag_color;

vec2 oct_encode(vec3 normal) {
    normal /= abs(normal.x) + abs(normal.y) + abs(normal.z);
    if (normal.z < 0)
        normal.xy = (1 - abs(normal.yx)) * vec2(normal.x >= 0 ? 1 : -1, normal.y >= 0 ? 1 : -1);
    return normal.xy;
}

void main() {
    ivec2 atlas_size = textureSize(probe_irradiance, 0);
    int tile_size = irradiance_side_length + 2;
    int probes_per_row = atlas_size.x / tile_size;
    ivec2 tile = ivec2(probe_index % probes_per_row, probe_index / probes_per_row);
    vec2 texel = vec2(tile * tile_size) + 1 + float(irradiance_side_length) * 0.5
        * (1 + oct_encode(normalize(probe_normal)));
    vec3 encoded = texture(probe_irradiance, texel / vec2(atlas_size)).rgb;

    // Decode the DDGI atlas's fifth-root encoding and display incoming irradiance.
    vec3 irradiance = pow(max(encoded, vec3(0)), vec3(5)) * (0.5 * 3.14159265359 * 0.95);
    frag_color = vec4(max(irradiance, vec3(0.02)), 1);
}
