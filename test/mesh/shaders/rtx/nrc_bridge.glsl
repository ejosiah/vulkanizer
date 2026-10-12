#ifndef LIGHT_TRANSPORT_NRC_BRIDGE_GLSL
#define LIGHT_TRANSPORT_NRC_BRIDGE_GLSL

#include "nrc_records.glsl"

layout(set = 2, binding = 1, std430) buffer nrc_query_data { nrc_vertex nrc_queries[]; };
layout(set = 2, binding = 2, std430) buffer nrc_training_data { nrc_vertex nrc_training_vertices[]; };
layout(set = 2, binding = 3, std430) buffer nrc_training_paths_data { nrc_training_path nrc_training_paths[]; };

#endif
