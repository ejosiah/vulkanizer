#pragma once

#include "vkz.hpp"
#include "builder_forwards.hpp"

namespace vkz {
    class builder_base{
    public:
        explicit builder_base(vkz::device device, builder_base* parent = nullptr)
                : _parent{ parent }
                , _device{ device }
        {}

        builder_base() = default;

        virtual builder_base* parent() {
            return _parent;
        }


        vkz::device device() const {
            return _device;
        }

    protected:
        builder_base* _parent{};
        vkz::device _device{};
    };
}
