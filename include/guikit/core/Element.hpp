#pragma once

#include <optional>
#include <source_location>
#include <string_view>
#include "guikit/core/ContainerScope.hpp"
#include "guikit/core/Control.hpp"
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/graphics/Icon.hpp"

namespace guikit {

class Context;
class ICanvas;
struct Node;

/// Configuration options for an `Element` wrapper (or standalone control-less `Element`).
struct ElementOptions {
    UiText label{};                                           ///< Primary label text
    UiText description{};                                     ///< Optional secondary description text
    UiText tooltip{};                                         ///< Optional hover tooltip text
    Icon icon{};                                              ///< Optional vector icon before label
    UiText badge{};                                           ///< Optional pill badge after label (supports independent StateColor)
    std::optional<ElementLayout> layout{std::nullopt};        ///< Text placement relative to control: Stacked, Top, Bottom, Inline (nullopt = control default)
    std::optional<InlineOrder> inlineOrder{std::nullopt};     ///< Sequencing order in Inline layout (nullopt = control default)
    Align align{Align::Start};                                ///< Horizontal alignment (Start, Center, End)
    std::optional<StretchMode> stretch{std::nullopt};         ///< Sizing behavior: Compact, Space, Fill (nullopt = inherit from child control or Fill when standalone)
    std::optional<float> width{std::nullopt};                 ///< Explicit element/control width in points (Compact/Space) or normalized ratio 0.0..1.0 (Fill)
    std::optional<float> height{std::nullopt};                ///< Explicit element/control height in points (nullopt = auto from UiSize)
    StateColor background{};                                  ///< Per-state outer surface backdrop color (Default/None = off, Surface/Primary/etc. = on)
    Border border{};                                          ///< Outer element border thickness, corner radius, and per-state color
    bool disabled{false};                                     ///< Whether the element and its child control are rendered in a disabled state
    const UiTheme* theme{nullptr};                            ///< Optional scoped theme override
    std::optional<float> scale{std::nullopt};                 ///< Optional local UI scale factor override
    std::optional<bool> debugBounds{std::nullopt};            ///< Scoped debug bounding box overlay override
    std::optional<bool> inspectMode{std::nullopt};            ///< Scoped interactive DevTools inspector override
};

/// RAII scope returned by `ui.Element(...)`.
/// Supports both scoped wrapping (`if (auto el = ui.Element({...})) { ui.Slider(...); }`)
/// and standalone invocation (`ui.Element({...});` or `Response r = ui.Element({...});`).
class ElementScope : public ContainerScope {
public:
    Response response{};

    ElementScope() noexcept : ContainerScope(nullptr, nullptr) {}
    ElementScope(Context* ctx, Node* node, Response res) noexcept
        : ContainerScope(ctx, node), response(res) {}

    ElementScope(ElementScope&&) noexcept = default;
    ElementScope& operator=(ElementScope&&) noexcept = default;

