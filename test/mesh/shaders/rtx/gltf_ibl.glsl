#ifndef VKZ_GLTF_IBL_GLSL
#define VKZ_GLTF_IBL_GLSL
#include "gltf_core.glsl"
#ifndef VKZ_IBL_SET
#define VKZ_IBL_SET 2
#endif
layout(set = VKZ_IBL_SET, binding = 0) uniform samplerCube ibl_environment;
layout(set = VKZ_IBL_SET, binding = 1) uniform samplerCube ibl_irradiance;
layout(set = VKZ_IBL_SET, binding = 2) uniform samplerCube ibl_specular;
layout(set = VKZ_IBL_SET, binding = 3) uniform sampler2D ibl_brdf_lut;
layout(std140, set = VKZ_IBL_SET, binding = 4) uniform IblUniform {
    mat4 inverse_view_projection;
    vec4 eye;
    vec4 parameters;
} ibl;

vec3 gltf_environment_direction(vec3 direction) {
    float c = cos(ibl.parameters.y), s = sin(ibl.parameters.y);
    return vec3(c * direction.x + s * direction.z, direction.y, -s * direction.x + c * direction.z);
}

vec3 gltf_environment(vec3 direction) {
    return textureLod(ibl_environment, gltf_environment_direction(direction), 0.0).rgb * ibl.parameters.x;
}

vec3 gltf_background(vec3 direction) {
    if (ibl.parameters.z > 0.5)
        return textureLod(ibl_irradiance, gltf_environment_direction(direction), 0.0).rgb * (ibl.parameters.x / GLTF_PI);
    return gltf_environment(direction);
}

vec3 gltf_ibl(vec3 base, float metallic, float roughness, vec3 n, vec3 v, float ior, float transmission) {
    float nv = clamp(dot(n, v), 0.0, 1.0);
    vec3 f0 = mix(vec3(gltf_f0(ior)), base, metallic);
    vec3 fresnel = f0 + (max(vec3(1.0 - roughness), f0) - f0) * pow(1.0 - nv, 5.0);
    if (ior == 1.0) fresnel *= metallic;
    vec3 irradiance = textureLod(ibl_irradiance, gltf_environment_direction(n), 0.0).rgb;
    vec3 diffuse = (1.0 - fresnel) * (1.0 - metallic) * (1.0 - transmission) * base * irradiance / 3.141592653589793;
    float lod = roughness * float(textureQueryLevels(ibl_specular) - 1);
    vec3 specular = textureLod(ibl_specular, gltf_environment_direction(reflect(-v, n)), lod).rgb;
    vec2 brdf = textureLod(ibl_brdf_lut, vec2(nv, roughness), 0.0).rg;
    return (diffuse + specular * (f0 * brdf.x + brdf.y) * (ior == 1.0 ? metallic : 1.0)) * ibl.parameters.x;
}
#endif
