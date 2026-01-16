
#include "vulkanizer/GraphicsPipelineBuilder.hpp"

namespace vkz {

    MultisampleStateBuilder::MultisampleStateBuilder(Device device, GraphicsPipelineBuilder *parent)
            : GraphicsPipelineBuilder(device, parent),
            _info{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
            }
    {}

    MultisampleStateBuilder &MultisampleStateBuilder::rasterizationSamples(VkSampleCountFlagBits flags) {
        _info.rasterizationSamples = flags;
        return *this;
    }

    MultisampleStateBuilder &MultisampleStateBuilder::enableSampleShading() {
        _info.sampleShadingEnable = VK_TRUE;
        return *this;
    }

    MultisampleStateBuilder &MultisampleStateBuilder::disableSampleShading() {
        _info.sampleShadingEnable = VK_FALSE;
        return *this;
    }

    MultisampleStateBuilder &MultisampleStateBuilder::minSampleShading(float value) {
        _info.minSampleShading = value;
        return *this;
    }

    MultisampleStateBuilder &MultisampleStateBuilder::sampleMask(const VkSampleMask *mask) {
        _info.pSampleMask = mask;
        return *this;
    }

    MultisampleStateBuilder &MultisampleStateBuilder::enableAlphaToCoverage() {
        _info.alphaToCoverageEnable = VK_TRUE;
        return *this;
    }

    MultisampleStateBuilder &MultisampleStateBuilder::disableAlphaToCoverage() {
        _info.alphaToCoverageEnable = VK_FALSE;
        return *this;
    }

    MultisampleStateBuilder &MultisampleStateBuilder::enableAlphaToOne() {
        _info.alphaToOneEnable = VK_TRUE;
        return *this;
    }

    VkPipelineMultisampleStateCreateInfo &MultisampleStateBuilder::buildMultisampleState() {
        _info.alphaToOneEnable = VK_FALSE;
        return _info;
    }

    void MultisampleStateBuilder::copy(const MultisampleStateBuilder &source) {
        _info = source._info;
    }

}