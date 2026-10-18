#include <vulkanizer/commands.hpp>
#include <vulkanizer/memory.hpp>
#include <vulkanizer/barrier.hpp>
#include <vulkanizer/descriptor_set_builder.hpp>

#include <cassert>
#include <array>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace {
    VkAccessFlags2 observed_src_access{};
    VkAccessFlags2 observed_dst_access{};
    VkPipelineStageFlags2 observed_src_stage{};
    VkPipelineStageFlags2 observed_dst_stage{};
    VkShaderStageFlags observed_shader_stages{};
    VkDependencyFlags observed_dependency{};
    VkCommandBufferUsageFlags observed_command_usage{};
    std::array<VkDescriptorType, 5> observed_descriptor_types{};

    template <typename Handle> Handle test_handle(uintptr_t value) {
        if constexpr (std::is_pointer_v<Handle>) {
            return reinterpret_cast<Handle>(value);
        } else {
            return static_cast<Handle>(value);
        }
    }

    void VKAPI_CALL capture_descriptor_updates(VkDevice, uint32_t count, const VkWriteDescriptorSet *writes, uint32_t copies, const VkCopyDescriptorSet *) {
        assert(count == observed_descriptor_types.size());
        assert(copies == 0);
        for (uint32_t i = 0; i < count; ++i) {
            assert(writes[i].dstBinding == i);
            observed_descriptor_types[i] = writes[i].descriptorType;
        }
    }

    VkResult VKAPI_CALL capture_begin(VkCommandBuffer, const VkCommandBufferBeginInfo *info) {
        observed_command_usage = info->flags;
        return VK_SUCCESS;
    }

    void VKAPI_CALL capture_push_constants(VkCommandBuffer, VkPipelineLayout, VkShaderStageFlags stages, uint32_t, uint32_t, const void *) {
        observed_shader_stages = stages;
    }

    void VKAPI_CALL capture_barrier(VkCommandBuffer, const VkDependencyInfo *info) {
        observed_dependency = info->dependencyFlags;
        assert(info->memoryBarrierCount == 1);
        observed_src_access = info->pMemoryBarriers[0].srcAccessMask;
        observed_dst_access = info->pMemoryBarriers[0].dstAccessMask;
        observed_src_stage = info->pMemoryBarriers[0].srcStageMask;
        observed_dst_stage = info->pMemoryBarriers[0].dstStageMask;
    }
}

