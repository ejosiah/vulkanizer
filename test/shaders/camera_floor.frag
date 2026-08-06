#version 460

layout(push_constant) uniform Camera {
    mat4 model;
    mat4 view;
    mat4 projection;
} camera;

layout(location = 0) in vec3 ray_direction;
layout(location = 0) out vec4 out_color;

float grid(vec2 p, float scale) {
    vec2 q = p / scale;
    vec2 w = fwidth(q);
    vec2 a = abs(fract(q - 0.5) - 0.5) / w;
    return 1.0 - min(min(a.x, a.y), 1.0);
}

void main() {
    vec3 origin = inverse(camera.view)[3].xyz;
    vec3 direction = normalize(ray_direction);
    float denominator = direction.y;
    if (abs(denominator) < 0.0001) discard;
    float t = -origin.y / denominator;
    if (t <= 0.0) discard;

    vec3 position = origin + direction * t;
    vec4 clip = camera.projection * camera.view * vec4(position, 1);
    gl_FragDepth = clip.z / clip.w;

    vec2 cell = floor(position.xz);
    float checker = mod(cell.x + cell.y, 2.0);
    vec3 base = mix(vec3(0.075), vec3(0.32), checker);
    float line = max(grid(position.xz, 1.0), grid(position.xz, 10.0) * 0.7);
    float fade = 1.0 - smoothstep(50.0, 300.0, length(position.xz - origin.xz));
    out_color = vec4(mix(base, vec3(0.52), line) * fade, 1.0);
}
