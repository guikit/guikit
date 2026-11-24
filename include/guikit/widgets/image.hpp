#pragma once

#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "keel/core.h"
#include "keel/optional.h"
#include "keel/ref.h"

namespace guikit {

class Context;
class ICanvas;

/// Configuration options for the stateless leaf Image control widget.
struct ImageOptions {
    keel::StringRef source{};          ///< Image file path (.svg, .png, .jpg) or inline <svg>
    UiText alt{};                      ///< Fallback text when image is missing or fails to load
    ObjectFit fit{ObjectFit::Contain}; ///< Fitting policy inside the control box
    Align alignX{Align::Center};       ///< Horizontal alignment of fitted image inside control box
    Align alignY{Align::Center};       ///< Vertical alignment of fitted image inside control box
    bool checkerboard{false};          ///< Render transparency checkerboard pattern behind image
    keel::u8 opacity{255};             ///< Image opacity [0..255]
    UiText tooltip{};                  ///< Optional hover tooltip text
    bool disabled{false};                            ///< Interaction suppression
    UiSize size{UiSize::Md};                         ///< Control size token (`Xs`..`Xl`, `Dp(...)`, `Factor(...)`)
    AxisSize width{};                                ///< Horizontal sizing policy (`Auto` -> `Fit`, `Stretch`, or explicit `dp`)
    AxisSize height{};                               ///< Vertical sizing policy (`Auto` -> `Fit`, `Stretch`, or explicit `dp`)
    keel::Optional<keel::f32> minWidth{keel::kNullOpt};  ///< Optional minimum width in logical dp
    keel::Optional<keel::f32> maxWidth{keel::kNullOpt};  ///< Optional maximum width in logical dp
    keel::Optional<keel::f32> minHeight{keel::kNullOpt}; ///< Optional minimum height in logical dp
    keel::Optional<keel::f32> maxHeight{keel::kNullOpt}; ///< Optional maximum height in logical dp
    StateColor color{};                              ///< Optional semantic tint color override
    StateColor background{};                         ///< Optional backdrop background fill (transparent by default)
    Border border{};                                 ///< Optional border width, color, and radius override
};

/// Internal frame-arena payload for Image nodes.
struct ImagePayload {
    keel::StringRef source{};
    ImageOptions opts{};
    mutable ICanvas* lastCanvas{nullptr};
};

/// Stateless leaf Control widget for rendering vector (SVG) and raster (PNG, JPG) images.
namespace detail {

class Image {
public:
    /// Immediate mode entry point for Image widget.
    static Response Run(
        Context& ctx,
        keel::StringRef source,
        const ImageOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Measures total dimensions of the composite Image Element given constrained available width.
    static Vec2 Measure(
        keel::StringRef source,
        const ImageOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        keel::f32 availableWidth = 0.0f
    );
    static Vec2 Measure(
        keel::StringRef source,
        const ImageOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr,
        keel::f32 availableWidth = 0.0f
    );

    /// Renders the Image widget, optional checkerboard/frame, and external label/description.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        keel::StringRef source,
        const ImageOptions& opts,
        UiSize size,
        bool hovered = false,
        bool disabled = false,
        const UiTheme& theme = UiTheme::Dark()
    );

    /// Shared utility: draws a clipped transparency checkerboard grid inside `rect`.
    static void DrawCheckerboard(
        ICanvas& canvas,
        Rect rect,
        keel::f32 radius,
        const UiTheme& theme,
        keel::f32 cellSize = 10.0f
    );
};

} // namespace detail

inline Response Image(
    Context& ui,
    keel::StringRef source,
    ImageOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    return detail::Image::Run(ui, source, opts, loc);
}

inline Response Image(
    Context& ui,
    ImageOptions opts,
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    return detail::Image::Run(ui, opts.source, opts, loc);
}

} // namespace guikit
