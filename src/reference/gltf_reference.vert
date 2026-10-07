#version 460
#extension GL_GOOGLE_include_directive : require

#include "gltf_reference_uniforms.glsl"

layout(set = 0, binding = 1, std430) readonly buffer Vertices { float data[]; } vertices;


layout(location = 21) flat out uint instance_id;
layout(location = 0) out vec3 world_position;
layout(location = 1) out vec3 world_normal;
layout(location = 2) out vec4 world_tangent;
layout(location = 3) out vec4 vertex_color;
layout(location = 4) out vec2 uv[17];

vec3 read3(int i) { return vec3(vertices.data[i], vertices.data[i+1], vertices.data[i+2]); }

void main() {
    instance_id = gl_InstanceIndex;
    int start = gl_VertexIndex * scene.vertex_layout.x;
    vec4 p = instance.model * vec4(read3(start), 1);
    world_position = p.xyz;
    world_normal = mat3(instance.normal_matrix) * read3(start + 3);
    world_tangent = vec4(mat3(instance.model) * read3(start + 6),
                        vertices.data[start + 9] * sign(determinant(mat3(instance.model))));
    vertex_color = vec4(read3(start + 10), vertices.data[start + 13]);
    for (int i = 0; i < 17; ++i) {
        int offset = start + material.textures[i].x;
        uv[i] = gltf_uv(material, i, material.textures[i].y != 0 ? vec2(vertices.data[offset], vertices.data[offset+1]) : vec2(0));
    }
    gl_Position = scene.view_projection * p;
}
