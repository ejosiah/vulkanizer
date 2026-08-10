#include "vulkanizer/compute_pipeline_builder.hpp"
#include "vulkanizer/struct_mapping.hpp"

vkz::compute_pipeline_builder::compute_pipeline_builder(vkz::device device)
: builder_base{device, nullptr}
, _shader_stage_builder{std::make_unique<compute_shader_stage_builder>(device, this)}
, _pipeline_layout_builder{std::make_unique<compute_pipeline_layout_builder>(device, this)}
{}

vkz::compute_pipeline_builder::compute_pipeline_builder(vkz::device device, vkz::compute_pipeline_builder *parent)
:builder_base(device, parent){}

vkz::compute_pipeline_builder::compute_pipeline_builder(vkz::compute_pipeline_builder &&source) noexcept {
    _shader_stage_builder = std::move(source._shader_stage_builder);
    _pipeline_layout_builder = std::move(source._pipeline_layout_builder);
    _name = std::move(source._name);
    _flags = source._flags;
    _pipeline_layout = std::exchange(source._pipeline_layout, nullptr);
    _pipeline_layout_owned = std::exchange(source._pipeline_layout_owned, nullptr);
    _base_pipeline = std::exchange(source._base_pipeline, nullptr);
    _pipeline_cache = std::exchange(source._pipeline_cache, nullptr);
    _next_chain = std::exchange(source._next_chain, nullptr);
    _parent = std::exchange(source._parent, nullptr);
    _device = source._device;
}

vkz::compute_pipeline_builder *vkz::compute_pipeline_builder::parent() {
    return dynamic_cast<compute_pipeline_builder *>(builder_base::parent());
}

vkz::compute_shader_stage_builder &vkz::compute_pipeline_builder::shader_stage() {
    if (parent()) {
        return parent()->shader_stage();
    }
    return *_shader_stage_builder;
}

vkz::compute_pipeline_layout_builder &vkz::compute_pipeline_builder::layout() {
    if (parent()) {
        return parent()->layout();
    }
    return *_pipeline_layout_builder;
}

vkz::compute_pipeline_builder &vkz::compute_pipeline_builder::name(const std::string &value) {
    if (parent()) {
        parent()->name(value);
    }
    _name = value;
    return *this;
}

VkPipeline vkz::compute_pipeline_builder::build() {
    if (parent()) {
        return parent()->build();
    }
    if (!_pipeline_layout) {
        throw std::runtime_error{"either provide or create a pipeline_layout"};
    }
    VkPipelineLayout unused{};
    return build(unused);
}

VkPipeline vkz::compute_pipeline_builder::build(VkPipelineLayout &pipeline_layout) {
    if (parent()) {
        return parent()->build(pipeline_layout);
    }
    auto info = create_info();
    pipeline_layout = std::move(_pipeline_layout_owned);

    VkPipeline pipeline;
    VKZ_CHECK_VULKAN(vkCreateComputePipelines(device().logical, nullptr, 1, &info, nullptr, &pipeline));
    if (!_name.empty()) {
        set_name<VK_OBJECT_TYPE_PIPELINE>(device(), _name, pipeline);
        set_name<VK_OBJECT_TYPE_PIPELINE_LAYOUT>(device(), _name, pipeline_layout);
    }
    _shader_stage_builder->clear_stages();
    return pipeline;
}

VkComputePipelineCreateInfo vkz::compute_pipeline_builder::create_info() {
    if (parent()) return parent()->create_info();

    auto &shader_stage = _shader_stage_builder->build_shader_stage();

    auto info = makeStruct<VkComputePipelineCreateInfo>();
    info.flags = _flags;
    info.stage = shader_stage;

    if (_flags & VK_PIPELINE_CREATE_DERIVATIVE_BIT) {
        assert(_base_pipeline);
        info.basePipelineHandle = _base_pipeline;
        info.basePipelineIndex = -1;
    }

    if (!_pipeline_layout) {
        _pipeline_layout_owned = _pipeline_layout_builder->build_pipeline_layout();
        info.layout = _pipeline_layout_owned;
    } else {
        info.layout = _pipeline_layout;
    }

    return info;
}
