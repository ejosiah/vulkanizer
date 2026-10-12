struct nrc_hit {
    vec3 position;
    float distance;
    vec3 normal;
    float roughness;
    vec3 diffuse;
    float valid;
    vec3 specular;
    float reserved;
    vec3 direct_radiance;
    float reserved_1;
    vec3 emission;
    float reserved_2;
};
