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
    std::string_view title{};
    std::string_view subtitle{};
    Icon icon{};                                   ///< Optional vector icon before title
    std::string_view badge{};                      ///< Optional pill badge after title
    UiVariant variant{UiVariant::Default};         ///< Accent color variant
    std::optional<UiSize> size{std::nullopt};      ///< Local size override (nullopt = inherit container size)
    bool divider{false};                           ///< Whether to render a horizontal divider line
    HeadingDivider dividerStyle{HeadingDivider::Bottom}; ///< Divider placement style (Bottom or Trailing)
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
    static Vec2 Measure(const HeadingOptions& opts, UiSize size, ICanvas* canvas, const UiTheme& theme);
    static Vec2 Measure(const HeadingOptions& opts, UiSize size, ICanvas* canvas = nullptr);

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
