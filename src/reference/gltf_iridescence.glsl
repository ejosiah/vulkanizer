#ifndef VKZ_GLTF_IRIDESCENCE_GLSL
#define VKZ_GLTF_IRIDESCENCE_GLSL
float gltf_square(float x) { return x * x; }
vec3 gltf_square(vec3 x) { return x * x; }
float gltf_schlick(float f0, float cosine) { return f0 + (1.0 - f0) * pow(1.0 - clamp(cosine, 0.0, 1.0), 5.0); }
vec3 gltf_schlick(vec3 f0, float cosine) { return f0 + (1.0 - f0) * pow(1.0 - clamp(cosine, 0.0, 1.0), 5.0); }

const mat3 gltf_xyz_to_rgb = mat3(
     3.2404542, -0.9692660,  0.0556434,
    -1.5371385,  1.8760108, -0.2040259,
    -0.4985314,  0.0415560,  1.0572252
);



vec3 gltf_fresnel_to_ior(vec3 fresnel0) {
    vec3 sqrtF0 = sqrt(fresnel0);
    return (vec3(1.0) + sqrtF0) / (vec3(1.0) - sqrtF0);
}


vec3 gltf_ior_to_fresnel(vec3 transmittedIor, float incidentIor) {
    return gltf_square((transmittedIor - vec3(incidentIor)) / (transmittedIor + vec3(incidentIor)));
}


float gltf_ior_to_fresnel(float transmittedIor, float incidentIor) {
    return gltf_square((transmittedIor - incidentIor) / (transmittedIor + incidentIor));
}




vec3 gltf_sensitivity(float OPD, vec3 shift) {
    float phase = 2.0 * GLTF_PI * OPD * 1.0e-9;
    vec3 val = vec3(5.4856e-13, 4.4201e-13, 5.2481e-13);
    vec3 pos = vec3(1.6810e+06, 1.7953e+06, 2.2084e+06);
    vec3 var = vec3(4.3278e+09, 9.3046e+09, 6.6121e+09);

    vec3 xyz = val * sqrt(2.0 * GLTF_PI * var) * cos(pos * phase + shift) * exp(-gltf_square(phase) * var);
    xyz.x += 9.7470e-14 * sqrt(2.0 * GLTF_PI * 4.5282e+09) * cos(2.2399e+06 * phase + shift[0]) * exp(-4.5282e+09 * gltf_square(phase));
    xyz /= 1.0685e-7;

    vec3 srgb = gltf_xyz_to_rgb * xyz;
    return srgb;
}

vec3 gltf_iridescence(float outsideIOR, float eta2, float cosTheta1, float thinFilmThickness, vec3 baseF0) {
    vec3 I;

    
    float iridescenceIor = mix(outsideIOR, eta2, smoothstep(0.0, 0.03, thinFilmThickness));
    
    float sinTheta2Sq = gltf_square(outsideIOR / iridescenceIor) * (1.0 - gltf_square(cosTheta1));

    
    float cosTheta2Sq = 1.0 - sinTheta2Sq;
    if (cosTheta2Sq < 0.0) {
        return vec3(1.0);
    }

    float cosTheta2 = sqrt(cosTheta2Sq);

    
    float R0 = gltf_ior_to_fresnel(iridescenceIor, outsideIOR);
    float R12 = gltf_schlick(R0, cosTheta1);
    float R21 = R12;
    float T121 = 1.0 - R12;
    float phi12 = 0.0;
    if (iridescenceIor < outsideIOR) phi12 = GLTF_PI;
    float phi21 = GLTF_PI - phi12;

    
    vec3 baseIOR = gltf_fresnel_to_ior(clamp(baseF0, 0.0, 0.9999)); 
    vec3 R1 = gltf_ior_to_fresnel(baseIOR, iridescenceIor);
    vec3 R23 = gltf_schlick(R1, cosTheta2);
    vec3 phi23 = vec3(0.0);
    if (baseIOR[0] < iridescenceIor) phi23[0] = GLTF_PI;
    if (baseIOR[1] < iridescenceIor) phi23[1] = GLTF_PI;
    if (baseIOR[2] < iridescenceIor) phi23[2] = GLTF_PI;

    
    float OPD = 2.0 * iridescenceIor * thinFilmThickness * cosTheta2;
    vec3 phi = vec3(phi21) + phi23;

    
    vec3 R123 = clamp(R12 * R23, 1e-5, 0.9999);
    vec3 r123 = sqrt(R123);
    vec3 Rs = gltf_square(T121) * R23 / (vec3(1.0) - R123);

    
    vec3 C0 = R12 + Rs;
    I = C0;

    
    vec3 Cm = Rs - T121;
    for (int m = 1; m <= 2; ++m)
    {
        Cm *= r123;
        vec3 Sm = 2.0 * gltf_sensitivity(float(m) * OPD, float(m) * phi);
        I += Cm * Sm;
    }

    
    return max(I, vec3(0.0));
}

#endif
