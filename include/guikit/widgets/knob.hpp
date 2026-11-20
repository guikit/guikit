#pragma once

#include "keel/core.h"
#include "keel/optional.h"
#include "keel/ref.h"
#include "guikit/core/control.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/types.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Clean configuration options for rotary Knob / Dial widget.
struct KnobOptions {
    UiText valueFormat{};                            ///< Format string for value (e.g. "%.1f dB", "%.0f %%"; empty = default numeric format)
    keel::Optional<double> step{keel::kNullOpt};     ///< Step quantization (defaults to 1.0 for integers, continuous for float/double)
    keel::Optional<double> defaultValue{keel::kNullOpt}; ///< Optional reset value on double-click
    keel::u32 tickCount{11};                         ///< Number of radial tick marks when showTicks is true (0 = auto from step or 11)
    bool showValue{true};                            ///< Whether to display current formatted value text
    bool showTicks{true};                            ///< Render radial scale tick marks around the dial
    bool showArc{true};                              ///< Render outer track & active value arc around the rotor
    bool bipolar{false};                             ///< Sweep active arc from center/zero instead of minimum
    Placement valuePlacement{Placement::Bottom};     ///< Value position relative to dial: Bottom, Top, Thumb (inside center), Leading, Trailing
    keel::Optional<float> diameter{keel::kNullOpt};  ///< Optional explicit dial diameter in logical dp (nullopt = auto from UiSize)
    UiText tooltip{};                                ///< Optional floating tooltip text on hover
    bool disabled{false};                            ///< Interaction suppression
    UiSize size{UiSize::Md};                         ///< Control size token (`Xs`..`Xl`, `Dp(...)`, `Factor(...)`)
    StateColor color{};                              ///< Per-state semantic accent color override
};

/// Internal runtime state stored per-widget ID for Knob drag interaction.
struct KnobState {
    bool dragging{false};
    double dragStartVal{0.0};
    Vec2 dragStartMouse{};
};

/// Internal runtime payload allocated in frame arena for Knob nodes.
struct KnobData {
    KnobOptions opts{};
    float normalizedValue{0.0f};
    float normalizedOrigin{0.0f};
    keel::StringRef valueText{};
    bool active{false};
};

/// Autonomous rotary Knob widget logic (execution, measuring, and rendering).
class Knob {
public:
    static Response Run(
        Context& ctx,
        double* doubleVal,
        float* floatVal,
        keel::i64* intVal,
        double minVal,
        double maxVal,
        const KnobOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    static Vec2 Measure(
        const KnobOptions& opts,
        keel::StringRef valueText,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    );
    static Vec2 Measure(
        const KnobOptions& opts,
        keel::StringRef valueText,
        UiSize size,
        ICanvas* canvas = nullptr,
        float availableWidth = 0.0f
    );

    static void Render(
        ICanvas& canvas,
        Rect bounds,
        float normalizedValue,
        float normalizedOrigin,
        keel::StringRef valueText,
        const KnobOptions& opts,
        UiSize size,
        bool hovered,
        bool active,
        bool disabled,
        const UiTheme& theme,
        bool focused = false
    );
};

} // namespace guikit
