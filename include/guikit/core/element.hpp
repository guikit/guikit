#pragma once

#include "guikit/core/container_scope.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"
#include "keel/core.h"

namespace guikit {

class Context;
class ICanvas;
struct Node;

/// Configuration options for a semantic `Element` form-field wrapper (`Label + [Control Slot] + Description`).
struct ElementOptions {
    UiText label{};                                           ///< Primary form-field label text
    UiText description{};                                     ///< Optional secondary description / helper text
    UiText tooltip{};                                         ///< Optional hover tooltip text
    Icon icon{};                                              ///< Optional leading vector icon before label
    UiText badge{};                                           ///< Optional trailing pill badge after label (supports independent StateColor)
    keel::Optional<ElementLayout> layout{keel::kNullOpt};     ///< Text placement relative to control slot: Stacked, Top, Bottom, Leading, Trailing (nullopt = control default)
    Align align{Align::Start};                                ///< Horizontal alignment (Start, Center, End)
    AxisSize width{};                                         ///< Outer width sizing policy: `Auto` (from child control & layout), `Fit`, `Stretch`, or `Fixed(dp)`
    bool disabled{false};                                     ///< Whether the element and its child control(s) are rendered in a disabled state
    const UiTheme* theme{nullptr};                            ///< Optional scoped theme override
    keel::Optional<float> scale{keel::kNullOpt};              ///< Optional local UI scale factor override
    keel::Optional<bool> debugBounds{keel::kNullOpt};         ///< Scoped debug bounding box overlay override
    keel::Optional<bool> inspectMode{keel::kNullOpt};         ///< Scoped interactive DevTools inspector override
};

/// RAII scope returned by `ui.Element(...)`.
/// Supports scoped wrapping of a control or layout container (`if (auto el = ui.Element({...})) { ui.Slider(...); }`)
/// as well as standalone invocation (`ui.Element({...});`).
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
    ControlSpec firstChildSpec{};
    Response interactionRes{};
    mutable ICanvas* lastCanvas{nullptr};
    keel::u32 controlChildCount{0};
    bool anyChildStretchesW{false};
    bool anyChildStretchesH{false};
};

/// Low-level layout and typography specification for `Element`.
struct ElementSpec {
    UiText label{};                                       ///< Primary form-field label
    UiText description{};                                 ///< Optional secondary description / helper text
    Icon icon{};                                          ///< Optional leading vector icon before label
    UiText badge{};                                       ///< Optional trailing pill badge after label (supports independent StateColor)
    ElementLayout layout{ElementLayout::Stacked};         ///< Placement of text relative to control slot
    Align align{Align::Start};                            ///< Horizontal alignment (Start, Center, End)
    AxisSize width{SizeMode::Stretch};                    ///< Resolved outer horizontal sizing policy (`Stretch`, `Fit`, or `Fixed(dp)`)
    ControlSpec control{};                                ///< Combined specification for the inner control slot
    UiSize widgetSize{UiSize::Md};                        ///< Active UiSize token
    float labelGap{6.0f};                                 ///< Vertical gap between label bar and control/description
    float descGap{6.0f};                                  ///< Vertical gap between control/label and description
    float inlineGap{10.0f};                               ///< Horizontal gap in Inline layout
    bool hasControl{true};                                ///< False for control-less Element; true when 1+ child controls are present
    bool disabled{false};                                 ///< Whether the element is disabled (dims text, icon, and badge)
    bool hovered{false};                                  ///< Whether the element is currently hovered
    bool active{false};                                   ///< Whether the element/control is currently active (pressed or focused)
};

/// Resolved bounding rectangles for an `Element` (`Label + [Control Slot] + Description`).
struct ElementGeometry {
    Rect bounds{};
    Rect controlRect{};
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

/// Semantic Form-Field `Element` (`Label + [Control Slot] + Description`).
class Element {
public:
    /// Converts `ElementOptions` into a control-less `ElementSpec` for standalone measurement and rendering.
    [[nodiscard]] static ElementSpec MakeSpec(
        const ElementOptions& opts,
        UiSize activeSize,
        bool isDisabled,
        const UiTheme& theme,
        bool isHovered = false,
        bool isActive = false
    ) noexcept;

    /// Synchronizes an `Element` container node's `widthMode`, `heightMode`, `minWidth`, `minHeight`,
    /// interaction states, and `visualHash` with its attached child controls.
    static void SyncWithChildren(Context& ctx, Node* elNode) noexcept;

    /// Opens an `Element` scope that can wrap one or more child controls or act as a standalone label/badge element.
    static ElementScope Open(
        Context& ctx,
        const ElementOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Immediate mode entry point for a standalone control-less `Element`.
    static Response Run(
        Context& ctx,
        const ElementOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
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

    /// Resolves bounding boxes for the control slot (or primary label bar), external label, and description within `bounds`.
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

    /// Renders the element's label, icon, badge, and description according to resolved geometry.
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
