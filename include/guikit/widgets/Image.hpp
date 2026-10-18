#pragma once

#include <cstdint>
#include <optional>
#include <source_location>
#include <string_view>
#include "guikit/core/Control.hpp"
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Configuration options for the stateless leaf Image control widget.
struct ImageOptions {
    std::string_view source{};                          ///< Image file path (.svg, .png, .jpg) or inline <svg>
    UiText alt{};                                       ///< Fallback text when image is missing or fails to load
    std::string_view tooltip{};                         ///< Optional hover tooltip text
    ObjectFit fit{ObjectFit::Contain};                  ///< Fitting policy inside the control box
    Align align{Align::Start};                          ///< Standalone horizontal alignment of compact image control
    Align alignX{Align::Center};                        ///< Horizontal alignment of fitted image inside control box
    Align alignY{Align::Center};                        ///< Vertical alignment of fitted image inside control box
    StretchMode stretch{StretchMode::Compact};          ///< Compact (natural/explicit size), Space, or Fill
    std::optional<float> width{std::nullopt};           ///< Explicit width in px (Compact/Space) or % 0..100 (Fill)
    std::optional<float> height{std::nullopt};          ///< Explicit height in px (nullopt = auto from aspect ratio)
    std::optional<float> maxHeight{std::nullopt};       ///< Optional maximum height cap in px
    SizeMode heightMode{SizeMode::Fit};                 ///< Fit (auto/fixed) or Stretch (expand to fill parent height)
    StateColor background{};                            ///< Per-state image control box backdrop color (Default/None = off, Surface/Primary/etc. = on)
    Border border{};                                    ///< Image control box border stroke & corner radius configuration
    bool checkerboard{false};                           ///< Render transparency checkerboard pattern behind image
    uint8_t opacity{255};                               ///< Image opacity [0..255]
    bool disabled{false};
};

/// Internal frame-arena payload for Image nodes.
struct ImagePayload {
    std::string_view source{};
    ImageOptions opts{};
    mutable ICanvas* lastCanvas{nullptr};
};

/// Stateless leaf Control widget for rendering vector (SVG) and raster (PNG, JPG) images.
class Image {
public:
    /// Immediate mode entry point for Image widget.
    static Response Run(
        Context& ctx,
        std::string_view source,
        const ImageOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Measures total dimensions of the composite Image Element given constrained available width.
    static Vec2 Measure(
        std::string_view source,
        const ImageOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    );
    static Vec2 Measure(
        std::string_view source,
        const ImageOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr,
        float availableWidth = 0.0f
    );

    /// Renders the Image widget, optional checkerboard/frame, and external label/description.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        std::string_view source,
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
