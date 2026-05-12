#include "vulkanizer/compute_pipeline_builder.hpp"

namespace vkz {

    compute_pipeline_layout_builder::compute_pipeline_layout_builder(vkz::device device, compute_pipeline_builder *builder)
            : compute_pipeline_builder(device, builder) {

    }

    compute_pipeline_layout_builder &compute_pipeline_layout_builder::add_descriptor_set_layout(VkDescriptorSetLayout layout) {
        _descriptorSetLayouts.push_back(layout);
        return *this;
    }

    compute_pipeline_layout_builder &
    compute_pipeline_layout_builder::add_push_constant_range(VkShaderStageFlags stage, uint32_t offset, uint32_t size) {
        _ranges.push_back({stage, offset, size});
        return *this;
    }

    VkPipelineLayout compute_pipeline_layout_builder::build_pipeline_layout() const {

        VkPipelineLayoutCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        create_info.setLayoutCount = _descriptorSetLayouts.size();
        create_info.pSetLayouts = _descriptorSetLayouts.data();
        create_info.pushConstantRangeCount = _ranges.size();
        create_info.pPushConstantRanges = _ranges.data();

        VkPipelineLayout pipeline_layout;
        VKZ_CHECK_VULKAN(vkCreatePipelineLayout(device().logical, &create_info, nullptr, &pipeline_layout));

        return pipeline_layout;
    }

    compute_pipeline_layout_builder &compute_pipeline_layout_builder::add_push_constant_range(VkPushConstantRange range) {
        _ranges.push_back(range);
        return *this;
    }

    compute_pipeline_layout_builder &compute_pipeline_layout_builder::clear_ranges() {
        _ranges.clear();
        return *this;
    }

    compute_pipeline_layout_builder &compute_pipeline_layout_builder::clear_layouts() {
        _descriptorSetLayouts.clear();
        return *this;
    }

    compute_pipeline_layout_builder &compute_pipeline_layout_builder::clear() {
        _ranges.clear();
        _descriptorSetLayouts.clear();
        return *this;
    }


    void compute_pipeline_layout_builder::copy(const compute_pipeline_layout_builder &source) {
        _ranges = decltype(_ranges)(source._ranges.begin(), source._ranges.end());
        _descriptorSetLayouts = decltype(_descriptorSetLayouts)(source._descriptorSetLayouts.begin(),
                                                                source._descriptorSetLayouts.end());

    }

}