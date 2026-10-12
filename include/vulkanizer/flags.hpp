#pragma once

#include "enum_bit_flags.hpp"
#include <volk.h>

namespace vkz {
    enum class queue : VkQueueFlags {
        none = 0,
        graphics = VK_QUEUE_GRAPHICS_BIT,
        compute = VK_QUEUE_COMPUTE_BIT,
        transfer = VK_QUEUE_TRANSFER_BIT,
        sparse_binding = VK_QUEUE_SPARSE_BINDING_BIT,
        protected_memory = VK_QUEUE_PROTECTED_BIT,
        video_decode = VK_QUEUE_VIDEO_DECODE_BIT_KHR,
        video_encode = VK_QUEUE_VIDEO_ENCODE_BIT_KHR,
        optical_flow = VK_QUEUE_OPTICAL_FLOW_BIT_NV,
    };

    enum class command_pool_flags : VkCommandPoolCreateFlags {
        none = 0,
        transient = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        reset_command_buffer = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        protected_memory = VK_COMMAND_POOL_CREATE_PROTECTED_BIT,
    };

    enum class command_buffer_usage : VkCommandBufferUsageFlags {
        none = 0,
        one_time_submit = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        render_pass_continue = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
        simultaneous_use = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
    };

    enum class sample_count : VkSampleCountFlags {
        none = 0,
        x1 = VK_SAMPLE_COUNT_1_BIT,
        x2 = VK_SAMPLE_COUNT_2_BIT,
        x4 = VK_SAMPLE_COUNT_4_BIT,
        x8 = VK_SAMPLE_COUNT_8_BIT,
        x16 = VK_SAMPLE_COUNT_16_BIT,
        x32 = VK_SAMPLE_COUNT_32_BIT,
        x64 = VK_SAMPLE_COUNT_64_BIT,
    };

    enum class cull_mode : VkCullModeFlags {
        none = VK_CULL_MODE_NONE,
        front = VK_CULL_MODE_FRONT_BIT,
        back = VK_CULL_MODE_BACK_BIT,
        front_and_back = VK_CULL_MODE_FRONT_AND_BACK,
    };

    enum class color_component : VkColorComponentFlags {
        none = 0,
        r = VK_COLOR_COMPONENT_R_BIT,
        g = VK_COLOR_COMPONENT_G_BIT,
        b = VK_COLOR_COMPONENT_B_BIT,
        a = VK_COLOR_COMPONENT_A_BIT,
        rgb = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT,
        rgba = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    enum class buffer_flags : VkBufferCreateFlags {
        none = 0,
        sparse_binding = VK_BUFFER_CREATE_SPARSE_BINDING_BIT,
        sparse_residency = VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT,
        sparse_aliased = VK_BUFFER_CREATE_SPARSE_ALIASED_BIT,
        protected_memory = VK_BUFFER_CREATE_PROTECTED_BIT,
        device_address_capture_replay = VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT,
        descriptor_buffer_capture_replay = VK_BUFFER_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT,
        video_profile_independent = VK_BUFFER_CREATE_VIDEO_PROFILE_INDEPENDENT_BIT_KHR,
    };

    enum class image_flags : VkImageCreateFlags {
        none = 0,
        sparse_binding = VK_IMAGE_CREATE_SPARSE_BINDING_BIT,
        sparse_residency = VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT,
        sparse_aliased = VK_IMAGE_CREATE_SPARSE_ALIASED_BIT,
        mutable_format = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
        cube_compatible = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
        alias = VK_IMAGE_CREATE_ALIAS_BIT,
        split_instance_bind_regions = VK_IMAGE_CREATE_SPLIT_INSTANCE_BIND_REGIONS_BIT,
        s2d_array_compatible = VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT,
        block_texel_view_compatible = VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT,
        extended_usage = VK_IMAGE_CREATE_EXTENDED_USAGE_BIT,
        protected_memory = VK_IMAGE_CREATE_PROTECTED_BIT,
        disjoint = VK_IMAGE_CREATE_DISJOINT_BIT,
        corner_sampled = VK_IMAGE_CREATE_CORNER_SAMPLED_BIT_NV,
        sample_locations_compatible_depth = VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT,
        subsampled = VK_IMAGE_CREATE_SUBSAMPLED_BIT_EXT,
        descriptor_buffer_capture_replay = VK_IMAGE_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT,
        multisampled_render_to_single_sampled = VK_IMAGE_CREATE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_BIT_EXT,
        s2d_view_compatible = VK_IMAGE_CREATE_2D_VIEW_COMPATIBLE_BIT_EXT,
        fragment_density_map_offset = VK_IMAGE_CREATE_FRAGMENT_DENSITY_MAP_OFFSET_BIT_QCOM,
        video_profile_independent = VK_IMAGE_CREATE_VIDEO_PROFILE_INDEPENDENT_BIT_KHR,
    };

