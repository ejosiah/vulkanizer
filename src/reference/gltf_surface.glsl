vec3 read3(uint offset) {
    return vec3(vertices.data[offset], vertices.data[offset + 1], vertices.data[offset + 2]);
}
vec4 read4(uint offset) {
    return vec4(read3(offset), vertices.data[offset + 3]);
}
vec2 read2(uint offset) {
    return vec2(vertices.data[offset], vertices.data[offset + 1]);
}
uvec3 triangle_vertices(uvec4 instance, uint primitive) {
    uint start = instance.y + primitive * 3;
    return (uvec3(indices.data[start], indices.data[start + 1], indices.data[start + 2]) + instance.x) * uint(reference.scene.vertex_layout.x);
}
vec4 fetch_vertex_color(uvec3 triangle, vec3 weights) {
    return read4(triangle.x + 10) * weights.x + read4(triangle.y + 10) * weights.y + read4(triangle.z + 10) * weights.z;
}
vec2 texture_uv(uvec3 triangle, vec3 weights, uint offset) {
    return read2(triangle.x + offset) * weights.x + read2(triangle.y + offset) * weights.y + read2(triangle.z + offset) * weights.z;
}
vec4 sample_texture(gltf_material material, int slot, uvec3 triangle, vec3 weights) {
    if (material.textures[slot].y == 0) return vec4(1);
    vec2 uv = gltf_uv(material, slot, texture_uv(triangle, weights, uint(material.textures[slot].x)));
    return textureLod(textures[nonuniformEXT(material.textures[slot].z)], uv, reference.ray_parameters.z);
}

gltf_surface evaluate_triangle(uvec4 instance, uint primitive, vec2 barycentric, mat4x3 object_to_world, mat4x3 world_to_object, bool front, vec3 direction, uint seed, inout gltf_volume_state volume, uint instance_id, float distance) {
    gltf_material material = materials.data[instance.z];
    uvec3 triangle = triangle_vertices(instance, primitive);
    vec3 weights = vec3(1.0 - barycentric.x - barycentric.y, barycentric);
    vec3 normal = read3(triangle.x + 3) * weights.x + read3(triangle.y + 3) * weights.y + read3(triangle.z + 3) * weights.z;
    normal = normalize(transpose(mat3(world_to_object)) * normal);
    vec4 tangent = read4(triangle.x + 6) * weights.x + read4(triangle.y + 6) * weights.y + read4(triangle.z + 6) * weights.z;
    tangent = vec4(mat3(object_to_world) * tangent.xyz, tangent.w * sign(determinant(mat3(object_to_world))));
    vec3 edge1 = mat3(object_to_world) * (read3(triangle.y) - read3(triangle.x));
    vec3 edge2 = mat3(object_to_world) * (read3(triangle.z) - read3(triangle.x));
    vec2 uv1 = vec2(0), uv2 = vec2(0);
    if (material.textures[2].y != 0) {
        uint offset = uint(material.textures[2].x);
        uv1 = gltf_uv(material, 2, read2(triangle.y + offset)) - gltf_uv(material, 2, read2(triangle.x + offset));
        uv2 = gltf_uv(material, 2, read2(triangle.z + offset)) - gltf_uv(material, 2, read2(triangle.x + offset));
    }
    mat3 frame = gltf_tangent_frame(normal, material.textures[2].w != 0 ? vec4(0) : tangent, edge1, edge2, uv1, uv2);
    vec4 samples[17];
    for (int i = 0; i < 17; ++i) samples[i] = sample_texture(material, i, triangle, weights);
    samples[6] = vec4(1);
    vec3 n = gltf_normal(material, normal, frame, samples[2], front);
    vec3 position = object_to_world * vec4(read3(triangle.x)*weights.x + read3(triangle.y)*weights.y + read3(triangle.z)*weights.z, 1);
    vec3 coat_normal = front ? normal : -normal;
    if (material.textures[11].y != 0) {
        uint offset = uint(material.textures[11].x);
        vec2 c1 = gltf_uv(material,11,read2(triangle.y+offset))-gltf_uv(material,11,read2(triangle.x+offset));
        vec2 c2 = gltf_uv(material,11,read2(triangle.z+offset))-gltf_uv(material,11,read2(triangle.x+offset));
        mat3 coat_frame = gltf_tangent_frame(normal, material.textures[11].w == 0 && (material.textures[2].y == 0 || material.textures[11].x == material.textures[2].x) ? tangent : vec4(0),edge1,edge2,c1,c2);
        vec3 sampled = samples[11].xyz*2.0-1.0;
        sampled.xy *= material.clearcoat.z;
        if(dot(coat_frame[0],coat_frame[0])>1e-10 && dot(sampled,sampled)>1e-10) coat_normal = normalize(coat_frame*sampled)*(front?1.0:-1.0);
    }
    gltf_geometry geometry = gltf_geometry(position,n,frame[0],frame[1],coat_normal);
    return gltf_evaluate(reference.scene, material, fetch_vertex_color(triangle, weights), samples, geometry, direction, front, seed, volume, instance_id, distance);
}
