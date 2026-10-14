#pragma once

#include <source_location>
#include <string_view>
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Types.hpp"

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
        UiText label,
        const TextOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Measures intrinsic dimensions based on text content and typography metrics.
    static Vec2 Measure(
        const UiText& label,
        const TextOptions& opts,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    );
    static Vec2 Measure(
        const UiText& label,
        const TextOptions& opts,
        ICanvas* canvas = nullptr,
        float availableWidth = 0.0f
    );

    /// Renders text into allocated bounds with alignment, wrapping/truncation, and semantic color.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        const UiText& label,
        const TextOptions& opts,
        bool hovered = false,
        bool disabled = false,
        const UiTheme& theme = UiTheme::Dark()
    );
};

} // namespace guikit
