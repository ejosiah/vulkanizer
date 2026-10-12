struct nrc_vertex {
    vec4 position_roughness;
    vec4 normal_distance;
    vec4 view_delta;
    vec4 diffuse_reflectance;
    vec4 specular_reflectance;
    vec4 direct_radiance;
    vec4 scatter_weight_pdf;
    vec4 throughput_valid;
    vec4 emission;
};

struct nrc_training_path {
    vec4 terminal_radiance;
    uint vertex_count;
    uint exited;
    uint reserved_0;
    uint reserved_1;
};

