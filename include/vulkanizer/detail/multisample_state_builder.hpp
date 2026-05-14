#pragma once

namespace vkz {

    class multisample_state_builder : public graphics_pipeline_builder {
    public:
        multisample_state_builder(vkz::device device, graphics_pipeline_builder *parent);

        multisample_state_builder &rasterization_samples(VkSampleCountFlagBits flags);

        multisample_state_builder &enable_sample_shading();

        multisample_state_builder &disable_sample_shading();

        multisample_state_builder &min_sample_shading(float value);

        multisample_state_builder &sample_mask(const VkSampleMask *mask);

        multisample_state_builder &enable_alpha_to_coverage();

        multisample_state_builder &disable_alpha_to_coverage();

        multisample_state_builder &enable_alpha_to_one();

        VkPipelineMultisampleStateCreateInfo &build_multisample_state();

        void copy(const multisample_state_builder &source);

    private:
        VkPipelineMultisampleStateCreateInfo _info;
    };
}