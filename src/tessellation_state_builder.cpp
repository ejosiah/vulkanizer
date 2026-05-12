#include "vulkanizer/graphics_pipeline_builder.hpp"

namespace vkz {

    tessellation_state_builder::tessellation_state_builder(vkz::device device, graphics_pipeline_builder *parent)
            : graphics_pipeline_builder(device, parent) {}

    tessellation_state_builder &tessellation_state_builder::patch_control_points(uint32_t count) {
        _info.patchControlPoints = count;
        return *this;
    }

    tessellation_state_builder &tessellation_state_builder::domain_origin(VkTessellationDomainOrigin origin) {
        originStateInfo.domainOrigin = origin;
        return *this;
    }

    VkPipelineTessellationStateCreateInfo &tessellation_state_builder::build_tessellation_state() {
        _info.pNext = &originStateInfo;
        return _info;
    }

    void tessellation_state_builder::copy(const tessellation_state_builder &source) {
        originStateInfo = source.originStateInfo;
        _info = source._info;
    }


    graphics_pipeline_builder &tessellation_state_builder::clear() {
        auto parent_builder = reinterpret_cast<graphics_pipeline_builder *>(_parent);
        _info = {VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO};
        return *parent_builder;
    }

}
