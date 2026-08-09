#include <vulkanizer/commands.hpp>

#include <cassert>
#include <type_traits>

int main() {
    assert(vkz::make_access_mask_pipeline_stage_flags(0) == VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    assert(vkz::make_access_mask_pipeline_stage_flags(VK_ACCESS_TRANSFER_WRITE_BIT) == VK_PIPELINE_STAGE_TRANSFER_BIT);
    assert(vkz::make_access_mask_pipeline_stage_flags(
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) == VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

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
    static_assert(std::is_constructible_v<vkz::scope_command_buffer, VkDevice, uint32_t, VkQueue>);
    static_assert(std::is_constructible_v<vkz::fenced_command_pools::scoped_cmd, vkz::fenced_command_pools&>);

    vkz::command_pool command_pool;
    vkz::ring_fences ring_fences;
    vkz::ring_command_pool ring_command_pool;
    vkz::batch_submission batch_submission;
    vkz::fenced_command_pools fenced_command_pools;
}
