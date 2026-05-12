#pragma once
#include "vkz.hpp"

template<typename Caller>
struct blend_factor{

    explicit blend_factor(Caller* caller = nullptr)
    : _caller{ caller }
    , value{VK_BLEND_FACTOR_ZERO }
    {}

    Caller& zero(){
        value = VK_BLEND_FACTOR_ZERO;
        return *_caller;
    }

    Caller& one(){
        value = VK_BLEND_FACTOR_ONE;
        return *_caller;
    }

    Caller& src_color(){
        value = VK_BLEND_FACTOR_SRC_COLOR;
        return *_caller;
    }

    Caller& one_minus_src_color(){
        value = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        return *_caller;
    }

    Caller& dst_color(){
        value = VK_BLEND_FACTOR_DST_COLOR;
        return *_caller;
    }

    Caller& one_minus_dst_color(){
        value = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        return *_caller;
    }

    Caller& src_alpha(){
        value = VK_BLEND_FACTOR_SRC_ALPHA;
        return *_caller;
    }

    Caller& one_minus_src_alpha(){
        value = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        return *_caller;
    }

    Caller& dst_alpha(){
        value = VK_BLEND_FACTOR_DST_ALPHA;
        return *_caller;
    }

    Caller& one_minus_dst_alpha(){
        value = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        return *_caller;
    }

    Caller& constant_color(){
        value = VK_BLEND_FACTOR_CONSTANT_COLOR;
        return *_caller;
    }

    Caller& one_minus_constant_color(){
        value = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        return *_caller;
    }

    Caller& constant_alpha(){
        value = VK_BLEND_FACTOR_CONSTANT_ALPHA;
        return *_caller;
    }

    Caller& one_minus_constant_alpha(){
        value = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
        return *_caller;
    }


    Caller& src_alphaSaturate(){
        value = VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
        return *_caller;
    }

    Caller& src1_color(){
        value = VK_BLEND_FACTOR_SRC1_COLOR;
        return *_caller;
    }

    Caller& one_minus_src1_color(){
        value = VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
        return *_caller;
    }
    Caller& src1_alpha(){
        value = VK_BLEND_FACTOR_SRC1_ALPHA;
        return *_caller;
    }

    Caller& one_minus_src1_alpha(){
        value = VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
        return *_caller;
    }

    Caller* _caller;
    VkBlendFactor value;
};
