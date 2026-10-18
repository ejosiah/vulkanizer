#include <vulkanizer/bvh.hpp>
#include <vulkanizer/status.hpp>
#include <algorithm>

namespace vkz::mesh {
    namespace {
        uint64_t hash_geometry_bytes(uint64_t hash, const void *data, size_t size) {
            const auto *bytes = static_cast<const uint8_t *>(data);
            for (size_t i = 0; i < size; ++i) {
                hash ^= bytes[i];
                hash *= 1099511628211ull;
            }
            return hash;
        }
    }
    scene_adapter::scene_adapter(const data &value, const geometry_buffers &geometry,
                                 const bvh_build_options &options, std::span<const bvh_instance_options> instances)
        : source(value), buffers(geometry), settings(instances) {
        build_options = options;
        VKZ_ASSERT(settings.empty() || settings.size() == source.instances.size(), "Instance options count mismatch");
    }

    size_t scene_adapter::geometry_count() const { return source.meshes.size(); }
    size_t scene_adapter::instance_count() const { return source.instances.size(); }

    void scene_adapter::validate_geometry() const {
        for (size_t i = 0; i < geometry_count(); ++i) {
            const auto geometry = geometry_at(i);
            const auto begin = geometry.index_offset / sizeof(uint32_t);
            const auto end = begin + uint64_t(geometry.primitive_count) * 3;
            for (auto j = begin; j < end; ++j)
                VKZ_ASSERT(source.index_data[j] < geometry.vertex_count, "Mesh index exceeds its vertex count");
        }
    }

    vkz::bvh::scene::geometry scene_adapter::geometry_at(size_t index) const {
        const auto &scene = source;
        const auto &options = build_options;
        VKZ_ASSERT(index < scene.meshes.size(), "Mesh index out of range");
        const auto usage = vkz::buffer_usage_acceleration_input;
        VKZ_ASSERT(buffers.vertices._ && buffers.indices._ && has_flag(static_cast<vkz::buffer_usage>(buffers.vertices.create_info.usage), usage) &&
                   has_flag(static_cast<vkz::buffer_usage>(buffers.indices.create_info.usage), usage),
               "BVH buffers require build-input and device-address usage");
        VKZ_ASSERT(buffers.vertices.create_info.size >= scene.vertex_data.size() && buffers.indices.create_info.size >= scene.index_data.size() * sizeof(uint32_t), "BVH buffers are smaller than scene data");
        auto attribute = std::find_if(scene.streams.attributes.begin(), scene.streams.attributes.end(), [](const auto &value) { return value.location == 0; });
        VKZ_ASSERT(attribute != scene.streams.attributes.end() && attribute->format == VK_FORMAT_R32G32B32_SFLOAT, "BVH requires float3 positions at location zero");
        auto binding = std::find_if(scene.streams.bindings.begin(), scene.streams.bindings.end(), [&](const auto &value) {return value.binding == attribute->binding; });
        VKZ_ASSERT(scene.streams.bindings.size() == 1 && binding != scene.streams.bindings.end() && binding->inputRate == VK_VERTEX_INPUT_RATE_VERTEX && binding->stride % 4 == 0 &&
                   uint64_t(attribute->offset) + 12 <= binding->stride && attribute->offset % 4 == 0,
               "BVH requires one interleaved vertex stream with aligned float3 positions");
        const auto &mesh = scene.meshes[index];
        VKZ_ASSERT(mesh.lod_count <= max_lods && options.lod < mesh.lod_count && mesh.vertex_count, "Invalid mesh LOD or vertex count");
        const uint64_t begin = uint64_t(mesh.index_offset) + mesh.lod_offset[options.lod];
        const uint64_t end = uint64_t(mesh.index_offset) + mesh.lod_offset[options.lod + 1];
        VKZ_ASSERT(end > begin && end <= scene.index_data.size() && (end - begin) % 3 == 0, "BVH mesh must contain indexed triangles");
        VKZ_ASSERT((uint64_t(mesh.vertex_offset) + mesh.vertex_count) * binding->stride <= scene.vertex_data.size(), "Mesh vertices exceed the vertex stream");

        VKZ_ASSERT((end - begin) / 3 <= std::numeric_limits<uint32_t>::max(), "Mesh exceeds primitive count limit");
        // Hash the selected triangle indices and position bytes only. Material
        // edits and other vertex attributes do not change acceleration geometry.
        auto revision = hash_geometry_bytes(14695981039346656037ull,
                                           scene.index_data.data() + begin, (end - begin) * sizeof(uint32_t));
        for (uint64_t i = 0; i < mesh.vertex_count; ++i) {
            const auto offset = (uint64_t(mesh.vertex_offset) + i) * binding->stride + attribute->offset;
            revision = hash_geometry_bytes(revision, scene.vertex_data.data() + offset, 12);
        }
        return {buffers.vertices, buffers.indices,
                uint64_t(mesh.vertex_offset) * binding->stride + attribute->offset,
                binding->stride, mesh.vertex_count, begin * sizeof(uint32_t),
                static_cast<uint32_t>((end - begin) / 3), revision};
    }

    vkz::bvh::scene::instance scene_adapter::instance_at(size_t index) const {
        VKZ_ASSERT(index < source.instances.size(), "Instance index out of range");
        VKZ_ASSERT(settings.empty() || settings.size() == source.instances.size(), "Instance options count mismatch");
        const auto &value = source.instances[index];
        VKZ_ASSERT(value.mesh_id < source.meshes.size(), "Instance references an invalid mesh");
        const auto material_id = value.material_id == inherit_material ? source.meshes[value.mesh_id].material_id : value.material_id;
        VKZ_ASSERT(material_id < source.materials.size(), "Instance references an invalid material");
        const auto &material = source.materials[material_id];
        instance result{value.mesh_id, value.model, settings.empty() ? bvh_instance_options{} : settings[index], {}};
        if (material.alpha_mode == alpha_mode::opaque) result.flags |= static_cast<VkGeometryInstanceFlagsKHR>(vkz::geometry_instance_force_opaque);
        if (material.double_sided || material.thickness_factor > 0) result.flags |= static_cast<VkGeometryInstanceFlagsKHR>(vkz::geometry_instance_triangle_facing_cull_disable);
        return result;
    }

}
