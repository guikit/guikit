#pragma once

#include "guikit/context.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"
#include "keel/core.h"
#include "keel/optional.h"

namespace guikit {

class Context;
class ICanvas;

/// Subtitle / description placement relative to the primary heading title bar.
enum class HeadingLayout : keel::u8 {
    Stacked = 0, ///< Title bar on top, subtitle/description stacked underneath (default)
    Overline,    ///< Subtitle/overline above the primary title bar
    Inline       ///< Title bar on the left, subtitle/description inline on the right
};

/// Configuration options for a standalone section `Heading` leaf widget.
struct HeadingOptions {
    UiText label{};                                         ///< Primary heading title text
    UiText description{};                                   ///< Optional secondary subtitle or overline text
    Icon icon{};                                            ///< Optional leading vector icon before title
    UiText badge{};                                         ///< Optional trailing pill badge after title
    HeadingLevel level{HeadingLevel::H3};                   ///< Semantic heading hierarchy level (`H1`–`H6`, default `H3`; independent of `UiSize`)
    HeadingLayout layout{HeadingLayout::Stacked};           ///< Subtitle placement relative to title bar (Stacked, Overline, Inline)
    Align align{Align::Start};                              ///< Horizontal alignment (Start, Center, End)
    AxisSize width{};                                       ///< Horizontal sizing policy: `Auto` (`Stretch`, or `Fit` in `Toolbar`), `Fit`, `Stretch`, or `Fixed(dp)`
    HeadingDivider separator{};                             ///< Built-in section divider placement (None, Bottom, Trailing), stroke color, and thickness
    UiText tooltip{};                                       ///< Optional hover tooltip text
    bool disabled{false};                                   ///< Whether the heading is rendered in a disabled state
};

/// Standalone section heading leaf widget with `HeadingMetrics` typography (`H1`–`H6`, default `H3`) and built-in dividers.
namespace detail {

class Heading {
public:
    [[nodiscard]] static keel::f32 ComputeBottomSeparatorSpace(
        const HeadingOptions& opts,
        const UiTheme& theme
    ) noexcept;

    static Response Run(
        Context& ctx,
        const HeadingOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    static Vec2 Measure(
        const HeadingOptions& opts,
        ICanvas* canvas,
        const UiTheme& theme,
        keel::f32 availableWidth = 0.0f
    ) noexcept;

    static Vec2 Measure(
        const HeadingOptions& opts,
        keel::f32 availableWidth,
        ICanvas* canvas,
        const UiTheme& theme
    ) noexcept {
        return Measure(opts, canvas, theme, availableWidth);
    }

    static Vec2 Measure(
        const HeadingOptions& opts,
        keel::f32 availableWidth = 0.0f,
        ICanvas* canvas = nullptr
    ) noexcept {
        return Measure(opts, canvas, UiTheme::Dark(), availableWidth);
    }

    static keel::f32 ResolveHeight(
        const HeadingOptions& opts,
        keel::f32 availableWidth,
        ICanvas* canvas,
        const UiTheme& theme
    ) noexcept {
        return Measure(opts, canvas, theme, availableWidth).y;
    }

    static void Render(
        ICanvas& canvas,
        Rect bounds,
        const HeadingOptions& opts,
        const UiTheme& theme,
        bool disabled = false,
        bool hovered = false,
        bool active = false
    ) noexcept;
};

} // namespace detail

inline Response Heading(
    Context& ui,
    HeadingOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    return detail::Heading::Run(ui, opts, loc);
}

} // namespace guikit
