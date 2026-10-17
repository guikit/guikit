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

/// Configuration options for the stateless leaf Image widget.
struct ImageOptions {
    std::string_view source{};                          ///< Image file path (.svg, .png, .jpg) or inline <svg>
    UiText label{};                                     ///< Optional external heading/label
    UiText description{};                               ///< Optional external caption/description
    Icon icon{};                                        ///< Optional leading icon in external Element label bar
    UiText badge{};                                     ///< Optional trailing pill badge in external Element label bar
    UiVariant variant{UiVariant::Default};              ///< Semantic variant for external Element label bar / divider
    UiText alt{};                                       ///< Fallback text when image is missing or fails to load
    std::string_view tooltip{};                         ///< Optional hover tooltip text
    ObjectFit fit{ObjectFit::Contain};                  ///< Fitting policy inside the control box
    ElementLayout layout{ElementLayout::Stacked};       ///< Compound layout: Stacked, Top, Bottom, or Inline
    InlineOrder inlineOrder{InlineOrder::LabelFirst};   ///< Sequencing order when layout == ElementLayout::Inline
    Align elementAlign{Align::Start};                   ///< Horizontal alignment of external texts & compact control
    Align alignX{Align::Center};                        ///< Horizontal alignment of fitted image inside control box
    Align alignY{Align::Center};                        ///< Vertical alignment of fitted image inside control box
    StretchMode stretch{StretchMode::Compact};          ///< Compact (natural/explicit size), Space, or Fill
    std::optional<float> width{std::nullopt};           ///< Explicit width in px (Compact/Space) or % 0..100 (Fill)
    std::optional<float> height{std::nullopt};          ///< Explicit height in px (nullopt = auto from aspect ratio)
    std::optional<float> maxHeight{std::nullopt};       ///< Optional maximum height cap in px
    SizeMode heightMode{SizeMode::Fit};                 ///< Fit (auto/fixed) or Stretch (expand to fill parent height)
    bool background{false};                             ///< Whether to render a surface backdrop around the outer Element
    float borderWidth{0.0f};                            ///< Outer Element border thickness in points (0.0f = no border)
    std::optional<float> borderRadius{std::nullopt};    ///< Outer Element corner radius in points
    ControlSpec control{};                              ///< Inner Control slot configuration (width, height, align, background, borderWidth, borderRadius)
    Color borderColor{0, 0, 0, 0};                      ///< Optional image frame border color (0 = semantic variant or borderSubtle)
    Color backgroundColor{0, 0, 0, 0};                  ///< Optional background fill color behind image
    bool checkerboard{false};                           ///< Render transparency checkerboard pattern behind image
    uint8_t opacity{255};                               ///< Image opacity [0..255]
    std::optional<UiSize> size{std::nullopt};           ///< Local size token override
    bool disabled{false};
};

/// Internal frame-arena payload for Image nodes.
struct ImagePayload {
    std::string_view source{};
    ImageOptions opts{};
};

/// Stateless leaf Element widget for rendering vector (SVG) and raster (PNG, JPG) images.
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
