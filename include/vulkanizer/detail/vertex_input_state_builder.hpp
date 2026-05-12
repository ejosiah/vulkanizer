#pragma once

namespace vkz {

    class vertex_input_state_builder : public graphics_pipeline_builder {
    public:
        explicit vertex_input_state_builder(vkz::device device, graphics_pipeline_builder *parent);

        vertex_input_state_builder &
        add_vertex_binding_description(uint32_t binding, uint32_t stride, VkVertexInputRate inputRate);

        vertex_input_state_builder &add_vertex_binding_description(const VkVertexInputBindingDescription &description);

        template<typename BindingDescriptions = std::vector<VkVertexInputBindingDescription>>
        inline vertex_input_state_builder &add_vertex_binding_descriptions(const BindingDescriptions &bindings) {
            for (const auto &binding: bindings) {
                add_vertex_binding_description(binding.binding, binding.stride, binding.inputRate);
            }
            return *this;
        }

        vertex_input_state_builder &
        add_vertex_attribute_description(uint32_t location, uint32_t binding, VkFormat format, uint32_t offset);

        vertex_input_state_builder &add_vertex_attribute_description(const VkVertexInputAttributeDescription &description);

        template<typename AttributeDescriptions = std::vector<VkVertexInputAttributeDescription>>
        inline vertex_input_state_builder &add_vertex_attribute_descriptions(const AttributeDescriptions &attributes) {
            for (const auto &attribute: attributes) {
                add_vertex_attribute_description(attribute.location, attribute.binding, attribute.format,
                                              attribute.offset);
            }
            return *this;
        }

        void validate() const;

        vertex_input_state_builder &clear();

        vertex_input_state_builder &clear_binding_desc();

        vertex_input_state_builder &clear_attribute_desc();

        VkPipelineVertexInputStateCreateInfo &build_vertex_input_state();

        void copy(const vertex_input_state_builder &source);


    private:
        std::vector<VkVertexInputBindingDescription> _bindings;
        std::vector<VkVertexInputAttributeDescription> _attributes;
        VkPipelineVertexInputStateCreateInfo _info{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    };

}