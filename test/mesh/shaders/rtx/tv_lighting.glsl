// Textured polygonal lighting from Heitz, Dupuy, Hill and Neubelt (2016).
// Matching 64² tables and integration: https://github.com/selfshadow/ltc_code.
// Textured-emitter projection: original authors' downloadable demo.
layout(set = 0, binding = 9) uniform sampler2DArray tv_filtered;
layout(set = 0, binding = 10) uniform sampler2D ltc_1;
layout(set = 0, binding = 11) uniform sampler2D ltc_2;
layout(set = 0, binding = 12, scalar) uniform tv_light_parameters {
    vec4 points[4];
    vec4 settings;
} tv_light;

const float lut_scale = 63.0 / 64.0;
const float lut_bias = 0.5 / 64.0;

vec3 ltc_emitter_color(vec3 p0, vec3 p1, vec3 p2, vec3 p3) {
    vec3 e1 = p1 - p0;
    vec3 e2 = p3 - p0;
    vec3 orthogonal = cross(e1, e2);
    float area_squared = dot(orthogonal, orthogonal);
    float distance_area = dot(orthogonal, p0);
    vec3 projection = distance_area * orthogonal / area_squared - p0;

    float cross_term = dot(e1, e2);
    float inverse_length = 1.0 / dot(e1, e1);
    vec3 perpendicular = e2 - e1 * cross_term * inverse_length;
    vec2 uv;
    uv.y = dot(perpendicular, projection) / dot(perpendicular, perpendicular);
    uv.x = dot(e1, projection) * inverse_length - cross_term * inverse_length * uv.y;

    float d = abs(distance_area) / pow(area_squared, 0.75);
    float level = clamp(log(max(2048.0 * d, 1e-8)) / log(3.0), 0.0, 10.0);
    vec2 padded_uv = vec2(0.125) + 0.75 * uv;

    return mix(texture(tv_filtered, vec3(padded_uv, floor(level))).rgb,
        texture(tv_filtered, vec3(padded_uv, ceil(level))).rgb, fract(level));
}

#include "ltc_integration.glsl"

vec3 tv_lighting(vec3 position, vec3 normal, vec3 view, vec3 base, float metallic, float roughness) {
    if (tv_light.settings.x == 0)
        return vec3(0);

    vec3 points[4];

    for (int i = 0; i < 4; ++i)
        points[i] = tv_light.points[i].xyz;

    // The TV emits into the room from its front face only.
    vec3 center = (points[0] + points[2]) * 0.5;
    vec3 emitter_normal = -normalize(cross(points[1] - points[0], points[3] - points[0]));

    if (dot(position - center, emitter_normal) <= 0)
        return vec3(0);

    vec2 uv = vec2(roughness, sqrt(1.0 - clamp(dot(normal, view), 0.0, 1.0))) * lut_scale + lut_bias;
    vec4 matrix = texture(ltc_1, uv);
    vec4 amplitude = texture(ltc_2, uv);
    mat3 inverse_matrix = mat3(vec3(matrix.x, 0, matrix.y), vec3(0, 1, 0), vec3(matrix.z, 0, matrix.w));
    vec3 specular_color = mix(vec3(0.04), base, metallic);
    vec3 specular = ltc_evaluate(normal, view, position, inverse_matrix, points, true);
    specular *= specular_color * amplitude.x + (1.0 - specular_color) * amplitude.y;
    vec3 diffuse = ltc_evaluate(normal, view, position, mat3(1), points, true);

    // Occlusion remains a separate visibility query; LTC approximates the BRDF integral.
    float distance = length(center - position);
    float visibility = scene.enable_shadows == 0u ? 1.0 : trace_surface_visibility(scene_acceleration, position + normal * 0.002,
        (center - position) / distance, max(distance - 0.01, 0.001), uint(scene.vertex_layout.x)).x;

    return (diffuse * base * (1.0 - metallic) + specular) * tv_light.settings.y * visibility;
}
