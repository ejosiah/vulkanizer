#pragma once

#include <vector>

namespace vkz {

    class viewport_builder;

    class scissor_builder;

    class viewport_state_builder : public graphics_pipeline_builder_proxy<viewport_state_builder> {
    public:
        friend class viewport_builder;
        friend class scissor_builder;

        viewport_state_builder(vkz::device device, graphics_pipeline_builder *builder);

        ~viewport_state_builder();

        viewport_builder &viewport();

        scissor_builder &scissor();

        VkPipelineViewportStateCreateInfo &build_viewport_state();

        viewport_state_builder &clear();

        void copy(const viewport_state_builder &source);

    protected:
        viewport_builder *_viewport_builder{nullptr};
        scissor_builder *_scissor_builder{nullptr};
        VkPipelineViewportStateCreateInfo _info{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    };

    class viewport_builder : public graphics_pipeline_builder_proxy<viewport_builder> {
    public:
        explicit viewport_builder(viewport_state_builder *builder);

        viewport_builder &origin(float xValue, float yValue);

        viewport_builder &x(float value);

        viewport_builder &y(float value);

        viewport_builder &width(float value);

        viewport_builder &height(float value);

        viewport_builder &dimension(VkExtent2D dim);

        viewport_builder &dimension(uint32_t width, uint32_t height);

        viewport_builder &min_depth(float value);

        viewport_builder &max_depth(float value);

        viewport_builder &add();

        viewport_builder &viewport();

        scissor_builder &scissor();

        void checkpoint();

        void reset_scratchpad();

        bool ready() const;

        std::vector<VkViewport> &build_viewports();

        void copy(const viewport_builder &source);

    private:
        std::vector<VkViewport> _viewports{};
        VkViewport _scratchpad{};
        viewport_state_builder *_parent{};
    };

    class scissor_builder : public graphics_pipeline_builder_proxy<scissor_builder> {
    public:
        explicit scissor_builder(viewport_state_builder *builder);

        scissor_builder &offset(int32_t x, int32_t y);

        scissor_builder &extent(int32_t width, int32_t height);

        scissor_builder &extent(VkExtent2D value);

        scissor_builder &add();

        void reset_scratchpad();

        std::vector<VkRect2D> &build_scissors();

        bool ready() const;

        void checkpoint();

        viewport_builder &viewport();

        scissor_builder &scissor();

        void copy(const scissor_builder &source);

    private:
        std::vector<VkRect2D> _scissors;
        VkRect2D _scratchpad{};
        viewport_state_builder *_parent{};
    };

}
