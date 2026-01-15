#pragma once

#include "GraphicsPipelineBuilder.hpp"

namespace vkz {

    class MultisampleStateBuilder : public GraphicsPipelineBuilder {
    public:
        MultisampleStateBuilder(Device device, GraphicsPipelineBuilder *parent);

        MultisampleStateBuilder &rasterizationSamples(VkSampleCountFlagBits flags);

        MultisampleStateBuilder &enableSampleShading();

        MultisampleStateBuilder &disableSampleShading();

        MultisampleStateBuilder &minSampleShading(float value);

        MultisampleStateBuilder &sampleMask(const VkSampleMask *mask);

        MultisampleStateBuilder &enableAlphaToCoverage();

        MultisampleStateBuilder &disableAlphaToCoverage();

        MultisampleStateBuilder &enableAlphaToOne();

        VkPipelineMultisampleStateCreateInfo &buildMultisampleState();

        void copy(const MultisampleStateBuilder &source);

    private:
        VkPipelineMultisampleStateCreateInfo _info;
    };
}