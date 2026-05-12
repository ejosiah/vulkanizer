#pragma once
#include "vulkanizer/vkz.hpp"

template <typename Caller>
struct logic_op{
    explicit logic_op(Caller* caller = nullptr)
    : _caller{caller}
    , value{VK_LOGIC_OP_CLEAR}
    , enabled{ false }
    {}

    Caller& enable(){
        enabled = true;
        return *_caller;
    }

    Caller& disable(){
        enabled = false;
        return *_caller;
    }

    Caller& clear() {
        value = VK_LOGIC_OP_CLEAR;
        return *_caller;
    }

    Caller& and_op(){
        value = VK_LOGIC_OP_AND;
        return *_caller;
    }

    Caller& and_reverse(){
        value = VK_LOGIC_OP_AND_REVERSE;
        return *_caller;
    }

    Caller& copy(){
        value = VK_LOGIC_OP_COPY;
        return *_caller;
    }

    Caller& and_inverted(){
        value = VK_LOGIC_OP_AND_INVERTED;
        return *_caller;
    }

    Caller& no_op(){
        value = VK_LOGIC_OP_NO_OP;
        return *_caller;
    }

    Caller& xor_op(){
        value = VK_LOGIC_OP_XOR;
        return *_caller;
    }

    Caller& or_op(){
        value = VK_LOGIC_OP_OR;
        return *_caller;
    }

    Caller& equivalent(){
        value = VK_LOGIC_OP_EQUIVALENT;
        return *_caller;
    }

    Caller& invert(){
        value = VK_LOGIC_OP_INVERT;
        return *_caller;
    }

    Caller& or_reverse(){
        value = VK_LOGIC_OP_OR_REVERSE;
        return *_caller;
    }

    Caller& copy_inverted(){
        value = VK_LOGIC_OP_COPY_INVERTED;
        return *_caller;
    }

    Caller& or_inverted(){
        value = VK_LOGIC_OP_OR_INVERTED;
        return *_caller;
    }

    Caller& nand(){
        value = VK_LOGIC_OP_NAND;
        return *_caller;
    }

    Caller& set(){
        value = VK_LOGIC_OP_SET;
        return *_caller;
    }

    VkLogicOp value;
    bool enabled;
private:
    Caller* _caller;
};
