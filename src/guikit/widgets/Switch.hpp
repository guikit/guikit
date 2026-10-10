#pragma once

#include <optional>
#include <source_location>
#include <string_view>
#include "guikit/core/Response.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Clean configuration options for Switch widget.
struct SwitchOptions {
    std::string_view label{};                      ///< Primary caption text
    std::string_view description{};                ///< Optional explanation text below label
    UiVariant variant{UiVariant::Primary};          ///< Accent fill color when switched ON
    Placement labelPlacement{Placement::Trailing}; ///< Placement of label relative to the switch track
    bool disabled{false};                          ///< Interaction suppression
    std::optional<UiSize> size{std::nullopt};      ///< Local size override (nullopt = inherit container size)
};
static_assert(sizeof(SwitchOptions) <= 40, "SwitchOptions must stay compact");

/// Standardized layout and geometry metrics for a Switch widget.
struct SwitchMetrics {
    Vec2 trackSize;
    float knobDiameter;
    float knobPadding;
    float gap;
    float fontSize;
    float descFontSize;
    float descGap;
};

/// Returns standardized metrics for Switch according to UiSize.
[[nodiscard]] constexpr SwitchMetrics GetSwitchMetrics(UiSize size) noexcept {
    switch (size) {
        case UiSize::None:
        case UiSize::Md: return { {36.0f, 20.0f}, 16.0f, 2.0f, 10.0f, 14.0f, 12.0f, 3.0f };
        case UiSize::Xs: return { {24.0f, 14.0f}, 10.0f, 2.0f,  6.0f, 11.0f,  9.0f, 2.0f };
        case UiSize::Sm: return { {28.0f, 16.0f}, 12.0f, 2.0f,  8.0f, 13.0f, 11.0f, 2.0f };
        case UiSize::Lg: return { {44.0f, 24.0f}, 20.0f, 2.0f, 12.0f, 16.0f, 13.0f, 3.0f };
        case UiSize::Xl: return { {52.0f, 28.0f}, 24.0f, 2.0f, 14.0f, 18.0f, 14.0f, 4.0f };
    }
    return { {36.0f, 20.0f}, 16.0f, 2.0f, 10.0f, 14.0f, 12.0f, 3.0f };
}

/// Autonomous Switch widget logic (execution, measuring, and rendering).
class Switch {
public:
    /// Primary immediate mode entry point for Switch widget with state pointer and label.
    static Response Run(
        Context& ctx,
        std::string_view label,
        bool* value,
        bool checkedState,
        const SwitchOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Measures intrinsic dimensions based on track size, gap, and label text.
    static Vec2 Measure(
        std::string_view label,
        const SwitchOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr
    );

    /// Renders the switch track, knob, and optional label text.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        std::string_view label,
        bool checked,
        const SwitchOptions& opts,
        UiSize size,
        bool hovered = false,
        bool pressed = false,
        bool disabled = false,
        ThemeMode theme = ThemeMode::Dark
    );
};

} // namespace guikit
