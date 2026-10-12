#include <vulkanizer/bvh.hpp>
#include <vulkanizer/barrier.hpp>
#include <vulkanizer/commands.hpp>
#include <vulkanizer/status.hpp>
#include <algorithm>
#include <cmath>

namespace vkz::mesh {
    namespace {
        VkDeviceAddress address(VkDevice device, const buffer &value) {
            VkBufferDeviceAddressInfo info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
            info.buffer = value;
            return vkGetBufferDeviceAddress(device, &info);
        }

        void build_barrier(VkCommandBuffer cmd) {
            barrier::push_and_flush(cmd, vkz::pipeline::stage::all_commands, vkz::pipeline::stage::acceleration_structure_build, vkz::access::memory_write | vkz::access::acceleration_structure_read,
                                    vkz::access::shader_read | vkz::access::acceleration_structure_read | vkz::access::acceleration_structure_write);
        }
    }

    struct bvh::impl {
        struct structure {
            acceleration_structure_view view;
            buffer storage;
            VkAccelerationStructureGeometryKHR geometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
            VkAccelerationStructureBuildSizesInfoKHR sizes{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
            uint32_t count{};
        };

        vma_memory_allocator &allocator;
        VkDevice device;
        bvh_build_options options;
        VkPhysicalDeviceAccelerationStructurePropertiesKHR properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR};
        std::vector<structure> bottom;
        structure top;
        buffer scratch;
        buffer instance_buffer;
        buffer instance_staging;
        VkDeviceAddress scratch_address{};
        std::vector<uint32_t> mesh_ids;
        std::vector<VkAccelerationStructureInstanceKHR> instance_data;
        std::vector<base> meshes;
        vertex_input streams;

        explicit impl(vma_memory_allocator &value) : allocator(value), device(value.device) {
        }

        ~impl() {
            release(top);
            for (auto &item : bottom)
                release(item);
            scratch.destroy();
            instance_buffer.destroy();
            instance_staging.destroy();
        }

        void release(structure &value) {
            if (value.view.handle) vkDestroyAccelerationStructureKHR(device, value.view.handle, nullptr);
            value.storage.destroy();
        }

        buffer allocate(VkDeviceSize size, vkz::buffer::usage usage, VmaMemoryUsage memory = VMA_MEMORY_USAGE_GPU_ONLY) {
            return buffer::builder(allocator)
                .size(size)
                .usage(usage | vkz::buffer::usage::device_address)
                .memory_usage(memory)
                .build();
        }

        VkAccelerationStructureBuildGeometryInfoKHR build_info(structure &value, bool is_top) {
            VkAccelerationStructureBuildGeometryInfoKHR info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
            info.type = is_top ? VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR : VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            info.flags = static_cast<VkBuildAccelerationStructureFlagsKHR>(vkz::acceleration_structure_build::prefer_fast_trace);
            if (is_top && options.allow_instance_updates) info.flags |= static_cast<VkBuildAccelerationStructureFlagsKHR>(vkz::acceleration_structure_build::allow_update);
            info.geometryCount = 1;
            info.pGeometries = &value.geometry;
            info.dstAccelerationStructure = value.view.handle;
            info.scratchData.deviceAddress = scratch_address;
            return info;
        }

