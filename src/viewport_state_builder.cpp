#include "vulkanizer/graphics_pipeline_builder.hpp"

namespace vkz {

    viewport_state_builder::viewport_state_builder(vkz::device device, graphics_pipeline_builder *parent)
            : graphics_pipeline_builder(device, parent), _viewport_builder{new viewport_builder{this}},
              _scissor_builder{new scissor_builder{this}} {
    }

    viewport_state_builder::viewport_state_builder(viewport_state_builder *parent)
            : graphics_pipeline_builder(parent->_device, parent) {
    }

    viewport_state_builder::~viewport_state_builder() {
        delete _viewport_builder;
        delete _scissor_builder;
    }


    viewport_builder &viewport_state_builder::viewport() {
        return *_viewport_builder;
    }

    scissor_builder &viewport_state_builder::scissor() {
        return *_scissor_builder;
    }

    VkPipelineViewportStateCreateInfo &viewport_state_builder::build_viewport_state() {
        auto &viewports = _viewport_builder->build_viewports();
        auto &scissors = _scissor_builder->build_scissors();
        _info.viewportCount = VKZ_COUNT(viewports);
        _info.pViewports = viewports.data();
        _info.scissorCount = VKZ_COUNT(scissors);
        _info.pScissors = scissors.data();
        return _info;
    }

    void viewport_state_builder::copy(const viewport_state_builder &source) {
        _viewport_builder->copy(*source._viewport_builder);
        _scissor_builder->copy(*source._scissor_builder);
    }

    viewport_state_builder &viewport_state_builder::clear() {
        delete _viewport_builder;
        delete _scissor_builder;
        _viewport_builder = new viewport_builder{this};
        _scissor_builder = new scissor_builder{this};
        return *this;
    }


    viewport_builder::viewport_builder(viewport_state_builder *builder) : viewport_state_builder(builder) {
        reset_scratchpad();
    }

    viewport_builder &viewport_builder::origin(float xValue, float yValue) {
        x(xValue);
        y(yValue);
        return *this;
    }


    viewport_builder &viewport_builder::x(float value) {
        _scratchpad.x = value;
        return *this;
    }

    viewport_builder &viewport_builder::y(float value) {
        _scratchpad.y = value;
        return *this;
    }

    viewport_builder &viewport_builder::dimension(VkExtent2D dim) {
        width(static_cast<float>(dim.width));
        height(static_cast<float>(dim.height));
        return *this;
    }

    viewport_builder &viewport_builder::dimension(uint32_t w, uint32_t h) {
        width(static_cast<float>(w));
        height(static_cast<float>(h));
        return *this;
    }

    viewport_builder &viewport_builder::width(float value) {
        _scratchpad.width = value;
        return *this;
    }

    viewport_builder &viewport_builder::height(float value) {
        _scratchpad.height = value;
        return *this;
    }

    viewport_builder &viewport_builder::min_depth(float value) {
        _scratchpad.maxDepth = value;
        return *this;
    }

    viewport_builder &viewport_builder::max_depth(float value) {
        _scratchpad.maxDepth = value;
        return *this;
    }

    viewport_builder &viewport_builder::add() {
        if (_scratchpad.width <= 0 || _scratchpad.height <= 0) {
            throw std::runtime_error{"viewport width and height required"};
        }
        _viewports.push_back(_scratchpad);
        reset_scratchpad();
        return *this;
    }

    std::vector<VkViewport> &viewport_builder::build_viewports() {
        if (_viewports.empty()) {
            throw std::runtime_error{"at least one viewport should be provided"};
        }
        return _viewports;
    }

    void viewport_builder::reset_scratchpad() {
        _scratchpad = VkViewport{0, 0, 0, 0, 0, 1};
    }

    bool viewport_builder::ready() const {
        return
                (_scratchpad.width > 0 && _scratchpad.height > 0)
                && (_scratchpad.minDepth != _scratchpad.maxDepth);
    }

    void viewport_builder::checkpoint() {
        if (ready()) {
            add();
        }
    }

    void viewport_builder::copy(const viewport_builder &source) {
        _viewports = decltype(_viewports)(source._viewports.begin(), source._viewports.end());
    }

    viewport_state_builder *viewport_builder::parent() {
        return dynamic_cast<viewport_state_builder *>(_parent);
    }

    viewport_builder &viewport_builder::viewport() {
        checkpoint();
        return *this;
    }

    scissor_builder &viewport_builder::scissor() {
        checkpoint();
        return parent()->scissor();
    }

    scissor_builder::scissor_builder(viewport_state_builder *builder)
            : viewport_state_builder(builder) {
        reset_scratchpad();
    }

    scissor_builder &scissor_builder::offset(int32_t x, int32_t y) {
        _scratchpad.offset = VkOffset2D{x, y};
        return *this;
    }

    scissor_builder &scissor_builder::extent(int32_t width, int32_t height) {
        _scratchpad.extent.width = width;
        _scratchpad.extent.height = height;
        return *this;
    }

    scissor_builder &scissor_builder::extent(VkExtent2D value) {
        _scratchpad.extent = value;
        return *this;
    }

    scissor_builder &scissor_builder::add() {
        if (_scratchpad.extent.width <= 0 || _scratchpad.extent.height <= 0) {
            throw std::runtime_error{"scissor width and height required"};
        }
        _scissors.push_back(_scratchpad);
        reset_scratchpad();
        return *this;
    }

    void scissor_builder::reset_scratchpad() {
        _scratchpad = {{0u, 0u},
                       {0u, 0u}};
    }

    std::vector<VkRect2D> &scissor_builder::build_scissors() {
        return _scissors;
    }

    void scissor_builder::checkpoint() {
        if (ready()) {
            add();
        }
    }

    viewport_state_builder *scissor_builder::parent() {
        return dynamic_cast<viewport_state_builder *>(_parent);
    }

    bool scissor_builder::ready() const {
        return _scratchpad.extent.width > 0 && _scratchpad.extent.height > 0;
    }

    void scissor_builder::copy(const scissor_builder &source) {
        _scissors = decltype(_scissors)(source._scissors.begin(), source._scissors.end());
    }

    viewport_builder &scissor_builder::viewport() {
        checkpoint();
        return parent()->viewport();
    }

    scissor_builder &scissor_builder::scissor() {
        checkpoint();
        return *this;
    }

}