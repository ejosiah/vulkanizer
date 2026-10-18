#pragma once

#include <vulkanizer/bvh.hpp>
#include <vulkanizer/barrier.hpp>
#include <vulkanizer/commands.hpp>
#include <vulkanizer/status.hpp>
#include <vector>
#include <memory>
#include <utility>

namespace vkz::detail {
    // Buffer wrappers in the public API are non-owning. These internal owners
    // make allocation failures and retained recording snapshots safe.
    struct bvh_buffer {
        buffer value;
        explicit bvh_buffer(buffer source = {}) : value(source) {}
        ~bvh_buffer() { value.destroy(); }
        bvh_buffer(const bvh_buffer &) = delete;
        bvh_buffer &operator=(const bvh_buffer &) = delete;
    };

    inline buffer allocate_bvh_buffer(vma_memory_allocator &allocator, VkDeviceSize size, buffer_usage usage) {
        return buffer::builder(allocator).size(size).usage(usage | buffer_usage_device_address)
            .memory_usage(VMA_MEMORY_USAGE_GPU_ONLY).build();
    }

    struct bvh_structure {
        VkDevice device{};
        acceleration_structure_view view;
        buffer storage;
        VkAccelerationStructureGeometryKHR geometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
        VkAccelerationStructureBuildSizesInfoKHR sizes{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
        uint32_t count{};

        bvh_structure() = default;
        bvh_structure(const bvh_structure &) = delete;
        bvh_structure &operator=(const bvh_structure &) = delete;
        bvh_structure(bvh_structure &&other) noexcept
            : device(other.device), view(std::exchange(other.view, {})), storage(std::exchange(other.storage, {})),
              geometry(other.geometry), sizes(other.sizes), count(other.count) {}
        ~bvh_structure() {
            if (view.handle) vkDestroyAccelerationStructureKHR(device, view.handle, nullptr);
            storage.destroy();
        }

        void create(vma_memory_allocator &allocator, const VkAccelerationStructureBuildGeometryInfoKHR &info) {
            device = allocator.device;
            vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &info, &count, &sizes);
            storage = allocate_bvh_buffer(allocator, sizes.accelerationStructureSize, buffer_usage_acceleration_structure_storage);
            VkAccelerationStructureCreateInfoKHR create{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
            create.buffer = storage;
            create.size = sizes.accelerationStructureSize;
            create.type = info.type;
            VKZ_CHECK_VULKAN(vkCreateAccelerationStructureKHR(device, &create, nullptr, &view.handle));
            VkAccelerationStructureDeviceAddressInfoKHR location{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
            location.accelerationStructure = view.handle;
            view.device_address = vkGetAccelerationStructureDeviceAddressKHR(device, &location);
        }
    };

    struct bvh_uploads {
        std::vector<std::unique_ptr<bvh_buffer>> snapshots;

        void record(VkCommandBuffer cmd, vma_memory_allocator &allocator, const buffer &destination,
                    std::span<const VkAccelerationStructureInstanceKHR> instances) {
            const auto size = instances.size_bytes();
            barrier::push_and_flush(cmd, pipeline_stage_acceleration_structure_build, pipeline_stage_transfer,
                                   access_acceleration_structure_read, access_transfer_write);
            if (size <= 65536) {
                // Vulkan copies pData into command-buffer storage at record time.
                vkCmdUpdateBuffer(cmd, destination, 0, size, instances.data());
            } else {
                auto snapshot = std::make_unique<bvh_buffer>();
                snapshot->value = buffer::builder(allocator).size(size).usage(buffer_usage_transfer_src)
                    .memory_usage(VMA_MEMORY_USAGE_CPU_ONLY).build();
                auto mapping = snapshot->value.map();
                std::memcpy(mapping.as<void>(), instances.data(), size);
                snapshot->value.flush();
                mapping.unmap();
                // Take ownership before recording commands that reference it.
                snapshots.push_back(std::move(snapshot));
                vkz::copy(cmd, snapshots.back()->value, destination);
            }
            barrier::push_and_flush(cmd, pipeline_stage_transfer, pipeline_stage_acceleration_structure_build,
                                   access_transfer_write, access_acceleration_structure_read);
        }
    };
}
