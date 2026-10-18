#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <source_location>
#include <span>
#include <string_view>
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/graphics/Colormap.hpp"
#include "guikit/graphics/Icon.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Vertical bin-height scaling law for the Histogram widget.
enum class HistogramScaleMode : uint8_t {
    Sqrt = 0,       ///< Square-root scaling sqrt(bin) / max(sqrt(bin)): highlights minor peaks alongside dominant background
    Logarithmic,    ///< Logarithmic scaling log(1 + bin) / log(1 + max(bin)) for extreme dynamic range
    Linear          ///< Linear proportional scaling bin / max(bin)
};

/// Active channel target for multi-channel (RGB + Luminance) histogram inspection and editing.
enum class HistogramChannel : uint8_t {
    Master = 0,     ///< Intensity (for 1-channel) or Master RGB / Luminance linked adjustment (for 3/4-channel)
    Red    = 1,     ///< Red channel independent window/level/gamma
    Green  = 2,     ///< Green channel independent window/level/gamma
    Blue   = 3      ///< Blue channel independent window/level/gamma
};

/// Window / Level / Gamma tonal transfer parameters for a single channel or master curve.
struct ChannelTransferParams {
    float window{65535.0f}; ///< Width of the active tonal window (high - low)
    float level{32767.5f};  ///< Center of the active tonal window (0.5 * (low + high))
    float gamma{1.0f};      ///< Midtone gamma exponent (0.10 .. 5.00; 1.0 = linear)
    bool invert{false};     ///< True to invert output slope (1.0 - t)

    [[nodiscard]] constexpr float Low() const noexcept {
        return level - window * 0.5f;
    }

    [[nodiscard]] constexpr float High() const noexcept {
        return level + window * 0.5f;
    }

    constexpr void SetLowHigh(float lo, float hi, float minSpan = 1e-4f) noexcept {
        if (hi < lo + minSpan) {
            hi = lo + minSpan;
        }
        window = hi - lo;
        level = 0.5f * (lo + hi);
    }

    constexpr void ResetToRange(float minVal, float maxVal) noexcept {
        const float span = std::max(1e-4f, maxVal - minVal);
        window = span;
        level = minVal + span * 0.5f;
        gamma = 1.0f;
        invert = false;
    }

    /// Evaluates normalized output intensity [0.0, 1.0] for an input sample `val`.
    [[nodiscard]] float Evaluate(float val) const noexcept {
        const float lo = Low();
        const float span = std::max(1e-5f, window);
        float t = std::clamp((val - lo) / span, 0.0f, 1.0f);
        const float safeGamma = std::clamp(gamma, 0.05f, 10.0f);
        if (std::abs(safeGamma - 1.0f) > 1e-3f) {
            t = std::pow(t, 1.0f / safeGamma);
        }
        if (invert) {
            t = 1.0f - t;
        }
        return t;
    }
};

/// Input bin distribution descriptor for single-channel (Intensity) or multi-channel (RGB + Luminance) histograms.
struct HistogramData {
    uint32_t channelCount{1};                   ///< 1 = monochrome intensity, 3 = RGB + Luminance
    std::span<const uint32_t> binsLum{};        ///< Primary intensity (1ch) or Luminance (3ch) bins
    std::span<const uint32_t> binsR{};          ///< Red channel bins (when channelCount >= 3)
    std::span<const uint32_t> binsG{};          ///< Green channel bins (when channelCount >= 3)
    std::span<const uint32_t> binsB{};          ///< Blue channel bins (when channelCount >= 3)
    float minValue{0.0f};                       ///< Minimum physical/intensity value corresponding to bin 0
    float maxValue{65535.0f};                   ///< Maximum physical/intensity value corresponding to last bin

    [[nodiscard]] static constexpr HistogramData Mono(
        std::span<const uint32_t> bins,
        float minVal = 0.0f,
        float maxVal = 65535.0f
    ) noexcept {
        return HistogramData{
            .channelCount = 1,
            .binsLum = bins,
            .minValue = minVal,
            .maxValue = maxVal
        };
    }

    [[nodiscard]] static constexpr HistogramData Rgb(
        std::span<const uint32_t> lum,
        std::span<const uint32_t> r,
        std::span<const uint32_t> g,
        std::span<const uint32_t> b,
        float minVal = 0.0f,
        float maxVal = 255.0f
    ) noexcept {
        return HistogramData{
            .channelCount = 3,
            .binsLum = lum,
            .binsR = r,
            .binsG = g,
            .binsB = b,
            .minValue = minVal,
            .maxValue = maxVal
        };
    }
};

/// Configuration options for the interactive `ui.Histogram` widget.
struct HistogramOptions {
    // --- Element Companion Header / Footer ---
    UiText label{};
    UiText description{};
    Icon icon{};
    UiText badge{};
    UiVariant variant{UiVariant::Default};
    ElementLayout layout{ElementLayout::Stacked};
    InlineOrder inlineOrder{InlineOrder::LabelFirst};
    Align align{Align::Start};
    StretchMode stretch{StretchMode::Fill};

    // --- Sizing & Geometry ---
    std::optional<float> width{std::nullopt};
    float height{136.0f};
    float minHeight{96.0f};
    SizeMode heightMode{SizeMode::Fit};
    std::optional<UiSize> size{std::nullopt};
    std::optional<float> borderRadius{std::nullopt};

    // --- Histogram Visualization & Interaction ---
    HistogramScaleMode scaleMode{HistogramScaleMode::Sqrt};
    HistogramChannel activeChannel{HistogramChannel::Master};
    Colormap colormap{Colormap::Grayscale};     ///< Colormap strip shown in 1-channel mode
    bool showColorbar{true};                    ///< Show bottom gradient strip
    bool showTransferCurve{true};               ///< Show tonal transfer response line/curve inside [low, high]
    bool showChannelPills{true};                ///< Show interactive [RGB | R | G | B] pills when channelCount >= 3
    bool showRangeReadout{true};                ///< Show [low .. high] numerical range overlay
    bool interactive{true};                     ///< Allow dragging low/high handles and window band

    /// Optional per-channel transfer parameters (indices 1..3 = R, G, B) for multi-curve overlay in Master RGB view.
    std::optional<std::array<ChannelTransferParams, 4>> multiChannelParams{std::nullopt};

    // --- State & Accessibility ---
    bool disabled{false};
    std::string_view tooltip{};
};

/// Extended telemetry and interaction response returned by `ui.Histogram`.
struct HistogramResponse : public Response {
    bool windowLevelChanged{false};             ///< True when window or level was modified by user drag or reset
    bool channelChanged{false};                 ///< True when activeChannel was switched via header pills
    HistogramChannel activeChannel{HistogramChannel::Master};
    float window{65535.0f};
    float level{32767.5f};
    float lowValue{0.0f};
    float highValue{65535.0f};

    int hoveredBin{-1};                         ///< Bin index [0..N-1] under cursor, or -1 if not hovered
    float hoveredValue{0.0f};                   ///< Physical intensity value at cursor X
    uint32_t hoveredCount{0};                   ///< Sample count in hoveredBin for the active channel
};

/// Autonomous interactive Histogram & Window/Level transfer curve widget.
class Histogram {
public:
    static Vec2 Measure(
        const HistogramData& data,
        const HistogramOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    ) noexcept;

    static HistogramResponse Run(
        Context& ctx,
        const HistogramData& data,
        ChannelTransferParams& transfer,
        const HistogramOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );
};

} // namespace guikit
