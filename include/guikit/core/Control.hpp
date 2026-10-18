#pragma once

#include <optional>
#include "guikit/core/Theme.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

class ICanvas;

/// Specification for the interactive or visual control slot (`controlRect`) inside an `Element`.
struct ControlSpec {
    std::optional<float> width{std::nullopt};        ///< Explicit control width in points (Compact/Space) or % 0..100 (Fill)
    std::optional<float> height{std::nullopt};       ///< Explicit control height in points (nullopt = auto from UiSize / aspect)
    std::optional<float> minWidth{std::nullopt};     ///< Minimum control width in points
    Align align{Align::Center};                      ///< Horizontal alignment of content inside the control slot
    std::optional<bool> background{std::nullopt};    ///< Whether to render a control backdrop / well inside controlRect (nullopt = widget default)
    float borderWidth{0.0f};                         ///< Explicit border thickness around controlRect (0.0f = default / none)
    std::optional<float> borderRadius{std::nullopt}; ///< Explicit corner radius for controlRect
    StateColor borderColor{};                        ///< Per-state border color for controlRect (normal, hover, active, disabled)
    StateColor bgColor{};                            ///< Per-state background/well color for controlRect (normal, hover, active, disabled)
    bool stretchable{true};                          ///< True when control stretches horizontally in StretchMode::Fill
    bool stretchHeightWhenStandalone{false};         ///< True when control stretches vertically when standalone
    bool hasIntrinsicBox{true};                      ///< True for Button/InputField/Select/Image; false for Slider/Switch/Checkbox

    /// Resolves whether the control background is enabled, falling back to `defaultValue` when `background` is `std::nullopt`.
    [[nodiscard]] constexpr bool HasBackground(bool defaultValue = false) const noexcept {
        return background.value_or(defaultValue);
    }
};

/// Foundational Control slot engine.
/// Manages explicit sizing, well/backdrop padding, inner content bounds, and frame rendering
/// for the interactive/visual control region (`geom.controlRect`) inside an `Element`.
class Control {
public:
    /// Returns true when the control requires an outer well backdrop or border drawn by `Control::RenderFrame`.
    [[nodiscard]] static constexpr bool HasWellFrame(const ControlSpec& spec) noexcept {
        return !spec.hasIntrinsicBox && (spec.HasBackground(false) || spec.borderWidth > 0.0f);
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
};

} // namespace guikit
