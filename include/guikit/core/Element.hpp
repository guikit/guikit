#pragma once

#include <optional>
#include <source_location>
#include <string_view>
#include "guikit/core/Control.hpp"
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/graphics/Icon.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Clean configuration options for a standalone `Element` or `Heading`.
struct ElementOptions {
    UiText title{};                                       ///< Primary heading/element title text (alias for label)
    UiText subtitle{};                                    ///< Optional secondary subtitle text (alias for description)
    UiText label{};                                       ///< Primary label or heading title text
    UiText description{};                                 ///< Optional secondary description or subtitle text
    Icon icon{};                                          ///< Optional vector icon before label
    UiText badge{};                                       ///< Optional pill badge after label (supports independent UiVariant)
    UiVariant variant{UiVariant::Default};                ///< Accent color variant
    std::optional<UiSize> size{std::nullopt};             ///< Local size override (nullopt = inherit container size)
    ElementLayout layout{ElementLayout::Stacked};         ///< Text placement: Stacked/Bottom (below), Top (above), Inline (beside)
    InlineOrder inlineOrder{InlineOrder::ControlFirst};   ///< Sequencing order in Inline layout (ControlFirst = Label first, LabelFirst = Description first)
    Align elementAlign{Align::Start};                     ///< Horizontal alignment (Start, Center, End)
    StretchMode stretch{StretchMode::Fill};               ///< Sizing behavior: Compact, Space, Fill
    std::optional<float> width{std::nullopt};             ///< Explicit element width in points (Compact/Space) or % 0..100 (Fill)
    std::optional<float> height{std::nullopt};            ///< Explicit element height in points (nullopt = auto from UiSize)
    bool background{false};                               ///< Whether to render a surface backdrop around outerBounds
    float borderWidth{0.0f};                              ///< Outer element border thickness in points (0.0f = no border)
    std::optional<float> borderRadius{std::nullopt};      ///< Outer element corner radius in points (nullopt = theme radius)
    bool divider{false};                                  ///< Whether to render a horizontal section divider line (Heading)
    ElementDivider dividerStyle{ElementDivider::Bottom};  ///< Divider placement style (Bottom or Trailing)
    bool wordWrap{false};                                 ///< Whether description wraps into multiple lines when space is constrained
    bool headingTypography{false};                        ///< True when styled as a Heading (uses HeadingMetrics scale and Secondary subtitle variant)
    bool disabled{false};                                 ///< Whether the element is rendered in a disabled state

    [[nodiscard]] const UiText& EffectiveTitle() const noexcept {
        return title.HasContentOrReserve() ? title : label;
    }

    [[nodiscard]] const UiText& EffectiveSubtitle() const noexcept {
        return subtitle.HasContentOrReserve() ? subtitle : description;
    }
};

using HeadingOptions = ElementOptions;

/// Low-level layout, typography, and decoration specification for `Element`.
/// By default (`hasControl = false`), `Element` represents a standalone header/caption element (`Heading`).
/// Widgets with interactive or visual controls (`Button`, `Checkbox`, `Switch`, `Slider`, `InputField`, `Select`, `Image`, `Scene`)
/// set `hasControl = true` and configure their control slot via `control` (`ControlSpec`).
struct ElementSpec {
    UiText label{};                                       ///< Primary label or heading title
    UiText description{};                                 ///< Optional secondary description or subtitle
    Icon icon{};                                          ///< Optional vector icon before label
    UiText badge{};                                       ///< Optional pill badge after label (supports independent UiVariant)
    UiVariant variant{UiVariant::Default};                ///< Semantic color variant for icon, badge, divider, and default title
    ElementLayout layout{ElementLayout::Stacked};         ///< Placement of text relative to control (or description relative to label bar)
    InlineOrder inlineOrder{InlineOrder::LabelFirst};     ///< Sequencing order in Inline layout
    Align elementAlign{Align::Start};                     ///< Horizontal alignment (Start, Center, End)
    StretchMode stretch{StretchMode::Compact};            ///< Sizing behavior: Compact, Space, Fill
    std::optional<float> width{std::nullopt};             ///< Explicit control/element width in points for Compact/Space, or % (0..100) for Fill
    std::optional<float> height{std::nullopt};            ///< Explicit control/element height in points (nullopt = intrinsic height)
    ControlSpec control{};                                ///< Specification for the inner control slot (`controlRect`)
    bool background{false};                               ///< Whether to render a surface backdrop around outerBounds
    float borderWidth{0.0f};                              ///< Outer element border thickness in points (0.0f = no border)
    std::optional<float> borderRadius{std::nullopt};      ///< Outer element corner radius in points (nullopt = theme radius)
    UiSize widgetSize{UiSize::Md};                        ///< Active UiSize token
    float labelGap{6.0f};                                 ///< Vertical gap between label bar and control/description
    float descGap{4.0f};                                  ///< Vertical gap between control/label and description
    float inlineGap{10.0f};                               ///< Horizontal gap in Inline layout
    bool hasControl{true};                                ///< False for control-less elements (Element / Heading); true when a widget control is present
    bool headingTypography{false};                        ///< True to use HeadingMetrics typography scale; false for standard control LabelLarge/LabelSmall
    bool divider{false};                                  ///< Whether to render a horizontal divider line (Heading)
    ElementDivider dividerStyle{ElementDivider::Bottom};  ///< Divider placement style (Bottom or Trailing)
    bool stretchableControl{true};                        ///< True for Button/InputField/Select/Slider/Image; false for fixed-size controls
    bool stretchHeightWhenStandalone{false};              ///< True to stretch control height when standalone
    bool disabled{false};                                 ///< Whether the element is disabled (dims text, icon, badge, and divider)
};

