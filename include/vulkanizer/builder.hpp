#pragma once

#include "vkz.hpp"
#include "builder_forwards.hpp"
#include <vulkan/vulkan.h>

namespace vkz {
    class Builder{
    public:
        explicit Builder(Device device, Builder* parent = nullptr)
                : _parent{ parent }
                , _device{ device }
        {}

        Builder() = default;

        [[nodiscard]]
        virtual Builder* parent() {
            return _parent;
        }


        [[nodiscard]]
        Device device() const {
            return _device;
        }

    protected:
        Builder* _parent{};
        Device _device{};
    };
}