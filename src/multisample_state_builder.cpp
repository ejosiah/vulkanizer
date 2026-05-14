
#include "vulkanizer/graphics_pipeline_builder.hpp"

namespace vkz {

    multisample_state_builder::multisample_state_builder(vkz::device device, graphics_pipeline_builder *parent)
            : graphics_pipeline_builder(device, parent),
            _info{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
            }
    {}

    multisample_state_builder &multisample_state_builder::rasterization_samples(VkSampleCountFlagBits flags) {
        _info.rasterizationSamples = flags;
        return *this;
    }

    multisample_state_builder &multisample_state_builder::enable_sample_shading() {
        _info.sampleShadingEnable = VK_TRUE;
        return *this;
    }

    multisample_state_builder &multisample_state_builder::disable_sample_shading() {
        _info.sampleShadingEnable = VK_FALSE;
        return *this;
    }

    multisample_state_builder &multisample_state_builder::min_sample_shading(float value) {
        _info.minSampleShading = value;
        return *this;
    }

    multisample_state_builder &multisample_state_builder::sample_mask(const VkSampleMask *mask) {
        _info.pSampleMask = mask;
        return *this;
    }

    multisample_state_builder &multisample_state_builder::enable_alpha_to_coverage() {
        _info.alphaToCoverageEnable = VK_TRUE;
        return *this;
    }

    multisample_state_builder &multisample_state_builder::disable_alpha_to_coverage() {
        _info.alphaToCoverageEnable = VK_FALSE;
        return *this;
    }

    multisample_state_builder &multisample_state_builder::enable_alpha_to_one() {
        _info.alphaToOneEnable = VK_TRUE;
        return *this;
    }

    VkPipelineMultisampleStateCreateInfo &multisample_state_builder::build_multisample_state() {
        _info.alphaToOneEnable = VK_FALSE;
        return _info;
    }

    void multisample_state_builder::copy(const multisample_state_builder &source) {
        _info = source._info;
    }

}