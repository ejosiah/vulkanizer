#ifndef LIGHT_TRANSPORT_NRD_SIGNALS_GLSL
#define LIGHT_TRANSPORT_NRD_SIGNALS_GLSL

#ifndef NRD_DESCRIPTOR_SET
#define NRD_DESCRIPTOR_SET 3
#endif

#ifndef NRD_IMAGE_ACCESS
#define NRD_IMAGE_ACCESS
#endif

layout(set = NRD_DESCRIPTOR_SET, binding = 0, rgba32f) uniform NRD_IMAGE_ACCESS image2D nrd_diffuse;
layout(set = NRD_DESCRIPTOR_SET, binding = 1, rgba32f) uniform NRD_IMAGE_ACCESS image2D nrd_specular;
layout(set = NRD_DESCRIPTOR_SET, binding = 2, rgba32f) uniform NRD_IMAGE_ACCESS image2D nrd_normal_roughness;
layout(set = NRD_DESCRIPTOR_SET, binding = 3, rgba32f) uniform NRD_IMAGE_ACCESS image2D nrd_view_z;
layout(set = NRD_DESCRIPTOR_SET, binding = 4, rgba32f) uniform NRD_IMAGE_ACCESS image2D nrd_motion;
layout(set = NRD_DESCRIPTOR_SET, binding = 5, rgba32f) uniform NRD_IMAGE_ACCESS image2D nrd_diffuse_factor;
layout(set = NRD_DESCRIPTOR_SET, binding = 6, rgba32f) uniform NRD_IMAGE_ACCESS image2D nrd_specular_factor;
layout(set = NRD_DESCRIPTOR_SET, binding = 7, rgba32f) uniform NRD_IMAGE_ACCESS image2D nrd_passthrough;
layout(set = NRD_DESCRIPTOR_SET, binding = 8, rgba32f) uniform NRD_IMAGE_ACCESS image2D nrd_cache_split;
layout(set = NRD_DESCRIPTOR_SET, binding = 9, rgba32f) uniform NRD_IMAGE_ACCESS image2D nrd_cache_prefix;
layout(set = NRD_DESCRIPTOR_SET, binding = 10, rgba32f) uniform NRD_IMAGE_ACCESS image2D nrd_diffuse_output;
layout(set = NRD_DESCRIPTOR_SET, binding = 11, rgba32f) uniform NRD_IMAGE_ACCESS image2D nrd_specular_output;

#endif
