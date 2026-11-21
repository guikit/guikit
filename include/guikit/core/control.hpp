#pragma once

#include "guikit/core/theme.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "keel/core.h"

namespace guikit {

class Context;
struct Node;
struct Response;

/// Layout and sizing specification for an interactive or visual `Control`
/// (either standalone or hosted inside an `Element` control slot).
struct ControlSpec {
    AxisSize width{};                                        ///< Requested horizontal sizing policy (`Auto`, `Fit`, `Stretch`, or `Fixed(dp)`)
    AxisSize height{};                                       ///< Requested vertical sizing policy (`Auto`, `Fit`, `Stretch`, or `Fixed(dp)`)
    SizeMode defaultWidthMode{SizeMode::Fit};                ///< Widget's natural horizontal fallback when `width` is `Auto`
    SizeMode defaultHeightMode{SizeMode::Fit};               ///< Widget's natural vertical fallback when `height` is `Auto`
    keel::Optional<keel::f32> minWidth{keel::kNullOpt};          ///< Optional minimum control width in logical dp
    keel::Optional<keel::f32> maxWidth{keel::kNullOpt};          ///< Optional maximum control width in logical dp
    keel::Optional<keel::f32> minHeight{keel::kNullOpt};         ///< Optional minimum control height in logical dp
    keel::Optional<keel::f32> maxHeight{keel::kNullOpt};         ///< Optional maximum control height in logical dp
    ElementLayout defaultLayout{ElementLayout::Stacked};     ///< Default parent Element layout when wrapped (Trailing for Switch/Checkbox/Radio, Leading for Knob, Stacked for others)
    keel::f32 labelGap{0.0f};                                    ///< Preferred vertical gap between parent Element label and control
    keel::f32 descGap{0.0f};                                     ///< Preferred vertical gap between control and parent Element description
    keel::f32 inlineGap{0.0f};                                   ///< Preferred horizontal gap in Inline layout
};

/// Foundational Control sizing and interaction binding helpers.
class Control {
public:
    /// Builds a `ControlSpec` from any widget `*Options` struct carrying flat
    /// `width`, `height`, `minWidth`, `maxWidth`, `minHeight`, and `maxHeight` fields.
    template <typename Opts>
    [[nodiscard]] static constexpr ControlSpec FromOptions(
        const Opts& opts,
        SizeMode defaultWidthMode = SizeMode::Fit,
        SizeMode defaultHeightMode = SizeMode::Fit,
        ElementLayout defaultLayout = ElementLayout::Stacked,
        keel::f32 labelGap = 0.0f,
        keel::f32 descGap = 0.0f,
        keel::f32 inlineGap = 0.0f
    ) noexcept {
        return ControlSpec{
            .width = opts.width,
            .height = opts.height,
            .defaultWidthMode = defaultWidthMode,
            .defaultHeightMode = defaultHeightMode,
            .minWidth = opts.minWidth,
            .maxWidth = opts.maxWidth,
            .minHeight = opts.minHeight,
            .maxHeight = opts.maxHeight,
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

    /// Resolves effective proportional `flexGrow` weight along the parent container's main axis
    /// from `AxisSize::Stretch(weight)` (falling back to `fallbackFlexGrow` when no custom weight is set).
    [[nodiscard]] static keel::f32 ResolveFlexGrow(
        const Node* parent,
        AxisSize width,
        AxisSize height,
        keel::f32 fallbackFlexGrow = 1.0f
    ) noexcept;

    [[nodiscard]] static keel::f32 ResolveFlexGrow(
        const Context& ctx,
        AxisSize width,
        AxisSize height,
        keel::f32 fallbackFlexGrow = 1.0f
    ) noexcept;

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
