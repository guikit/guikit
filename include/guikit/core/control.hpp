#pragma once

#include "guikit/core/theme.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "keel/core.h"

namespace guikit {

class Context;
struct Node;
struct Response;

/// Shared layout sizing and min/max constraints for controls.
struct ControlLayout {
    AxisSize width{};                                ///< Horizontal sizing policy (`Auto`, `Fit`, `Stretch`, or explicit `dp`)
    AxisSize height{};                               ///< Vertical sizing policy (`Auto`, `Fit`, `Stretch`, or explicit `dp`)
    keel::Optional<float> minWidth{keel::kNullOpt};  ///< Optional minimum width in logical dp
    keel::Optional<float> maxWidth{keel::kNullOpt};  ///< Optional maximum width in logical dp
    keel::Optional<float> minHeight{keel::kNullOpt}; ///< Optional minimum height in logical dp
    keel::Optional<float> maxHeight{keel::kNullOpt}; ///< Optional maximum height in logical dp
};

/// Shared sizing and semantic accent/tint style for frameless controls
/// (`Switch`, `Checkbox`, `Radio`, `Knob`, `Slider`, `StepSlider`, `RangeSlider`,
/// `ProgressBar`, `LevelMeter`, `Breadcrumb`, `Pagination`).
struct ControlStyle {
    UiSize size{UiSize::Md}; ///< Control size token (`Xs`..`Xl`) or explicit `UiSize::Dp(...)` / `UiSize::Factor(...)`
    StateColor color{};      ///< Per-state semantic color/accent override
};

/// Shared sizing, tint, surface background and border style for boxed controls
/// (`Button`, `InputField`, `SpinBox`, `Select`, `Segmented`, `DatePicker`,
/// `ColorPicker`, `TagInput`, `Image`, `TextArea`).
struct SurfaceStyle {
    UiSize size{UiSize::Md};                      ///< Control size token (`Xs`..`Xl`) or explicit `UiSize::Dp(...)` / `UiSize::Factor(...)`
    StateColor color{};                           ///< Per-state semantic fill/accent color override
    StateColor background{StateColor::Surface()}; ///< Control frame background fill (`StateColor::Surface()` by default)
    Border border{};                              ///< Optional border width, color and radius override
};

/// Layout and sizing specification for an interactive or visual `Control`
/// (either standalone or hosted inside an `Element` control slot).
struct ControlSpec {
    AxisSize width{};                                        ///< Requested horizontal sizing policy (`Auto`, `Fit`, `Stretch`, or `Fixed(dp)`)
    AxisSize height{};                                       ///< Requested vertical sizing policy (`Auto`, `Fit`, `Stretch`, or `Fixed(dp)`)
    SizeMode defaultWidthMode{SizeMode::Fit};                ///< Widget's natural horizontal fallback when `width` is `Auto`
    SizeMode defaultHeightMode{SizeMode::Fit};               ///< Widget's natural vertical fallback when `height` is `Auto`
    keel::Optional<float> minWidth{keel::kNullOpt};          ///< Optional minimum control width in logical dp
    keel::Optional<float> maxWidth{keel::kNullOpt};          ///< Optional maximum control width in logical dp
    keel::Optional<float> minHeight{keel::kNullOpt};         ///< Optional minimum control height in logical dp
    keel::Optional<float> maxHeight{keel::kNullOpt};         ///< Optional maximum control height in logical dp
    ElementLayout defaultLayout{ElementLayout::Stacked};     ///< Default parent Element layout when wrapped (Trailing for Switch/Checkbox/Radio, Leading for Knob, Stacked for others)
    float labelGap{0.0f};                                    ///< Preferred vertical gap between parent Element label and control
    float descGap{0.0f};                                     ///< Preferred vertical gap between control and parent Element description
    float inlineGap{0.0f};                                   ///< Preferred horizontal gap in Inline layout
};

/// Foundational Control sizing and interaction binding helpers.
class Control {
public:
    /// Builds a `ControlSpec` from a shared `ControlLayout` plus widget-specific defaults.
    [[nodiscard]] static constexpr ControlSpec FromLayout(
        const ControlLayout& layout,
        SizeMode defaultWidthMode = SizeMode::Fit,
        SizeMode defaultHeightMode = SizeMode::Fit,
        ElementLayout defaultLayout = ElementLayout::Stacked,
        float labelGap = 0.0f,
        float descGap = 0.0f,
        float inlineGap = 0.0f
    ) noexcept {
        return ControlSpec{
            .width = layout.width,
            .height = layout.height,
            .defaultWidthMode = defaultWidthMode,
            .defaultHeightMode = defaultHeightMode,
            .minWidth = layout.minWidth,
            .maxWidth = layout.maxWidth,
            .minHeight = layout.minHeight,
            .maxHeight = layout.maxHeight,
            .defaultLayout = defaultLayout,
            .labelGap = labelGap,
            .descGap = descGap,
            .inlineGap = inlineGap
        };
    }

    /// Resolves effective horizontal `SizeMode` for a control in `ctx`
    /// (`Fixed(dp)` -> `Fixed`; explicit `Fit`/`Stretch` -> respected as-is;
    ///  `Auto` inside horizontal `Toolbar` -> `Fit`, otherwise `defaultMode`).
    [[nodiscard]] static SizeMode ResolveWidthMode(
        const Context& ctx,
        AxisSize width,
        SizeMode defaultMode = SizeMode::Stretch
    ) noexcept;

    /// Resolves effective vertical `SizeMode` (`Auto` -> `defaultMode`).
    [[nodiscard]] static constexpr SizeMode ResolveHeightMode(
        AxisSize height,
        SizeMode defaultMode = SizeMode::Fit
    ) noexcept {
        return height.ResolveMode(defaultMode);
    }

    /// Measures the control's dimensions given its natural `intrinsicSize`, applying
    /// `minWidth`, `minHeight`, and explicit `width` / `height` overrides in logical `dp`.
    [[nodiscard]] static Vec2 Measure(
        Vec2 intrinsicSize,
        const ControlSpec& spec,
        const UiTheme& theme
    ) noexcept;

    /// Forwards hover, press, and click interactions from an enclosing `NodeType::Element` container
    /// (e.g. clicking the label or description text) to toggle controls such as `Switch`, `Checkbox`, `Radio`, and `Knob`.
    static void ForwardParentElementInteraction(
        Context& ctx,
        bool disabled,
        Response& ioRes
    ) noexcept;

    /// Configures `node` sizing (`widthMode`, `heightMode`, `fixedWidth`, `fixedHeight`, `minWidth`, `minHeight`),
    /// binds to an enclosing `NodeType::Element` or `NodeType::TreeItem` when present, and adapts to parent container.
    static void SetupNode(
        Context& ctx,
        Node* node,
        const ControlSpec& spec
    ) noexcept;
};

} // namespace guikit
