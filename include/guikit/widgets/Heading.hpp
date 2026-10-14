#pragma once

#include <optional>
#include <source_location>
#include <string_view>
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/graphics/Icon.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Divider placement style for Heading widget.
enum class HeadingDivider : uint8_t {
    None = 0,
    Bottom,   ///< Full-width horizontal divider line underneath heading
    Trailing  ///< Inline horizontal divider line extending from title to right edge
};

/// Clean configuration options for a composite Heading widget (Title + Subtitle + Icon + Badge).
struct HeadingOptions {
    UiText title{};                                ///< Primary heading title text
    UiText subtitle{};                             ///< Optional secondary subtitle text
    Icon icon{};                                   ///< Optional vector icon before title
    UiText badge{};                                ///< Optional pill badge after title (supports independent UiVariant)
    UiVariant variant{UiVariant::Default};         ///< Accent color variant
    std::optional<UiSize> size{std::nullopt};      ///< Local size override (nullopt = inherit container size)
    bool divider{false};                           ///< Whether to render a horizontal divider line
    HeadingDivider dividerStyle{HeadingDivider::Bottom}; ///< Divider placement style (Bottom or Trailing)
    bool wordWrap{false};                          ///< Whether subtitle wraps into multiple lines when space is constrained
};

/// Autonomous Heading widget logic (execution, measuring, and rendering).
class Heading {
public:
    /// Primary immediate mode entry point for Heading widget.
    static Response Run(
        Context& ctx,
        const HeadingOptions& opts,
        std::source_location loc = std::source_location::current()
    );

    /// Measures intrinsic dimensions based on title, subtitle, icon, badge, and UiSize.
    /// When opts.wordWrap is true and availableWidth > 0, accounts for wrapped multi-line subtitle height.
    static Vec2 Measure(const HeadingOptions& opts, UiSize size, float availableWidth, ICanvas* canvas, const UiTheme& theme);
    static Vec2 Measure(const HeadingOptions& opts, UiSize size, float availableWidth = 0.0f, ICanvas* canvas = nullptr);

    /// Resolves required height given available width, accounting for multi-line word wrapping if enabled.
    static float ResolveHeight(
        const HeadingOptions& opts,
        UiSize size,
        float availableWidth,
        ICanvas* canvas,
        const UiTheme& theme
    );

    /// Renders the composite heading into allocated bounds.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        const HeadingOptions& opts,
        UiSize size,
        const UiTheme& theme = UiTheme::Dark()
    );
};

} // namespace guikit
