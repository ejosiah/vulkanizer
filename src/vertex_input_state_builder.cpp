#include "vulkanizer/graphics_pipeline_builder.hpp"
#include "vulkanizer/log.hpp"

#include <format>

namespace vkz {

    vertex_input_state_builder::vertex_input_state_builder(vkz::device device, graphics_pipeline_builder *parent)
            : graphics_pipeline_builder(device, parent) {

    }

    vertex_input_state_builder &
    vertex_input_state_builder::add_vertex_binding_description(uint32_t binding, uint32_t stride,
                                                         VkVertexInputRate inputRate) {
        _bindings.push_back({binding, stride, inputRate});
        return *this;
    }

    vertex_input_state_builder &
    vertex_input_state_builder::add_vertex_binding_description(const VkVertexInputBindingDescription &description) {
        _bindings.push_back(description);
        return *this;
    }

    vertex_input_state_builder &
    vertex_input_state_builder::add_vertex_attribute_description(uint32_t location, uint32_t binding, VkFormat format,
                                                           uint32_t offset) {
        _attributes.push_back({location, binding, format, offset});
        return *this;
    }

    vertex_input_state_builder &
    vertex_input_state_builder::add_vertex_attribute_description(const VkVertexInputAttributeDescription &description) {
        _attributes.push_back(description);
        return *this;
    }

    void vertex_input_state_builder::validate() const {
        if (_bindings.empty()) {
            vkz::warn("No vertex binding descriptions defined for vertex_input_state");
        }

        for (const auto &binding: _bindings) {
            auto itr = std::find_if(begin(_attributes), end(_attributes), [&binding](const auto &attribute) {
                return binding.binding == attribute.binding;
            });
            if (itr == end(_attributes)) {
                throw std::runtime_error{
                        std::format("No vertex attribute description defined for binding {}", binding.binding)};
            }
        }
    }

    VkPipelineVertexInputStateCreateInfo &vertex_input_state_builder::build_vertex_input_state() {
        validate();
        _info.vertexBindingDescriptionCount = _bindings.size();
        _info.pVertexBindingDescriptions = _bindings.data();
        _info.vertexAttributeDescriptionCount = _attributes.size();
        _info.pVertexAttributeDescriptions = _attributes.data();
        return _info;
    }

    vertex_input_state_builder &vertex_input_state_builder::clear() {
        _bindings.clear();
        _attributes.clear();
        return *this;
    }

    vertex_input_state_builder &vertex_input_state_builder::clear_binding_desc() {
        _bindings.clear();
        return *this;
    }

    vertex_input_state_builder &vertex_input_state_builder::clear_attribute_desc() {
        _attributes.clear();
        return *this;
    }


    void vertex_input_state_builder::copy(const vertex_input_state_builder &source) {
        _bindings = std::vector<VkVertexInputBindingDescription>(source._bindings.begin(), source._bindings.end());
        _attributes = std::vector<VkVertexInputAttributeDescription>(source._attributes.begin(),
                                                                     source._attributes.end());
    }

}