#include <vulkanizer/bvh.hpp>
#include "bvh_commands.hpp"
#include "bvh_resources.hpp"
#include <vulkanizer/commands.hpp>
#include <vulkanizer/barrier.hpp>
#include <vulkanizer/status.hpp>
#include <cmath>
#include <algorithm>

namespace vkz {
    namespace {
        void require_update(bool condition, const char *message) {
            if (!condition) VKZ_THROW(message);
        }

        VkDeviceAddress address(VkDevice device, const buffer &value) {
            VkBufferDeviceAddressInfo info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
            info.buffer = value;
            return vkGetBufferDeviceAddress(device, &info);
        }

    } // namespace

    struct bvh::impl {
        using structure = detail::bvh_structure;

        vma_memory_allocator &allocator;
        VkDevice device;
        bvh_build_options options;
        VkPhysicalDeviceAccelerationStructurePropertiesKHR properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR};
        std::vector<structure> bottom;
        structure top;
        buffer scratch;
        buffer instance_buffer;
        detail::bvh_uploads uploads;
        std::unique_ptr<impl> previous;
        VkDeviceAddress scratch_address{};
        std::vector<uint32_t> mesh_ids;
        bool gpu_instances{};
        std::vector<VkAccelerationStructureInstanceKHR> instance_data;
        std::vector<vkz::bvh::scene::geometry> geometries;

        explicit impl(vma_memory_allocator &value) : allocator(value), device(value.device) {
        }

        ~impl() {
            scratch.destroy();
            instance_buffer.destroy();
            // Release the retirement chain iteratively to avoid recursive
            // destruction after many rebuilds.
            while (previous) {
                auto item = std::move(previous);
                previous = std::move(item->previous);
            }
        }

        buffer allocate(VkDeviceSize size, vkz::buffer_usage usage) {
            return detail::allocate_bvh_buffer(allocator, size, usage);
        }

