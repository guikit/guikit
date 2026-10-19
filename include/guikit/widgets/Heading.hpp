#pragma once

#include <optional>
#include <source_location>
#include "guikit/core/Element.hpp"

namespace guikit {

/// Configuration options for a `Heading` element (inherits `Element` layout/styling + exclusive `divider` support).
struct HeadingOptions {
    UiText label{};                                       ///< Primary heading title text
    UiText description{};                                 ///< Optional secondary subtitle text
    UiText tooltip{};                                     ///< Optional hover tooltip text
    Icon icon{};                                          ///< Optional vector icon before label
    UiText badge{};                                       ///< Optional pill badge after label (inherited from Element)
    ElementLayout layout{ElementLayout::Stacked};         ///< Subtitle placement relative to title bar (Stacked/Bottom, Top, Inline)
    InlineOrder inlineOrder{InlineOrder::ControlFirst};   ///< Sequencing order in Inline layout (ControlFirst = Label first, LabelFirst = Description first)
    Align align{Align::Start};                            ///< Horizontal alignment (Start, Center, End)
    StretchMode stretch{StretchMode::Fill};               ///< Sizing behavior: Compact, Space, Fill
    std::optional<float> width{std::nullopt};             ///< Explicit heading width in points (Compact/Space) or ratio 0.0..1.0 (Fill)
    std::optional<float> height{std::nullopt};            ///< Explicit heading height in points (nullopt = auto from UiSize)
    StateColor background{};                              ///< Per-state outer surface backdrop color (Default/None = off, Surface/Primary/etc. = on)
    Border border{};                                      ///< Outer border thickness, corner radius, and per-state border/divider color
    bool divider{false};                                  ///< Whether to render a horizontal section divider line
    HeadingDivider dividerStyle{HeadingDivider::Bottom};  ///< Divider placement style (Bottom or Trailing)
    bool disabled{false};                                 ///< Whether the heading is rendered in a disabled state
    const UiTheme* theme{nullptr};                        ///< Optional scoped theme override
    std::optional<float> scale{std::nullopt};             ///< Optional local UI scale factor override
    std::optional<bool> debugBounds{std::nullopt};        ///< Scoped debug bounding box overlay override
    std::optional<bool> inspectMode{std::nullopt};        ///< Scoped interactive DevTools inspector override
};

/// `Heading` inherits from `Element`, uses `HeadingMetrics` typography (`headingTypography = true`),
/// and exclusively owns section `divider` / `dividerStyle` rendering.
class Heading : public Element {
public:
    using Element::Measure;
    using Element::Render;

    [[nodiscard]] static ElementSpec MakeSpec(
        const HeadingOptions& opts,
        UiSize activeSize,
        bool isDisabled,
        const UiTheme& theme,
        bool isHovered = false,
        bool isActive = false
    ) noexcept;

    [[nodiscard]] static float ComputeBottomDividerSpace(
        const HeadingOptions& opts,
        UiSize size,
        const UiTheme& theme
    ) noexcept;

    static Response Run(
        Context& ctx,
        const HeadingOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    static Vec2 Measure(
        const HeadingOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    ) noexcept;

    static Vec2 Measure(
        const HeadingOptions& opts,
        UiSize size,
        float availableWidth,
        ICanvas* canvas,
        const UiTheme& theme
    ) noexcept {
        return Measure(opts, size, canvas, theme, availableWidth);
    }

    static Vec2 Measure(
        const HeadingOptions& opts,
        UiSize size,
        float availableWidth = 0.0f,
        ICanvas* canvas = nullptr
    ) noexcept {
        return Measure(opts, size, canvas, UiTheme::Dark(), availableWidth);
    }

    static float ResolveHeight(
        const HeadingOptions& opts,
        UiSize size,
        float availableWidth,
        ICanvas* canvas,
        const UiTheme& theme
    ) noexcept {
        return Measure(opts, size, canvas, theme, availableWidth).y;
    }

    static void Render(
        ICanvas& canvas,
        Rect bounds,
        const HeadingOptions& opts,
        UiSize size,
        const UiTheme& theme,
        bool disabled = false,
        bool hovered = false,
        bool active = false
    ) noexcept;
};

} // namespace guikit
