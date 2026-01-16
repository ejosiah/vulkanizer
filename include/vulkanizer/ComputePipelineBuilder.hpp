#pragma once

#include "vkz.hpp"
#include "builder.hpp"

#include <string>
#include <memory>

namespace vkz {

    class ComputePipelineBuilder : public Builder {
    public:
        explicit ComputePipelineBuilder(Device device);

        ComputePipelineBuilder(Device device, ComputePipelineBuilder* parent);

        ComputePipelineBuilder() = default;

        ComputePipelineBuilder(ComputePipelineBuilder&& source) noexcept ;

        virtual ~ComputePipelineBuilder() = default;

        [[nodiscard]]
        ComputePipelineBuilder* parent() override;

        virtual ComputeShaderStageBuilder& shaderStage();

        ComputePipelineLayoutBuilder& layout(VkPipelineLayout  aLayout);

        VkPipeline build();

        VkPipeline build(VkPipelineLayout& pipelineLayout);

        VkComputePipelineCreateInfo createInfo();

    protected:
        VkPipelineCreateFlags _flags = 0;
        VkPipelineLayout _pipelineLayout{};
        VkPipelineLayout _pipelineLayoutOwned{};
        std::string _name;

        std::unique_ptr<ComputeShaderStageBuilder> _shaderStageBuilder{};
        std::unique_ptr<ComputePipelineLayoutBuilder> _pipelineLayoutBuilder{};

        VkPipeline _basePipeline{};
        VkPipelineCache _pipelineCache{};
        void* _nextChain{};
    };
}

#include "detail/ComputeShaderStageBuilder.hpp"
#include "detail/ComputePipelineLayoutBuilder.hpp"