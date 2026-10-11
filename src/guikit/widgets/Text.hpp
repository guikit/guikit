#pragma once

#include <source_location>
#include <string_view>
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/layout/LayoutTypes.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Compact 3-byte configuration options for Text widget based purely on design tokens.
struct TextOptions {
    TextRole role{TextRole::BodyMedium};
    UiVariant variant{UiVariant::Default};
    Alignment align{Alignment::Start};
};
static_assert(sizeof(TextOptions) == 3, "TextOptions must remain exactly 3 bytes");

/// Autonomous Text widget logic (execution, measuring, and rendering).
class Text {
public:
    /// Primary immediate mode entry point for Text widget.
    static Response Run(
        Context& ctx,
        std::string_view label,
        const TextOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Measures intrinsic dimensions based on text content and typography metrics.
    static Vec2 Measure(std::string_view label, const TextOptions& opts, ICanvas* canvas, const UiTheme& theme);
    static Vec2 Measure(std::string_view label, const TextOptions& opts, ICanvas* canvas = nullptr);

    /// Renders text into allocated bounds with alignment and semantic color.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        std::string_view label,
        const TextOptions& opts,
        bool hovered = false,
        bool disabled = false,
        const UiTheme& theme = UiTheme::Dark()
    );
};

} // namespace guikit
