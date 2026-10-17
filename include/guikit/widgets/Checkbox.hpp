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

/// Clean configuration options for Checkbox widget.
struct CheckboxOptions {
    UiText label{};                                ///< Primary caption text
    UiText description{};                          ///< Optional explanation text below label
    UiText tooltip{};                              ///< Optional floating tooltip text on hover
    Icon icon{};                                   ///< Optional leading icon in external Element label bar
    UiText badge{};                                ///< Optional trailing pill badge in external Element label bar
    UiVariant variant{UiVariant::Primary};          ///< Accent fill color when checked
    ElementLayout layout{ElementLayout::Inline};     ///< Layout geometry: Stacked, Top, Bottom, Inline
    InlineOrder inlineOrder{InlineOrder::ControlFirst}; ///< Sequencing order in Inline layout: LabelFirst, ControlFirst
    Align elementAlign{Align::Start};                ///< Element composition and label alignment (Start, Center, End)
    StretchMode stretch{StretchMode::Compact};     ///< Sizing behavior: Compact, Space, Fill
    std::optional<float> width{std::nullopt};      ///< Control width: in points for Compact/Space (nullopt = auto), or % of bounds (0..100) for Fill (nullopt = 100%)
    std::optional<float> height{std::nullopt};     ///< Explicit control height in points (nullopt = auto from UiSize)
    bool background{false};                        ///< Whether to render a surface backdrop around the outer Element
    float borderWidth{0.0f};                       ///< Outer Element border thickness in points (0.0f = no border)
    std::optional<float> borderRadius{std::nullopt}; ///< Outer Element corner radius in points
    ControlSpec control{};                         ///< Inner Control slot configuration (width, height, align, background, borderWidth, borderRadius)
    bool disabled{false};                          ///< Interaction suppression
    bool indeterminate{false};                     ///< Tri-state / partial check (horizontal dash)
    bool radio{false};                             ///< Renders circular radio button indicator instead of square checkbox
    std::optional<UiSize> size{std::nullopt};      ///< Local size override (nullopt = inherit container size)
};
static_assert(sizeof(CheckboxOptions) <= 320, "CheckboxOptions must stay compact");

/// Autonomous Checkbox widget logic (execution, measuring, and rendering).
class Checkbox {
public:
    /// Primary immediate mode entry point for Checkbox widget with state pointer and options.
    static Response Run(
        Context& ctx,
        bool* value,
        bool checkedState,
        const CheckboxOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Measures intrinsic dimensions based on box size, gap, and label text.
    static Vec2 Measure(
        const CheckboxOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    );
    static Vec2 Measure(
        const CheckboxOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr,
        float availableWidth = 0.0f
    );

    /// Renders the checkbox square, checkmark/dash, and optional label text.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        bool checked,
        const CheckboxOptions& opts,
        UiSize size,
        bool hovered,
        bool pressed,
        bool disabled,
        const UiTheme& theme,
        bool focused = false
    );
};

} // namespace guikit