    explicit operator bool() const noexcept { return m_node != nullptr; }
    operator Response() const noexcept { return response; }
    [[nodiscard]] const Response* operator->() const noexcept { return &response; }
};

/// Internal per-frame arena payload for `NodeType::Element`.
struct ElementPayload {
    ElementOptions opts{};
    ControlSpec childControlSpec{};
    Response interactionRes{};
    mutable ICanvas* lastCanvas{nullptr};
    bool hasChildControl{false};
};

/// Low-level layout, typography, and decoration specification for `Element`.
struct ElementSpec {
    UiText label{};                                       ///< Primary label or heading title
    UiText description{};                                 ///< Optional secondary description or subtitle
    Icon icon{};                                          ///< Optional vector icon before label
    UiText badge{};                                       ///< Optional trailing pill badge after label (supports independent StateColor)
    ElementLayout layout{ElementLayout::Stacked};         ///< Placement of text relative to control (or description relative to label bar)
    InlineOrder inlineOrder{InlineOrder::LabelFirst};     ///< Sequencing order in Inline layout
    Align align{Align::Start};                            ///< Horizontal alignment (Start, Center, End)
    StretchMode stretch{StretchMode::Compact};            ///< Sizing behavior: Compact, Space, Fill
    std::optional<float> width{std::nullopt};             ///< Explicit control/element width in points for Compact/Space, or normalized ratio 0.0..1.0 for Fill
    std::optional<float> height{std::nullopt};            ///< Explicit control/element height in points (nullopt = intrinsic height)
    ControlSpec control{};                                ///< Specification for the inner control slot (`controlRect`)
    StateColor background{};                              ///< Per-state outer surface backdrop color (Default/None = off, Surface/Primary/etc. = on)
    Border border{};                                      ///< Outer element border thickness, corner radius, and per-state color
    UiSize widgetSize{UiSize::Md};                        ///< Active UiSize token
    float labelGap{6.0f};                                 ///< Vertical gap between label bar and control/description
    float descGap{4.0f};                                  ///< Vertical gap between control/label and description
    float inlineGap{10.0f};                               ///< Horizontal gap in Inline layout
    bool hasControl{true};                                ///< False for control-less elements (Element / Heading); true when a widget control is present
    bool headingTypography{false};                        ///< True to use HeadingMetrics typography scale; false for standard control LabelLarge/LabelSmall
    bool stretchableControl{true};                        ///< True for Button/InputField/Select/Slider/Image; false for fixed-size controls
    bool stretchHeightWhenStandalone{false};              ///< True to stretch control height when standalone
    bool disabled{false};                                 ///< Whether the element is disabled (dims text, icon, and badge)
    bool hovered{false};                                  ///< Whether the element is currently hovered
    bool active{false};                                   ///< Whether the element/control is currently active (pressed or focused)
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
/// Serves as an outer wrapper over any `Control` (or as a standalone label/description element when empty),
/// and as the base class for `Heading`.
class Element {
public:
    /// Returns true when the outer `Element` envelope (`outerBounds`) has a visible background or border.
    [[nodiscard]] static constexpr bool HasOuterFrame(const ElementSpec& spec) noexcept {
        return spec.background.IsBackgroundEnabled(false) || spec.border.width > 0.0f;
    }

    /// Computes internal padding inside `outerBounds` when `HasOuterFrame(spec)` is true.
    [[nodiscard]] static float ComputeOuterPadding(
        const ElementSpec& spec,
        const UiTheme& theme
    ) noexcept;

    /// Converts `ElementOptions` into a control-less `ElementSpec` for standalone measurement and rendering.
    [[nodiscard]] static ElementSpec MakeSpec(
        const ElementOptions& opts,
        UiSize activeSize,
        bool isDisabled,
        const UiTheme& theme,
        bool isHovered = false,
        bool isActive = false
    ) noexcept;

    /// Opens an `Element` scope that can wrap a child `Control` or act as a standalone element.
    static ElementScope Open(
        Context& ctx,
        const ElementOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Immediate mode entry point for a standalone control-less `Element`.
    static Response Run(
        Context& ctx,
        const ElementOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Open(ctx, opts, loc).response;
    }

    /// Arrangement delegate for `NodeType::Element` nodes.
    static void Arrange(
        Node* node,
        const Rect& availableRect,
        const UiTheme& theme
    ) noexcept;

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

    /// Renders the element's outer frame, text, icon, and badge according to resolved geometry.
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
        bool disabled = false,
        bool hovered = false,
        bool active = false
    ) noexcept {
        Render(canvas, bounds, MakeSpec(opts, size, disabled || opts.disabled, theme, hovered, active), theme);
    }

protected:
    [[nodiscard]] static float ResolveLabelFontSize(const ElementSpec& spec, const UiTheme& theme) noexcept;
    [[nodiscard]] static float ResolveDescFontSize(const ElementSpec& spec, const UiTheme& theme) noexcept;
    [[nodiscard]] static Vec2 MeasureLabelBar(
        const ElementSpec& spec,
        float labelFontSize,
        ICanvas* canvas,
        const UiTheme& theme
    ) noexcept;
};

} // namespace guikit
