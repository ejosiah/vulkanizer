#pragma once

#include "vkz.hpp"
#include "pipeline.hpp"

#include <memory>
#include <string>

namespace vkz {

    class compute_pipeline_builder;
    class compute_shader_stage_builder;
    class compute_pipeline_layout_builder;

    template <typename Derived> class compute_pipeline_builder_proxy {
    public:
        explicit compute_pipeline_builder_proxy(compute_pipeline_builder *builder)
            : _builder{builder} {}

        compute_shader_stage_builder &shader_stage();

        compute_pipeline_layout_builder &layout();

        Derived &name(const std::string &value);

        [[nodiscard]] VkPipeline build_native();

        [[nodiscard]] VkPipeline build(VkPipelineLayout &pipeline_layout);

        [[nodiscard]] vkz::pipeline build();

        [[nodiscard]] VkComputePipelineCreateInfo create_info();

        [[nodiscard]] vkz::device device() const;

        void rebind(compute_pipeline_builder *builder) {
            _builder = builder;
        }

    protected:
        [[nodiscard]] Derived &derived() {
            return static_cast<Derived &>(*this);
        }

        compute_pipeline_builder *_builder{};
    };

    class compute_pipeline_builder {
    public:
        explicit compute_pipeline_builder(vkz::device device);

        compute_pipeline_builder() = default;

        compute_pipeline_builder(compute_pipeline_builder&& source) noexcept;

        ~compute_pipeline_builder() = default;

        compute_shader_stage_builder& shader_stage();

        compute_pipeline_layout_builder& layout();

        compute_pipeline_builder& name(const std::string& value);

        [[nodiscard]] VkPipeline build_native();

        [[nodiscard]] VkPipeline build(VkPipelineLayout& pipeline_layout);

        [[nodiscard]] vkz::pipeline build();

        [[nodiscard]] VkComputePipelineCreateInfo create_info();

        [[nodiscard]] vkz::device device() const {
            return _device;
        }

    protected:
        VkPipelineCreateFlags _flags = 0;
        VkPipelineLayout _pipeline_layout{};
        VkPipelineLayout _pipeline_layout_owned{};
        std::string _name;

        std::unique_ptr<compute_shader_stage_builder> _shader_stage_builder{};
        std::unique_ptr<compute_pipeline_layout_builder> _pipeline_layout_builder{};

        VkPipeline _base_pipeline{};
        VkPipelineCache _pipeline_cache{};
        void* _next_chain{};
        vkz::device _device{};
    };
}

#include "detail/compute_shader_stage_builder.hpp"
#include "detail/compute_pipeline_layout_builder.hpp"

namespace vkz {

    template <typename Derived> compute_shader_stage_builder &compute_pipeline_builder_proxy<Derived>::shader_stage() {
        return _builder->shader_stage();
    }

    template <typename Derived> compute_pipeline_layout_builder &compute_pipeline_builder_proxy<Derived>::layout() {
        return _builder->layout();
    }

    template <typename Derived> Derived &compute_pipeline_builder_proxy<Derived>::name(const std::string &value) {
        _builder->name(value);
        return derived();
    }

    template <typename Derived> VkPipeline compute_pipeline_builder_proxy<Derived>::build_native() {
        return _builder->build_native();
    }

    template <typename Derived> VkPipeline compute_pipeline_builder_proxy<Derived>::build(VkPipelineLayout &pipeline_layout) {
        return _builder->build(pipeline_layout);
    }

    template <typename Derived> vkz::pipeline compute_pipeline_builder_proxy<Derived>::build() {
        return _builder->build();
    }

    template <typename Derived> VkComputePipelineCreateInfo compute_pipeline_builder_proxy<Derived>::create_info() {
        return _builder->create_info();
    }

    template <typename Derived> vkz::device compute_pipeline_builder_proxy<Derived>::device() const {
        return _builder->device();
    }

}
