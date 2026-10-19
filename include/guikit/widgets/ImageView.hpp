#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <source_location>
#include <span>
#include <string_view>
#include <vector>
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/graphics/Colormap.hpp"
#include "guikit/graphics/Icon.hpp"
#include "guikit/widgets/Histogram.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Pixel sample data type for `ImageViewData`.
enum class ImageSampleFormat : uint8_t {
    Float32 = 0,    ///< 32-bit floating-point intensity or RGB[A] samples
    UInt16,         ///< 16-bit unsigned integer (e.g. 12/14/16-bit radiograph, thermal, scientific CMOS)
    UInt8           ///< 8-bit unsigned integer (0..255)
};

/// Which tail(s) of the intensity distribution are clipped by quantile Auto Window.
enum class AutoWindowTarget : uint8_t {
    Black = 0,      ///< Clip only the low (black) tail quantile
    White,          ///< Clip only the high (white) tail quantile
    All             ///< Clip both low and high tails symmetrically
};

/// Quantile clipping strength for Auto Window.
enum class AutoWindowStrength : uint8_t {
    Off = 0,        ///< 0% tail clip (full [min, max] dynamic range)
    Medium,         ///< 1.0% tail clip (removes outlier hot/dead pixels and stretches contrast)
    Strong          ///< 5.0% tail clip (aggressive contrast expansion)
};

/// Orthogonal rotation angle applied to the displayed image.
enum class ImageRotation : uint8_t {
     deg0 = 0,
    deg90,
    deg180,
    deg270
};

/// Channel isolation mode when viewing a multi-channel (RGB/RGBA) image.
enum class ImageDisplayChannel : uint8_t {
    CompositeRgb = 0, ///< Full multi-channel RGB color composite
    RedOnly,          ///< Isolate Red channel (allows Colormap pseudo-coloring)
    GreenOnly,        ///< Isolate Green channel (allows Colormap pseudo-coloring)
    BlueOnly,         ///< Isolate Blue channel (allows Colormap pseudo-coloring)
    LuminanceOnly     ///< Isolate perceptual Luminance 0.2126*R + 0.7152*G + 0.0722*B (allows Colormap)
};

/// Non-owning descriptor for a 1-, 3-, or 4-channel image buffer passed to `ui.ImageView`.
struct ImageViewData {
    const void* rawPixels{nullptr};
    size_t sampleCount{0};
    uint32_t width{0};
    uint32_t height{0};
    uint32_t channels{1};                       ///< 1 (Mono), 3 (RGB), or 4 (RGBA)
    ImageSampleFormat format{ImageSampleFormat::Float32};
    uint64_t dataGeneration{0};                 ///< Optional generation counter; 0 = auto-hash

    [[nodiscard]] bool Empty() const noexcept {
        return !rawPixels || width == 0 || height == 0 || channels == 0 ||
               sampleCount < static_cast<size_t>(width) * height * channels;
    }

    [[nodiscard]] static ImageViewData FromFloat(
        std::span<const float> pixels,
        uint32_t w,
        uint32_t h,
        uint32_t ch = 1,
        uint64_t gen = 0
    ) noexcept {
        return ImageViewData{
            .rawPixels = pixels.data(),
            .sampleCount = pixels.size(),
            .width = w,
            .height = h,
            .channels = ch,
            .format = ImageSampleFormat::Float32,
            .dataGeneration = gen
        };
    }

    [[nodiscard]] static ImageViewData FromU16(
        std::span<const uint16_t> pixels,
        uint32_t w,
        uint32_t h,
        uint32_t ch = 1,
        uint64_t gen = 0
    ) noexcept {
        return ImageViewData{
            .rawPixels = pixels.data(),
            .sampleCount = pixels.size(),
            .width = w,
            .height = h,
            .channels = ch,
            .format = ImageSampleFormat::UInt16,
            .dataGeneration = gen
        };
    }

    [[nodiscard]] static ImageViewData FromU8(
        std::span<const uint8_t> pixels,
        uint32_t w,
        uint32_t h,
        uint32_t ch = 3,
        uint64_t gen = 0
    ) noexcept {
        return ImageViewData{
            .rawPixels = pixels.data(),
            .sampleCount = pixels.size(),
            .width = w,
            .height = h,
            .channels = ch,
            .format = ImageSampleFormat::UInt8,
            .dataGeneration = gen
        };
    }
};

/// Interactive state and tonal parameters for `ui.ImageView` (can be persisted by caller or managed internally).
struct ImageViewParams {
    /// Per-channel Window/Level/Gamma/Invert: [0] = Master/Mono, [1] = Red, [2] = Green, [3] = Blue.
    std::array<ChannelTransferParams, 4> channels{};
    bool initialized{false};