int main() {
    static_assert(std::is_same_v<decltype(vkz::image_usage_sampled_upload), const vkz::image::usage>);
    static_assert(std::is_same_v<vkz::pipeline_stage, vkz::pipeline::stage>);
    static_assert(has_flag(vkz::image_usage_sampled_upload, vkz::image_usage_sampled | vkz::image_usage_transfer_dst));
    const auto original_begin = vkBeginCommandBuffer;
    vkBeginCommandBuffer = capture_begin;
    vkz::cmd_begin(VK_NULL_HANDLE, vkz::command_buffer_usage_one_time_submit | vkz::command_buffer_usage_simultaneous_use);
    assert(observed_command_usage == (VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT));
    vkz::cmd_begin(VK_NULL_HANDLE, vkz::command_buffer_usage_none);
    assert(observed_command_usage == 0);
    vkBeginCommandBuffer = original_begin;
    static_assert(has_flag(vkz::shader_stage_all_graphics, vkz::shader_stage_vertex | vkz::shader_stage_fragment));
    static_assert((vkz::shader_stage_all_graphics & vkz::shader_stage_compute) == vkz::shader_stage_none);

    const auto original_push_constants = vkCmdPushConstants;
    vkCmdPushConstants = capture_push_constants;
    vkz::push_constants(VK_NULL_HANDLE, VkPipelineLayout{}, uint32_t{42}, vkz::shader_stage_vertex | vkz::shader_stage_fragment);
    vkCmdPushConstants = original_push_constants;
    assert(observed_shader_stages == (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT));

    vkz::device descriptor_device{};
    std::vector<VkDescriptorSetLayoutBinding> descriptor_bindings;
    std::vector<std::vector<VkSampler>> immutable_samplers;
    vkz::descriptor_set_layout_builder::descriptor_set_layout_binding_builder binding{descriptor_device, descriptor_bindings, immutable_samplers, "", false, 0};
    const auto next_binding = binding.descriptor_type(vkz::descriptor_type_uniform_buffer).descriptor_count(1).shader_stages(vkz::shader_stage_vertex | vkz::shader_stage_fragment).binding(1);
    assert(descriptor_bindings.size() == 1);
    assert(descriptor_bindings[0].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    assert(descriptor_bindings[0].stageFlags == (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT));
    const auto descriptor_buffer = vkz::descriptor<vkz::buffer_descriptor>(vkz::buffer{}, 3);
    assert(descriptor_buffer.binding == 3);
    const auto descriptor_texture = vkz::descriptor<vkz::texture_descriptor>(vkz::texture{}, 4);
    assert(descriptor_texture.binding == 4);
    static_assert(!enable_bitmask_operators<vkz::descriptor_type>::enable);
    const auto original_update_descriptors = vkUpdateDescriptorSets;
    vkUpdateDescriptorSets = capture_descriptor_updates;
    vkz::device update_device{};
    update_device.logical = test_handle<VkDevice>(1);
    vkz::descriptor_set update_set{};
    update_set.handle = test_handle<VkDescriptorSet>(2);
    vkz::update_descriptor(update_device, {
        .descriptor_set = update_set,
        .bindings = {
            vkz::buffer_descriptor{.binding = 0},
            vkz::ubo_descriptor{.binding = 1},
            vkz::image_descriptor{.binding = 2},
            vkz::input_attachment_descriptor{.binding = 3},
            vkz::texture_descriptor{.binding = 4},
        },
    });
    vkUpdateDescriptorSets = original_update_descriptors;
    assert((observed_descriptor_types == std::array<VkDescriptorType, 5>{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                                      VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}));

    static_assert(sizeof(vkz::pipeline_stage) == sizeof(VkPipelineStageFlags2));
    static_assert(sizeof(vkz::access) == sizeof(VkAccessFlags2));
    static_assert((vkz::access_color_attachment_read | vkz::access_color_attachment_write) == vkz::access_color_attachment_read_write);
    static_assert(static_cast<VkAccessFlags>(vkz::access_shader_read_write) == (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT));
    static_assert(static_cast<VkAccessFlags2>(vkz::access_shader_storage_read_write) == (VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT));

    const auto original_pipeline_barrier = vkCmdPipelineBarrier2;
    vkCmdPipelineBarrier2 = capture_barrier;
    vkz::barrier::push(vkz::pipeline_stage_video_decode | vkz::pipeline_stage_copy, vkz::pipeline_stage_compute_shader | vkz::pipeline_stage_resolve, vkz::access_video_decode_read_write,
                       vkz::access_shader_storage_read_write);
    vkz::barrier::flush(VK_NULL_HANDLE, vkz::dependency_by_region | vkz::dependency_view_local);
    vkCmdPipelineBarrier2 = original_pipeline_barrier;
    assert(observed_dependency == (VK_DEPENDENCY_BY_REGION_BIT | VK_DEPENDENCY_VIEW_LOCAL_BIT));
    assert(observed_src_access == (VK_ACCESS_2_VIDEO_DECODE_READ_BIT_KHR | VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR));
    assert(observed_dst_access == (VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT));
    assert(observed_src_stage == (VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR | VK_PIPELINE_STAGE_2_COPY_BIT));
    assert(observed_dst_stage == (VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_RESOLVE_BIT));

    vkz::batch_submission legacy_submission;
    bool rejected_wide_stage{};
    try {
        legacy_submission.enqueue_wait({}, vkz::pipeline_stage_copy);
    } catch (const std::invalid_argument &) {
        rejected_wide_stage = true;
    }
    assert(rejected_wide_stage);

    static_assert(std::is_invocable_v<decltype(&vkz::buffer_builder::usage), vkz::buffer_builder &, vkz::buffer_usage>);
    static_assert(!std::is_invocable_v<decltype(&vkz::buffer_builder::usage), vkz::buffer_builder &, VkBufferUsageFlags>);
    static_assert(std::is_invocable_v<decltype(&vkz::image_builder::usage), vkz::image_builder &, vkz::image_usage>);
    static_assert(!std::is_invocable_v<decltype(&vkz::image_builder::usage), vkz::image_builder &, VkImageUsageFlags>);
    static_assert(std::is_invocable_v<decltype(&vkz::image_view_builder::aspect_mask), vkz::image_view_builder &, vkz::image_aspect>);
    static_assert(!std::is_invocable_v<decltype(&vkz::image_view_builder::aspect_mask), vkz::image_view_builder &, VkImageAspectFlags>);
    static_assert((vkz::image_aspect_depth | vkz::image_aspect_stencil) == vkz::image_aspect_depth_stencil);

    auto image_view_builder = vkz::image_view::builder(vkz::device{});
    image_view_builder.aspect_mask(vkz::image_aspect_depth_stencil);
    assert(image_view_builder.create_info().subresourceRange.aspectMask == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT));
    const vkz::sub_resource resource{.aspect_mask = vkz::image_aspect_plane_1};
    assert(vkz::image_subresource_range(resource).aspectMask == VK_IMAGE_ASPECT_PLANE_1_BIT);

    vkz::vma_memory_allocator allocator{};
    auto buffer_builder = vkz::buffer::builder(allocator);
    buffer_builder.flags(vkz::buffer_flags_sparse_binding | vkz::buffer_flags_sparse_residency);
    assert(buffer_builder.create_info().flags == (VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT));
    buffer_builder.flags(vkz::buffer_flags_none);
    assert(buffer_builder.create_info().flags == 0);
    static_assert(!std::is_invocable_v<decltype(&vkz::buffer_builder::flags), vkz::buffer_builder &, VkBufferCreateFlags>);
    buffer_builder.usage(vkz::buffer_usage_storage_transfer);
    assert(buffer_builder.create_info().usage == (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT));
    buffer_builder.usage(vkz::buffer_usage_vertex | vkz::buffer_usage_acceleration_input);
    assert(buffer_builder.create_info().usage == (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR));
    buffer_builder.usage(vkz::buffer_usage_none);
    assert(buffer_builder.create_info().usage == 0);

    auto image_builder = vkz::image::builder(allocator);
    image_builder.flags(vkz::image_flags_cube_compatible | vkz::image_flags_mutable_format).samples(vkz::sample_count_x4);
    assert(image_builder.create_info().flags == (VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT | VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT));
    assert(image_builder.create_info().samples == VK_SAMPLE_COUNT_4_BIT);
    image_builder.flags(vkz::image_flags_none);
    assert(image_builder.create_info().flags == 0);
    image_builder.usage(vkz::image_usage_sampled_storage_transfer);
    assert(image_builder.create_info().usage == (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT));
    image_builder.usage(vkz::image_usage_color_sampled | vkz::image_usage_transfer_src);
    assert(image_builder.create_info().usage == (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT));
    image_builder.usage(vkz::image_usage_none);
    assert(image_builder.create_info().usage == 0);

    assert(vkz::make_access_mask_pipeline_stage_flags(vkz::access_none) == vkz::pipeline_stage_top_of_pipe);
    assert(vkz::make_access_mask_pipeline_stage_flags(vkz::access_transfer_write) == vkz::pipeline_stage_transfer);
    assert(vkz::make_access_mask_pipeline_stage_flags(vkz::access_shader_storage_read_write, vkz::pipeline_stage_compute_shader) == vkz::pipeline_stage_compute_shader);
    assert(vkz::make_access_mask_pipeline_stage_flags(vkz::access_shader_read, vkz::pipeline_stage_compute_shader) == vkz::pipeline_stage_compute_shader);

    VkCommandBuffer command_buffers[2]{};
    VkSemaphore signal{};
    const auto submit_info = vkz::make_submit_info(2, command_buffers, 1, &signal);
    assert(submit_info.sType == VK_STRUCTURE_TYPE_SUBMIT_INFO);
    assert(submit_info.commandBufferCount == 2);
    assert(submit_info.pCommandBuffers == command_buffers);
    assert(submit_info.signalSemaphoreCount == 1);
    assert(submit_info.pSignalSemaphores == &signal);

    static_assert(!std::is_copy_constructible_v<vkz::command_pool>);
    static_assert(!std::is_copy_constructible_v<vkz::ring_fences>);
    static_assert(!std::is_copy_constructible_v<vkz::ring_command_pool>);
    static_assert(!std::is_copy_constructible_v<vkz::batch_submission>);
    static_assert(!std::is_copy_constructible_v<vkz::fenced_command_pools>);
    static_assert(std::is_constructible_v<vkz::scope_command_buffer, vkz::device, uint32_t, VkQueue>);
    static_assert(std::is_constructible_v<vkz::fenced_command_pools::scoped_cmd, vkz::fenced_command_pools&>);

    vkz::command_pool command_pool;
    vkz::ring_fences ring_fences;
    vkz::ring_command_pool ring_command_pool;
    vkz::submission_batch submission_batch;
    submission_batch.enqueue(command_buffers[0]);
    submission_batch.enqueue_wait({}, 1, vkz::pipeline_stage_compute_shader);
    submission_batch.enqueue_signal({}, 2, vkz::pipeline_stage_draw_indirect);
    assert(submission_batch.get_command_buffer_count() == 1);
    vkz::batch_submission batch_submission;
    batch_submission.enqueue_batch(std::move(submission_batch));
    vkz::fenced_command_pools fenced_command_pools;
}
