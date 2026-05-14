#version 460

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 color;

layout(push_constant) uniform Constants {
    mat4 model;
    mat4 viewProjection;
} constants;

layout(location = 0) out vec3 worldNormal;
layout(location = 1) out vec4 vertexColor;

void main() {
    vec4 worldPosition = constants.model * vec4(position, 1.0);
    worldNormal = mat3(transpose(inverse(constants.model))) * normal;
    vertexColor = color;
    gl_Position = constants.viewProjection * worldPosition;
}
