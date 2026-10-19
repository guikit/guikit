#pragma once

#include <optional>
#include "guikit/core/Theme.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

class Context;
class ICanvas;
struct Node;
struct Response;

/// Specification for an interactive or visual `Control` (either standalone or inside an `Element`).
struct ControlSpec {
    std::optional<float> width{std::nullopt};                ///< Explicit control width in points (Compact/Space) or normalized ratio 0.0..1.0 (Fill)
    std::optional<float> height{std::nullopt};               ///< Explicit control height in points (nullopt = auto from UiSize / aspect)
    std::optional<float> minWidth{std::nullopt};             ///< Minimum control width in points
    Align align{Align::Center};                              ///< Horizontal alignment of content inside the control slot
    StateColor background{};                                 ///< Per-state control backdrop / well fill color (Default/None/Surface/Primary/etc.)
    Border border{};                                         ///< Border thickness, corner radius, and per-state color for controlRect
    ElementLayout defaultLayout{ElementLayout::Stacked};     ///< Default parent Element layout when wrapped (Inline for Switch/Checkbox, Stacked for others)
    InlineOrder defaultInlineOrder{InlineOrder::LabelFirst}; ///< Default parent Element inlineOrder when wrapped (ControlFirst for Switch/Checkbox, LabelFirst for others)
    float labelGap{0.0f};                                    ///< Preferred vertical gap between parent Element label and control
    float descGap{0.0f};                                     ///< Preferred vertical gap between control and parent Element description
    float inlineGap{0.0f};                                   ///< Preferred horizontal gap in Inline layout
    bool stretchable{true};                                  ///< True when control stretches horizontally in StretchMode::Fill
    bool stretchHeightWhenStandalone{false};                 ///< True when control stretches vertically when standalone
    bool hasIntrinsicBox{true};                              ///< True for Button/InputField/Select/Image; false for Slider/Switch/Checkbox

    /// Resolves whether the control background is enabled, falling back to `defaultValue` when `background` is `StateColor::Default()`.
    [[nodiscard]] constexpr bool HasBackground(bool defaultValue = false) const noexcept {
        return background.IsBackgroundEnabled(defaultValue);
    }
};

/// Foundational Control engine.
/// Manages explicit sizing, well/backdrop padding, inner content bounds, frame rendering,
/// standalone node arrangement, and seamless binding when placed inside an `Element`.
class Control {
public:
    /// Returns true when the control requires an outer well backdrop or border drawn by `Control::RenderFrame`.
    [[nodiscard]] static constexpr bool HasWellFrame(const ControlSpec& spec) noexcept {
        return !spec.hasIntrinsicBox && (spec.HasBackground(false) || spec.border.width > 0.0f);
    }

    /// Computes internal padding added around controls without an intrinsic box (Slider, Switch, Checkbox)
    /// when a control well background or border is enabled.
    [[nodiscard]] static float ComputeWellPadding(
        const ControlSpec& spec,
        UiSize size,
        const UiTheme& theme
    ) noexcept;

    /// Measures the control slot size given its natural `intrinsicSize`, applying well padding
    /// and explicit `width` / `height` overrides from `spec`.
    [[nodiscard]] static Vec2 Measure(
        Vec2 intrinsicSize,
        const ControlSpec& spec,
        StretchMode stretch,
        UiSize size,
        const UiTheme& theme
    ) noexcept;

    /// Resolves the inner working rectangle inside `controlRect` after subtracting any control well padding.
    [[nodiscard]] static Rect ResolveInnerRect(
        Rect controlRect,
        const ControlSpec& spec,
        UiSize size,
        const UiTheme& theme
    ) noexcept;

    /// Renders the control slot's well background and border into `controlRect` when enabled.
    static void RenderFrame(
        ICanvas& canvas,
        Rect controlRect,
        const ControlSpec& spec,
        UiSize size,
        bool disabled,
        const UiTheme& theme,
        bool hovered = false,
        bool active = false
    ) noexcept;

    /// Inherits `stretch`, `width`, and `height` from an enclosing `NodeType::Element` container when present.
    static void InheritElementSizing(
        Context& ctx,
        StretchMode& ioStretch,
        std::optional<float>& ioWidth,
        std::optional<float>& ioHeight,
        bool inheritWidth = true
    ) noexcept;

    /// Forwards hover, press, and click interactions from an enclosing `NodeType::Element` container
    /// (e.g. clicking the label or description text) to toggle controls such as `Switch` and `Checkbox`.
    static void ForwardParentElementInteraction(
        Context& ctx,
        bool disabled,
        Response& ioRes
    ) noexcept;

    /// Configures layout and arrangement for a control `node` (either standalone or wrapped inside `NodeType::Element`).
    static void SetupNode(
        Context& ctx,
        Node* node,
        const ControlSpec& spec,
        StretchMode stretch,
        Align standaloneAlign = Align::Start
    ) noexcept;

    /// Arrangement delegate for a standalone `Control` node (resolving `StretchMode::Fill`, `Space`, and `Compact`).
    static void ArrangeStandaloneNode(
        Node* node,
        const Rect& availableRect,
        const UiTheme& theme
    ) noexcept;
};

} // namespace guikit
