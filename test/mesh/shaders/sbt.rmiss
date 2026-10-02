#version 460
#extension GL_EXT_ray_tracing : require
layout(shaderRecordEXT, std430) buffer record_data {
    vec4 color;
};
layout(location = 0) rayPayloadInEXT vec3 hit_value;
void main() {
    hit_value = color.rgb;
}
