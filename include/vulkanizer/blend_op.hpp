#pragma once

#include "vkz.hpp"

template<typename Caller>
struct blend_op{
    explicit blend_op(Caller* caller = nullptr)
    : _caller{ caller }
    , value{VK_BLEND_OP_ADD}
    {
    }

    Caller& add(){
        value = VK_BLEND_OP_ADD;
        return *_caller;
    }

    Caller& subtract(){
        value = VK_BLEND_OP_SUBTRACT;
        return *_caller;
    }

    Caller& reverse_subtract(){
        value = VK_BLEND_OP_REVERSE_SUBTRACT;
        return *_caller;
    }

    Caller& min(){
        value = VK_BLEND_OP_MIN;
        return *_caller;
    }

    Caller& max(){
        value = VK_BLEND_OP_MAX;
        return *_caller;
    }

    Caller* _caller;
    VkBlendOp value;
};
