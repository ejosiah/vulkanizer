#define ENABLE_NRC 1
#include "Nrc.hlsli"

struct nrc_vertex {
    float4 position_roughness;
    float4 normal_distance;
    float4 view_delta;
    float4 diffuse_reflectance;
    float4 specular_reflectance;
    float4 direct_radiance;
    float4 scatter_weight_pdf;
    float4 throughput_valid;
    float4 emission;
};

struct nrc_training_path {
    float4 terminal_radiance;
    uint vertex_count;
    uint exited;
    uint reserved_0;
    uint reserved_1;
};

[[vk::binding(0, 0)]] StructuredBuffer<NrcConstants> constants;
[[vk::binding(1, 0)]] StructuredBuffer<nrc_vertex> queries;
[[vk::binding(2, 0)]] StructuredBuffer<nrc_vertex> vertices;
[[vk::binding(3, 0)]] StructuredBuffer<nrc_training_path> paths;
[[vk::binding(4, 0)]] RWStructuredBuffer<uint> counters;
[[vk::binding(5, 0)]] RWStructuredBuffer<NrcPackedQueryPathInfo> query_paths;
[[vk::binding(6, 0)]] RWStructuredBuffer<NrcPackedTrainingPathInfo> training_paths;
[[vk::binding(7, 0)]] RWStructuredBuffer<NrcPackedPathVertex> training_vertices;
[[vk::binding(8, 0)]] RWStructuredBuffer<NrcRadianceParams> query_parameters;

NrcSurfaceAttributes attributes(nrc_vertex vertex, NrcConstants parameters) {
    NrcSurfaceAttributes result;
    result.encodedPosition = NrcEncodePosition(vertex.position_roughness.xyz, parameters);
    result.roughness = vertex.position_roughness.w;
    result.shadingNormal = vertex.normal_distance.xyz;
    result.viewVector = vertex.view_delta.xyz;
    result.diffuseReflectance = vertex.diffuse_reflectance.xyz;
    result.specularF0 = vertex.specular_reflectance.xyz;
    result.isDeltaLobe = vertex.view_delta.w != 0.0;
    return result;
}

NrcContext context_for(uint2 pixel, NrcConstants parameters) {
    NrcBuffers buffers;
    buffers.queryPathInfo = query_paths;
    buffers.trainingPathInfo = training_paths;
    buffers.trainingPathVertices = training_vertices;
    buffers.queryRadianceParams = query_parameters;
    buffers.countersData = counters;
    return NrcCreateContext(parameters, buffers, pixel);
}

[numthreads(8, 8, 1)]
void train_bridge(uint3 thread : SV_DispatchThreadID) {
    g_nrcMode = NrcMode::Update;
    NrcConstants parameters = constants[0];
    if (any(thread.xy >= parameters.trainingDimensions)) return;
    uint index = thread.x + thread.y * parameters.trainingDimensions.x;
    NrcContext context = context_for(thread.xy, parameters);
    NrcPathState state = NrcCreatePathState(parameters, 0.0);
    nrc_training_path path = paths[index];
    float3 throughput = 1.0;
    float3 radiance = 0.0;
    bool terminated = false;

    for (uint bounce = 0; bounce < path.vertex_count; ++bounce) {
        nrc_vertex vertex = vertices[index * parameters.maxPathVertices + bounce];
        radiance += throughput * vertex.emission.rgb;
        NrcProgressState progress = NrcUpdateOnHit(context, state, attributes(vertex, parameters), vertex.normal_distance.w, bounce, throughput, radiance);
        if (progress == NrcProgressState::TerminateImmediately) {
            terminated = true;
            break;
        }
        radiance += throughput * vertex.direct_radiance.rgb;
        if (progress == NrcProgressState::TerminateAfterDirectLighting) {
            terminated = true;
            break;
        }
        NrcSetBrdfPdf(state, vertex.scatter_weight_pdf.w);
        throughput *= vertex.scatter_weight_pdf.rgb;
    }
    if (!terminated) {
        radiance += throughput * path.terminal_radiance.rgb;
        if (path.exited != 0) NrcUpdateOnMiss(state);
    }
    NrcWriteFinalPathInfo(context, state, throughput, radiance);
}

[numthreads(8, 8, 1)]
void query_bridge(uint3 thread : SV_DispatchThreadID) {
    g_nrcMode = NrcMode::Query;
    NrcConstants parameters = constants[0];
    if (any(thread.xy >= parameters.frameDimensions)) return;
    parameters.enableTerminationHeuristic = 0;
    uint index = thread.x + thread.y * parameters.frameDimensions.x;
    NrcContext context = context_for(thread.xy, parameters);
    NrcPathState state = NrcCreatePathState(parameters, 1.0);
    nrc_vertex vertex = queries[index];
    float3 throughput = vertex.throughput_valid.xyz;
    float3 radiance = 0.0;
    if (vertex.throughput_valid.w != 0.0) {
        NrcUpdateOnHit(context, state, attributes(vertex, parameters), vertex.normal_distance.w, 1, throughput, radiance);
    }
    NrcWriteFinalPathInfo(context, state, throughput, radiance);
}
