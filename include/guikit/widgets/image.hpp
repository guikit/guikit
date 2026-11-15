#pragma once

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
    keel::StringRef source{};                           ///< Image file path (.svg, .png, .jpg) or inline <svg>
    UiText alt{};                                       ///< Fallback text when image is missing or fails to load
    UiText tooltip{};                                   ///< Optional hover tooltip text
    ObjectFit fit{ObjectFit::Contain};                  ///< Fitting policy inside the control box
    Align alignX{Align::Center};                        ///< Horizontal alignment of fitted image inside control box
    Align alignY{Align::Center};                        ///< Vertical alignment of fitted image inside control box
    AxisSize width{};                                   ///< Horizontal sizing policy (`Auto` -> `Fit`, `Stretch`, or `Fixed(dp)`)
    AxisSize height{SizeMode::Fit};                     ///< Vertical sizing policy (`Auto`/`Fit` = aspect ratio, `Stretch`, or `Fixed(dp)`)
    keel::Optional<float> maxHeight{keel::kNullOpt};    ///< Optional maximum height cap in px
    StateColor background{};                            ///< Per-state image control box backdrop color (Default/None = off, Surface/Primary/etc. = on)
    Border border{};                                    ///< Image control box border stroke & corner radius configuration
    bool checkerboard{false};                           ///< Render transparency checkerboard pattern behind image
    keel::u8 opacity{255};                               ///< Image opacity [0..255]
    bool disabled{false};
    const UiTheme* theme{nullptr};
    keel::Optional<float> scale{keel::kNullOpt};
    keel::Optional<bool> debugBounds{keel::kNullOpt};
    keel::Optional<bool> inspectMode{keel::kNullOpt};
};

/// Internal frame-arena payload for Image nodes.
struct ImagePayload {
    keel::StringRef source{};
    ImageOptions opts{};
    mutable ICanvas* lastCanvas{nullptr};
};

/// Stateless leaf Control widget for rendering vector (SVG) and raster (PNG, JPG) images.
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
        float availableWidth = 0.0f
    );
    static Vec2 Measure(
        keel::StringRef source,
        const ImageOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr,
        float availableWidth = 0.0f
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
        float radius,
        const UiTheme& theme,
        float cellSize = 10.0f
    );
};

} // namespace guikit