    HistogramChannel activeHistChannel{HistogramChannel::Master};
    HistogramScaleMode histScaleMode{HistogramScaleMode::Sqrt};
    ImageDisplayChannel displayChannel{ImageDisplayChannel::CompositeRgb};
    Colormap colormap{Colormap::Grayscale};

    AutoWindowTarget autoTarget{AutoWindowTarget::All};
    AutoWindowStrength autoStrength{AutoWindowStrength::Off};

    ImageRotation rotation{ImageRotation::deg0};
    bool flipHorizontal{false};
    bool flipVertical{false};
};

/// Configuration options for the `ui.ImageView` widget.
struct ImageViewOptions {
    UiVariant variant{UiVariant::Default};
    Align align{Align::Start};
    StretchMode stretch{StretchMode::Fill};

    // --- Sizing & Geometry ---
    std::optional<float> width{std::nullopt};
    float height{620.0f};
    float minHeight{360.0f};
    SizeMode heightMode{SizeMode::Fit};
    StateColor background{};
    Border border{};

    // --- Workstation Layout & Features ---
    bool showSidebar{true};             ///< Render built-in control sidebar (Histogram, W/L, Colormap, Auto Window, Transform)
    bool sidebarOnRight{false};         ///< False = Sidebar on Left (drs-xview style); True = Sidebar on Right
    float sidebarWidth{300.0f};         ///< Width of the built-in sidebar in logical points
    bool showTopToolbar{true};          ///< Show quick zoom/fit/rotate/channel bar at top of image viewport
    std::optional<UiSize> toolbarSize{std::nullopt}; ///< Size token for top toolbar buttons (nullopt = inherit widget UiSize)
    bool showStatusBar{true};           ///< Show bottom telemetry bar (X, Y, Raw pixel values, Zoom, W/L)
    bool showPixelGridAtHighZoom{true}; ///< Draw pixel boundary grid when zoom >= 1200%
    bool enableLineProfile{true};       ///< Enable RMB cross-section Line Profile measurement tool
    bool enableCtrlWlDrag{true};        ///< Enable Ctrl + LMB drag on viewport for direct Window/Level adjustment

    // --- State & Accessibility ---
    bool disabled{false};
    std::string_view tooltip{};
};

/// Telemetry and interaction response returned by `ui.ImageView`.
struct ImageViewResponse : public Response {
    bool paramsChanged{false};          ///< True if Window/Level/Gamma/Colormap/Transform changed this frame
    float zoom{1.0f};                   ///< Current viewport zoom factor (1.0 = 100%)
    Vec2 panOffset{0.0f, 0.0f};         ///< Current pan offset in screen pixels

    bool cursorInsideImage{false};      ///< True when mouse cursor is over a valid source image pixel
    uint32_t cursorImageX{0};           ///< Source image X coordinate [0 .. width-1]
    uint32_t cursorImageY{0};           ///< Source image Y coordinate [0 .. height-1]
    std::array<float, 4> cursorSample{};///< Raw sample values under cursor (ch 0..3)
    float cursorLuminance{0.0f};        ///< Raw intensity (1ch) or luminance (3ch) under cursor

    bool profileActive{false};          ///< True when a cross-section RMB line profile is active
    Vec2 profileStartPx{0.0f, 0.0f};    ///< Line profile start in source image coordinates
    Vec2 profileEndPx{0.0f, 0.0f};      ///< Line profile end in source image coordinates
    uint32_t profileSampleCount{0};     ///< Number of samples along the cross-section
    float profileMin{0.0f};
    float profileMax{0.0f};
    float profileMean{0.0f};

    float dataMin{0.0f};                ///< Minimum sample value in the image
    float dataMax{65535.0f};            ///< Maximum sample value in the image
};

/// Production-ready single-channel (16-bit/float radiograph/scientific) and multi-channel (RGB) Image Viewer.
class ImageView {
public:
    static Vec2 Measure(
        const ImageViewData& data,
        const ImageViewOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    ) noexcept;

    static ImageViewResponse Run(
        Context& ctx,
        const ImageViewData& data,
        ImageViewParams& params,
        const ImageViewOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    static ImageViewResponse Run(
        Context& ctx,
        const ImageViewData& data,
        const ImageViewOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Computes quantile-based Auto Window on a 256-bin histogram distribution.
    static void ApplyQuantileAutoWindow(
        std::span<const uint32_t> bins,
        float minVal,
        float maxVal,
        AutoWindowTarget target,
        AutoWindowStrength strength,
        ChannelTransferParams& ioTransfer
    ) noexcept;
};

} // namespace guikit
