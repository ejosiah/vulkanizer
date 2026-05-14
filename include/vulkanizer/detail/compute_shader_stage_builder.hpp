#pragma once

#include "../types.hpp"

#include <vector>
#include <variant>

namespace vkz {

    class compute_shader_stage_builder : public compute_pipeline_builder {
    public:
        using shader_source = std::variant<byte_string, std::vector<uint32_t>, std::string>;

        compute_shader_stage_builder(vkz::device device, compute_pipeline_builder *parent);

        explicit compute_shader_stage_builder(compute_shader_stage_builder *parent);

        ~compute_shader_stage_builder();

        [[nodiscard]]
        virtual compute_shader_stage_builder &compute_shader(const shader_source &source);

        template<typename T>
        compute_shader_stage_builder &add_specialization(T value, uint32_t constant_id) {
            auto dataSize = sizeof(value);
            VkSpecializationMapEntry entry{constant_id, _offset, dataSize};

            auto bytes = reinterpret_cast<char *>(&value);
            _data.insert(_data.end(), bytes, bytes + dataSize);
            _offset = _data.size();
            _entries.push_back(entry);
            return *this;
        }

        void validate() const;

        void clear_stages();

        [[nodiscard]]
        VkPipelineShaderStageCreateInfo &build_shader_stage();



    private:
        shader_info _shader;
        std::vector<VkSpecializationMapEntry> _entries;
        std::vector<char> _data;
        uint32_t _offset{};
        VkPipelineShaderStageCreateInfo _create_info{};
        VkSpecializationInfo _specialization{};
        std::vector<VkPipelineShaderStageCreateInfo> _vk_stages;
        VkPhysicalDeviceFeatures2 _features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        VkPhysicalDeviceMeshShaderFeaturesEXT _mesh_features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
    };
}