    enum class image_view_flags : VkImageViewCreateFlags {
        none = 0,
        fragment_density_map_dynamic = VK_IMAGE_VIEW_CREATE_FRAGMENT_DENSITY_MAP_DYNAMIC_BIT_EXT,
        descriptor_buffer_capture_replay = VK_IMAGE_VIEW_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT,
        fragment_density_map_deferred = VK_IMAGE_VIEW_CREATE_FRAGMENT_DENSITY_MAP_DEFERRED_BIT_EXT,
    };

    enum class sampler_flags : VkSamplerCreateFlags {
        none = 0,
        subsampled = VK_SAMPLER_CREATE_SUBSAMPLED_BIT_EXT,
        subsampled_coarse_reconstruction = VK_SAMPLER_CREATE_SUBSAMPLED_COARSE_RECONSTRUCTION_BIT_EXT,
        descriptor_buffer_capture_replay = VK_SAMPLER_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT,
        non_seamless_cube_map = VK_SAMPLER_CREATE_NON_SEAMLESS_CUBE_MAP_BIT_EXT,
        image_processing = VK_SAMPLER_CREATE_IMAGE_PROCESSING_BIT_QCOM,
    };

    enum class pipeline_flags : VkPipelineCreateFlags {
        none = 0,
        disable_optimization = VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT,
        allow_derivatives = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT,
        derivative = VK_PIPELINE_CREATE_DERIVATIVE_BIT,
        view_index_from_device_index = VK_PIPELINE_CREATE_VIEW_INDEX_FROM_DEVICE_INDEX_BIT,
        dispatch_base = VK_PIPELINE_CREATE_DISPATCH_BASE_BIT,
        fail_on_pipeline_compile_required = VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT,
        early_return_on_failure = VK_PIPELINE_CREATE_EARLY_RETURN_ON_FAILURE_BIT,
        rendering_fragment_shading_rate_attachment = VK_PIPELINE_CREATE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR,
        rendering_fragment_density_map_attachment = VK_PIPELINE_CREATE_RENDERING_FRAGMENT_DENSITY_MAP_ATTACHMENT_BIT_EXT,
        ray_tracing_no_null_any_hit_shaders = VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_ANY_HIT_SHADERS_BIT_KHR,
        ray_tracing_no_null_closest_hit_shaders = VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_CLOSEST_HIT_SHADERS_BIT_KHR,
        ray_tracing_no_null_miss_shaders = VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_MISS_SHADERS_BIT_KHR,
        ray_tracing_no_null_intersection_shaders = VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_INTERSECTION_SHADERS_BIT_KHR,
        ray_tracing_skip_triangles = VK_PIPELINE_CREATE_RAY_TRACING_SKIP_TRIANGLES_BIT_KHR,
        ray_tracing_skip_aabbs = VK_PIPELINE_CREATE_RAY_TRACING_SKIP_AABBS_BIT_KHR,
        ray_tracing_shader_group_handle_capture_replay = VK_PIPELINE_CREATE_RAY_TRACING_SHADER_GROUP_HANDLE_CAPTURE_REPLAY_BIT_KHR,
        defer_compile = VK_PIPELINE_CREATE_DEFER_COMPILE_BIT_NV,
        capture_statistics = VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR,
        capture_internal_representations = VK_PIPELINE_CREATE_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR,
        indirect_bindable = VK_PIPELINE_CREATE_INDIRECT_BINDABLE_BIT_NV,
        library = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR,
        descriptor_buffer = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
        retain_link_time_optimization_info = VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT,
        link_time_optimization = VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT,
        ray_tracing_allow_motion = VK_PIPELINE_CREATE_RAY_TRACING_ALLOW_MOTION_BIT_NV,
        color_attachment_feedback_loop = VK_PIPELINE_CREATE_COLOR_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT,
        depth_stencil_attachment_feedback_loop = VK_PIPELINE_CREATE_DEPTH_STENCIL_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT,
        ray_tracing_opacity_micromap = VK_PIPELINE_CREATE_RAY_TRACING_OPACITY_MICROMAP_BIT_EXT,
#ifdef VK_ENABLE_BETA_EXTENSIONS
        ray_tracing_displacement_micromap = VK_PIPELINE_CREATE_RAY_TRACING_DISPLACEMENT_MICROMAP_BIT_NV,
#endif
        no_protected_access = VK_PIPELINE_CREATE_NO_PROTECTED_ACCESS_BIT_EXT,
        protected_access_only = VK_PIPELINE_CREATE_PROTECTED_ACCESS_ONLY_BIT_EXT,
    };

