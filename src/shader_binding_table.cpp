#include <vulkanizer/ray_tracing_pipeline.hpp>
#include <vulkanizer/status.hpp>
#include <vulkanizer/commands.hpp>
#include <algorithm>
#include <cstring>
#include <utility>

namespace vkz {
    namespace {
        VkDeviceSize aligned(VkDeviceSize value, VkDeviceSize alignment) {
            return (value + alignment - 1) & ~(alignment - 1);
        }
    }

    shader_binding_tables::~shader_binding_tables() {
        destroy();
    }

    shader_binding_tables::shader_binding_tables(shader_binding_tables &&other) noexcept {
        move_from(std::move(other));
    }

    shader_binding_tables &shader_binding_tables::operator=(shader_binding_tables &&other) noexcept {
        if (this != &other) {
            destroy();
            move_from(std::move(other));
        }
        return *this;
    }

    void shader_binding_tables::move_from(shader_binding_tables &&other) {
        storage = std::exchange(other.storage, {});
        staging = std::exchange(other.staging, {});
        upload_pending = std::exchange(other.upload_pending, false);
        miss = std::exchange(other.miss, {});
        hit = std::exchange(other.hit, {});
        callable = std::exchange(other.callable, {});
        ray_generations = std::move(other.ray_generations);
        hit_names = std::move(other.hit_names);
        miss_names = std::move(other.miss_names);
        callable_names = std::move(other.callable_names);
    }

    void shader_binding_tables::destroy() {
        storage.destroy();
        staging.destroy();
        upload_pending = false;
        miss = {};
        hit = {};
        callable = {};
        ray_generations.clear();
        hit_names.clear();
        miss_names.clear();
        callable_names.clear();
    }

    void shader_binding_tables::upload(VkCommandBuffer command_buffer) const {
        if (!upload_pending) {
            return;
        }
        vkz::copy(command_buffer, staging, storage);
        barrier::push_and_flush(command_buffer, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
            VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT);
        upload_pending = false;
    }

    uint32_t shader_binding_tables::index(std::span<const std::string> names, std::string_view name) {
        const auto found = std::find(names.begin(), names.end(), name);
        VKZ_ASSERT(found != names.end(), "Unknown shader binding table record");
        return static_cast<uint32_t>(found - names.begin());
    }

    uint32_t shader_binding_tables::hit_index(std::string_view name) const {
        return index(hit_names, name);
    }

    uint32_t shader_binding_tables::miss_index(std::string_view name) const {
        return index(miss_names, name);
    }

    uint32_t shader_binding_tables::callable_index(std::string_view name) const {
        return index(callable_names, name);
    }

    const VkStridedDeviceAddressRegionKHR &shader_binding_tables::ray_generation(std::string_view name) const {
        auto found = std::find_if(ray_generations.begin(), ray_generations.end(), [&](const auto &value) {
            return value.name == name;
        });
        VKZ_ASSERT(found != ray_generations.end(), "Unknown ray generation record");
        return found->region;
    }

