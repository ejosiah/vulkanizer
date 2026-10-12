// Authors LTC quad integration, see LICENSES/ltc.txt.
vec3 integrate_edge_vector(vec3 v1, vec3 v2)
{
    float x = dot(v1, v2);
    float y = abs(x);

    float a = 0.8543985 + (0.4965155 + 0.0145206*y)*y;
    float b = 3.4175940 + (4.1616724 + y)*y;
    float v = a / b;

    float theta_sin_theta = (x > 0.0) ? v : 0.5*inversesqrt(max(1.0 - x*x, 1e-7)) - v;

    return cross(v1, v2)*theta_sin_theta;
}

float integrate_edge(vec3 v1, vec3 v2)
{
    return integrate_edge_vector(v1, v2).z;
}

void clip_quad_to_horizon(inout vec3 vertices[5], out int n)
{
    // detect clipping config
    int config = 0;
    if (vertices[0].z > 0.0) config += 1;
    if (vertices[1].z > 0.0) config += 2;
    if (vertices[2].z > 0.0) config += 4;
    if (vertices[3].z > 0.0) config += 8;

    // clip
    n = 0;

    if (config == 0)
    {
        // clip all
    }
    else if (config == 1) // V1 clip V2 V3 V4
    {
        n = 3;
        vertices[1] = -vertices[1].z * vertices[0] + vertices[0].z * vertices[1];
        vertices[2] = -vertices[3].z * vertices[0] + vertices[0].z * vertices[3];
    }
    else if (config == 2) // V2 clip V1 V3 V4
    {
        n = 3;
        vertices[0] = -vertices[0].z * vertices[1] + vertices[1].z * vertices[0];
        vertices[2] = -vertices[2].z * vertices[1] + vertices[1].z * vertices[2];
    }
    else if (config == 3) // V1 V2 clip V3 V4
    {
        n = 4;
        vertices[2] = -vertices[2].z * vertices[1] + vertices[1].z * vertices[2];
        vertices[3] = -vertices[3].z * vertices[0] + vertices[0].z * vertices[3];
    }
    else if (config == 4) // V3 clip V1 V2 V4
    {
        n = 3;
        vertices[0] = -vertices[3].z * vertices[2] + vertices[2].z * vertices[3];
        vertices[1] = -vertices[1].z * vertices[2] + vertices[2].z * vertices[1];
    }
    else if (config == 5) // V1 V3 clip V2 V4) impossible
    {
        n = 0;
    }
    else if (config == 6) // V2 V3 clip V1 V4
    {
        n = 4;
        vertices[0] = -vertices[0].z * vertices[1] + vertices[1].z * vertices[0];
        vertices[3] = -vertices[3].z * vertices[2] + vertices[2].z * vertices[3];
    }
    else if (config == 7) // V1 V2 V3 clip V4
    {
        n = 5;
        vertices[4] = -vertices[3].z * vertices[0] + vertices[0].z * vertices[3];
        vertices[3] = -vertices[3].z * vertices[2] + vertices[2].z * vertices[3];
    }
    else if (config == 8) // V4 clip V1 V2 V3
    {
        n = 3;
        vertices[0] = -vertices[0].z * vertices[3] + vertices[3].z * vertices[0];
        vertices[1] = -vertices[2].z * vertices[3] + vertices[3].z * vertices[2];
        vertices[2] =  vertices[3];
    }
    else if (config == 9) // V1 V4 clip V2 V3
    {
        n = 4;
        vertices[1] = -vertices[1].z * vertices[0] + vertices[0].z * vertices[1];
        vertices[2] = -vertices[2].z * vertices[3] + vertices[3].z * vertices[2];
    }
    else if (config == 10) // V2 V4 clip V1 V3) impossible
    {
        n = 0;
    }
    else if (config == 11) // V1 V2 V4 clip V3
    {
        n = 5;
        vertices[4] = vertices[3];
        vertices[3] = -vertices[2].z * vertices[3] + vertices[3].z * vertices[2];
        vertices[2] = -vertices[2].z * vertices[1] + vertices[1].z * vertices[2];
    }
    else if (config == 12) // V3 V4 clip V1 V2
    {
        n = 4;
        vertices[1] = -vertices[1].z * vertices[2] + vertices[2].z * vertices[1];
        vertices[0] = -vertices[0].z * vertices[3] + vertices[3].z * vertices[0];
    }
    else if (config == 13) // V1 V3 V4 clip V2
    {
        n = 5;
        vertices[4] = vertices[3];
        vertices[3] = vertices[2];
        vertices[2] = -vertices[1].z * vertices[2] + vertices[2].z * vertices[1];
        vertices[1] = -vertices[1].z * vertices[0] + vertices[0].z * vertices[1];
    }
    else if (config == 14) // V2 V3 V4 clip V1
    {
        n = 5;
        vertices[4] = -vertices[0].z * vertices[3] + vertices[3].z * vertices[0];
        vertices[0] = -vertices[0].z * vertices[1] + vertices[1].z * vertices[0];
    }
    else if (config == 15) // V1 V2 V3 V4
    {
        n = 4;
    }

    if (n == 3)
        vertices[3] = vertices[0];
    if (n == 4)
        vertices[4] = vertices[0];
}


vec3 ltc_evaluate(
    vec3 normal, vec3 view, vec3 position, mat3 inverse_transform, vec3 points[4], bool two_sided)
{
    // construct orthonormal basis around normal
    vec3 tangent, bitangent;
    tangent = view - normal*dot(view, normal);
    tangent = dot(tangent, tangent) > 1e-8 ? normalize(tangent) : normalize(cross(normal, abs(normal.z) < 0.9 ? vec3(0, 0, 1) : vec3(0, 1, 0)));
    bitangent = cross(normal, tangent);

    // rotate area light in (tangent, bitangent, normal) basis
    inverse_transform = inverse_transform * transpose(mat3(tangent, bitangent, normal));

    // polygon (allocate 5 vertices for clipping)
    vec3 vertices[5];
    vertices[0] = (inverse_transform * (points[0] - position));
    vertices[1] = (inverse_transform * (points[1] - position));
    vertices[2] = (inverse_transform * (points[2] - position));
    vertices[3] = (inverse_transform * (points[3] - position));
    vertices[4] = vertices[0];

    vec3 emitter_color = ltc_emitter_color(vertices[0], vertices[1], vertices[2], vertices[3]);

    // integrate
    float sum = 0.0;

    {
        int n;
        clip_quad_to_horizon(vertices, n);

        if (n == 0)
            return vec3(0, 0, 0);
        // project onto sphere
        vertices[0] = normalize(vertices[0]);
        vertices[1] = normalize(vertices[1]);
        vertices[2] = normalize(vertices[2]);
        vertices[3] = normalize(vertices[3]);
        vertices[4] = normalize(vertices[4]);

        // integrate
        sum += integrate_edge(vertices[0], vertices[1]);
        sum += integrate_edge(vertices[1], vertices[2]);
        sum += integrate_edge(vertices[2], vertices[3]);
        if (n >= 4)
            sum += integrate_edge(vertices[3], vertices[4]);
        if (n == 5)
            sum += integrate_edge(vertices[4], vertices[0]);

        sum = two_sided ? abs(sum) : max(0.0, sum);
    }

    vec3 outgoing_radiance = vec3(sum) * emitter_color;

    return outgoing_radiance;
}