    enum class descriptor_pool_flags : VkDescriptorPoolCreateFlags {
        none = 0,
        free_descriptor_set = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        update_after_bind = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        host_only = VK_DESCRIPTOR_POOL_CREATE_HOST_ONLY_BIT_EXT,
        allow_overallocation_sets = VK_DESCRIPTOR_POOL_CREATE_ALLOW_OVERALLOCATION_SETS_BIT_NV,
        allow_overallocation_pools = VK_DESCRIPTOR_POOL_CREATE_ALLOW_OVERALLOCATION_POOLS_BIT_NV,
    };

    enum class descriptor_layout_flags : VkDescriptorSetLayoutCreateFlags {
        none = 0,
        update_after_bind_pool = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        push_descriptor = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
        descriptor_buffer = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
        embedded_immutable_samplers = VK_DESCRIPTOR_SET_LAYOUT_CREATE_EMBEDDED_IMMUTABLE_SAMPLERS_BIT_EXT,
        indirect_bindable = VK_DESCRIPTOR_SET_LAYOUT_CREATE_INDIRECT_BINDABLE_BIT_NV,
        host_only_pool = VK_DESCRIPTOR_SET_LAYOUT_CREATE_HOST_ONLY_POOL_BIT_EXT,
        per_stage = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PER_STAGE_BIT_NV,
    };

    enum class descriptor_binding_flags : VkDescriptorBindingFlags {
        none = 0,
        update_after_bind = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        update_unused_while_pending = VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT,
        partially_bound = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
        variable_descriptor_count = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT,
        bindless = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
    };

    enum class dependency : VkDependencyFlags {
        none = 0,
        by_region = VK_DEPENDENCY_BY_REGION_BIT,
        device_group = VK_DEPENDENCY_DEVICE_GROUP_BIT,
        view_local = VK_DEPENDENCY_VIEW_LOCAL_BIT,
        feedback_loop = VK_DEPENDENCY_FEEDBACK_LOOP_BIT_EXT,
    };

    enum class surface_transform : VkSurfaceTransformFlagsKHR {
        none = 0,
        identity = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        rotate_90 = VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR,
        rotate_180 = VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR,
        rotate_270 = VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR,
        horizontal_mirror = VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR,
        horizontal_mirror_rotate_90 = VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR,
        horizontal_mirror_rotate_180 = VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR,
        horizontal_mirror_rotate_270 = VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR,
        inherit = VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR,
    };

    enum class composite_alpha : VkCompositeAlphaFlagsKHR {
        none = 0,
        opaque = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        pre_multiplied = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        post_multiplied = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        inherit = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
    };

    enum class acceleration_structure_build : VkBuildAccelerationStructureFlagsKHR {
        none = 0,
        allow_update = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
        allow_compaction = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR,
        prefer_fast_trace = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        prefer_fast_build = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR,
        low_memory = VK_BUILD_ACCELERATION_STRUCTURE_LOW_MEMORY_BIT_KHR,
        motion = VK_BUILD_ACCELERATION_STRUCTURE_MOTION_BIT_NV,
        allow_opacity_micromap_update = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_OPACITY_MICROMAP_UPDATE_EXT,
        allow_disable_opacity_micromaps = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DISABLE_OPACITY_MICROMAPS_EXT,
        allow_opacity_micromap_data_update = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_OPACITY_MICROMAP_DATA_UPDATE_EXT,
#ifdef VK_ENABLE_BETA_EXTENSIONS
        allow_displacement_micromap_update = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DISPLACEMENT_MICROMAP_UPDATE_NV,
#endif
        allow_data_access = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS_KHR,
    };