        VkAccelerationStructureBuildGeometryInfoKHR build_info(structure &value, bool is_top) {
            VkAccelerationStructureBuildGeometryInfoKHR info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
            info.type = is_top ? VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR : VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            info.flags = static_cast<VkBuildAccelerationStructureFlagsKHR>(vkz::acceleration_structure_build_prefer_fast_trace);
            if (is_top && options.allow_instance_updates) info.flags |= static_cast<VkBuildAccelerationStructureFlagsKHR>(vkz::acceleration_structure_build_allow_update);
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

        void prepare_geometry(const vkz::bvh::scene &scene) {
            VKZ_ASSERT(scene.geometry_count() && scene.instance_count(), "BVH requires meshes and instances");
            VkFormatProperties format{};
            vkGetPhysicalDeviceFormatProperties(allocator.device.physical, VK_FORMAT_R32G32B32_SFLOAT, &format);
            VKZ_ASSERT(format.bufferFeatures & static_cast<VkFormatFeatureFlags>(vkz::format_feature_acceleration_structure_vertex_buffer),
                       "Device does not support float3 acceleration structure vertices");
            bottom.resize(scene.geometry_count());
            for (size_t i = 0; i < bottom.size(); ++i) {
                const auto source = scene.geometry_at(i);
                const auto usage = vkz::buffer_usage_acceleration_input;
                VKZ_ASSERT(source.vertices._ && source.indices._ && has_flag(static_cast<vkz::buffer_usage>(source.vertices.create_info.usage), usage) &&
                               has_flag(static_cast<vkz::buffer_usage>(source.indices.create_info.usage), usage),
                           "BVH buffers require build-input and device-address usage");
                VKZ_ASSERT(source.vertex_count && source.vertex_stride >= 12 && source.vertex_stride % 4 == 0 && source.vertex_offset % 4 == 0 && source.index_offset % 4 == 0,
                           "Invalid triangle geometry layout");
                VKZ_ASSERT(source.vertex_offset <= source.vertices.create_info.size && source.vertices.create_info.size - source.vertex_offset >= 12 &&
                               uint64_t(source.vertex_count - 1) <= (source.vertices.create_info.size - source.vertex_offset - 12) / source.vertex_stride,
                           "Mesh vertices exceed the vertex buffer");
                VKZ_ASSERT(source.primitive_count && source.primitive_count <= properties.maxPrimitiveCount && source.index_offset <= source.indices.create_info.size &&
                               uint64_t(source.primitive_count) * 12 <= source.indices.create_info.size - source.index_offset,
                           "Invalid triangle index range");
                const auto vertices = address(device, source.vertices);
                const auto indices = address(device, source.indices);
                VKZ_ASSERT(vertices && indices && vertices % 4 == 0 && indices % 4 == 0, "Invalid geometry device addresses");
                auto &value = bottom[i];
                value.count = source.primitive_count;
                value.geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
                auto &triangles = value.geometry.geometry.triangles;
                triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
                triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
                triangles.vertexData.deviceAddress = vertices + source.vertex_offset;
                triangles.vertexStride = source.vertex_stride;
                triangles.maxVertex = source.vertex_count - 1;
                triangles.indexType = VK_INDEX_TYPE_UINT32;
                triangles.indexData.deviceAddress = indices + source.index_offset;
                geometries.push_back(source);
            }
        }

        void create_structure(structure &value, bool is_top) {
            value.create(allocator, build_info(value, is_top));
        }

        void create_blas() {
            for (auto &value : bottom)
                create_structure(value, false);
        }

        std::vector<VkAccelerationStructureInstanceKHR> prepare_instances(const vkz::bvh::scene &scene) {
            VKZ_ASSERT(scene.instance_count() <= 0x1000000 && scene.instance_count() <= properties.maxInstanceCount, "Too many BVH instances");
            std::vector<VkAccelerationStructureInstanceKHR> result(scene.instance_count());
            for (size_t i = 0; i < result.size(); ++i) {
                const auto source = scene.instance_at(i);
                VKZ_ASSERT(source.mesh_id < bottom.size(), "Instance references an invalid mesh");
                const auto settings_value = source.options;
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
                target.flags = source.flags;
            }
            return result;
        }

        void upload_instances(VkCommandBuffer cmd, const vkz::bvh::scene &scene) {
            if (gpu_instances) {
                std::vector<acceleration_structure_view> views;
                for (const auto &value : bottom) {
                    views.push_back(value.view);
                }
                scene.record_instances(cmd, instance_buffer, views);
                barrier::push_and_flush(cmd, pipeline_stage_compute_shader | pipeline_stage_transfer, pipeline_stage_acceleration_structure_build, access_shader_write | access_transfer_write,
                                        access_acceleration_structure_read);
            } else {
                uploads.record(cmd, allocator, instance_buffer, instance_data);
            }
        }

        void create_tlas(const vkz::bvh::scene &scene) {
            VKZ_ASSERT(scene.instance_count() <= 0x1000000 && scene.instance_count() <= properties.maxInstanceCount, "Too many BVH instances");
            gpu_instances = scene.gpu_instances();
            if (!gpu_instances) {
                instance_data = prepare_instances(scene);
                for (size_t i = 0; i < scene.instance_count(); ++i) {
                    mesh_ids.push_back(scene.instance_at(i).mesh_id);
                }
            }
            instance_buffer = allocate(scene.instance_count() * sizeof(VkAccelerationStructureInstanceKHR),
                                       vkz::buffer_usage_acceleration_structure_input | vkz::buffer_usage_transfer_dst | vkz::buffer_usage_storage);
            top.count = static_cast<uint32_t>(scene.instance_count());
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
            scratch = allocate(size + alignment, vkz::buffer_usage_storage);
            scratch_address = (address(device, scratch) + alignment - 1) & ~(alignment - 1);
        }

        void record_build(VkCommandBuffer cmd, structure &value, bool is_top, bool update = false) {
            detail::record_bvh_build(cmd, build_info(value, is_top), value.count, update);
        }

        void finish_build(VkCommandBuffer cmd) {
            detail::finish_bvh_build(cmd);
        }

        void initialize(const vkz::bvh::scene &scene) {
            query_properties();
            scene.validate_geometry();
            prepare_geometry(scene);
            create_blas();
            create_tlas(scene);
            create_scratch_buffer();
        }
    };

    bvh::bvh(vma_memory_allocator &value) : allocator(&value) {
    }

    bvh::~bvh() {
        // Device loss also ends pending GPU access; destructors cannot throw.
        (void)wait_for_resources();
    }

    bvh::bvh(bvh &&other) noexcept : allocator(other.allocator), state(std::move(other.state)), retired(std::move(other.retired)) {
    }

    bvh &bvh::operator=(bvh &&other) noexcept {
        if (this == &other) return *this;
        retire_state();
        if (other.retired) {
            auto *tail = other.retired.get();
            while (tail->previous)
                tail = tail->previous.get();
            tail->previous = std::move(retired);
            retired = std::move(other.retired);
        }
        allocator = other.allocator;
        state = std::move(other.state);
        return *this;
    }

    void bvh::retire_state() noexcept {
        if (!state) return;
        state->previous = std::move(retired);
        retired = std::move(state);
    }

    VkResult bvh::wait_for_resources() const noexcept {
        auto status = VK_SUCCESS;
        VkDevice last_device{};
        if (state) {
            const auto result = vkDeviceWaitIdle(state->device);
            last_device = state->device;
            if (result != VK_SUCCESS) status = result;
        }
        for (auto *item = retired.get(); item; item = item->previous.get()) {
            if (item->device == last_device) continue;
            const auto result = vkDeviceWaitIdle(item->device);
            last_device = item->device;
            if (result != VK_SUCCESS && status == VK_SUCCESS) status = result;
        }
        return status;
    }

