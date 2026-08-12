#pragma once

#include "../descriptors.hpp"

namespace vkz {

    class pipeline_layout_builder : public graphics_pipeline_builder {
    public:
        pipeline_layout_builder(vkz::device device, graphics_pipeline_builder *builder);

        pipeline_layout_builder &add_descriptor_set_layout(descriptor_set_layout layout);

        template<typename DescriptorSetLayouts = std::vector<descriptor_set_layout>>
        pipeline_layout_builder &add_descriptor_set_layouts(const DescriptorSetLayouts &layouts) {
            for (auto &layout: layouts) {
                add_descriptor_set_layout(layout);
            }
            return *this;
        }

        pipeline_layout_builder &add_push_constant_range(VkShaderStageFlags stage, uint32_t offset, uint32_t size);

        pipeline_layout_builder &add_push_constant_range(VkPushConstantRange range);

        template<typename Ranges = std::vector<VkPushConstantRange>>
        pipeline_layout_builder &add_push_constant_ranges(const Ranges &ranges) {
            for (auto &range: ranges) {
                add_push_constant_range(range.stageflags, range.offset, range.size);
            }
            return *this;
        }

        pipeline_layout_builder &clear();

        pipeline_layout_builder &clear_ranges();

        pipeline_layout_builder &clear_layouts();

        void copy(const pipeline_layout_builder &source);

        VkPipelineLayout build_pipeline_layout() const;


    private:
        std::vector<VkDescriptorSetLayout> _descriptorSetLayouts;
        std::vector<VkPushConstantRange> _ranges;
    };
}
