#pragma once

#include <vector>

namespace vkz {

    class ComputePipelineLayoutBuilder : public ComputePipelineBuilder {
    public:
        ComputePipelineLayoutBuilder(Device device, ComputePipelineBuilder *builder);

        ComputePipelineLayoutBuilder &addDescriptorSetLayout(VkDescriptorSetLayout layout);

        template<typename DescriptorSetLayouts = std::vector<VkDescriptorSetLayout>>
        ComputePipelineLayoutBuilder &addDescriptorSetLayouts(const DescriptorSetLayouts &layouts) {
            for (auto &layout: layouts) {
                addDescriptorSetLayout(layout);
            }
            return *this;
        }

        ComputePipelineLayoutBuilder &addPushConstantRange(VkShaderStageFlags stage, uint32_t offset, uint32_t size);

        ComputePipelineLayoutBuilder &addPushConstantRange(VkPushConstantRange range);

        template<typename Ranges = std::vector<VkPushConstantRange>>
        ComputePipelineLayoutBuilder &addPushConstantRanges(const Ranges &ranges) {
            for (auto &range: ranges) {
                addPushConstantRange(range.stageFlags, range.offset, range.size);
            }
            return *this;
        }

        ComputePipelineLayoutBuilder &clear();

        ComputePipelineLayoutBuilder &clearRanges();

        ComputePipelineLayoutBuilder &clearLayouts();

        void copy(const ComputePipelineLayoutBuilder &source);

        [[nodiscard]]
        VkPipelineLayout buildPipelineLayout() const;


    private:
        std::vector<VkDescriptorSetLayout> _descriptorSetLayouts;
        std::vector<VkPushConstantRange> _ranges;
    };
}