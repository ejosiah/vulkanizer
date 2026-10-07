#version 460

layout(location = 0) in vec4 color;
layout(location = 1) in vec3 world_position;
layout(location = 2) in vec3 world_normal;

layout(std430, set = 0, binding = 0) readonly buffer Scene {
    mat4 view_projection;
    vec4 light_position;
    vec4 eye;
};
layout(set = 0, binding = 1) uniform samplerCube shadow_map;
layout(constant_id = 1) const float light_range = 1024.0;

layout(location = 0) out vec4 fragColor;

void main() {
    vec3 n = normalize(world_normal);
    vec3 l = normalize(light_position.xyz - world_position);
    vec3 v = normalize(eye.xyz - world_position);
    float diffuse = max(dot(n, l), 0.0);
    float specular = diffuse > 0.0 ? pow(max(dot(v, reflect(-l, n)), 0.0), 32.0) : 0.0;
    vec3 light_to_fragment = world_position - light_position.xyz;
    float light_distance = length(light_to_fragment);
    float sample_distance = texture(shadow_map, light_to_fragment).r;
    // Match the radial-distance tolerance used by the vglib example.
    float visibility = light_distance - sample_distance <= 0.15 ? 1.0 : 0.0;
    // Fade direct lighting before the radial range limit so distant surfaces cannot flicker between shadowed and lit.
    float attenuation = 1.0 - smoothstep(0.8 * light_range, light_range, light_distance);
    fragColor = vec4(0.1 * color.rgb + attenuation * visibility * (diffuse * color.rgb + 0.25 * specular), color.a);
}
