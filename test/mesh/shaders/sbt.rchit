#version 460
#extension GL_EXT_ray_tracing : require
layout(set = 0, binding = 3, std430) readonly buffer vertex_data { float vertices[]; };
layout(set = 0, binding = 4, std430) readonly buffer index_data { uint indices[]; };
layout(shaderRecordEXT, std430) buffer record_data { vec4 color; };
layout(push_constant) uniform parameters {
    uint cull_mask;
    uint ray_offset;
    uint ray_stride;
    uint miss_index;
    uint vertex_stride;
    uint normal_offset;
    uint vertex_offset;
    uint index_offset;
    uint flat_color;
};
layout(location = 0) rayPayloadInEXT vec3 hit_value;
hitAttributeEXT vec2 attributes;
vec3 normal_at(uint index) {
    uint start = (index + vertex_offset) * vertex_stride + normal_offset;
    return vec3(vertices[start], vertices[start + 1], vertices[start + 2]);
}
void main() {
    uint start = index_offset + uint(gl_PrimitiveID) * 3;
    vec3 weights = vec3(1 - attributes.x - attributes.y, attributes);
    vec3 normal = normal_at(indices[start]) * weights.x + normal_at(indices[start + 1]) * weights.y + normal_at(indices[start + 2]) * weights.z;
    normal = normalize(normal * mat3(gl_WorldToObjectEXT));
    float lighting = flat_color != 0 ? 1.0 : 0.15 + 0.85 * abs(dot(normal, -gl_WorldRayDirectionEXT));
    hit_value = color.rgb * lighting;
}
