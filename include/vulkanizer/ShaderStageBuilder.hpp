#pragma once

#include "types.hpp"
#include "ShaderInfo.hpp"
#include "GraphicsPipelineBuilder.hpp"

#include <tuple>
#include <variant>
#include <map>

namespace vkz {

    class ShaderBuilder;

    class ShaderStageBuilder : public GraphicsPipelineBuilder {
    public:
        using ShaderSource = std::variant<byte_string, std::vector<uint32_t>, std::string>;

        ShaderStageBuilder(Device device, GraphicsPipelineBuilder *parent);

        explicit ShaderStageBuilder(ShaderStageBuilder *parent);

        [[nodiscard]]
        virtual ShaderBuilder &vertexShader(const ShaderSource &source);

        virtual ShaderBuilder &taskSShader(const ShaderSource &source);

        virtual ShaderBuilder &meshShader(const ShaderSource &source);

        [[nodiscard]]
        virtual ShaderBuilder &fragmentShader(const ShaderSource &source);

        [[nodiscard]]
        virtual ShaderBuilder &geometryShader(const ShaderSource &source);

        [[nodiscard]]
        virtual ShaderBuilder &tessellationEvaluationShader(const ShaderSource &source);


        [[nodiscard]]
        virtual ShaderBuilder &tessellationControlShader(const ShaderSource &source);

        ShaderStageBuilder &clear();

        void validate() const;

        void copy(const ShaderStageBuilder &source);

        [[nodiscard]]
        std::vector<VkPipelineShaderStageCreateInfo> &buildShaderStage();

        void clearStages();

    protected:
        ShaderBuilder &addShader(const ShaderSource &source, VkShaderStageFlagBits stage);

        bool hasVertexShader() const;

        bool hasTessControlShader() const;

        bool hasTessEvalShader() const;

        bool meshShaderSupported() const;

        bool taskShaderSupported() const;

    private:
        std::vector<VkPipelineShaderStageCreateInfo> _vkStages;
        std::vector<std::unique_ptr<ShaderBuilder>> _shaderBuilders;
        VkPhysicalDeviceFeatures2 _features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        VkPhysicalDeviceMeshShaderFeaturesEXT _meshFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
    };

    class ShaderBuilder : public ShaderStageBuilder {
    public:
        explicit ShaderBuilder(ShaderStageBuilder *parent);

        ShaderBuilder(const ShaderSource &source, VkShaderStageFlagBits stage, ShaderStageBuilder *parent);

        template<typename T>
        ShaderBuilder &addSpecialization(T value, uint32_t constantID) {
            auto dataSize = sizeof(value);
            VkSpecializationMapEntry entry{constantID, _offset, dataSize};

            auto bytes = reinterpret_cast<char *>(&value);
            _data.insert(_data.end(), bytes, bytes + dataSize);
            _offset = _data.size();
            _entries.push_back(entry);
            return *this;
        }

        ShaderStageBuilder *parent() override;

        ShaderBuilder &vertexShader(const ShaderSource &source) override;

        ShaderBuilder &taskSShader(const ShaderSource &source) override;

        ShaderBuilder &meshShader(const ShaderSource &source) override;

        ShaderBuilder &fragmentShader(const ShaderSource &source) override;

        ShaderBuilder &geometryShader(const ShaderSource &source) override;

        ShaderBuilder &tessellationEvaluationShader(const ShaderSource &source) override;

        ShaderBuilder &tessellationControlShader(const ShaderSource &source) override;

        VkPipelineShaderStageCreateInfo &buildShader();

        bool isVertexShader() const;

        bool isMeshShader() const;

        bool isTessEvalShader() const;

        bool isTessControlShader() const;

        bool isStage(VkShaderStageFlagBits stage) const;

        void copy(const ShaderBuilder &source);

    private:
        ShaderInfo _stage;
        std::vector<VkSpecializationMapEntry> _entries;
        std::vector<char> _data;
        uint32_t _offset{};
        VkPipelineShaderStageCreateInfo _createInfo{};
        VkSpecializationInfo _specialization{};
    };
}