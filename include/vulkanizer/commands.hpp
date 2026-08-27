/*
 * Copyright (c) 2014-2021, NVIDIA CORPORATION. All rights reserved.
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
 * SPDX-FileCopyrightText: Copyright (c) 2014-2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "vkz.hpp"
#include <volk.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace vkz {
    constexpr uint32_t default_ring_size = 3;

    [[nodiscard]] VkPipelineStageFlags make_access_mask_pipeline_stage_flags(
        VkAccessFlags access_mask,
        VkPipelineStageFlags supported_shader_bits =
            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
            VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
            VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT |
            VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT |
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    void cmd_begin(
        VkCommandBuffer command_buffer,
        VkCommandBufferUsageFlags flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    [[nodiscard]] VkSubmitInfo make_submit_info(
        uint32_t command_buffer_count,
        const VkCommandBuffer* command_buffers,
        uint32_t signal_count,
        const VkSemaphore* signals);

    class command_pool {
    public:
        command_pool();
        command_pool(
            vkz::device device,
            uint32_t family_index,
            VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            VkQueue default_queue = VK_NULL_HANDLE);
        ~command_pool();

        command_pool(const command_pool&) = delete;
        command_pool& operator=(const command_pool&) = delete;

        void init(
            vkz::device device,
            uint32_t family_index,
            VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            VkQueue default_queue = VK_NULL_HANDLE);
        void deinit();

        [[nodiscard]] VkCommandBuffer create_command_buffer(
            VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            bool begin = true,
            VkCommandBufferUsageFlags flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            const VkCommandBufferInheritanceInfo* inheritance_info = nullptr);

        void destroy(std::size_t count, const VkCommandBuffer* command_buffers);
        void destroy(const std::vector<VkCommandBuffer>& command_buffers);
        void destroy(VkCommandBuffer command_buffer);

        [[nodiscard]] VkCommandPool get_command_pool() const;

        void submit(
            std::size_t count,
            const VkCommandBuffer* command_buffers,
            VkQueue queue,
            VkFence fence = VK_NULL_HANDLE);
        void submit(
            std::size_t count,
            const VkCommandBuffer* command_buffers,
            VkFence fence = VK_NULL_HANDLE);
        void submit(const std::vector<VkCommandBuffer>& command_buffers, VkFence fence = VK_NULL_HANDLE);

        void submit_and_wait(std::size_t count, const VkCommandBuffer* command_buffers, VkQueue queue);
        void submit_and_wait(const std::vector<VkCommandBuffer>& command_buffers, VkQueue queue);
        void submit_and_wait(VkCommandBuffer command_buffer, VkQueue queue);
        void submit_and_wait(std::size_t count, const VkCommandBuffer* command_buffers);
        void submit_and_wait(const std::vector<VkCommandBuffer>& command_buffers);
        void submit_and_wait(VkCommandBuffer command_buffer);

    protected:
        vkz::device device_{};
        VkQueue queue_{VK_NULL_HANDLE};
        VkCommandPool command_pool_{VK_NULL_HANDLE};
    };

    class scope_command_buffer : public command_pool {
    public:
        scope_command_buffer(vkz::device device, uint32_t family_index, VkQueue queue = VK_NULL_HANDLE);
        ~scope_command_buffer();

        operator VkCommandBuffer() const;

    private:
        VkCommandBuffer command_buffer_{VK_NULL_HANDLE};
    };

    class ring_fences {
    public:
        ring_fences();
        explicit ring_fences(vkz::device device, uint32_t ring_size = default_ring_size);
        ~ring_fences();

        ring_fences(const ring_fences&) = delete;
        ring_fences& operator=(const ring_fences&) = delete;

        void init(vkz::device device, uint32_t ring_size = default_ring_size);
        void deinit();
        void reset();
        void set_cycle_and_wait(uint32_t cycle);

        [[nodiscard]] VkFence get_fence();
        [[nodiscard]] uint32_t get_cycle_index() const;
        [[nodiscard]] uint32_t get_cycle_size() const;

    private:
        struct entry {
            VkFence fence{VK_NULL_HANDLE};
            bool active{};
        };

        uint32_t cycle_index_{};
        uint32_t cycle_size_{};
        std::vector<entry> fences_;
        vkz::device device_{};
    };

    class ring_command_pool {
    public:
        ring_command_pool();
        ring_command_pool(
            VkDevice device,
            uint32_t queue_family_index,
            VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            uint32_t ring_size = default_ring_size);
        ~ring_command_pool();

        ring_command_pool(const ring_command_pool&) = delete;
        ring_command_pool& operator=(const ring_command_pool&) = delete;

        void init(
            VkDevice device,
            uint32_t queue_family_index,
            VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            uint32_t ring_size = default_ring_size);
        void deinit();
        void reset();
        void set_cycle(uint32_t cycle);

        [[nodiscard]] VkCommandBuffer create_command_buffer(
            VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            bool begin = true,
            VkCommandBufferUsageFlags flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            const VkCommandBufferInheritanceInfo* inheritance_info = nullptr);
        [[nodiscard]] const VkCommandBuffer* create_command_buffers(VkCommandBufferLevel level, uint32_t count);

    protected:
        struct entry {
            VkCommandPool pool{VK_NULL_HANDLE};
            std::vector<VkCommandBuffer> command_buffers;
        };

        uint32_t cycle_index_{};
        uint32_t cycle_size_{};
        std::vector<entry> pools_;
        VkDevice device_{VK_NULL_HANDLE};
        VkCommandPoolCreateFlags flags_{};
        uint32_t family_index_{};
    };

    class submission_batch {
    public:
        void enqueue(uint32_t count, const VkCommandBuffer* command_buffers);
        void enqueue(VkCommandBuffer command_buffer);
        void enqueue_signal(
            VkSemaphore semaphore,
            uint64_t value = 0,
            VkPipelineStageFlags2 stage_mask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
        void enqueue_wait(
            VkSemaphore semaphore,
            uint64_t value,
            VkPipelineStageFlags2 stage_mask);

        [[nodiscard]] uint32_t get_command_buffer_count() const;

    private:
        friend class batch_submission;

        struct semaphore {
            VkSemaphore handle{VK_NULL_HANDLE};
            uint64_t value{};
            VkPipelineStageFlags2 stage_mask{};
        };

        std::vector<VkCommandBuffer> command_buffers_;
        std::vector<semaphore> waits_;
        std::vector<semaphore> signals_;
    };

    class batch_submission {
    public:
        batch_submission();
        explicit batch_submission(VkQueue queue);

        batch_submission(const batch_submission&) = delete;
        batch_submission& operator=(const batch_submission&) = delete;

        [[nodiscard]] uint32_t get_command_buffer_count() const;
        [[nodiscard]] VkQueue get_queue() const;

        void init(VkQueue queue);
        void enqueue(uint32_t count, const VkCommandBuffer* command_buffers);
        void enqueue(VkCommandBuffer command_buffer);
        void enqueue_signal(VkSemaphore semaphore);
        void enqueue_wait(VkSemaphore semaphore, VkPipelineStageFlags flags);
        void enqueue_batch(submission_batch batch);
        VkResult execute(VkFence fence = VK_NULL_HANDLE, uint32_t device_mask = 0);
        void wait_idle() const;

    private:
        VkQueue queue_{VK_NULL_HANDLE};
        std::vector<VkSemaphore> waits_;
        std::vector<VkPipelineStageFlags> wait_flags_;
        std::vector<VkSemaphore> signals_;
        std::vector<VkCommandBuffer> command_buffers_;
        std::vector<submission_batch> batches_;
    };

    class fenced_command_pools : protected ring_fences, protected ring_command_pool, protected batch_submission {
    public:
        fenced_command_pools();
        fenced_command_pools(
            vkz::device device,
            VkQueue queue,
            uint32_t queue_family_index,
            VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            uint32_t ring_size = default_ring_size);
        ~fenced_command_pools();

        fenced_command_pools(const fenced_command_pools&) = delete;
        fenced_command_pools& operator=(const fenced_command_pools&) = delete;

        void init(
            vkz::device device,
            VkQueue queue,
            uint32_t queue_family_index,
            VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            uint32_t ring_size = default_ring_size);
        void deinit();
        void reset();

        void enqueue(uint32_t count, const VkCommandBuffer* command_buffers);
        void enqueue(VkCommandBuffer command_buffer);
        void enqueue_signal(VkSemaphore semaphore);
        void enqueue_wait(VkSemaphore semaphore, VkPipelineStageFlags flags);
        void enqueue_batch(submission_batch batch);
        VkResult execute(uint32_t device_mask = 0);
        void wait_idle() const;
        void set_cycle_and_wait(uint32_t cycle);

        [[nodiscard]] VkCommandBuffer create_command_buffer(
            VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            bool begin = true,
            VkCommandBufferUsageFlags flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            const VkCommandBufferInheritanceInfo* inheritance_info = nullptr);
        [[nodiscard]] const VkCommandBuffer* create_command_buffers(VkCommandBufferLevel level, uint32_t count);

        class scoped_cmd {
        public:
            explicit scoped_cmd(fenced_command_pools& command_pools);
            ~scoped_cmd();

            operator VkCommandBuffer() const;

        private:
            fenced_command_pools* command_pools_{};
            VkCommandBuffer command_buffer_{VK_NULL_HANDLE};
        };
    };
}
