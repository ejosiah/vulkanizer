/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2018-2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */

#include "vulkanizer/commands.hpp"

#include "vulkanizer/status.hpp"

#include <algorithm>
#include <array>
#include <cassert>

namespace vkz {
    VkPipelineStageFlags make_access_mask_pipeline_stage_flags(
            VkAccessFlags access_mask,
            VkPipelineStageFlags supported_shader_bits) {
        struct access_stage {
            VkAccessFlags access;
            VkPipelineStageFlags stages;
        };
        const access_stage access_stages[]{
            {VK_ACCESS_INDIRECT_COMMAND_READ_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT},
            {VK_ACCESS_INDEX_READ_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT},
            {VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT},
            {VK_ACCESS_UNIFORM_READ_BIT, supported_shader_bits},
            {VK_ACCESS_INPUT_ATTACHMENT_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT},
            {VK_ACCESS_SHADER_READ_BIT, supported_shader_bits},
            {VK_ACCESS_SHADER_WRITE_BIT, supported_shader_bits},
            {VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
            {VK_ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
            {VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
            {VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT},
            {VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT},
            {VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT},
            {VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT},
            {VK_ACCESS_HOST_READ_BIT, VK_PIPELINE_STAGE_HOST_BIT},
            {VK_ACCESS_HOST_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT},
            {VK_ACCESS_MEMORY_READ_BIT, 0},
            {VK_ACCESS_MEMORY_WRITE_BIT, 0},
#if VK_NV_device_generated_commands
            {VK_ACCESS_COMMAND_PREPROCESS_READ_BIT_NV, VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV},
            {VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_NV, VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV},
#endif
#if VK_NV_ray_tracing
            {VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV | supported_shader_bits | VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV},
            {VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV},
#endif
        };

        if (!access_mask) {
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        }

        VkPipelineStageFlags stages{};
        for (const auto& mapping : access_stages) {
            if (mapping.access & access_mask) {
                stages |= mapping.stages;
            }
        }
        assert(stages != 0);
        return stages;
    }

    void cmd_begin(VkCommandBuffer command_buffer, VkCommandBufferUsageFlags flags) {
        VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin_info.flags = flags;
        VKZ_CHECK_VULKAN(vkBeginCommandBuffer(command_buffer, &begin_info));
    }

    VkSubmitInfo make_submit_info(
            uint32_t command_buffer_count,
            const VkCommandBuffer* command_buffers,
            uint32_t signal_count,
            const VkSemaphore* signals) {
        VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit_info.commandBufferCount = command_buffer_count;
        submit_info.pCommandBuffers = command_buffers;
        submit_info.signalSemaphoreCount = signal_count;
        submit_info.pSignalSemaphores = signals;
        return submit_info;
    }

    command_pool::command_pool() = default;

    command_pool::command_pool(
            vkz::device device,
            uint32_t family_index,
            VkCommandPoolCreateFlags flags,
            VkQueue default_queue) {
        init(device, family_index, flags, default_queue);
    }

    command_pool::~command_pool() {
        deinit();
    }

    void command_pool::init(
            vkz::device device,
            uint32_t family_index,
            VkCommandPoolCreateFlags flags,
            VkQueue default_queue) {
        assert(!device_.logical);
        device_ = device;

        VkCommandPoolCreateInfo create_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        create_info.flags = flags;
        create_info.queueFamilyIndex = family_index;
        VKZ_CHECK_VULKAN(vkCreateCommandPool(device_, &create_info, nullptr, &command_pool_));

        if (default_queue) {
            queue_ = default_queue;
        } else {
            vkGetDeviceQueue(device_, family_index, 0, &queue_);
        }
    }

    void command_pool::deinit() {
        if (command_pool_) {
            vkDestroyCommandPool(device_, command_pool_, nullptr);
            command_pool_ = VK_NULL_HANDLE;
        }
        queue_ = VK_NULL_HANDLE;
        device_ = {};
    }

    VkCommandBuffer command_pool::create_command_buffer(
            VkCommandBufferLevel level,
            bool begin,
            VkCommandBufferUsageFlags flags,
            const VkCommandBufferInheritanceInfo* inheritance_info) {
        VkCommandBufferAllocateInfo allocate_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocate_info.level = level;
        allocate_info.commandPool = command_pool_;
        allocate_info.commandBufferCount = 1;

        VkCommandBuffer command_buffer{};
        VKZ_CHECK_VULKAN(vkAllocateCommandBuffers(device_, &allocate_info, &command_buffer));
        if (begin) {
            VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            begin_info.flags = flags;
            begin_info.pInheritanceInfo = inheritance_info;
            VKZ_CHECK_VULKAN(vkBeginCommandBuffer(command_buffer, &begin_info));
        }
        return command_buffer;
    }

    void command_pool::destroy(std::size_t count, const VkCommandBuffer* command_buffers) {
        vkFreeCommandBuffers(device_, command_pool_, static_cast<uint32_t>(count), command_buffers);
    }

    void command_pool::destroy(const std::vector<VkCommandBuffer>& command_buffers) {
        destroy(command_buffers.size(), command_buffers.data());
    }

    void command_pool::destroy(VkCommandBuffer command_buffer) {
        destroy(1, &command_buffer);
    }

    VkCommandPool command_pool::get_command_pool() const {
        return command_pool_;
    }

    void command_pool::submit(
            std::size_t count,
            const VkCommandBuffer* command_buffers,
            VkQueue queue,
            VkFence fence) {
        for (std::size_t i = 0; i < count; ++i) {
            VKZ_CHECK_VULKAN(vkEndCommandBuffer(command_buffers[i]));
        }
        auto submit_info = make_submit_info(static_cast<uint32_t>(count), command_buffers, 0, nullptr);
        VKZ_CHECK_VULKAN(vkQueueSubmit(queue, 1, &submit_info, fence));
    }

    void command_pool::submit(std::size_t count, const VkCommandBuffer* command_buffers, VkFence fence) {
        submit(count, command_buffers, queue_, fence);
    }

    void command_pool::submit(const std::vector<VkCommandBuffer>& command_buffers, VkFence fence) {
        submit(command_buffers.size(), command_buffers.data(), queue_, fence);
    }

    void command_pool::submit_and_wait(
            std::size_t count,
            const VkCommandBuffer* command_buffers,
            VkQueue queue) {
        submit(count, command_buffers, queue);
        VKZ_CHECK_VULKAN(vkQueueWaitIdle(queue));
        destroy(count, command_buffers);
    }

    void command_pool::submit_and_wait(const std::vector<VkCommandBuffer>& command_buffers, VkQueue queue) {
        submit_and_wait(command_buffers.size(), command_buffers.data(), queue);
    }

    void command_pool::submit_and_wait(VkCommandBuffer command_buffer, VkQueue queue) {
        submit_and_wait(1, &command_buffer, queue);
    }

    void command_pool::submit_and_wait(std::size_t count, const VkCommandBuffer* command_buffers) {
        submit_and_wait(count, command_buffers, queue_);
    }

    void command_pool::submit_and_wait(const std::vector<VkCommandBuffer>& command_buffers) {
        submit_and_wait(command_buffers.size(), command_buffers.data(), queue_);
    }

    void command_pool::submit_and_wait(VkCommandBuffer command_buffer) {
        submit_and_wait(1, &command_buffer, queue_);
    }

    scope_command_buffer::scope_command_buffer(vkz::device device, uint32_t family_index, VkQueue queue) {
        command_pool::init(device, family_index, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queue);
        command_buffer_ = create_command_buffer();
    }

    scope_command_buffer::~scope_command_buffer() {
        const auto end_result = vkEndCommandBuffer(command_buffer_);
        assert(end_result == VK_SUCCESS);
        const auto submit_info = make_submit_info(1, &command_buffer_, 0, nullptr);
        const auto submit_result = vkQueueSubmit(queue_, 1, &submit_info, VK_NULL_HANDLE);
        assert(submit_result == VK_SUCCESS);
        const auto wait_result = vkQueueWaitIdle(queue_);
        assert(wait_result == VK_SUCCESS);
        destroy(command_buffer_);
    }

    scope_command_buffer::operator VkCommandBuffer() const {
        return command_buffer_;
    }

    ring_fences::ring_fences() = default;

    ring_fences::ring_fences(vkz::device device, uint32_t ring_size) {
        init(device, ring_size);
    }

    ring_fences::~ring_fences() {
        deinit();
    }

    void ring_fences::init(vkz::device device, uint32_t ring_size) {
        assert(!device_);
        device_ = device;
        cycle_index_ = 0;
        cycle_size_ = ring_size;
        fences_.resize(ring_size);

        for (auto& entry : fences_) {
            VkFenceCreateInfo create_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            VKZ_CHECK_VULKAN(vkCreateFence(device_, &create_info, nullptr, &entry.fence));
            entry.active = false;
        }
    }

    void ring_fences::deinit() {
        if (!device_) {
            return;
        }
        for (const auto& entry : fences_) {
            vkDestroyFence(device_, entry.fence, nullptr);
        }
        fences_.clear();
        cycle_index_ = 0;
        cycle_size_ = 0;
        device_ = {};
    }

    void ring_fences::reset() {
        const auto device = device_;
        const auto ring_size = cycle_size_;
        deinit();
        init(device, ring_size);
    }

    void ring_fences::set_cycle_and_wait(uint32_t cycle) {
        cycle_index_ = cycle % cycle_size_;
        auto& entry = fences_[cycle_index_];
        if (entry.active) {
            VKZ_CHECK_VULKAN(vkWaitForFences(device_, 1, &entry.fence, VK_TRUE, UINT64_MAX));
            entry.active = false;
        }
        VKZ_CHECK_VULKAN(vkResetFences(device_, 1, &entry.fence));
    }

    VkFence ring_fences::get_fence() {
        fences_[cycle_index_].active = true;
        return fences_[cycle_index_].fence;
    }

    uint32_t ring_fences::get_cycle_index() const {
        return cycle_index_;
    }

    uint32_t ring_fences::get_cycle_size() const {
        return cycle_size_;
    }

    ring_command_pool::ring_command_pool() = default;

    ring_command_pool::ring_command_pool(
            VkDevice device,
            uint32_t queue_family_index,
            VkCommandPoolCreateFlags flags,
            uint32_t ring_size) {
        init(device, queue_family_index, flags, ring_size);
    }

    ring_command_pool::~ring_command_pool() {
        deinit();
    }

    void ring_command_pool::init(
            VkDevice device,
            uint32_t queue_family_index,
            VkCommandPoolCreateFlags flags,
            uint32_t ring_size) {
        assert(!device_);
        device_ = device;
        cycle_index_ = 0;
        cycle_size_ = ring_size;
        flags_ = flags;
        family_index_ = queue_family_index;
        pools_.resize(ring_size);

        for (auto& entry : pools_) {
            VkCommandPoolCreateInfo create_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
            create_info.queueFamilyIndex = family_index_;
            create_info.flags = flags_;
            VKZ_CHECK_VULKAN(vkCreateCommandPool(device_, &create_info, nullptr, &entry.pool));
        }
    }

    void ring_command_pool::deinit() {
        if (!device_) {
            return;
        }
        for (auto& entry : pools_) {
            if (!entry.command_buffers.empty()) {
                vkFreeCommandBuffers(device_, entry.pool, static_cast<uint32_t>(entry.command_buffers.size()), entry.command_buffers.data());
                entry.command_buffers.clear();
            }
            vkDestroyCommandPool(device_, entry.pool, nullptr);
        }
        pools_.clear();
        cycle_index_ = 0;
        cycle_size_ = 0;
        device_ = VK_NULL_HANDLE;
    }

    void ring_command_pool::reset() {
        const auto device = device_;
        const auto flags = flags_;
        const auto family_index = family_index_;
        const auto ring_size = cycle_size_;
        deinit();
        init(device, family_index, flags, ring_size);
    }

    void ring_command_pool::set_cycle(uint32_t cycle) {
        cycle_index_ = cycle % cycle_size_;
        auto& entry = pools_[cycle_index_];
        if (!entry.command_buffers.empty()) {
            vkFreeCommandBuffers(device_, entry.pool, static_cast<uint32_t>(entry.command_buffers.size()), entry.command_buffers.data());
            VKZ_CHECK_VULKAN(vkResetCommandPool(device_, entry.pool, 0));
            entry.command_buffers.clear();
        }
    }

    VkCommandBuffer ring_command_pool::create_command_buffer(
            VkCommandBufferLevel level,
            bool begin,
            VkCommandBufferUsageFlags flags,
            const VkCommandBufferInheritanceInfo* inheritance_info) {
        auto& cycle = pools_[cycle_index_];
        VkCommandBufferAllocateInfo allocate_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocate_info.commandBufferCount = 1;
        allocate_info.commandPool = cycle.pool;
        allocate_info.level = level;

        VkCommandBuffer command_buffer{};
        VKZ_CHECK_VULKAN(vkAllocateCommandBuffers(device_, &allocate_info, &command_buffer));
        cycle.command_buffers.push_back(command_buffer);

        if (begin) {
            VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            begin_info.flags = flags;
            begin_info.pInheritanceInfo = inheritance_info;
            VKZ_CHECK_VULKAN(vkBeginCommandBuffer(command_buffer, &begin_info));
        }
        return command_buffer;
    }

    const VkCommandBuffer* ring_command_pool::create_command_buffers(VkCommandBufferLevel level, uint32_t count) {
        auto& cycle = pools_[cycle_index_];
        VkCommandBufferAllocateInfo allocate_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocate_info.commandBufferCount = count;
        allocate_info.commandPool = cycle.pool;
        allocate_info.level = level;

        const auto begin = cycle.command_buffers.size();
        cycle.command_buffers.resize(begin + count);
        auto* command_buffers = cycle.command_buffers.data() + begin;
        VKZ_CHECK_VULKAN(vkAllocateCommandBuffers(device_, &allocate_info, command_buffers));
        return command_buffers;
    }

    batch_submission::batch_submission() = default;

    batch_submission::batch_submission(VkQueue queue) {
        init(queue);
    }

    uint32_t batch_submission::get_command_buffer_count() const {
        return static_cast<uint32_t>(command_buffers_.size());
    }

    VkQueue batch_submission::get_queue() const {
        return queue_;
    }

    void batch_submission::init(VkQueue queue) {
        assert(waits_.empty() && wait_flags_.empty() && signals_.empty() && command_buffers_.empty());
        queue_ = queue;
    }

    void batch_submission::enqueue(uint32_t count, const VkCommandBuffer* command_buffers) {
        command_buffers_.insert(command_buffers_.end(), command_buffers, command_buffers + count);
    }

    void batch_submission::enqueue(VkCommandBuffer command_buffer) {
        command_buffers_.push_back(command_buffer);
    }

    void batch_submission::enqueue_signal(VkSemaphore semaphore) {
        signals_.push_back(semaphore);
    }

    void batch_submission::enqueue_wait(VkSemaphore semaphore, VkPipelineStageFlags flags) {
        waits_.push_back(semaphore);
        wait_flags_.push_back(flags);
    }

    VkResult batch_submission::execute(VkFence fence, uint32_t device_mask) {
        VkResult result = VK_SUCCESS;
        if (queue_ && (fence || !command_buffers_.empty() || !signals_.empty() || !waits_.empty())) {
            VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submit_info.commandBufferCount = static_cast<uint32_t>(command_buffers_.size());
            submit_info.pCommandBuffers = command_buffers_.data();
            submit_info.signalSemaphoreCount = static_cast<uint32_t>(signals_.size());
            submit_info.pSignalSemaphores = signals_.data();
            submit_info.waitSemaphoreCount = static_cast<uint32_t>(waits_.size());
            submit_info.pWaitSemaphores = waits_.data();
            submit_info.pWaitDstStageMask = wait_flags_.data();

            std::vector<uint32_t> device_masks;
            std::vector<uint32_t> device_indices;
            VkDeviceGroupSubmitInfo device_group_info{VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO};
            if (device_mask) {
                device_masks.resize(command_buffers_.size(), device_mask);
                device_indices.resize(std::max(signals_.size(), waits_.size()), 0);
                submit_info.pNext = &device_group_info;
                device_group_info.commandBufferCount = submit_info.commandBufferCount;
                device_group_info.pCommandBufferDeviceMasks = device_masks.data();
                device_group_info.signalSemaphoreCount = submit_info.signalSemaphoreCount;
                device_group_info.pSignalSemaphoreDeviceIndices = device_indices.data();
                device_group_info.waitSemaphoreCount = submit_info.waitSemaphoreCount;
                device_group_info.pWaitSemaphoreDeviceIndices = device_indices.data();
            }

            result = vkQueueSubmit(queue_, 1, &submit_info, fence);
            command_buffers_.clear();
            waits_.clear();
            wait_flags_.clear();
            signals_.clear();
        }
        return result;
    }

    void batch_submission::wait_idle() const {
        VKZ_CHECK_VULKAN(vkQueueWaitIdle(queue_));
    }

    fenced_command_pools::fenced_command_pools() = default;

    fenced_command_pools::fenced_command_pools(
            vkz::device device,
            VkQueue queue,
            uint32_t queue_family_index,
            VkCommandPoolCreateFlags flags,
            uint32_t ring_size) {
        init(device, queue, queue_family_index, flags, ring_size);
    }

    fenced_command_pools::~fenced_command_pools() {
        deinit();
    }

    void fenced_command_pools::init(
            vkz::device device,
            VkQueue queue,
            uint32_t queue_family_index,
            VkCommandPoolCreateFlags flags,
            uint32_t ring_size) {
        ring_fences::init(device, ring_size);
        ring_command_pool::init(device, queue_family_index, flags, ring_size);
        batch_submission::init(queue);
    }

    void fenced_command_pools::deinit() {
        ring_fences::deinit();
        ring_command_pool::deinit();
    }

    void fenced_command_pools::reset() {
        wait_idle();
        ring_fences::reset();
        ring_command_pool::reset();
    }

    void fenced_command_pools::enqueue(uint32_t count, const VkCommandBuffer* command_buffers) {
        batch_submission::enqueue(count, command_buffers);
    }

    void fenced_command_pools::enqueue(VkCommandBuffer command_buffer) {
        batch_submission::enqueue(command_buffer);
    }

    void fenced_command_pools::enqueue_signal(VkSemaphore semaphore) {
        batch_submission::enqueue_signal(semaphore);
    }

    void fenced_command_pools::enqueue_wait(VkSemaphore semaphore, VkPipelineStageFlags flags) {
        batch_submission::enqueue_wait(semaphore, flags);
    }

    VkResult fenced_command_pools::execute(uint32_t device_mask) {
        return batch_submission::execute(ring_fences::get_fence(), device_mask);
    }

    void fenced_command_pools::wait_idle() const {
        batch_submission::wait_idle();
    }

    void fenced_command_pools::set_cycle_and_wait(uint32_t cycle) {
        ring_fences::set_cycle_and_wait(cycle);
        ring_command_pool::set_cycle(cycle);
    }

    VkCommandBuffer fenced_command_pools::create_command_buffer(
            VkCommandBufferLevel level,
            bool begin,
            VkCommandBufferUsageFlags flags,
            const VkCommandBufferInheritanceInfo* inheritance_info) {
        return ring_command_pool::create_command_buffer(level, begin, flags, inheritance_info);
    }

    const VkCommandBuffer* fenced_command_pools::create_command_buffers(VkCommandBufferLevel level, uint32_t count) {
        return ring_command_pool::create_command_buffers(level, count);
    }

    fenced_command_pools::scoped_cmd::scoped_cmd(fenced_command_pools& command_pools)
        : command_pools_{&command_pools}
        , command_buffer_{command_pools.create_command_buffer()} {
    }

    fenced_command_pools::scoped_cmd::~scoped_cmd() {
        const auto end_result = vkEndCommandBuffer(command_buffer_);
        assert(end_result == VK_SUCCESS);
        command_pools_->enqueue(command_buffer_);
        const auto submit_result = command_pools_->batch_submission::execute();
        assert(submit_result == VK_SUCCESS);
        const auto wait_result = vkQueueWaitIdle(command_pools_->batch_submission::get_queue());
        assert(wait_result == VK_SUCCESS);
    }

    fenced_command_pools::scoped_cmd::operator VkCommandBuffer() const {
        return command_buffer_;
    }
}
