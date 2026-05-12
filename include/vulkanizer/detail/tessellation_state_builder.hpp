#pragma once

namespace vkz {

    class tessellation_state_builder : public graphics_pipeline_builder {
    public:
        tessellation_state_builder(vkz::device device, graphics_pipeline_builder *parent);

        tessellation_state_builder &patch_control_points(uint32_t count);

        tessellation_state_builder &domain_origin(VkTessellationDomainOrigin origin);

        graphics_pipeline_builder &clear();

        VkPipelineTessellationStateCreateInfo &build_tessellation_state();

        void copy(const tessellation_state_builder &source);

    private:
        VkPipelineTessellationStateCreateInfo _info{VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO};
        VkPipelineTessellationDomainOriginStateCreateInfo originStateInfo{
                VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO};
    };
}