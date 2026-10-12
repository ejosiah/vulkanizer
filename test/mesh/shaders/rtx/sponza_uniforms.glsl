#include "gltf_types.glsl"

layout(set = 0, binding = 0, scalar) uniform Scene { gltf_scene scene; };
layout(set = 1, binding = 0, scalar) uniform Material { gltf_material material; };

layout(push_constant) uniform Instance {
    mat4 model;
    mat4 normal_matrix;
} instance;