    enum class geometry_instance : VkGeometryInstanceFlagsKHR {
        none = 0,
        triangle_facing_cull_disable = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
        triangle_flip_facing = VK_GEOMETRY_INSTANCE_TRIANGLE_FLIP_FACING_BIT_KHR,
        force_opaque = VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR,
        force_no_opaque = VK_GEOMETRY_INSTANCE_FORCE_NO_OPAQUE_BIT_KHR,
        force_opacity_micromap_2_state = VK_GEOMETRY_INSTANCE_FORCE_OPACITY_MICROMAP_2_STATE_EXT,
        disable_opacity_micromaps = VK_GEOMETRY_INSTANCE_DISABLE_OPACITY_MICROMAPS_EXT,
    };

    enum class format_feature : VkFormatFeatureFlags {
        none = 0,
        sampled_image = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT,
        storage_image = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT,
        storage_image_atomic = VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT,
        uniform_texel_buffer = VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT,
        storage_texel_buffer = VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT,
        storage_texel_buffer_atomic = VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_ATOMIC_BIT,
        vertex_buffer = VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT,
        color_attachment = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT,
        color_attachment_blend = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT,
        depth_stencil_attachment = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,
        blit_src = VK_FORMAT_FEATURE_BLIT_SRC_BIT,
        blit_dst = VK_FORMAT_FEATURE_BLIT_DST_BIT,
        sampled_image_filter_linear = VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT,
        transfer_src = VK_FORMAT_FEATURE_TRANSFER_SRC_BIT,
        transfer_dst = VK_FORMAT_FEATURE_TRANSFER_DST_BIT,
        midpoint_chroma_samples = VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT,
        sampled_image_ycbcr_conversion_linear_filter = VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT,
        sampled_image_ycbcr_conversion_separate_reconstruction_filter = VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER_BIT,
        sampled_image_ycbcr_conversion_chroma_reconstruction_explicit = VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_BIT,
        sampled_image_ycbcr_conversion_chroma_reconstruction_explicit_forceable = VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_FORCEABLE_BIT,
        disjoint = VK_FORMAT_FEATURE_DISJOINT_BIT,
        cosited_chroma_samples = VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT,
        sampled_image_filter_minmax = VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_MINMAX_BIT,
        video_decode_output = VK_FORMAT_FEATURE_VIDEO_DECODE_OUTPUT_BIT_KHR,
        video_decode_dpb = VK_FORMAT_FEATURE_VIDEO_DECODE_DPB_BIT_KHR,
        acceleration_structure_vertex_buffer = VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR,
        sampled_image_filter_cubic = VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_CUBIC_BIT_EXT,
        fragment_density_map = VK_FORMAT_FEATURE_FRAGMENT_DENSITY_MAP_BIT_EXT,
        fragment_shading_rate_attachment = VK_FORMAT_FEATURE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR,
        video_encode_input = VK_FORMAT_FEATURE_VIDEO_ENCODE_INPUT_BIT_KHR,
        video_encode_dpb = VK_FORMAT_FEATURE_VIDEO_ENCODE_DPB_BIT_KHR,
        blit = VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT,
        storage_atomic = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT,
        color_sampled_blend = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT,
    };

    enum class debug_severity : VkDebugUtilsMessageSeverityFlagsEXT {
        none = 0,
        verbose = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
        info = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
        warning = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
        error = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        all = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
    };

    enum class debug_type : VkDebugUtilsMessageTypeFlagsEXT {
        none = 0,
        general = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
        validation = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
        performance = VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        device_address_binding = VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT,
        standard = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
    };

    enum class device_diagnostics : VkDeviceDiagnosticsConfigFlagsNV {
        none = 0,
        enable_shader_debug_info = VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_DEBUG_INFO_BIT_NV,
        enable_resource_tracking = VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_RESOURCE_TRACKING_BIT_NV,
        enable_automatic_checkpoints = VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_AUTOMATIC_CHECKPOINTS_BIT_NV,
        enable_shader_error_reporting = VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_ERROR_REPORTING_BIT_NV,
        crash_dump = VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_RESOURCE_TRACKING_BIT_NV | VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_DEBUG_INFO_BIT_NV | VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_ERROR_REPORTING_BIT_NV,
    };

    enum class query_result : VkQueryResultFlags {
        none = 0,
        bits_64 = VK_QUERY_RESULT_64_BIT,
        wait = VK_QUERY_RESULT_WAIT_BIT,
        with_availability = VK_QUERY_RESULT_WITH_AVAILABILITY_BIT,
        partial = VK_QUERY_RESULT_PARTIAL_BIT,
        with_status = VK_QUERY_RESULT_WITH_STATUS_BIT_KHR,
        wait_64 = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT,
    };