        void query_properties() {
            VKZ_ASSERT(vkCreateAccelerationStructureKHR && vkDestroyAccelerationStructureKHR && vkGetAccelerationStructureBuildSizesKHR && vkGetAccelerationStructureDeviceAddressKHR &&
                           vkCmdBuildAccelerationStructuresKHR && vkGetBufferDeviceAddress,
                       "Acceleration structure and buffer device address features must be enabled");
            VkPhysicalDeviceProperties2 info{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
            info.pNext = &properties;
            vkGetPhysicalDeviceProperties2(allocator.device.physical, &info);
        }

        void prepare_geometry(const data &scene, const geometry_buffers &buffers) {
            VKZ_ASSERT(!scene.meshes.empty() && !scene.instances.empty(), "BVH requires meshes and instances");
            const auto usage = vkz::buffer::usage::acceleration_input;
            VKZ_ASSERT(buffers.vertices._ && buffers.indices._ && has_flag(static_cast<buffer::usage>(buffers.vertices.create_info.usage), usage) &&
                           has_flag(static_cast<buffer::usage>(buffers.indices.create_info.usage), usage),
                       "BVH buffers require build-input and device-address usage");
            VKZ_ASSERT(buffers.vertices.create_info.size >= scene.vertex_data.size() && buffers.indices.create_info.size >= scene.index_data.size() * sizeof(uint32_t), "BVH buffers are smaller than scene data");
            auto attribute = std::find_if(scene.streams.attributes.begin(), scene.streams.attributes.end(), [](const auto &value) { return value.location == 0; });
            VKZ_ASSERT(attribute != scene.streams.attributes.end() && attribute->format == VK_FORMAT_R32G32B32_SFLOAT, "BVH requires float3 positions at location zero");
            auto binding = std::find_if(scene.streams.bindings.begin(), scene.streams.bindings.end(), [&](const auto &value) {return value.binding == attribute->binding; });
            VKZ_ASSERT(scene.streams.bindings.size() == 1 && binding != scene.streams.bindings.end() && binding->inputRate == VK_VERTEX_INPUT_RATE_VERTEX && binding->stride % 4 == 0 &&
                           uint64_t(attribute->offset) + 12 <= binding->stride && attribute->offset % 4 == 0,
                       "BVH requires one interleaved vertex stream with aligned float3 positions");
            VkFormatProperties format{};
            vkGetPhysicalDeviceFormatProperties(allocator.device.physical, attribute->format, &format);
            VKZ_ASSERT(format.bufferFeatures & static_cast<VkFormatFeatureFlags>(vkz::format_feature::acceleration_structure_vertex_buffer), "Device does not support float3 acceleration structure vertices");

            const auto vertices = address(device, buffers.vertices);
            const auto indices = address(device, buffers.indices);
            VKZ_ASSERT(vertices && indices && vertices % 4 == 0 && indices % 4 == 0, "Invalid geometry device addresses");
            bottom.resize(scene.meshes.size());

            for (size_t i = 0; i < bottom.size(); ++i) {
                const auto &mesh = scene.meshes[i];
                VKZ_ASSERT(mesh.lod_count <= max_lods && options.lod < mesh.lod_count && mesh.vertex_count, "Invalid mesh LOD or vertex count");
                const uint64_t begin = uint64_t(mesh.index_offset) + mesh.lod_offset[options.lod];
                const uint64_t end = uint64_t(mesh.index_offset) + mesh.lod_offset[options.lod + 1];
                VKZ_ASSERT(end > begin && end <= scene.index_data.size() && (end - begin) % 3 == 0, "BVH mesh must contain indexed triangles");
                VKZ_ASSERT((uint64_t(mesh.vertex_offset) + mesh.vertex_count) * binding->stride <= scene.vertex_data.size(), "Mesh vertices exceed the vertex stream");

                for (uint64_t j = begin; j < end; ++j) {
                    VKZ_ASSERT(scene.index_data[j] < mesh.vertex_count, "Mesh index exceeds its vertex count");
                }

                VKZ_ASSERT((end - begin) / 3 <= properties.maxPrimitiveCount && (end - begin) / 3 <= std::numeric_limits<uint32_t>::max(), "Mesh exceeds primitive count limit");

                auto &value = bottom[i];
                value.count = static_cast<uint32_t>((end - begin) / 3);
                value.geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;

                auto &triangles = value.geometry.geometry.triangles;
                triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
                triangles.vertexFormat = attribute->format;
                triangles.vertexData.deviceAddress = vertices + uint64_t(mesh.vertex_offset) * binding->stride + attribute->offset;
                triangles.vertexStride = binding->stride;
                triangles.maxVertex = mesh.vertex_count - 1;
                triangles.indexType = VK_INDEX_TYPE_UINT32;
                triangles.indexData.deviceAddress = indices + begin * sizeof(uint32_t);
            }
            meshes = scene.meshes;
            streams = scene.streams;
        }

        void create_structure(structure &value, bool is_top) {
            auto info = build_info(value, is_top);
            vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &info, &value.count, &value.sizes);
            value.storage = allocate(value.sizes.accelerationStructureSize, vkz::buffer::usage::acceleration_structure_storage);
            VkAccelerationStructureCreateInfoKHR create{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
            create.buffer = value.storage;
            create.size = value.sizes.accelerationStructureSize;
            create.type = info.type;
            VKZ_CHECK_VULKAN(vkCreateAccelerationStructureKHR(device, &create, nullptr, &value.view.handle));
            VkAccelerationStructureDeviceAddressInfoKHR location{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
            location.accelerationStructure = value.view.handle;
            value.view.device_address = vkGetAccelerationStructureDeviceAddressKHR(device, &location);
        }

        void create_blas() {
            for (auto &value : bottom)
                create_structure(value, false);
        }

        std::vector<VkAccelerationStructureInstanceKHR> prepare_instances(const data &scene, std::span<const bvh_instance_options> settings) {
            VKZ_ASSERT(settings.empty() || settings.size() == scene.instances.size(), "Instance options count mismatch");
            VKZ_ASSERT(scene.instances.size() <= 0x1000000 && scene.instances.size() <= properties.maxInstanceCount, "Too many BVH instances");
            std::vector<VkAccelerationStructureInstanceKHR> result(scene.instances.size());
            for (size_t i = 0; i < result.size(); ++i) {
                const auto &source = scene.instances[i];
                VKZ_ASSERT(source.mesh_id < bottom.size(), "Instance references an invalid mesh");
                const auto material_id = source.material_id == inherit_material ? meshes[source.mesh_id].material_id : source.material_id;
                VKZ_ASSERT(material_id < scene.materials.size(), "Instance references an invalid material");
                const auto &material = scene.materials[material_id];
                const auto settings_value = settings.empty() ? bvh_instance_options{} : settings[i];
                VKZ_ASSERT(settings_value.shader_binding_table_offset <= 0xffffff, "SBT offset exceeds 24 bits");
                const auto determinant = glm::determinant(glm::mat3(source.model));
                VKZ_ASSERT(std::isfinite(determinant) && determinant != 0 && source.model[0][3] == 0 && source.model[1][3] == 0 && source.model[2][3] == 0 && source.model[3][3] == 1,
                           "Instance transform must be invertible and affine");
                auto &target = result[i];
                for (int row = 0; row < 3; ++row)
                    for (int column = 0; column < 4; ++column) {
                        VKZ_ASSERT(std::isfinite(source.model[column][row]), "Nonfinite instance transform");
                        target.transform.matrix[row][column] = source.model[column][row];
                    }
                target.instanceCustomIndex = static_cast<uint32_t>(i);
                target.mask = settings_value.mask;
                target.instanceShaderBindingTableRecordOffset = settings_value.shader_binding_table_offset;
                target.accelerationStructureReference = bottom[source.mesh_id].view.device_address;
                if (material.alpha_mode == alpha_mode::opaque) target.flags |= static_cast<VkGeometryInstanceFlagsKHR>(vkz::geometry_instance::force_opaque);
                if (material.double_sided || material.thickness_factor > 0) target.flags |= static_cast<VkGeometryInstanceFlagsKHR>(vkz::geometry_instance::triangle_facing_cull_disable);
            }
            return result;
        }

        void upload_instances(VkCommandBuffer cmd) {
            auto mapping = instance_staging.map();
            std::memcpy(mapping.as<void>(), instance_data.data(), VKZ_BYTE_SIZE(instance_data));
            instance_staging.flush();
            mapping.unmap();
            barrier::push_and_flush(cmd, vkz::pipeline::stage::acceleration_structure_build, vkz::pipeline::stage::transfer, vkz::access::acceleration_structure_read, vkz::access::transfer_write);
            vkz::copy(cmd, instance_staging, instance_buffer);
            barrier::push_and_flush(cmd, vkz::pipeline::stage::transfer, vkz::pipeline::stage::acceleration_structure_build, vkz::access::transfer_write, vkz::access::acceleration_structure_read);
        }

        void create_tlas(const data &scene, std::span<const bvh_instance_options> settings) {
            instance_data = prepare_instances(scene, settings);
            for (const auto &value : scene.instances)
                mesh_ids.push_back(value.mesh_id);
            instance_buffer = allocate(VKZ_BYTE_SIZE(instance_data), vkz::buffer::usage::acceleration_structure_input | vkz::buffer::usage::transfer_dst);
            instance_staging = buffer::builder(allocator)
                .size(VKZ_BYTE_SIZE(instance_data))
                .usage(vkz::buffer::usage::transfer_src)
                .memory_usage(VMA_MEMORY_USAGE_CPU_ONLY)
                .build();
            top.count = VKZ_COUNT(instance_data);
            top.geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
            top.geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
            top.geometry.geometry.instances.data.deviceAddress = address(device, instance_buffer);
            VKZ_ASSERT(top.geometry.geometry.instances.data.deviceAddress % 16 == 0, "Unaligned instance buffer");
            create_structure(top, true);
        }

        void create_scratch_buffer() {
            auto size = std::max(top.sizes.buildScratchSize, top.sizes.updateScratchSize);
            for (const auto &value : bottom)
                size = std::max(size, value.sizes.buildScratchSize);
            const VkDeviceSize alignment = properties.minAccelerationStructureScratchOffsetAlignment;
            scratch = allocate(size + alignment, vkz::buffer::usage::storage);
            scratch_address = (address(device, scratch) + alignment - 1) & ~(alignment - 1);
        }

        void record_build(VkCommandBuffer cmd, structure &value, bool is_top, bool update = false) {
            build_barrier(cmd);
            auto info = build_info(value, is_top);
            info.mode = update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
            info.srcAccelerationStructure = update ? value.view.handle : VK_NULL_HANDLE;
            VkAccelerationStructureBuildRangeInfoKHR range{value.count, 0, 0, 0};
            const auto *ranges = &range;
            vkCmdBuildAccelerationStructuresKHR(cmd, 1, &info, &ranges);
        }

        void finish_build(VkCommandBuffer cmd) {
            barrier::push_and_flush(cmd, vkz::pipeline::stage::acceleration_structure_build, vkz::pipeline::stage::all_commands, vkz::access::acceleration_structure_write,
                                    vkz::access::acceleration_structure_read);
        }

        void initialize(const data &scene, const geometry_buffers &geometry, std::span<const bvh_instance_options> settings) {
            query_properties();
            prepare_geometry(scene, geometry);
            create_blas();
            create_tlas(scene, settings);
            create_scratch_buffer();
        }
    };

    bvh::bvh(vma_memory_allocator &value) : allocator(&value) {
    }

    bvh::~bvh() = default;
    bvh::bvh(bvh &&) noexcept = default;
    bvh &bvh::operator=(bvh &&) noexcept = default;

    void bvh::build(VkCommandBuffer cmd, const data &scene, const geometry_buffers &geometry, const bvh_build_options &options, std::span<const bvh_instance_options> instances) {
        VKZ_ASSERT(cmd != VK_NULL_HANDLE, "BVH build requires a command buffer");
        auto next = std::make_unique<impl>(*allocator);
        next->options = options;
        next->initialize(scene, geometry, instances);
        next->upload_instances(cmd);
        for (auto &value : next->bottom)
            next->record_build(cmd, value, false);
        next->record_build(cmd, next->top, true);
        next->finish_build(cmd);
        state = std::move(next);
    }

    void bvh::update_instances(VkCommandBuffer cmd, const data &scene, std::span<const bvh_instance_options> instances) {
        VKZ_ASSERT(cmd && state && state->options.allow_instance_updates, "BVH was not built with instance updates enabled");
        VKZ_ASSERT(scene.instances.size() == state->mesh_ids.size(), "Instance count changed; rebuild the BVH");
        VKZ_ASSERT(scene.meshes.size() == state->meshes.size(), "Mesh count changed; rebuild the BVH");
        VKZ_ASSERT(scene.streams.bindings.size() == state->streams.bindings.size() && scene.streams.attributes.size() == state->streams.attributes.size(), "Vertex streams changed; rebuild the BVH");
        for (size_t i = 0; i < scene.streams.bindings.size(); ++i) {
            const auto &a = scene.streams.bindings[i];
            const auto &b = state->streams.bindings[i];
            VKZ_ASSERT(a.binding == b.binding && a.stride == b.stride && a.inputRate == b.inputRate, "Vertex binding changed; rebuild the BVH");
        }
        for (size_t i = 0; i < scene.streams.attributes.size(); ++i) {
            const auto &a = scene.streams.attributes[i];
            const auto &b = state->streams.attributes[i];
            VKZ_ASSERT(a.location == b.location && a.binding == b.binding && a.format == b.format && a.offset == b.offset, "Vertex attribute changed; rebuild the BVH");
        }
        for (size_t i = 0; i < scene.instances.size(); ++i)
            VKZ_ASSERT(scene.instances[i].mesh_id == state->mesh_ids[i], "Instance mesh changed; rebuild the BVH");
        for (size_t i = 0; i < scene.meshes.size(); ++i) {
            const auto &a = scene.meshes[i];
            const auto &b = state->meshes[i];
            VKZ_ASSERT(a.vertex_offset == b.vertex_offset && a.vertex_count == b.vertex_count && a.index_offset == b.index_offset && a.lod_count == b.lod_count && a.lod_offset == b.lod_offset &&
                           a.material_id == b.material_id,
                       "Mesh layout changed; rebuild the BVH");
        }
        auto next = state->prepare_instances(scene, instances);
        state->instance_data = std::move(next);
        state->upload_instances(cmd);
        state->record_build(cmd, state->top, true, true);
        state->finish_build(cmd);
    }

    acceleration_structure_view bvh::tlas() const {
        return state ? state->top.view : acceleration_structure_view{};
    }

    acceleration_structure_view bvh::blas(uint32_t mesh_id) const {
        VKZ_ASSERT(state, "BVH has not been built");
        VKZ_ASSERT(mesh_id < state->bottom.size(), "BVH mesh index is out of range");
        return state->bottom[mesh_id].view;
    }

    void bvh::destroy() {
        state.reset();
    }
}
