#pragma once

#include "../types.hpp"

#include <vector>
#include <variant>

namespace vkz {

    class ComputeShaderStageBuilder : public ComputePipelineBuilder {
    public:
        using ShaderSource = std::variant<byte_string, std::vector<uint32_t>, std::string>;

        ComputeShaderStageBuilder(Device device, ComputePipelineBuilder *parent);

        explicit ComputeShaderStageBuilder(ComputeShaderStageBuilder *parent);

        [[nodiscard]]
        virtual ComputeShaderStageBuilder &computeShader(const ShaderSource &source);

        template<typename T>
        ComputeShaderStageBuilder &addSpecialization(T value, uint32_t constantID) {
            auto dataSize = sizeof(value);
            VkSpecializationMapEntry entry{constantID, _offset, dataSize};

            auto bytes = reinterpret_cast<char *>(&value);
            _data.insert(_data.end(), bytes, bytes + dataSize);
            _offset = _data.size();
            _entries.push_back(entry);
            return *this;
        }

        void validate() const;

        void clearStages();

        [[nodiscard]]
        VkPipelineShaderStageCreateInfo &buildShaderStage();



    private:
        ShaderInfo _stage;
        std::vector<VkSpecializationMapEntry> _entries;
        std::vector<char> _data;
        uint32_t _offset{};
        VkPipelineShaderStageCreateInfo _createInfo{};
        VkSpecializationInfo _specialization{};
        std::vector<VkPipelineShaderStageCreateInfo> _vkStages;
        VkPhysicalDeviceFeatures2 _features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        VkPhysicalDeviceMeshShaderFeaturesEXT _meshFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
    };
}