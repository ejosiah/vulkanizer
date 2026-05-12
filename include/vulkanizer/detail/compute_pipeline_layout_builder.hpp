#pragma once

#include <vector>

namespace vkz {

    class compute_pipeline_layout_builder : public compute_pipeline_builder {
    public:
        compute_pipeline_layout_builder(vkz::device device, compute_pipeline_builder *builder);

        compute_pipeline_layout_builder &add_descriptor_set_layout(VkDescriptorSetLayout layout);

        template<typename DescriptorSetLayouts = std::vector<VkDescriptorSetLayout>>
        compute_pipeline_layout_builder &add_descriptor_set_layouts(const DescriptorSetLayouts &layouts) {
            for (auto &layout: layouts) {
                add_descriptor_set_layout(layout);
            }
            return *this;
        }

        compute_pipeline_layout_builder &add_push_constant_range(VkShaderStageFlags stage, uint32_t offset, uint32_t size);

        compute_pipeline_layout_builder &add_push_constant_range(VkPushConstantRange range);

        template<typename Ranges = std::vector<VkPushConstantRange>>
        compute_pipeline_layout_builder &add_push_constant_ranges(const Ranges &ranges) {
            for (auto &range: ranges) {
                add_push_constant_range(range.stageflags, range.offset, range.size);
            }
            return *this;
        }

        compute_pipeline_layout_builder &clear();

        compute_pipeline_layout_builder &clear_ranges();

        compute_pipeline_layout_builder &clear_layouts();

        void copy(const compute_pipeline_layout_builder &source);

        [[nodiscard]]
        VkPipelineLayout build_pipeline_layout() const;


    private:
        std::vector<VkDescriptorSetLayout> _descriptorSetLayouts;
        std::vector<VkPushConstantRange> _ranges;
    };
}