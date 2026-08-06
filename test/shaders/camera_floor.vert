#version 460

layout(push_constant) uniform Camera {
    mat4 model;
    mat4 view;
    mat4 projection;
} camera;

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 uv;
layout(location = 0) out vec3 ray_direction;

void main() {
    vec4 ray = inverse(camera.projection) * vec4(position, 1, 1);
    ray = inverse(camera.view) * vec4(ray.xyz / ray.w, 0);
    ray_direction = normalize(ray.xyz);
    gl_Position = vec4(position, 0, 1);
}
