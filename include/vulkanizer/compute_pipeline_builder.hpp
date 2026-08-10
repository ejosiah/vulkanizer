#pragma once

#include "vkz.hpp"
#include "builder.hpp"

#include <string>
#include <memory>

namespace vkz {

    class compute_pipeline_builder : public builder_base {
    public:
        explicit compute_pipeline_builder(vkz::device device);

        compute_pipeline_builder(vkz::device device, compute_pipeline_builder* parent);

        compute_pipeline_builder() = default;

        compute_pipeline_builder(compute_pipeline_builder&& source) noexcept ;

        virtual ~compute_pipeline_builder() = default;

        [[nodiscard]]
        compute_pipeline_builder* parent() override;

        virtual compute_shader_stage_builder& shader_stage();

        compute_pipeline_layout_builder& layout();

        VkPipeline build();

        VkPipeline build(VkPipelineLayout& pipeline_layout);

        VkComputePipelineCreateInfo create_info();

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
    };
}

#include "detail/compute_shader_stage_builder.hpp"
#include "detail/compute_pipeline_layout_builder.hpp"