    shader_binding_tables shader_binding_tables::create(vma_memory_allocator &allocator, VkPipeline pipeline, std::span<const shader_group_record> records) {
        VKZ_ASSERT(pipeline && !records.empty(), "SBT requires a pipeline and records");
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
        VkPhysicalDeviceProperties2 physical{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &properties};
        vkGetPhysicalDeviceProperties2(allocator.device.physical, &physical);
        const VkDeviceSize handle_size = properties.shaderGroupHandleSize;
        const VkDeviceSize handle_alignment = properties.shaderGroupHandleAlignment;
        const VkDeviceSize base_alignment = properties.shaderGroupBaseAlignment;
        VKZ_ASSERT(handle_size && handle_alignment && base_alignment, "Ray tracing pipeline properties unavailable");
        shader_binding_tables result;

        struct region_plan {
            std::vector<const shader_group_record *> records;
            VkDeviceSize offset{};
            VkDeviceSize stride{};
        };

        std::vector<region_plan> plans;
        for (const auto &record : records) {
            if (record.type == shader_group_type::ray_generation) {
                plans.push_back({{&record}});
                result.ray_generations.push_back({record.name});
            }
        }
        VKZ_ASSERT(!plans.empty(), "SBT requires a ray generation record");
        const auto ray_count = plans.size();
        plans.resize(ray_count + 3);
        uint32_t group_count{};
        for (const auto &record : records) {
            VKZ_ASSERT(record.group_index < UINT32_MAX, "Shader group index exceeds Vulkan limits");
            group_count = std::max(group_count, record.group_index + 1);
            switch (record.type) {
            case shader_group_type::ray_generation:
                break;
            case shader_group_type::miss:
                plans[ray_count].records.push_back(&record);
                result.miss_names.push_back(record.name);
                break;
            case shader_group_type::triangles_hit:
            case shader_group_type::procedural_hit:
                plans[ray_count + 1].records.push_back(&record);
                result.hit_names.push_back(record.name);
                break;
            case shader_group_type::callable:
                plans[ray_count + 2].records.push_back(&record);
                result.callable_names.push_back(record.name);
                break;
            }
        }
        std::vector<uint8_t> handles(group_count * handle_size);
        VKZ_CHECK_VULKAN(vkGetRayTracingShaderGroupHandlesKHR(allocator.device, pipeline, 0, group_count, handles.size(), handles.data()));
        VkDeviceSize size{};
        for (auto &plan : plans) {
            for (const auto *record : plan.records)
                plan.stride = std::max(plan.stride, aligned(handle_size + record->data.size(), handle_alignment));
            VKZ_ASSERT(plan.stride <= properties.maxShaderGroupStride, "Shader record exceeds maxShaderGroupStride");
            plan.offset = aligned(size, base_alignment);
            size = plan.offset + plan.stride * plan.records.size();
        }
        result.storage = buffer::builder(allocator)
                .size(size + base_alignment - 1)
                .usage(VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)
                .memory_usage(VMA_MEMORY_USAGE_GPU_ONLY)
                .build();
        VkBufferDeviceAddressInfo address_info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
        address_info.buffer = result.storage;
        const auto buffer_address = vkGetBufferDeviceAddress(allocator.device, &address_info);
        VKZ_ASSERT(buffer_address, "SBT allocation requires buffer device address support");
        const auto base = aligned(buffer_address, base_alignment);
        const auto padding = base - buffer_address;
        result.staging = buffer::builder(allocator)
            .size(result.storage.create_info.size)
            .usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
            .memory_usage(VMA_MEMORY_USAGE_CPU_ONLY)
            .build();
        auto mapping = result.staging.map();
        auto *mapped = mapping.as<void>();
        std::memset(mapped, 0, static_cast<size_t>(result.storage.create_info.size));
        for (size_t i = 0; i < plans.size(); ++i) {
            const auto &plan = plans[i];
            VkStridedDeviceAddressRegionKHR region{};
            if (!plan.records.empty()) region = {base + plan.offset, plan.stride, plan.stride * plan.records.size()};
            if (i < ray_count)
                result.ray_generations[i].region = region;
            else if (i == ray_count)
                result.miss = region;
            else if (i == ray_count + 1)
                result.hit = region;
            else
                result.callable = region;
            auto *destination = static_cast<uint8_t *>(mapped) + padding + plan.offset;
            for (const auto *record : plan.records) {
                std::memcpy(destination, handles.data() + record->group_index * handle_size, static_cast<size_t>(handle_size));
                if (!record->data.empty()) std::memcpy(destination + handle_size, record->data.data(), record->data.size());
                destination += plan.stride;
            }
        }
        result.staging.flush();
        mapping.unmap();
        result.upload_pending = true;
        return result;
    }

    ray_tracing_pipeline::ray_tracing_pipeline() {
        bind_point = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
    }

    ray_tracing_pipeline::~ray_tracing_pipeline() {
        destroy();
    }

    ray_tracing_pipeline::ray_tracing_pipeline(ray_tracing_pipeline &&other) noexcept {
        *this = std::move(other);
    }

    ray_tracing_pipeline &ray_tracing_pipeline::operator=(ray_tracing_pipeline &&other) noexcept {
        if (this != &other) {
            destroy();
            static_cast<pipeline &>(*this) = std::move(static_cast<pipeline &>(other));
            other.handle = {};
            other.layout = {};
            sbt = std::move(other.sbt);
        }
        return *this;
    }

    void ray_tracing_pipeline::destroy() {
        sbt.destroy();
        pipeline::destroy();
        handle = {};
        layout = {};
        descriptor_sets.clear();
    }
}