    void bvh::release_completed_resources() {
        VKZ_CHECK_VULKAN(wait_for_resources());
        retired.reset();
        if (state) state->uploads.snapshots.clear();
    }

    void bvh::build(VkCommandBuffer cmd, const mesh::data &scene, const geometry_buffers &geometry, const bvh_build_options &options, std::span<const bvh_instance_options> instances) {
        build(cmd, mesh::scene_adapter(scene, geometry, options, instances));
    }

    void bvh::build(VkCommandBuffer cmd, const vkz::bvh::scene &scene) {
        VKZ_ASSERT(cmd != VK_NULL_HANDLE, "BVH build requires a command buffer");
        auto next = std::make_unique<impl>(*allocator);
        next->options = scene.build_options;
        next->initialize(scene);
        next->upload_instances(cmd, scene);
        for (auto &value : next->bottom)
            next->record_build(cmd, value, false);
        next->record_build(cmd, next->top, true);
        next->finish_build(cmd);
        retire_state();
        state = std::move(next);
    }

    void bvh::rebuild(VkCommandBuffer cmd, const vkz::bvh::scene &scene) {
        require_update(cmd && state, "BVH has not been built");
        require_update(scene.geometry_count() == state->geometries.size() && scene.instance_count() == state->top.count && scene.gpu_instances() == state->gpu_instances,
                       "Scene size changed; use build()");
        for (size_t i = 0; i < scene.geometry_count(); ++i) {
            const auto a = scene.geometry_at(i);
            const auto &b = state->geometries[i];
            require_update(a.vertices._ == b.vertices._ && a.indices._ == b.indices._ && a.vertex_offset == b.vertex_offset && a.vertex_stride == b.vertex_stride && a.vertex_count == b.vertex_count &&
                               a.index_offset == b.index_offset && a.primitive_count == b.primitive_count,
                           "Geometry layout changed; use build()");
        }
        if (!state->gpu_instances) {
            for (size_t i = 0; i < scene.instance_count(); ++i) {
                require_update(scene.instance_at(i).mesh_id == state->mesh_ids[i], "Instance geometry changed; use build()");
            }
        }
        scene.validate_geometry();
        std::vector<bool> changed;
        for (size_t i = 0; i < scene.geometry_count(); ++i) {
            changed.push_back(scene.geometry_at(i).content_revision != state->geometries[i].content_revision);
        }
        state->geometries.clear();
        state->prepare_geometry(scene);
        if (!state->gpu_instances) {
            state->instance_data = state->prepare_instances(scene);
        }
        state->upload_instances(cmd, scene);
        for (size_t i = 0; i < state->bottom.size(); ++i) {
            if (changed[i]) {
                state->record_build(cmd, state->bottom[i], false);
            }
        }
        state->record_build(cmd, state->top, true);
        state->finish_build(cmd);
    }

    void bvh::update_instances(VkCommandBuffer cmd, const mesh::data &scene, std::span<const bvh_instance_options> instances) {
        require_update(state && !state->geometries.empty(), "BVH has not been built");
        const auto &geometry = state->geometries.front();
        update_instances(cmd, mesh::scene_adapter(scene, {geometry.vertices, geometry.indices}, state->options, instances));
    }

    void bvh::update_instances(VkCommandBuffer cmd, const vkz::bvh::scene &scene) {
        require_update(cmd && state && state->options.allow_instance_updates, "BVH was not built with instance updates enabled");
        require_update(scene.instance_count() == state->top.count && scene.gpu_instances() == state->gpu_instances, "Instance count changed; rebuild the BVH");
        require_update(scene.geometry_count() == state->geometries.size(), "Mesh count changed; rebuild the BVH");
        require_update(scene.build_options.lod == state->options.lod, "Mesh LOD changed; rebuild the BVH");
        for (size_t i = 0; i < scene.geometry_count(); ++i) {
            const auto a = scene.geometry_at(i);
            const auto &b = state->geometries[i];
            require_update(a.vertices._ == b.vertices._ && a.indices._ == b.indices._ && a.vertex_offset == b.vertex_offset && a.vertex_stride == b.vertex_stride && a.vertex_count == b.vertex_count &&
                               a.index_offset == b.index_offset && a.primitive_count == b.primitive_count && a.content_revision == b.content_revision,
                           "Mesh geometry changed; rebuild the BVH");
        }
        if (!state->gpu_instances) {
            for (size_t i = 0; i < scene.instance_count(); ++i) {
                require_update(scene.instance_at(i).mesh_id == state->mesh_ids[i], "Instance mesh changed; rebuild the BVH");
            }
            state->instance_data = state->prepare_instances(scene);
        }
        state->upload_instances(cmd, scene);
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
        VKZ_CHECK_VULKAN(wait_for_resources());
        state.reset();
        retired.reset();
    }
} // namespace vkz
