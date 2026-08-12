#pragma once

namespace vkz {

    class viewport_builder;

    class scissor_builder;

    class viewport_state_builder : public graphics_pipeline_builder {
    public:
        viewport_state_builder(vkz::device device, graphics_pipeline_builder *builder);

        explicit viewport_state_builder(viewport_state_builder *parent);

        ~viewport_state_builder();

        virtual viewport_builder &viewport();

        virtual scissor_builder &scissor();

        VkPipelineViewportStateCreateInfo &build_viewport_state();

        viewport_state_builder &clear();

        void copy(const viewport_state_builder &source);

    protected:
        viewport_builder *_viewport_builder{nullptr};
        scissor_builder *_scissor_builder{nullptr};
        VkPipelineViewportStateCreateInfo _info{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    };

    class viewport_builder : public viewport_state_builder {
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

        viewport_state_builder *parent() override;

        viewport_builder &viewport() override;

        scissor_builder &scissor() override;

        void checkpoint();

        void reset_scratchpad();

        bool ready() const;

        std::vector<VkViewport> &build_viewports();

        void copy(const viewport_builder &source);

    private:
        std::vector<VkViewport> _viewports{};
        VkViewport _scratchpad{};
    };

    class scissor_builder : public viewport_state_builder {
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

        viewport_builder &viewport() override;

        scissor_builder &scissor() override;

        viewport_state_builder *parent() override;

        void copy(const scissor_builder &source);

    private:
        std::vector<VkRect2D> _scissors;
        VkRect2D _scratchpad{};
    };

}
