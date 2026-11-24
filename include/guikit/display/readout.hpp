#pragma once

#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/response.hpp"
#include "guikit/style/theme.hpp"
#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"
#include "keel/core.h"
#include "keel/optional.h"
#include "keel/ref.h"

namespace guikit {

class Context;
class ICanvas;
struct Node;

/// Unit conversion specification for `Readout`.
/// Computes `displayed = baseValue * factor + offset`, or invokes `convert(baseValue, userData)` when non-null.
struct ReadoutUnit {
    UiText label{};                                            ///< Display symbol (e.g. "V", "mV", "kV", "A", "Hz", "C")
    keel::f64 factor{1.0};                                        ///< Linear scale factor from channel base unit
    keel::f64 offset{0.0};                                        ///< Affine offset added after scaling (e.g. °C -> °F: factor=1.8, offset=32.0)
    keel::f64 (*convert)(keel::f64 baseValue, void* userData){nullptr}; ///< Optional custom non-linear converter
    void* userData{nullptr};                                   ///< Opaque pointer passed to `convert`

    [[nodiscard]] keel::f64 Convert(keel::f64 baseValue) const noexcept {
        return convert != nullptr ? convert(baseValue, userData) : (baseValue * factor + offset);
    }
};

/// Selectable telemetry channel descriptor for `Readout`.
struct ReadoutChannel {
    UiText label{};                                ///< Full name shown in channel dropdown (e.g. "Supply Voltage", "Output Current")
    UiText headerLabel{};                          ///< Optional compact/uppercase card header text (falls back to `label`)
    keel::f64 value{0.0};                             ///< Current measurement in the channel's base unit
    UiText formattedValue{};                       ///< Optional pre-formatted display string (e.g. "01:23.4"); disables precision menu
    keel::ArrayRef<ReadoutUnit> units{};           ///< Allowed display units for this channel (empty = unitless)
    keel::i32 defaultUnitIndex{0};                 ///< Unit selected when switching to this channel
    keel::i32 defaultPrecision{2};                 ///< Decimal precision selected when switching to this channel
    UiVariant variant{UiVariant::Primary};         ///< Semantic value color (Primary, Info, Warning, Danger for threshold alert...)
    bool showAction{true};                         ///< Show trailing action button (e.g. ◎ relative zero / reset)
    bool actionActive{false};                      ///< Highlight trailing action button as active/engaged (e.g. relative zero active)
    bool actionDisabled{false};                    ///< Disable only the trailing action button
    UiText actionTooltip{};                        ///< Optional hover tooltip for the trailing action button
};

/// Persistent caller-owned state for a multi-channel `Readout` slot.
struct ReadoutState {
    keel::i32 channelIndex{0};  ///< Currently selected channel index
    keel::i32 unitIndex{0};     ///< Currently selected unit index within the active channel
    keel::i32 precision{2};     ///< Currently selected decimal places [minPrecision..maxPrecision]
};

/// Configuration options for the interactive engineering `Readout` widget.
struct ReadoutOptions {
    UiSize size{UiSize::Md};
    AxisSize width{};                              ///< Horizontal sizing (`Auto` -> `Fit` with `minWidth`)
    AxisSize height{};                             ///< Vertical sizing (`Auto` -> `Fit`)
    keel::Optional<keel::f32> minWidth{188.0f};        ///< Stable minimum card width in logical dp
    keel::Optional<keel::f32> maxWidth{keel::kNullOpt};
    keel::Optional<keel::f32> minHeight{keel::kNullOpt};
    keel::Optional<keel::f32> maxHeight{keel::kNullOpt};
    keel::Optional<Padding> padding{keel::kNullOpt};
    StateColor background{};
    Border border{.width = 1.0f};
    SurfaceRole surface{SurfaceRole::Flat};
    bool disabled{false};

    // Interactive behavior & numeric formatting
    keel::i32 minPrecision{0};                     ///< Minimum selectable decimal places [0..8]
    keel::i32 maxPrecision{5};                     ///< Maximum selectable decimal places [minPrecision..8]
    bool allowChannelMenu{true};                   ///< Clicking header opens channel dropdown when channels.size() > 1
    bool allowPrecisionMenu{true};                 ///< Clicking numeric value opens decimal precision dropdown
    bool allowUnitMenu{true};                      ///< Clicking unit label opens unit dropdown when units.size() > 1
    bool showPlusSign{false};                      ///< Prefix strictly positive numbers with '+'
    UiText invalidText{"---"};                     ///< Placeholder shown when value is NaN or Inf

    // Trailing action button (default: ◎ Target / Tare)
    Icon actionIcon{Icon::Target()};
    UiText actionTooltip{};                        ///< Fallback tooltip for action button when channel.actionTooltip is empty

    // Localizable dropdown titles & precision entries
    UiText channelMenuTitle{"CHANNEL"};
    UiText precisionMenuTitle{"PRECISION"};
    UiText unitMenuTitle{"UNITS"};
    keel::ArrayRef<UiText> precisionLabels{};      ///< Optional localized precision labels indexed by (precision - minPrecision)
};

/// Extended response returned by `ui.Readout(...)`.
struct ReadoutResponse : public Response {
    bool channelChanged{false};    ///< True on the frame a new channel was selected from the header dropdown
    bool unitChanged{false};       ///< True on the frame a new display unit was selected from the unit dropdown
    bool precisionChanged{false};  ///< True on the frame a new decimal precision was selected from the value dropdown
    bool actionClicked{false};     ///< True on the frame the trailing action button (e.g. ◎ tare) was clicked
    keel::f64 displayedValue{0.0};    ///< Converted numeric value currently rendered (before string formatting)
};

/// Autonomous engineering Digital Readout widget with direct-click channel, precision, unit dropdowns and trailing action button.
namespace detail {

class Readout {
public:
    /// Multi-channel engineering Readout slot (header -> channel, value -> precision, unit -> units, trailing -> action).
    static ReadoutResponse Run(
        Context& ctx,
        ReadoutState& state,
        keel::ArrayRef<ReadoutChannel> channels,
        const ReadoutOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Single-channel convenience overload.
    static ReadoutResponse Run(
        Context& ctx,
        UiText label,
        keel::f64 value,
        keel::i32& unitIndex,
        keel::i32& precision,
        keel::ArrayRef<ReadoutUnit> units = {},
        const ReadoutOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    static Vec2 Measure(
        keel::StringRef headerText,
        keel::StringRef valueText,
        keel::StringRef unitText,
        bool showAction,
        const ReadoutOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme
    );

    static void Render(const Node* node, ICanvas& canvas, const UiTheme& theme);
};

} // namespace detail

inline ReadoutResponse Readout(
    Context& ui,
    ReadoutState& state,
    keel::ArrayRef<ReadoutChannel> channels,
    ReadoutOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    return detail::Readout::Run(ui, state, channels, opts, loc);
}

inline ReadoutResponse Readout(
    Context& ui,
    UiText label,
    keel::f64 value,
    keel::i32& unitIndex,
    keel::i32& precision,
    keel::ArrayRef<ReadoutUnit> units = {},
    ReadoutOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    return detail::Readout::Run(ui, label, value, unitIndex, precision, units, opts, loc);
}

} // namespace guikit
