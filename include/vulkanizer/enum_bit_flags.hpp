#pragma once

#include <type_traits>

// Ported from VulkanBase/include/EnumBitFlags.hpp.
// https://www.justsoftwaresolutions.co.uk/cplusplus/using-enum-classes-as-bitfields.html
// Specialize this trait at global scope before using an enum's flag operators.
template <typename E>
struct enable_bitmask_operators {
    static constexpr bool enable = false;
};

template <typename E>
    requires (std::is_enum_v<E> && enable_bitmask_operators<E>::enable)
constexpr E operator|(E lhs, E rhs) noexcept {
    using underlying = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
}

template <typename E>
    requires (std::is_enum_v<E> && enable_bitmask_operators<E>::enable)
constexpr E& operator|=(E& lhs, E rhs) noexcept {
    return lhs = lhs | rhs;
}

template <typename E>
    requires (std::is_enum_v<E> && enable_bitmask_operators<E>::enable)
constexpr E operator&(E lhs, E rhs) noexcept {
    using underlying = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
}

template <typename E>
    requires (std::is_enum_v<E> && enable_bitmask_operators<E>::enable)
constexpr E& operator&=(E& lhs, E rhs) noexcept {
    return lhs = lhs & rhs;
}

template <typename E>
    requires (std::is_enum_v<E> && enable_bitmask_operators<E>::enable)
constexpr E operator~(E value) noexcept {
    using underlying = std::underlying_type_t<E>;
    return static_cast<E>(~static_cast<underlying>(value));
}

// True when all bits in flags are present, including an empty mask.
template <typename E>
    requires (std::is_enum_v<E> && enable_bitmask_operators<E>::enable)
constexpr bool has_flag(E value, E flags) noexcept {
    return (value & flags) == flags;
}
