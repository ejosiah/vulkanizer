#ifndef VKZ_GLTF_CORE_GLSL
#define VKZ_GLTF_CORE_GLSL
// glTF 2.0 Appendix B: GGX, height-correlated Smith, Schlick Fresnel.
// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#appendix-b-brdf-implementation
const float GLTF_PI = 3.141592653589793;
float gltf_f0(float ior) {
    if (ior == 0.0) return 1.0;
    float ratio = 1.0 - 2.0 / (ior + 1.0);
    return ratio * ratio;
}
float gltf_fresnel(float ior, float cosine) {
    float f0 = gltf_f0(ior);
    return ior == 1.0 ? 0.0 : f0 + (1.0 - f0) * pow(1.0 - clamp(cosine, 0.0, 1.0), 5.0);
}
vec3 gltf_brdf(vec3 base_color, float metallic, float roughness, vec3 n, vec3 v, vec3 l, float ior, float transmission) {
    float nv = max(dot(n, v), 0.0), nl = max(dot(n, l), 0.0);
    if (nv <= 0.0 || nl <= 0.0 || dot(v + l, v + l) < 1e-10) return vec3(0);
    vec3 h = normalize(v + l);
    float nh = max(dot(n, h), 0.0), vh = clamp(dot(v, h), 0.0, 1.0);
    // Finite raster approximation to the delta lobe at roughness == 0.
    float alpha = max(roughness * roughness, 0.001);
    float a2 = alpha * alpha;
    float d = (1.0 - nh * nh) + nh * nh * a2;
    float distribution = a2 / (GLTF_PI * d * d);
    float visibility = 0.5 / max(nl * sqrt(nv * nv * (1.0 - a2) + a2)
                              + nv * sqrt(nl * nl * (1.0 - a2) + a2), 1e-10);
    float w = pow(1.0 - vh, 5.0);
    float dielectric_fresnel = gltf_fresnel(ior, vh);
    vec3 metal_fresnel = base_color + (1.0 - base_color) * w;
    vec3 dielectric = (1.0 - dielectric_fresnel) * (1.0 - transmission) * base_color / GLTF_PI
                    + dielectric_fresnel * distribution * visibility;
    return mix(dielectric, metal_fresnel * distribution * visibility, metallic);
}
vec3 gltf_brdf(vec3 base_color, float metallic, float roughness, vec3 n, vec3 v, vec3 l) {
    return gltf_brdf(base_color, metallic, roughness, n, v, l, 1.5, 0.0);
}
#endif
