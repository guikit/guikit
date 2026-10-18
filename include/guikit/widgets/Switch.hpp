#pragma once

#include <optional>
#include <source_location>
#include <string_view>
#include "guikit/core/Control.hpp"
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Clean configuration options for Switch widget.
struct SwitchOptions {
    UiText label{};                                ///< Primary external caption text
    UiText textLeft{};                             ///< Optional companion text inside control to the left of the switch track (off state)
    UiText textRight{};                            ///< Optional companion text inside control to the right of the switch track (on state)
    UiText description{};                          ///< Optional external explanation text
    UiText tooltip{};                              ///< Optional floating tooltip text on hover
    Icon icon{};                                   ///< Optional leading icon in external Element label bar
    UiText badge{};                                ///< Optional trailing pill badge in external Element label bar
    UiVariant variant{UiVariant::Primary};          ///< Accent fill color when switched ON
    ElementLayout layout{ElementLayout::Inline};     ///< Layout geometry: Stacked, Top, Bottom, Inline
    InlineOrder inlineOrder{InlineOrder::ControlFirst}; ///< Sequencing order in Inline layout: LabelFirst, ControlFirst
    Align elementAlign{Align::Start};                ///< Element composition and label alignment (Start, Center, End)
    bool symmetric{false};                         ///< Neutral/symmetric track style (highlights active side label)
    StretchMode stretch{StretchMode::Compact};     ///< Sizing behavior: Compact, Space, Fill
    std::optional<float> width{std::nullopt};      ///< Control width: in points for Compact/Space (nullopt = auto), or % of bounds (0..100) for Fill (nullopt = 100%)
    std::optional<float> height{std::nullopt};     ///< Explicit control height in points (nullopt = auto from UiSize)
    bool background{false};                        ///< Whether to render a surface backdrop around the outer Element
    float borderWidth{0.0f};                       ///< Outer Element border thickness in points (0.0f = no border)
    std::optional<float> borderRadius{std::nullopt}; ///< Outer Element corner radius in points
    StateColor borderColor{};                      ///< Per-state outer Element border color (normal, hover, active, disabled)
    StateColor bgColor{};                          ///< Per-state outer Element background color (normal, hover, active, disabled)
    ControlSpec control{};                         ///< Inner Control slot configuration (width, height, align, background, borderWidth, borderRadius, borderColor, bgColor)
    bool disabled{false};                          ///< Interaction suppression
    std::optional<UiSize> size{std::nullopt};      ///< Local size override (nullopt = inherit container size)
};
static_assert(sizeof(SwitchOptions) <= 840, "SwitchOptions must stay compact");

/// Autonomous Switch widget logic (execution, measuring, and rendering).
class Switch {
public:
    /// Primary immediate mode entry point for Switch widget with state pointer and options.
    static Response Run(
        Context& ctx,
        bool* value,
        bool checkedState,
        const SwitchOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Measures intrinsic dimensions based on track size, gap, and label text.
    static Vec2 Measure(
        const SwitchOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    );
    static Vec2 Measure(
        const SwitchOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr,
        float availableWidth = 0.0f
    );

    /// Renders the switch track, knob, and optional label text.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        bool checked,
        const SwitchOptions& opts,
        UiSize size,
        bool hovered,
        bool pressed,
        bool disabled,
        const UiTheme& theme,
        bool focused = false
    );
};

} // namespace guikit