/// Resolved bounding rectangles for a composite UI element.
struct ElementGeometry {
    Rect outerBounds{};
    Rect controlRect{};
    Rect innerControlRect{};
    Rect labelRect{};
    Rect descRect{};
    float measuredLabelW{0.0f};
    float measuredDescW{0.0f};
    float labelFontSize{14.0f};
    float descFontSize{11.0f};
    bool hasLabel{false};
    bool hasDesc{false};
    bool hasExternal{false};
};

/// Foundational UI Element class.
/// Serves both as a standalone immediate-mode element (`Element::Run` / `Heading::Run` when `hasControl == false`)
/// and as the shared outer envelope & text composition engine for all control-bearing widgets (`hasControl == true`).
class Element {
public:
    /// Returns true when the outer `Element` envelope (`outerBounds`) has a visible background or border.
    [[nodiscard]] static constexpr bool HasOuterFrame(const ElementSpec& spec) noexcept {
        return spec.background || spec.borderWidth > 0.0f;
    }

    /// Computes internal padding inside `outerBounds` when `HasOuterFrame(spec)` is true.
    [[nodiscard]] static float ComputeOuterPadding(
        const ElementSpec& spec,
        const UiTheme& theme
    ) noexcept;

    /// Converts `ElementOptions` into an `ElementSpec` for measurement and rendering.
    [[nodiscard]] static ElementSpec MakeSpec(
        const ElementOptions& opts,
        UiSize activeSize,
        bool isDisabled,
        const UiTheme& theme
    ) noexcept;

    /// Immediate mode entry point for a standalone control-less `Element` (or `Heading` when `opts.headingTypography == true`).
    static Response Run(
        Context& ctx,
        const ElementOptions& opts,
        std::source_location loc = std::source_location::current()
    );

    /// Measures intrinsic dimensions of the element given `intrinsicControlSize` (ignored when `spec.hasControl == false`).
    static Vec2 Measure(
        Vec2 intrinsicControlSize,
        const ElementSpec& spec,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    ) noexcept;

    /// Measures intrinsic dimensions of a control-less `ElementSpec`.
    static Vec2 Measure(
        const ElementSpec& spec,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    ) noexcept {
        return Measure({0.0f, 0.0f}, spec, canvas, theme, availableWidth);
    }

    /// Measures intrinsic dimensions from `ElementOptions`.
    static Vec2 Measure(
        const ElementOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    ) noexcept {
        return Measure({0.0f, 0.0f}, MakeSpec(opts, size, opts.disabled, theme), canvas, theme, availableWidth);
    }

    /// Resolves bounding boxes for the control (or primary label bar), external label, and description within `bounds`.
    static ElementGeometry Resolve(
        Rect bounds,
        Vec2 intrinsicControlSize,
        const ElementSpec& spec,
        ICanvas* canvas,
        const UiTheme& theme
    ) noexcept;

    /// Resolves bounding boxes for a control-less element within `bounds`.
    static ElementGeometry Resolve(
        Rect bounds,
        const ElementSpec& spec,
        ICanvas* canvas,
        const UiTheme& theme
    ) noexcept {
        return Resolve(bounds, {0.0f, 0.0f}, spec, canvas, theme);
    }

    /// Renders the outer `Element` frame (`outerBounds`) background and border when enabled.
    static void RenderFrame(
        ICanvas& canvas,
        const ElementGeometry& geom,
        const ElementSpec& spec,
        const UiTheme& theme
    ) noexcept;

    /// Renders the element's outer frame, control well frame, text, icon, badge, and divider according to resolved geometry.
    static void RenderText(
        ICanvas& canvas,
        const ElementGeometry& geom,
        const ElementSpec& spec,
        const UiTheme& theme
    ) noexcept;

    /// Resolves geometry and renders a complete control-less `ElementSpec`.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        const ElementSpec& spec,
        const UiTheme& theme
    ) noexcept {
        if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;
        const ElementGeometry geom = Resolve(bounds, {0.0f, 0.0f}, spec, &canvas, theme);
        RenderText(canvas, geom, spec, theme);
    }

    /// Resolves geometry and renders a complete control-less `ElementOptions`.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        const ElementOptions& opts,
        UiSize size,
        const UiTheme& theme,
        bool disabled = false
    ) noexcept {
        Render(canvas, bounds, MakeSpec(opts, size, disabled || opts.disabled, theme), theme);
    }
};

using ElementComposer = Element;

} // namespace guikit