    enum class resolve_mode : VkResolveModeFlags {
        none = VK_RESOLVE_MODE_NONE,
        sample_zero = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT,
        average = VK_RESOLVE_MODE_AVERAGE_BIT,
        min = VK_RESOLVE_MODE_MIN_BIT,
        max = VK_RESOLVE_MODE_MAX_BIT,
        external_format_downsample = VK_RESOLVE_MODE_EXTERNAL_FORMAT_DOWNSAMPLE_ANDROID,
    };

    enum class video_chroma_subsampling : VkVideoChromaSubsamplingFlagsKHR {
        invalid = VK_VIDEO_CHROMA_SUBSAMPLING_INVALID_KHR,
        monochrome = VK_VIDEO_CHROMA_SUBSAMPLING_MONOCHROME_BIT_KHR,
        s420 = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,
        s422 = VK_VIDEO_CHROMA_SUBSAMPLING_422_BIT_KHR,
        s444 = VK_VIDEO_CHROMA_SUBSAMPLING_444_BIT_KHR,
    };

    enum class video_codec_operation : VkVideoCodecOperationFlagsKHR {
        none = VK_VIDEO_CODEC_OPERATION_NONE_KHR,
        encode_h264 = VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR,
        encode_h265 = VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR,
        decode_h264 = VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR,
        decode_h265 = VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR,
        decode_av1 = VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR,
    };

    enum class video_coding_control : VkVideoCodingControlFlagsKHR {
        none = 0,
        reset = VK_VIDEO_CODING_CONTROL_RESET_BIT_KHR,
        encode_rate_control = VK_VIDEO_CODING_CONTROL_ENCODE_RATE_CONTROL_BIT_KHR,
        encode_quality_level = VK_VIDEO_CODING_CONTROL_ENCODE_QUALITY_LEVEL_BIT_KHR,
    };

    enum class video_component_bit_depth : VkVideoComponentBitDepthFlagsKHR {
        invalid = VK_VIDEO_COMPONENT_BIT_DEPTH_INVALID_KHR,
        bits_8 = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,
        bits_10 = VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR,
        bits_12 = VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR,
    };

    enum class video_decode_capability : VkVideoDecodeCapabilityFlagsKHR {
        none = 0,
        dpb_and_output_coincide = VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_COINCIDE_BIT_KHR,
        dpb_and_output_distinct = VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_DISTINCT_BIT_KHR,
    };

    enum class video_decode_h264_picture_layout : VkVideoDecodeH264PictureLayoutFlagsKHR {
        progressive = VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_PROGRESSIVE_KHR,
        interlaced_interleaved_lines = VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_INTERLACED_INTERLEAVED_LINES_BIT_KHR,
        interlaced_separate_planes = VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_INTERLACED_SEPARATE_PLANES_BIT_KHR,
    };

} // namespace vkz

template <>
struct enable_bitmask_operators<vkz::queue> {
    static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<vkz::command_pool_flags> {
    static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<vkz::command_buffer_usage> {
    static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<vkz::sample_count> {
    static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<vkz::cull_mode> {
    static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<vkz::color_component> {
    static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<vkz::buffer_flags> {
    static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<vkz::image_flags> {
    static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<vkz::image_view_flags> {
    static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<vkz::sampler_flags> {
    static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<vkz::pipeline_flags> {
    static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<vkz::descriptor_pool_flags> {
    static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<vkz::descriptor_layout_flags> {
    static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<vkz::descriptor_binding_flags> {
    static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<vkz::dependency> {
    static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<vkz::surface_transform> {
    static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<vkz::composite_alpha> {
    static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<vkz::acceleration_structure_build> {
    static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<vkz::geometry_instance> {
    static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<vkz::format_feature> {
    static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<vkz::debug_severity> {
    static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<vkz::debug_type> {
    static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<vkz::device_diagnostics> {
    static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<vkz::query_result> {
    static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<vkz::resolve_mode> {
    static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<vkz::video_chroma_subsampling> {
    static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<vkz::video_codec_operation> {
    static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<vkz::video_coding_control> {
    static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<vkz::video_component_bit_depth> {
    static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<vkz::video_decode_capability> {
    static constexpr bool enable = true;
};

template <>
struct enable_bitmask_operators<vkz::video_decode_h264_picture_layout> {
    static constexpr bool enable = true;
};
