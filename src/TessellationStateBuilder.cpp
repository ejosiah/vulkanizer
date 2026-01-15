#include "vulkanizer/TessellationStateBuilder.hpp"

namespace vkz {

    TessellationStateBuilder::TessellationStateBuilder(Device device, GraphicsPipelineBuilder *parent)
            : GraphicsPipelineBuilder(device, parent) {}

    TessellationStateBuilder &TessellationStateBuilder::patchControlPoints(uint32_t count) {
        _info.patchControlPoints = count;
        return *this;
    }

    TessellationStateBuilder &TessellationStateBuilder::domainOrigin(VkTessellationDomainOrigin origin) {
        originStateInfo.domainOrigin = origin;
        return *this;
    }

    VkPipelineTessellationStateCreateInfo &TessellationStateBuilder::buildTessellationState() {
        _info.pNext = &originStateInfo;
        return _info;
    }

    void TessellationStateBuilder::copy(const TessellationStateBuilder &source) {
        originStateInfo = source.originStateInfo;
        _info = source._info;
    }


    GraphicsPipelineBuilder &TessellationStateBuilder::clear() {
        auto graphicsPipelineBuilder = reinterpret_cast<GraphicsPipelineBuilder *>(_parent);
        _info = {VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO};
        return *graphicsPipelineBuilder;
    }

}