#include "vulkanizer/builders.hpp"

vkz::graphics_pipeline_builder vkz::make_graphics_pipeline_builder(vkz::device device) {
    return vkz::graphics_pipeline_builder(device);
}

vkz::compute_pipeline_builder vkz::make_compute_pipeline_builder(vkz::device device) {
    return vkz::compute_pipeline_builder(device);
}

vkz::descriptor_set_layout_builder vkz::make_descriptor_set_layout_builder(vkz::device device) {
    return vkz::descriptor_set_layout_builder(device);
}
