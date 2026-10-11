#pragma once

#include <algorithm>
#include <cstdint>
#include "guikit/core/Types.hpp"

namespace guikit {

/// Direction of primary layout axis.
enum class FlexDirection : uint8_t {
    Row,    ///< Children laid out horizontally from left to right
    Column, ///< Children laid out vertically from top to bottom
    Wrap,   ///< Children laid out horizontally and wrapped to subsequent lines upon overflow
    Grid    ///< 12-column responsive grid with automatic line wrapping
};

/// Sizing policy along an axis.
enum class SizeMode : uint8_t {
    Stretch,    ///< Expands to fill available space allocated by parent container
    Fit,        ///< Shrinks to hug content / intrinsic child sizes
    Fixed       ///< Explicit dimension in points
};

/// Alignment along the cross axis (perpendicular to FlexDirection).
enum class Alignment : uint8_t {
    Start,      ///< Align to start of cross axis (Top in Row, Left in Column)
    Center,     ///< Center along cross axis
    End,        ///< Align to end of cross axis (Bottom in Row, Right in Column)
    Stretch     ///< Stretch to fill full cross-axis dimension
};

/// Type alias for backward compatibility.
using Align = Alignment;

/// Distribution along the main axis (parallel to FlexDirection) when space is not fully stretched.
enum class Justify : uint8_t {
    Start,          ///< Pack children at start of main axis
    Center,         ///< Center children along main axis
    End,            ///< Pack children at end of main axis
    SpaceBetween    ///< Distribute items evenly; first item at start, last at end
};

/// Inset margins or internal padding on 4 sides.
struct Padding {
    float left{0.0f};
    float top{0.0f};
    float right{0.0f};
    float bottom{0.0f};

    constexpr Padding() noexcept = default;
    constexpr Padding(float l, float t, float r, float b) noexcept
        : left(l), top(t), right(r), bottom(b) {}

    [[nodiscard]] static constexpr Padding All(float val) noexcept {
        return {val, val, val, val};
    }

    [[nodiscard]] static constexpr Padding Symmetric(float horizontal, float vertical) noexcept {
        return {horizontal, vertical, horizontal, vertical};
    }

    [[nodiscard]] constexpr float Horizontal() const noexcept {
        return left + right;
    }

    [[nodiscard]] constexpr float Vertical() const noexcept {
        return top + bottom;
    }
};

} // namespace guikit
