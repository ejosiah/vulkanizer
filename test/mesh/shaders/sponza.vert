#version 460
layout(location = 0) in vec3 position;
layout(push_constant) uniform Camera {
    mat4 model;
    mat4 view_projection;
} camera;
void main() {
    gl_Position = camera.view_projection * camera.model * vec4(position, 1.0);
}
