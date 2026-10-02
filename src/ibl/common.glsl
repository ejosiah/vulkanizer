#ifndef VKZ_IBL_COMMON_GLSL
#define VKZ_IBL_COMMON_GLSL

const float pi = 3.14159265358979323846;

vec3 cube_direction(ivec3 texel, ivec2 size) {
    vec2 uv = (vec2(texel.xy) + 0.5) / vec2(size) * 2.0 - 1.0;
    uv.y = -uv.y;
    if (texel.z == 0) return normalize(vec3( 1.0, uv.y, -uv.x));
    if (texel.z == 1) return normalize(vec3(-1.0, uv.y,  uv.x));
    if (texel.z == 2) return normalize(vec3( uv.x,  1.0, -uv.y));
    if (texel.z == 3) return normalize(vec3( uv.x, -1.0,  uv.y));
    if (texel.z == 4) return normalize(vec3( uv.x, uv.y,  1.0));
    return normalize(vec3(-uv.x, uv.y, -1.0));
}

float radical_inverse(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 hammersley(uint index, uint count) {
    return vec2(float(index) / float(count), radical_inverse(index));
}

void tangent_basis(vec3 normal, out vec3 tangent, out vec3 bitangent) {
    vec3 up = abs(normal.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    tangent = normalize(cross(up, normal));
    bitangent = cross(normal, tangent);
}

#endif
