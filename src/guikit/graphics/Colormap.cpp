#include "guikit/graphics/Colormap.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include "guikit/backend/ICanvas.hpp"

namespace guikit {

namespace {

struct RgbaStop {
    float t;
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

[[nodiscard]] Color SampleGradientStops(std::span<const RgbaStop> stops, float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    if (stops.empty()) {
        return Color(0, 0, 0, 255);
    }
    for (size_t i = 0; i + 1 < stops.size(); ++i) {
        if (t >= stops[i].t && t <= stops[i + 1].t) {
            const float span = std::max(1e-5f, stops[i + 1].t - stops[i].t);
            const float u = (t - stops[i].t) / span;
            return Color(
                static_cast<uint8_t>(std::clamp(std::round(stops[i].r + u * (stops[i + 1].r - stops[i].r)), 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(std::round(stops[i].g + u * (stops[i + 1].g - stops[i].g)), 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(std::round(stops[i].b + u * (stops[i + 1].b - stops[i].b)), 0.0f, 255.0f)),
                255
            );
        }
    }
    const auto& last = stops.back();
    return Color(last.r, last.g, last.b, 255);
}

[[nodiscard]] std::span<const RgbaStop> GetStopsForColormap(Colormap map) noexcept {
    static constexpr RgbaStop kGrayscale[] = {
        {0.00f,   0,   0,   0},
        {1.00f, 255, 255, 255}
    };
    static constexpr RgbaStop kBone[] = {
        {0.00f,   0,   0,   0},
        {0.375f, 84,  84, 116},
        {0.75f, 168, 199, 199},
        {1.00f, 255, 255, 255}
    };
    static constexpr RgbaStop kAmber[] = {
        {0.00f,   6,   4,   2},
        {0.30f,  72,  42,  12},
        {0.65f, 178, 116,  34},
        {0.88f, 242, 196,  92},
        {1.00f, 255, 248, 220}
    };
    static constexpr RgbaStop kInferno[] = {
        {0.00f,   6,   5,  18},
        {0.20f,  52,  15,  94},
        {0.44f, 138,  34, 106},
        {0.68f, 218,  78,  42},
        {0.88f, 250, 174,  20},
        {1.00f, 252, 252, 164}
    };
    static constexpr RgbaStop kMagma[] = {
        {0.00f,   8,   6,  24},
        {0.20f,  48,  18,  96},
        {0.42f, 126,  36, 130},
        {0.65f, 204,  68, 108},
        {0.85f, 248, 138,  98},
        {1.00f, 252, 244, 196}
    };
    static constexpr RgbaStop kViridis[] = {
        {0.00f,  18,  10,  46},
        {0.22f,  49,  64, 122},
        {0.46f,  36, 126, 141},
        {0.72f,  58, 182, 114},
        {0.88f, 144, 214,  68},
        {1.00f, 253, 231,  37}
    };
    static constexpr RgbaStop kPlasma[] = {
        {0.00f,  13,   8, 135},
        {0.25f, 106,   0, 168},
        {0.50f, 177,  42, 144},
        {0.75f, 225, 100,  98},
        {0.90f, 252, 166,  54},
        {1.00f, 240, 249,  33}
    };
    static constexpr RgbaStop kCividis[] = {
        {0.00f,   0,  32,  76},
        {0.25f,  42,  64, 108},
        {0.50f, 102, 105, 112},
        {0.75f, 166, 156,  86},
        {1.00f, 253, 234,  69}
    };
    static constexpr RgbaStop kTurbo[] = {
        {0.00f,   5,   8,  22},
        {0.16f,  12,  36, 108},
        {0.36f,   6, 148, 196},
        {0.56f,  34, 197,  94},
        {0.76f, 250, 204,  21},
        {0.92f, 244,  63,  94},
        {1.00f, 255, 245, 245}
    };
    static constexpr RgbaStop kJet[] = {
        {0.00f,   5,   9,  20},
        {0.18f,  10,  38,  98},
        {0.40f,   8, 135, 175},
        {0.62f,  34, 197,  94},
        {0.82f, 250, 204,  21},
        {1.00f, 244,  63,  94}
    };
    static constexpr RgbaStop kCyanHot[] = {
        {0.00f,   7,  11,  24},
        {0.20f,  14,  52, 112},
        {0.42f,  14, 158, 218},
        {0.64f,  72, 226, 210},
        {0.84f, 249, 152,  36},
        {1.00f, 255, 250, 230}
    };
    static constexpr RgbaStop kHot[] = {
        {0.00f,   4,   0,   0},
        {0.35f, 185,   0,   0},
        {0.70f, 255, 165,   0},
        {0.90f, 255, 240,  90},
        {1.00f, 255, 255, 255}
    };
    static constexpr RgbaStop kRed[] = {
        {0.00f,   8,   4,   6},
        {0.25f,  48,   8,  14},
        {0.55f, 128,  18,  24},
        {0.82f, 220,  48,  38},
        {1.00f, 255, 165, 120}
    };
    static constexpr RgbaStop kGreen[] = {
        {0.00f,   4,  11,   8},
        {0.20f,   8,  38,  20},
        {0.48f,  16, 112,  48},
        {0.76f,  34, 212,  84},
        {0.92f, 134, 248, 158},
        {1.00f, 232, 255, 238}
    };
    static constexpr RgbaStop kBlue[] = {
        {0.00f,   4,   8,  18},
        {0.22f,   8,  28,  68},
        {0.48f,  14,  82, 158},
        {0.74f,  38, 178, 238},
        {0.92f, 145, 232, 255},
        {1.00f, 240, 252, 255}
    };
    static constexpr RgbaStop kGold[] = {
        {0.00f,   5,   8,  15},
        {0.15f,  24,  16,  12},
        {0.38f,  82,  42,  14},
        {0.62f, 168,  88,  20},
        {0.84f, 238, 168,  42},
        {1.00f, 255, 246, 195}
    };

    switch (map) {
        case Colormap::Grayscale: return kGrayscale;
        case Colormap::Bone:      return kBone;
        case Colormap::Amber:     return kAmber;
        case Colormap::Inferno:   return kInferno;
        case Colormap::Magma:     return kMagma;
        case Colormap::Viridis:   return kViridis;
        case Colormap::Plasma:    return kPlasma;
        case Colormap::Cividis:   return kCividis;
        case Colormap::Turbo:     return kTurbo;
        case Colormap::Jet:       return kJet;
        case Colormap::CyanHot:   return kCyanHot;
        case Colormap::Hot:       return kHot;
        case Colormap::Red:       return kRed;
        case Colormap::Green:     return kGreen;
        case Colormap::Blue:      return kBlue;
        case Colormap::Gold:      return kGold;
        case Colormap::Count:     break;
    }
    return kGrayscale;
}

constexpr size_t kColormapCount = static_cast<size_t>(Colormap::Count);

struct ColormapLutCache {
    std::array<std::array<Color, 256>, kColormapCount> colorNormal{};
    std::array<std::array<Color, 256>, kColormapCount> colorInverted{};
    std::array<std::array<uint32_t, 256>, kColormapCount> abgrNormal{};
    std::array<std::array<uint32_t, 256>, kColormapCount> abgrInverted{};

    ColormapLutCache() noexcept {
        for (size_t m = 0; m < kColormapCount; ++m) {
            const auto cmap = static_cast<Colormap>(m);
            const auto stops = GetStopsForColormap(cmap);
            for (size_t i = 0; i < 256; ++i) {
                const float t = static_cast<float>(i) / 255.0f;
                const Color c = SampleGradientStops(stops, t);
                colorNormal[m][i] = c;
                abgrNormal[m][i] = PackAbgr(c);
            }
            for (size_t i = 0; i < 256; ++i) {
                colorInverted[m][i] = colorNormal[m][255 - i];
                abgrInverted[m][i] = abgrNormal[m][255 - i];
            }
        }
    }
};

[[nodiscard]] const ColormapLutCache& GetCache() noexcept {
    static const ColormapLutCache s_cache{};
    return s_cache;
}

[[nodiscard]] constexpr size_t SanitizeIndex(Colormap map) noexcept {
    const size_t idx = static_cast<size_t>(map);
    return idx < kColormapCount ? idx : 0u;
}

} // namespace

Color SampleColormap(Colormap map, float t, bool invert) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    if (invert) {
        t = 1.0f - t;
    }
    return SampleGradientStops(GetStopsForColormap(map), t);
}

uint32_t SampleColormapAbgr(Colormap map, float t, bool invert) noexcept {
    return PackAbgr(SampleColormap(map, t, invert));
}

const std::array<Color, 256>& GetColormapLut(Colormap map, bool invert) noexcept {
    const auto& cache = GetCache();
    const size_t idx = SanitizeIndex(map);
    return invert ? cache.colorInverted[idx] : cache.colorNormal[idx];
}

const std::array<uint32_t, 256>& GetColormapLutAbgr(Colormap map, bool invert) noexcept {
    const auto& cache = GetCache();
    const size_t idx = SanitizeIndex(map);
    return invert ? cache.abgrInverted[idx] : cache.abgrNormal[idx];
}

void CopyColormapLutAbgr(Colormap map, uint32_t outAbgr[256], bool invert) noexcept {
    if (!outAbgr) return;
    const auto& lut = GetColormapLutAbgr(map, invert);
    std::memcpy(outAbgr, lut.data(), 256 * sizeof(uint32_t));
}

std::string_view GetColormapName(Colormap map) noexcept {
    const auto names = GetAllColormapNames();
    const size_t idx = SanitizeIndex(map);
    return names[idx];
}

std::span<const std::string_view> GetAllColormapNames() noexcept {
    static constexpr std::array<std::string_view, kColormapCount> kNames = {{
        "Grayscale",
        "Bone",
        "Amber",
        "Inferno",
        "Magma",
        "Viridis",
        "Plasma",
        "Cividis",
        "Turbo",
        "Jet",
        "CyanHot",
        "Hot",
        "Red",
        "Green",
        "Blue",
        "Gold"
    }};
    return kNames;
}

void DrawColormapBar(
    ICanvas& canvas,
    Rect rect,
    Colormap map,
    bool vertical,
    bool invert,
    float cornerRadius,
    Color borderColor
) {
    if (rect.w <= 0.0f || rect.h <= 0.0f) {
        return;
    }
    if (!vertical) {
        // Horizontal 256x1 pixel strip stretched smoothly by DrawPixels (left = 0.0, right = 1.0)
        const auto& lut = GetColormapLutAbgr(map, invert);
        canvas.DrawPixels(lut.data(), 256, 1, rect, cornerRadius, 255);
    } else {
        // Vertical 1x256 pixel strip (top = 1.0, bottom = 0.0) backed by static LUT cache
        const auto& vertLut = GetColormapLutAbgr(map, !invert);
        canvas.DrawPixels(vertLut.data(), 1, 256, rect, cornerRadius, 255);
    }
    if (borderColor.a > 0) {
        canvas.DrawRoundRect(rect, cornerRadius, Color::Transparent(), borderColor, 1.0f);
    }
}

} // namespace guikit
