#include "guikit/geo/gis_render_engine.hpp"
#include "guikit/backend/vector_backend.hpp"
#include "guikit/geo/polygon_triangulator.hpp"
#include "keel/algo.h"
#include "keel/array.h"
#include "keel/core.h"
#include "keel/format.h"
#include "keel/fs.h"
#include "keel/hash.h"
#include "keel/hash_map.h"
#include "keel/hash_set.h"
#include "keel/math.h"
#include "keel/smart_ptr.h"
#include "keel/string.h"
#include "keel/time.h"
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstring>
#if defined(__GLIBC__)
#include <malloc.h>
#endif
#include <mutex>
#include <thread>

namespace guikit::geo {

namespace {

std::atomic<GisRenderEngine::BreadcrumbCallback> g_breadcrumbCallback{nullptr};

void EmitRenderBreadcrumb(const char* stage, keel::i32 z = -1, keel::i32 x = -1, keel::i32 y = -1) noexcept {
    if (auto cb = g_breadcrumbCallback.load(std::memory_order_relaxed)) {
        cb(stage, z, x, y);
    }
}

std::mutex& GetDefaultBasemapMutex() {
    static std::mutex s_mtx;
    return s_mtx;
}

keel::Arc<IRasterTileProvider>& GetDefaultBasemapStorage() {
    static keel::Arc<IRasterTileProvider> s_provider;
    return s_provider;
}

keel::Arc<IRasterTileProvider>& GetLastActiveBasemapStorage() {
    static keel::Arc<IRasterTileProvider> s_lastProvider;
    return s_lastProvider;
}

constexpr bool kEnableFileDebugLog = false;

[[maybe_unused]] void AppendRenderDebugLog(keel::StringRef line) noexcept {
    if constexpr (!kEnableFileDebugLog) {
        (void)line;
        return;
    }
}

constexpr float kTileGuardBandPx = 8.0f;

[[nodiscard]] constexpr keel::u32 ResolveTileSizePx(keel::u32 reqSize) noexcept {
    if (reqSize <= 192u) return 128u;
    if (reqSize >= 384u) return 512u;
    return 256u;
}

[[nodiscard]] bool IsCautionOrRestrictedArea(const geo::GeoFeature& feat) noexcept {
    if (feat.chartClass != geo::ChartFeatureClass::SeabedOrMarineArea) return false;
    return feat.featureCode == "CTNARE" ||
           feat.featureCode == "RESARE" ||
           feat.featureCode == "TSEZNE" ||
           feat.featureCode == "MIPARE" ||
           feat.featureCode == "SPLARE" ||
           feat.featureCode == "FSHZNE" ||
           feat.featureCode == "ICNARE";
}

// ============================================================================
// Color & Hash Utilities
// ============================================================================

[[nodiscard]] guikit::Color WithAlpha(guikit::Color c, float alphaFactor) noexcept {
    const float a = keel::clamp(alphaFactor * 255.0f, 0.0f, 255.0f);
    return guikit::Color(c.r, c.g, c.b, static_cast<keel::u8>(keel::round_i32(a)));
}

[[nodiscard]] guikit::Color BrightenColor(guikit::Color c, keel::i32 delta) noexcept {
    return guikit::Color(
        static_cast<keel::u8>(keel::clamp(static_cast<keel::i32>(c.r) + delta, 0, 255)),
        static_cast<keel::u8>(keel::clamp(static_cast<keel::i32>(c.g) + delta, 0, 255)),
        static_cast<keel::u8>(keel::clamp(static_cast<keel::i32>(c.b) + delta, 0, 255)),
        c.a
    );
}

[[nodiscard]] guikit::Color LerpColor(guikit::Color a, guikit::Color b, float t) noexcept {
    const float ct = keel::clamp(t, 0.0f, 1.0f);
    return guikit::Color(
        static_cast<keel::u8>(
            keel::clamp(keel::round_i32(keel::lerp(static_cast<float>(a.r), static_cast<float>(b.r), ct)), 0, 255)
        ),
        static_cast<keel::u8>(
            keel::clamp(keel::round_i32(keel::lerp(static_cast<float>(a.g), static_cast<float>(b.g), ct)), 0, 255)
        ),
        static_cast<keel::u8>(
            keel::clamp(keel::round_i32(keel::lerp(static_cast<float>(a.b), static_cast<float>(b.b), ct)), 0, 255)
        ),
        static_cast<keel::u8>(
            keel::clamp(keel::round_i32(keel::lerp(static_cast<float>(a.a), static_cast<float>(b.a), ct)), 0, 255)
        )
    );
}

void HashCombineBytes(keel::u64& h, const void* data, keel::usize len) noexcept {
    h = keel::fnv1a64_bytes(data, len, h);
}

template <typename T>
void HashCombineVal(keel::u64& h, const T& val) noexcept {
    HashCombineBytes(h, &val, sizeof(T));
}

// ============================================================================
// IHO S-52 Marine Symbology & Depth Shading Resolver
// ============================================================================

[[nodiscard]] bool ShouldSkipFeatureAtZoom(
    const geo::GeoFeature& feat,
    keel::i32 tileZoom,
    const GisRenderConfig& cfg
) noexcept {
    if (feat.chartClass == geo::ChartFeatureClass::CoverageMask) {
        return !cfg.showCoverageBounds;
    }
    if (!cfg.showCautionAndRestrictedZones && IsCautionOrRestrictedArea(feat)) {
        return true;
    }
    if (feat.chartClass == geo::ChartFeatureClass::DepthContour && !cfg.showDepthContours) {
        return true;
    }
    if (feat.chartClass == geo::ChartFeatureClass::Sounding && !cfg.showSoundings) {
        return true;
    }
    if (!cfg.scaleFiltering) {
        return false;
    }

    // Progressive detail disclosure by zoom level for dense bathymetric contours
    if (feat.chartClass == geo::ChartFeatureClass::DepthContour) {
        const float d = feat.depthMeters;
        if (!std::isnan(d)) {
            const float twiceD = std::round(d * 2.0f);
            const bool isHalfMeter = (std::abs(twiceD - std::round(d) * 2.0f) > 0.2f);
            const bool isFiveMeter = (std::abs(std::fmod(d, 5.0f)) < 0.15f) || (std::abs(d - cfg.safetyContourM) < 0.15f);
            if (tileZoom <= 9 && !isFiveMeter) {
                return true;
            }
            if (tileZoom <= 11 && isHalfMeter) {
                return true;
            }
        }
    }

    return false;
}

struct ResolvedFeatureStyle {
    guikit::Color fillColor{0, 0, 0, 0};
    guikit::Color strokeColor{0, 0, 0, 0};
    float strokeWidthPx{1.0f};
    bool renderFill{false};
    bool renderStroke{false};
};

[[nodiscard]] guikit::Color SampleBathymetryColor(
    float depthM,
    const GisRenderConfig& cfg
) noexcept {
    const float safeShallow = keel::max(0.2f, keel::min(cfg.shallowContourM, cfg.safetyContourM));
    const float safeContour = keel::max(safeShallow + 0.2f, cfg.safetyContourM);
    const float safeDeep    = keel::max(safeContour + 1.0f, cfg.deepContourM);

    if (cfg.chartPalette == ChartPalette::MarineNight) {
        if (depthM <= 0.02f) return guikit::Color(22, 58, 48, 255);     // Intertidal dark teal
        if (depthM <= safeShallow) return guikit::Color(14, 68, 108, 255);
        if (depthM <= safeContour) {
            const float t = (depthM - safeShallow) / keel::max(0.1f, safeContour - safeShallow);
            return LerpColor(guikit::Color(14, 68, 108, 255), guikit::Color(11, 48, 80, 255), t);
        }
        if (depthM <= safeDeep) {
            const float t = (depthM - safeContour) / keel::max(0.1f, safeDeep - safeContour);
            return LerpColor(guikit::Color(11, 48, 80, 255), guikit::Color(8, 28, 48, 255), t);
        }
        return guikit::Color(6, 19, 34, 255);
    }

    if (cfg.chartPalette == ChartPalette::MarineDusk) {
        if (depthM <= 0.02f) return guikit::Color(72, 132, 108, 255);
        if (depthM <= safeShallow) return guikit::Color(36, 118, 178, 255);
        if (depthM <= safeContour) {
            const float t = (depthM - safeShallow) / keel::max(0.1f, safeContour - safeShallow);
            return LerpColor(guikit::Color(44, 128, 188, 255), guikit::Color(32, 92, 142, 255), t);
        }
        if (depthM <= safeDeep) {
            const float t = (depthM - safeContour) / keel::max(0.1f, safeDeep - safeContour);
            return LerpColor(guikit::Color(32, 92, 142, 255), guikit::Color(22, 58, 92, 255), t);
        }
        return guikit::Color(17, 42, 68, 255);
    }

    // Default: MarineDay S-52 high-contrast hydrographic chart palette
    if (depthM <= 0.02f) {
        return guikit::Color(166, 217, 184, 255); // Intertidal green-teal
    }
    if (depthM <= safeShallow) {
        const float t = keel::clamp(depthM / safeShallow, 0.0f, 1.0f);
        return LerpColor(guikit::Color(66, 174, 242, 255), guikit::Color(94, 190, 248, 255), t);
    }
    if (depthM <= safeContour) {
        const float t = (depthM - safeShallow) / keel::max(0.1f, safeContour - safeShallow);
        return LerpColor(guikit::Color(118, 202, 250, 255), guikit::Color(154, 219, 252, 255), t);
    }
    const float midDeep = safeContour + (safeDeep - safeContour) * 0.45f;
    if (depthM <= midDeep) {
        const float t = (depthM - safeContour) / keel::max(0.1f, midDeep - safeContour);
        return LerpColor(guikit::Color(182, 228, 253, 255), guikit::Color(214, 240, 254, 255), t);
    }
    if (depthM <= safeDeep) {
        const float t = (depthM - midDeep) / keel::max(0.1f, safeDeep - midDeep);
        return LerpColor(guikit::Color(218, 241, 254, 255), guikit::Color(239, 249, 255, 255), t);
    }
    const float tDeep = keel::clamp((depthM - safeDeep) / 15.0f, 0.0f, 1.0f);
    return LerpColor(guikit::Color(242, 250, 255, 255), guikit::Color(250, 253, 255, 255), tDeep);
}

[[nodiscard]] guikit::Color ScaleColorAlpha(guikit::Color c, float alphaScale) noexcept {
    const float a = keel::clamp((static_cast<float>(c.a) / 255.0f) * alphaScale, 0.0f, 1.0f);
    return WithAlpha(c, a);
}

[[nodiscard]] ResolvedFeatureStyle ResolveMarineFeatureStyle(
    const geo::GeoLayer& layer,
    const geo::GeoFeature& feat,
    const GisRenderConfig& cfg,
    bool bakeFillOpacity = false
) noexcept {
    const geo::LayerStyle& ls = layer.style;
    const float strokeScale = keel::clamp(ls.strokeWidthPx / 2.0f, 0.15f, 5.0f);
    const float fillAlphaScale = bakeFillOpacity ? keel::clamp(ls.fillOpacity, 0.0f, 1.0f) : 1.0f;
    const bool customCol = ls.useCustomColor;

    // Classic GIS mode or unclassified user features use the layer's custom style
    if (cfg.chartPalette == ChartPalette::ClassicGis ||
        feat.chartClass == geo::ChartFeatureClass::Generic) {
        return {
            .fillColor = WithAlpha(ls.primaryColor, 0.48f * fillAlphaScale),
            .strokeColor = WithAlpha(ls.primaryColor, ls.strokeOpacity),
            .strokeWidthPx = ls.strokeWidthPx,
            .renderFill = ls.renderFill && feat.HasPolygons(),
            .renderStroke = ls.renderStroke
        };
    }

    const bool isNight = (cfg.chartPalette == ChartPalette::MarineNight);
    const bool isDusk  = (cfg.chartPalette == ChartPalette::MarineDusk);

    switch (feat.chartClass) {
        case geo::ChartFeatureClass::CoverageMask: {
            const guikit::Color covCol = customCol
                ? WithAlpha(ls.primaryColor, 0.75f)
                : (isNight ? guikit::Color(120, 80, 40, 140) : guikit::Color(217, 119, 6, 150));
            return {
                .fillColor = guikit::Color::Transparent(),
                .strokeColor = covCol,
                .strokeWidthPx = 1.0f * strokeScale,
                .renderFill = false,
                .renderStroke = cfg.showCoverageBounds && ls.renderStroke
            };
        }

        case geo::ChartFeatureClass::DepthArea: {
            const float dMin = feat.minDepthMeters;
            const float dMax = feat.maxDepthMeters;
            float dSample = 1.5f;
            if (!std::isnan(dMin) && !std::isnan(dMax)) {
                dSample = dMin * 0.7f + dMax * 0.3f;
            } else if (!std::isnan(dMin)) {
                dSample = (dMin > 0.02f || dMin < -0.02f) ? dMin : 1.5f;
            } else if (!std::isnan(dMax)) {
                dSample = dMax * 0.6f;
            }
            guikit::Color bathyCol = SampleBathymetryColor(dSample, cfg);
            if (customCol) {
                bathyCol = LerpColor(bathyCol, ls.primaryColor, 0.58f);
            }
            const bool explicitOutline = !ls.renderFill || (std::abs(ls.strokeWidthPx - 2.0f) > 0.08f);
            return {
                .fillColor = ScaleColorAlpha(bathyCol, fillAlphaScale),
                .strokeColor = BrightenColor(bathyCol, isNight ? 22 : -28),
                .strokeWidthPx = 0.65f * strokeScale,
                .renderFill = ls.renderFill,
                .renderStroke = explicitOutline && ls.renderStroke
            };
        }

        case geo::ChartFeatureClass::LandArea: {
            const bool isVeg = (feat.featureCode == "VEGATN");
            const bool hasBasemapOverlay = (cfg.showOsmBasemap || cfg.customBasemapProvider != nullptr);
            if (hasBasemapOverlay && cfg.hideLandOverBasemap && !isVeg) {
                const guikit::Color landStroke = customCol
                    ? BrightenColor(ls.primaryColor, isNight ? 28 : -38)
                    : (isNight
                        ? guikit::Color(88, 74, 54, 210)
                        : (isDusk ? guikit::Color(74, 60, 42, 220) : guikit::Color(139, 110, 62, 210)));
                return {
                    .fillColor = guikit::Color::Transparent(),
                    .strokeColor = landStroke,
                    .strokeWidthPx = 1.05f * strokeScale,
                    .renderFill = false,
                    .renderStroke = ls.renderStroke
                };
            }
            guikit::Color landFill = isVeg
                ? (isNight ? guikit::Color(28, 42, 26, 220) : (isDusk ? guikit::Color(86, 112, 74, 225) : guikit::Color(200, 224, 172, 235)))
                : (isNight ? guikit::Color(36, 31, 24, 255) : (isDusk ? guikit::Color(118, 102, 76, 255) : guikit::Color(234, 218, 179, 255)));
            if (customCol) {
                landFill = LerpColor(landFill, ls.primaryColor, 0.85f);
            }
            const guikit::Color landStroke = customCol
                ? BrightenColor(ls.primaryColor, isNight ? 28 : -38)
                : (isNight
                    ? guikit::Color(88, 74, 54, 235)
                    : (isDusk ? guikit::Color(74, 60, 42, 240) : guikit::Color(139, 110, 62, 240)));
            const bool explicitOutline = !ls.renderFill || (std::abs(ls.strokeWidthPx - 2.0f) > 0.08f);
            return {
                .fillColor = ScaleColorAlpha(landFill, fillAlphaScale),
                .strokeColor = landStroke,
                .strokeWidthPx = 1.15f * strokeScale,
                .renderFill = ls.renderFill,
                .renderStroke = explicitOutline && ls.renderStroke
            };
        }

        case geo::ChartFeatureClass::BuiltUpOrStructure: {
            guikit::Color structFill = isNight
                ? guikit::Color(58, 48, 36, 130)
                : (isDusk ? guikit::Color(138, 118, 88, 130) : guikit::Color(206, 182, 138, 120));
            guikit::Color structStroke = isNight
                ? guikit::Color(130, 110, 82, 220)
                : guikit::Color(92, 73, 46, 215);
            if (customCol) {
                structFill = WithAlpha(ls.primaryColor, 225.0f / 255.0f);
                structStroke = BrightenColor(ls.primaryColor, -35);
            }
            return {
                .fillColor = ScaleColorAlpha(structFill, fillAlphaScale),
                .strokeColor = structStroke,
                .strokeWidthPx = 1.25f * strokeScale,
                .renderFill = feat.HasPolygons() && ls.renderFill,
                .renderStroke = ls.renderStroke
            };
        }

        case geo::ChartFeatureClass::FairwayArea: {
            guikit::Color fwFill = isNight
                ? guikit::Color(48, 28, 68, 75)
                : guikit::Color(168, 85, 247, 45);
            guikit::Color fwStroke = isNight
                ? guikit::Color(192, 132, 252, 185)
                : guikit::Color(147, 51, 234, 175);
            if (customCol) {
                fwFill = WithAlpha(ls.primaryColor, 0.32f);
                fwStroke = WithAlpha(ls.primaryColor, 0.85f);
            }
            return {
                .fillColor = ScaleColorAlpha(fwFill, fillAlphaScale),
                .strokeColor = fwStroke,
                .strokeWidthPx = 1.1f * strokeScale,
                .renderFill = feat.HasPolygons() && ls.renderFill,
                .renderStroke = ls.renderStroke
            };
        }

        case geo::ChartFeatureClass::SeabedOrMarineArea: {
            guikit::Color marFill = isNight
                ? guikit::Color(18, 52, 82, 65)
                : guikit::Color(56, 189, 248, 42);
            guikit::Color marStroke = isNight
                ? guikit::Color(56, 189, 248, 140)
                : guikit::Color(2, 132, 199, 140);
            if (customCol) {
                marFill = WithAlpha(ls.primaryColor, 0.30f);
                marStroke = WithAlpha(ls.primaryColor, 0.75f);
            }
            return {
                .fillColor = ScaleColorAlpha(marFill, fillAlphaScale),
                .strokeColor = marStroke,
                .strokeWidthPx = 0.9f * strokeScale,
                .renderFill = feat.HasPolygons() && ls.renderFill,
                .renderStroke = ls.renderStroke
            };
        }

        case geo::ChartFeatureClass::DepthContour: {
            const float d = feat.depthMeters;
            const bool isSafety = !std::isnan(d) && (d <= cfg.safetyContourM + 0.08f);
            const bool isMajor  = !std::isnan(d) && (std::abs(std::fmod(d, 5.0f)) < 0.12f);

            guikit::Color cntCol;
            if (customCol) {
                const float alpha = isSafety ? 0.96f : (isMajor ? 0.86f : 0.74f);
                cntCol = WithAlpha(isSafety ? BrightenColor(ls.primaryColor, -18) : ls.primaryColor, alpha);
            } else if (isNight) {
                cntCol = isSafety
                    ? guikit::Color(56, 189, 248, 225)
                    : (isMajor ? guikit::Color(38, 132, 185, 195) : guikit::Color(28, 96, 142, 165));
            } else if (isDusk) {
                cntCol = isSafety
                    ? guikit::Color(125, 211, 252, 230)
                    : (isMajor ? guikit::Color(72, 162, 212, 200) : guikit::Color(54, 128, 178, 175));
            } else {
                cntCol = isSafety
                    ? guikit::Color(24, 92, 145, 235)
                    : (isMajor ? guikit::Color(52, 116, 168, 210) : guikit::Color(88, 146, 192, 185));
            }
            const float widthPx = (isSafety ? 1.35f : (isMajor ? 1.05f : 0.78f)) * strokeScale;
            return {
                .fillColor = guikit::Color::Transparent(),
                .strokeColor = cntCol,
                .strokeWidthPx = widthPx,
                .renderFill = false,
                .renderStroke = ls.renderStroke && cfg.showDepthContours
            };
        }

        case geo::ChartFeatureClass::Coastline: {
            const guikit::Color coastCol = customCol
                ? WithAlpha(ls.primaryColor, 0.96f)
                : (isNight
                    ? guikit::Color(168, 142, 102, 245)
                    : (isDusk ? guikit::Color(214, 190, 145, 245) : guikit::Color(62, 47, 28, 250)));
            return {
                .fillColor = guikit::Color::Transparent(),
                .strokeColor = coastCol,
                .strokeWidthPx = 1.65f * strokeScale,
                .renderFill = false,
                .renderStroke = ls.renderStroke
            };
        }

        case geo::ChartFeatureClass::SubmarineCablePipe:
        case geo::ChartFeatureClass::NavigationLine: {
            const guikit::Color navCol = customCol
                ? WithAlpha(ls.primaryColor, 0.90f)
                : (isNight
                    ? guikit::Color(216, 110, 232, 215)
                    : guikit::Color(162, 28, 175, 215));
            return {
                .fillColor = guikit::Color::Transparent(),
                .strokeColor = navCol,
                .strokeWidthPx = 1.35f * strokeScale,
                .renderFill = false,
                .renderStroke = ls.renderStroke
            };
        }

        case geo::ChartFeatureClass::UnderwaterHazard: {
            guikit::Color hazFill = isNight
                ? guikit::Color(14, 116, 144, 115)
                : guikit::Color(56, 189, 248, 95);
            guikit::Color hazStroke = isNight
                ? guikit::Color(248, 113, 113, 210)
                : guikit::Color(30, 58, 138, 210);
            if (customCol) {
                hazFill = WithAlpha(ls.primaryColor, 0.42f);
                hazStroke = WithAlpha(ls.primaryColor, 0.90f);
            }
            return {
                .fillColor = ScaleColorAlpha(hazFill, fillAlphaScale),
                .strokeColor = hazStroke,
                .strokeWidthPx = 1.05f * strokeScale,
                .renderFill = feat.HasPolygons() && ls.renderFill,
                .renderStroke = ls.renderStroke
            };
        }

        default:
            return {
                .fillColor = WithAlpha(ls.primaryColor, 0.45f * fillAlphaScale),
                .strokeColor = WithAlpha(ls.primaryColor, ls.strokeOpacity),
                .strokeWidthPx = ls.strokeWidthPx,
                .renderFill = feat.HasPolygons() && ls.renderFill,
                .renderStroke = ls.renderStroke
            };
    }
}

[[nodiscard]] keel::i32 CartographicPainterRank(const geo::GeoFeature& feat) noexcept {
    // Standard IHO S-52 painter order: base water (DepthArea) -> LandArea -> marine/fairway areas -> structures/hazards -> contours -> coastline
    switch (feat.chartClass) {
        case geo::ChartFeatureClass::DepthArea:           return 10;
        case geo::ChartFeatureClass::LandArea:            return 15;
        case geo::ChartFeatureClass::SeabedOrMarineArea:  return 20;
        case geo::ChartFeatureClass::FairwayArea:         return 25;
        case geo::ChartFeatureClass::UnderwaterHazard:    return 30;
        case geo::ChartFeatureClass::BuiltUpOrStructure:  return 35;
        case geo::ChartFeatureClass::Generic:
            return feat.HasPolygons() ? 40 : (feat.HasLines() ? 75 : 95);
        case geo::ChartFeatureClass::DepthContour:        return 50;
        case geo::ChartFeatureClass::SubmarineCablePipe:  return 60;
        case geo::ChartFeatureClass::NavigationLine:      return 65;
        case geo::ChartFeatureClass::Coastline:           return 70;
        case geo::ChartFeatureClass::CoverageMask:        return 80;
        default:                                          return 90;
    }
}

[[nodiscard]] keel::i32 CartographicLayerRank(const geo::GeoLayer& layer) noexcept {
    switch (layer.dominantChartClass) {
        case geo::ChartFeatureClass::DepthArea:           return 10;
        case geo::ChartFeatureClass::LandArea:            return 15;
        case geo::ChartFeatureClass::SeabedOrMarineArea:  return 20;
        case geo::ChartFeatureClass::FairwayArea:         return 25;
        case geo::ChartFeatureClass::UnderwaterHazard:    return 30;
        case geo::ChartFeatureClass::BuiltUpOrStructure:  return 35;
        case geo::ChartFeatureClass::DepthContour:        return 50;
        case geo::ChartFeatureClass::SubmarineCablePipe:  return 60;
        case geo::ChartFeatureClass::NavigationLine:      return 65;
        case geo::ChartFeatureClass::Coastline:           return 70;
        case geo::ChartFeatureClass::CoverageMask:        return 80;
        default:                                          break;
    }
    keel::i32 minRank = 90;
    const keel::usize sampleLimit = keel::min<keel::usize>(layer.features.size(), 32);
    for (keel::usize i = 0; i < sampleLimit; ++i) {
        const geo::GeoFeature& f = layer.features[i];
        if (!f.HasPolygons() && !f.HasLines()) continue;
        minRank = keel::min(minRank, CartographicPainterRank(f));
    }
    return minRank;
}

// ============================================================================
// Multi-CRS Tile Bounds & Vertex Reprojection + Sutherland-Hodgman Clipper
// ============================================================================

struct CellNorm3857Bounds {
    float u0{0.0f};
    float u1{1.0f};
    float v0{0.0f};
    float v1{1.0f};
    float spanU{1.0f};
    float spanV{1.0f};
};

// Computes the normalized EPSG:3857 [u0..u1] x [v0..v1] bounding box of a tile cell
// whose geographic bounds `cell.geoBounds` were generated in `crs` (EPSG:3857, EPSG:3395, or EPSG:4326).
[[nodiscard]] CellNorm3857Bounds ComputeCellNorm3857Bounds(
    const guikit::OsmTileCell& cell,
    guikit::MapCrs crs
) noexcept {
    if (crs == guikit::MapCrs::EPSG3857_WebMercator) {
        const double invTileN = 1.0 / static_cast<double>(1 << keel::clamp(cell.id.z, 0, 22));
        const float u0 = static_cast<float>(static_cast<double>(cell.id.x) * invTileN);
        const float u1 = static_cast<float>(static_cast<double>(cell.id.x + 1) * invTileN);
        const float v0 = static_cast<float>(static_cast<double>(cell.id.y) * invTileN);
        const float v1 = static_cast<float>(static_cast<double>(cell.id.y + 1) * invTileN);
        return {u0, u1, v0, v1, keel::max(1e-9f, u1 - u0), keel::max(1e-9f, v1 - v0)};
    }
    const guikit::Vec2 nw3857 = guikit::MapGrid::GeoToNormalized(cell.geoBounds.nw, guikit::MapCrs::EPSG3857_WebMercator);
    const guikit::Vec2 se3857 = guikit::MapGrid::GeoToNormalized(cell.geoBounds.se, guikit::MapCrs::EPSG3857_WebMercator);
    const float u0 = keel::min(nw3857.x, se3857.x);
    const float u1 = keel::max(nw3857.x, se3857.x);
    const float v0 = keel::min(nw3857.y, se3857.y);
    const float v1 = keel::max(nw3857.y, se3857.y);
    return {u0, u1, v0, v1, keel::max(1e-9f, u1 - u0), keel::max(1e-9f, v1 - v0)};
}

// Projects a pre-computed EPSG:3857 normalized 2D point into tile pixel space [0..tileSizeF] for any active CRS.
// - For EPSG:3857 (and cylindrical CRS at z >= 8), uses the ultra-fast 2-multiply affine mapping.
// - For cylindrical CRS (EPSG:4326, EPSG:3395) at overview zooms z < 8, applies exact non-linear Y reprojection.
// - For non-cylindrical CRS (EPSG:32636 UTM, EPSG:20906 GSK-2011, EPSG:28406 Pulkovo-42, EPSG:3995 Polar),
//   applies exact 64-bit double-precision 2D reprojection so adjacent tiles match seamlessly at all zooms.
[[nodiscard]] inline guikit::Vec2 ProjectNorm3857PtToTilePx(
    guikit::Vec2 normPt3857,
    float u0,
    float v0,
    float invSpanU,
    float invSpanV,
    const guikit::OsmTileCell& cell,
    guikit::MapCrs crs,
    float tileSizeF
) noexcept {
    if (crs == guikit::MapCrs::EPSG3857_WebMercator ||
        (guikit::MapGrid::IsCylindricalCrs(crs) && cell.id.z >= 8)) {
        return {
            (normPt3857.x - u0) * invSpanU,
            (normPt3857.y - v0) * invSpanV
        };
    }

    const double tileN = static_cast<double>(1 << keel::clamp(cell.id.z, 0, 22));
    const double n = keel::kPi * (1.0 - 2.0 * static_cast<double>(normPt3857.y));
    const double latDeg = keel::rad_to_deg(std::atan(std::sinh(n)));

    if (guikit::MapGrid::IsCylindricalCrs(crs)) {
        const auto [uCrs, vCrs] = guikit::MapGrid::GeoToNormalizedDouble({latDeg, 0.0}, crs);
        (void)uCrs;
        return {
            (normPt3857.x - u0) * invSpanU,
            static_cast<float>((vCrs * tileN - static_cast<double>(cell.id.y)) * static_cast<double>(tileSizeF))
        };
    }

    const double lonDeg = static_cast<double>(normPt3857.x) * 360.0 - 180.0;
    const auto [uCrs, vCrs] = guikit::MapGrid::GeoToNormalizedDouble({latDeg, lonDeg}, crs);
    return {
        static_cast<float>((uCrs * tileN - static_cast<double>(cell.id.x)) * static_cast<double>(tileSizeF)),
        static_cast<float>((vCrs * tileN - static_cast<double>(cell.id.y)) * static_cast<double>(tileSizeF))
    };
}

void ClipRingToTileGuardBand(
    keel::ArrayRef<guikit::Vec2> normRing,
    float u0,
    float v0,
    float invSpanU,
    float invSpanV,
    const guikit::OsmTileCell& cell,
    guikit::MapCrs crs,
    float tileSizeF,
    float minDecimateSqPx,
    keel::Array<guikit::Vec2>& outClippedPx,
    keel::Array<guikit::Vec2>& scratchA,
    keel::Array<guikit::Vec2>& scratchB
) {
    outClippedPx.clear();
    if (normRing.size() < 3) return;

    scratchA.clear();
    scratchA.reserve(normRing.size());

    // Project from normalized EPSG:3857 [0..1] to tile pixel space [0..tileSizeF] in active CRS with sub-pixel decimation
    guikit::Vec2 prevPx{-1e9f, -1e9f};
    float minX =  1e30f;
    float maxX = -1e30f;
    float minY =  1e30f;
    float maxY = -1e30f;
    for (keel::usize i = 0; i < normRing.size(); ++i) {
        const guikit::Vec2 ptPx = ProjectNorm3857PtToTilePx(
            normRing[i], u0, v0, invSpanU, invSpanV, cell, crs, tileSizeF
        );
        const float dx = ptPx.x - prevPx.x;
        const float dy = ptPx.y - prevPx.y;
        if (i == 0 || i + 1 == normRing.size() || (dx * dx + dy * dy >= minDecimateSqPx)) {
            scratchA.push_back(ptPx);
            prevPx = ptPx;
            if (ptPx.x < minX) minX = ptPx.x;
            if (ptPx.x > maxX) maxX = ptPx.x;
            if (ptPx.y < minY) minY = ptPx.y;
            if (ptPx.y > maxY) maxY = ptPx.y;
        }
    }

    if (scratchA.size() < 3) return;

    const float kMinBound = -kTileGuardBandPx;
    const float kMaxBound = tileSizeF + kTileGuardBandPx;

    // Fast AABB Trivial Reject: all projected vertices lie strictly outside one of the 4 guard-band half-planes
    if (maxX < kMinBound || minX > kMaxBound || maxY < kMinBound || minY > kMaxBound) {
        return;
    }

    // Fast AABB Trivial Accept: all projected vertices lie completely inside the tile guard-band
    if (minX >= kMinBound && maxX <= kMaxBound && minY >= kMinBound && maxY <= kMaxBound) {
        keel::swap(outClippedPx, scratchA);
        return;
    }

    // Sutherland-Hodgman 4-plane axis-aligned box clip
    // Plane 1: x >= kMinBound
    scratchB.clear();
    {
        guikit::Vec2 s = scratchA.back();
        for (const guikit::Vec2& e : scratchA) {
            const bool eIn = (e.x >= kMinBound);
            const bool sIn = (s.x >= kMinBound);
            if (eIn) {
                if (!sIn) {
                    const float t = (kMinBound - s.x) / (e.x - s.x);
                    scratchB.push_back({kMinBound, s.y + t * (e.y - s.y)});
                }
                scratchB.push_back(e);
            } else if (sIn) {
                const float t = (kMinBound - s.x) / (e.x - s.x);
                scratchB.push_back({kMinBound, s.y + t * (e.y - s.y)});
            }
            s = e;
        }
    }
    if (scratchB.size() < 3) return;

    // Plane 2: x <= kMaxBound
    scratchA.clear();
    {
        guikit::Vec2 s = scratchB.back();
        for (const guikit::Vec2& e : scratchB) {
            const bool eIn = (e.x <= kMaxBound);
            const bool sIn = (s.x <= kMaxBound);
            if (eIn) {
                if (!sIn) {
                    const float t = (kMaxBound - s.x) / (e.x - s.x);
                    scratchA.push_back({kMaxBound, s.y + t * (e.y - s.y)});
                }
                scratchA.push_back(e);
            } else if (sIn) {
                const float t = (kMaxBound - s.x) / (e.x - s.x);
                scratchA.push_back({kMaxBound, s.y + t * (e.y - s.y)});
            }
            s = e;
        }
    }
    if (scratchA.size() < 3) return;

    // Plane 3: y >= kMinBound
    scratchB.clear();
    {
        guikit::Vec2 s = scratchA.back();
        for (const guikit::Vec2& e : scratchA) {
            const bool eIn = (e.y >= kMinBound);
            const bool sIn = (s.y >= kMinBound);
            if (eIn) {
                if (!sIn) {
                    const float t = (kMinBound - s.y) / (e.y - s.y);
                    scratchB.push_back({s.x + t * (e.x - s.x), kMinBound});
                }
                scratchB.push_back(e);
            } else if (sIn) {
                const float t = (kMinBound - s.y) / (e.y - s.y);
                scratchB.push_back({s.x + t * (e.x - s.x), kMinBound});
            }
            s = e;
        }
    }
    if (scratchB.size() < 3) return;

    // Plane 4: y <= kMaxBound
    outClippedPx.reserve(scratchB.size() + 4);
    {
        guikit::Vec2 s = scratchB.back();
        for (const guikit::Vec2& e : scratchB) {
            const bool eIn = (e.y <= kMaxBound);
            const bool sIn = (s.y <= kMaxBound);
            if (eIn) {
                if (!sIn) {
                    const float t = (kMaxBound - s.y) / (e.y - s.y);
                    outClippedPx.push_back({s.x + t * (e.x - s.x), kMaxBound});
                }
                outClippedPx.push_back(e);
            } else if (sIn) {
                const float t = (kMaxBound - s.y) / (e.y - s.y);
                outClippedPx.push_back({s.x + t * (e.x - s.x), kMaxBound});
            }
            s = e;
        }
    }
    if (outClippedPx.size() < 3) {
        outClippedPx.clear();
    }
}

// Cohen-Sutherland 2D segment clipper against [-kTileGuardBandPx, tileSizeF + kTileGuardBandPx]
[[nodiscard]] bool ClipSegmentToTile(guikit::Vec2& p0, guikit::Vec2& p1, float tileSizeF) noexcept {
    const float kMin = -kTileGuardBandPx;
    const float kMax = tileSizeF + kTileGuardBandPx;

    auto outCode = [kMin, kMax](float x, float y) noexcept -> keel::i32 {
        keel::i32 c = 0;
        if (x < kMin) c |= 1;
        else if (x > kMax) c |= 2;
        if (y < kMin) c |= 4;
        else if (y > kMax) c |= 8;
        return c;
    };

    keel::i32 c0 = outCode(p0.x, p0.y);
    keel::i32 c1 = outCode(p1.x, p1.y);

    for (keel::i32 iter = 0; iter < 8; ++iter) {
        if ((c0 | c1) == 0) return true;
        if ((c0 & c1) != 0) return false;

        const keel::i32 cOut = c0 ? c0 : c1;
        float x = 0.0f;
        float y = 0.0f;
        const float dx = p1.x - p0.x;
        const float dy = p1.y - p0.y;

        if (cOut & 8) {
            x = p0.x + dx * (kMax - p0.y) / dy;
            y = kMax;
        } else if (cOut & 4) {
            x = p0.x + dx * (kMin - p0.y) / dy;
            y = kMin;
        } else if (cOut & 2) {
            y = p0.y + dy * (kMax - p0.x) / dx;
            x = kMax;
        } else {
            y = p0.y + dy * (kMin - p0.x) / dx;
            x = kMin;
        }

        if (cOut == c0) {
            p0 = {x, y};
            c0 = outCode(p0.x, p0.y);
        } else {
            p1 = {x, y};
            c1 = outCode(p1.x, p1.y);
        }
    }
    return false;
}

keel::usize AppendPolylineToVectorPath(
    guikit::VectorPath& path,
    keel::ArrayRef<guikit::Vec2> normLine,
    float u0,
    float v0,
    float invSpanU,
    float invSpanV,
    const guikit::OsmTileCell& cell,
    guikit::MapCrs crs,
    float tileSizeF,
    float minDecimateSqPx,
    bool closed
) {
    if (normLine.size() < 2) return 0;

    keel::usize emittedSegments = 0;
    bool penDown = false;
    guikit::Vec2 lastDrawn{-1e9f, -1e9f};
    guikit::Vec2 prevPx = ProjectNorm3857PtToTilePx(
        normLine[0], u0, v0, invSpanU, invSpanV, cell, crs, tileSizeF
    );
    const guikit::Vec2 firstPx = prevPx;

    auto emitSegment = [&](guikit::Vec2 a, guikit::Vec2 b) {
        if (!ClipSegmentToTile(a, b, tileSizeF)) {
            penDown = false;
            return;
        }
        const float cdx = b.x - a.x;
        const float cdy = b.y - a.y;
        if (cdx * cdx + cdy * cdy < 0.01f) {
            return;
        }
        if (!penDown || std::abs(a.x - lastDrawn.x) > 0.25f || std::abs(a.y - lastDrawn.y) > 0.25f) {
            path.MoveTo(a);
        }
        path.LineTo(b);
        lastDrawn = b;
        penDown = true;
        ++emittedSegments;
    };

    for (keel::usize i = 1; i < normLine.size(); ++i) {
        const guikit::Vec2 currPx = ProjectNorm3857PtToTilePx(
            normLine[i], u0, v0, invSpanU, invSpanV, cell, crs, tileSizeF
        );
        const float dx = currPx.x - prevPx.x;
        const float dy = currPx.y - prevPx.y;
        const bool isLast = (i + 1 == normLine.size());
        if (!isLast && (dx * dx + dy * dy < minDecimateSqPx)) {
            continue;
        }
        emitSegment(prevPx, currPx);
        prevPx = currPx;
    }

    if (closed && normLine.size() >= 3) {
        const float dx = firstPx.x - prevPx.x;
        const float dy = firstPx.y - prevPx.y;
        if (dx * dx + dy * dy > 0.25f) {
            emitSegment(prevPx, firstPx);
        }
    }
    return emittedSegments;
}

// ============================================================================
// Two-Tier Offscreen [z/x/y] Vector Tile Cache & Atomic Compositor
//   Tier 1: Per-Layer Sparse Premultiplied ABGR8888 Tiles
//   Tier 2: Per-Cell Composited Opaque ABGR8888 Screen Tiles + Mipmap Fallback
// ============================================================================

[[nodiscard]] constexpr keel::u32 PackColorABGR8888(guikit::Color c) noexcept {
    return (static_cast<keel::u32>(c.a) << 24) |
           (static_cast<keel::u32>(c.b) << 16) |
           (static_cast<keel::u32>(c.g) << 8)  |
           static_cast<keel::u32>(c.r);
}

void BlendPremulTileOntoOpaqueComposite(
    keel::u32* __restrict dst,
    const keel::u32* __restrict src,
    keel::u8 layerOpacity,
    keel::u32 tileSizePx
) noexcept {
    const keel::usize kNumPx = static_cast<keel::usize>(tileSizePx) * static_cast<keel::usize>(tileSizePx);
    if (layerOpacity == 0) return;

    if (layerOpacity == 255) {
        for (keel::usize i = 0; i < kNumPx; ++i) {
            const keel::u32 s = src[i];
            if (s == 0u) continue;
            const keel::u32 sa = s >> 24;
            if (sa == 255u) {
                dst[i] = s;
            } else {
                const keel::u32 d = dst[i];
                const keel::u32 invA = 255u - sa;
                const keel::u32 sr = s & 0xFFu;
                const keel::u32 sg = (s >> 8) & 0xFFu;
                const keel::u32 sb = (s >> 16) & 0xFFu;
                const keel::u32 dr = keel::min(255u, sr + (((d & 0xFFu) * invA + 127u) >> 8));
                const keel::u32 dg = keel::min(255u, sg + ((((d >> 8) & 0xFFu) * invA + 127u) >> 8));
                const keel::u32 db = keel::min(255u, sb + ((((d >> 16) & 0xFFu) * invA + 127u) >> 8));
                dst[i] = 0xFF000000u | (db << 16) | (dg << 8) | dr;
            }
        }
    } else {
        const keel::u32 op = layerOpacity;
        for (keel::usize i = 0; i < kNumPx; ++i) {
            const keel::u32 s = src[i];
            if (s == 0u) continue;
            const keel::u32 sa = ((s >> 24) * op + 127u) >> 8;
            if (sa == 0u) continue;
            const keel::u32 sr = ((s & 0xFFu) * op + 127u) >> 8;
            const keel::u32 sg = (((s >> 8) & 0xFFu) * op + 127u) >> 8;
            const keel::u32 sb = (((s >> 16) & 0xFFu) * op + 127u) >> 8;
            const keel::u32 d = dst[i];
            const keel::u32 invA = 255u - sa;
            const keel::u32 dr = keel::min(255u, sr + (((d & 0xFFu) * invA + 127u) >> 8));
            const keel::u32 dg = keel::min(255u, sg + ((((d >> 8) & 0xFFu) * invA + 127u) >> 8));
            const keel::u32 db = keel::min(255u, sb + ((((d >> 16) & 0xFFu) * invA + 127u) >> 8));
            dst[i] = 0xFF000000u | (db << 16) | (dg << 8) | dr;
        }
    }
}

std::mutex& GetCanvasLifecycleMutex() {
    static std::mutex s_mtx;
    return s_mtx;
}

[[nodiscard]] keel::UniquePtr<guikit::ICanvas> CreateCanvasThreadSafe() {
    std::scoped_lock lk(GetCanvasLifecycleMutex());
    return guikit::CreateOffscreenVectorCanvas();
}

void DestroyCanvasThreadSafe(keel::UniquePtr<guikit::ICanvas>& canvas) noexcept {
    if (!canvas) return;
    std::scoped_lock lk(GetCanvasLifecycleMutex());
    canvas.reset();
}

void BlendStraightRgbaBasemapOntoComposite(
    keel::u32* __restrict dst,
    const keel::u32* __restrict src,
    keel::u8 basemapOpacity,
    keel::u32 tileSizePx
) noexcept {
    const keel::usize kNumPx = static_cast<keel::usize>(tileSizePx) * static_cast<keel::usize>(tileSizePx);
    if (basemapOpacity == 0) return;

    if (basemapOpacity == 255) {
        for (keel::usize i = 0; i < kNumPx; ++i) {
            const keel::u32 s = src[i];
            const keel::u32 sa = s >> 24;
            if (sa == 0u) continue;
            if (sa == 255u) {
                dst[i] = s;
            } else {
                const keel::u32 d = dst[i];
                const keel::u32 invA = 255u - sa;
                const keel::u32 sr = s & 0xFFu;
                const keel::u32 sg = (s >> 8) & 0xFFu;
                const keel::u32 sb = (s >> 16) & 0xFFu;
                const keel::u32 dr = ((sr * sa + (d & 0xFFu) * invA) + 127u) >> 8;
                const keel::u32 dg = ((sg * sa + ((d >> 8) & 0xFFu) * invA) + 127u) >> 8;
                const keel::u32 db = ((sb * sa + ((d >> 16) & 0xFFu) * invA) + 127u) >> 8;
                dst[i] = 0xFF000000u | (db << 16) | (dg << 8) | dr;
            }
        }
    } else {
        const keel::u32 op = basemapOpacity;
        for (keel::usize i = 0; i < kNumPx; ++i) {
            const keel::u32 s = src[i];
            const keel::u32 sa = ((s >> 24) * op + 127u) >> 8;
            if (sa == 0u) continue;
            const keel::u32 d = dst[i];
            const keel::u32 invA = 255u - sa;
            const keel::u32 sr = s & 0xFFu;
            const keel::u32 sg = (s >> 8) & 0xFFu;
            const keel::u32 sb = (s >> 16) & 0xFFu;
            const keel::u32 dr = ((sr * sa + (d & 0xFFu) * invA) + 127u) >> 8;
            const keel::u32 dg = ((sg * sa + ((d >> 8) & 0xFFu) * invA) + 127u) >> 8;
            const keel::u32 db = ((sb * sa + ((d >> 16) & 0xFFu) * invA) + 127u) >> 8;
            dst[i] = 0xFF000000u | (db << 16) | (dg << 8) | dr;
        }
    }
}

struct TileCoordKey {
    keel::i32 z{0};
    keel::i32 x{0};
    keel::i32 y{0};

    [[nodiscard]] bool operator==(const TileCoordKey& o) const noexcept {
        return z == o.z && x == o.x && y == o.y;
    }
};

struct TileCoordKeyHasher {
    [[nodiscard]] keel::usize operator()(const TileCoordKey& k) const noexcept {
        keel::u64 h = static_cast<keel::u64>(k.z) * 0x9E3779B97F4A7C15ULL;
        h ^= static_cast<keel::u64>(k.x) * 0xBF58476D1CE4E5B9ULL;
        h ^= static_cast<keel::u64>(k.y) * 0x94D049BB133111EBULL;
        return static_cast<keel::usize>(h);
    }
};

struct CachedVectorTile {
    keel::Array<keel::u32> pixels;
    guikit::Rect normQueryBox{};
    keel::u64 styleHash{0};
    keel::u64 lastUsedFrame{0};
    keel::u64 lastRasterizedFrame{0};
    bool hasAnyGeometry{false};
};

struct CompositeTile {
    keel::Array<keel::u32> pixels;
    guikit::Rect normQueryBox{};
    keel::u64 compositeSignature{0};
    keel::u64 lastUsedFrame{0};
    keel::u64 lastUpdatedFrame{0};
    bool isComplete{false};
};

struct AsyncTileCandidate {
    keel::u32 featureIndex{0};
    keel::u64 featureUid{0};
    keel::i64 sqliteRowId{0};
    keel::i32 srsId{4326};
    geo::ChartFeatureClass chartClass{geo::ChartFeatureClass::Generic};
    float maxDimPx{256.0f};
    ResolvedFeatureStyle style{};
    keel::Arc<const geo::DecodedGeometryPayload> payload;
};

struct AsyncLayerTileJobKey {
    keel::u64 layerUid{0};
    keel::i32 z{0};
    keel::i32 x{0};
    keel::i32 y{0};
    keel::u32 tileSizePx{256};

    [[nodiscard]] bool operator==(const AsyncLayerTileJobKey& o) const noexcept {
        return layerUid == o.layerUid && z == o.z && x == o.x && y == o.y && tileSizePx == o.tileSizePx;
    }
};

struct AsyncLayerTileJobKeyHasher {
    [[nodiscard]] keel::usize operator()(const AsyncLayerTileJobKey& k) const noexcept {
        keel::u64 h = k.layerUid ^ 0x9E3779B97F4A7C15ULL;
        HashCombineVal(h, k.z);
        HashCombineVal(h, k.x);
        HashCombineVal(h, k.y);
        HashCombineVal(h, k.tileSizePx);
        return static_cast<keel::usize>(h);
    }
};

struct InFlightTileStamp {
    keel::u64 styleHash{0};
    keel::u64 windowUpdateFrame{0};
};

struct WindowGenCounter {
    alignas(8) keel::u64 value{0};

    keel::u64 FetchAddRelaxed(keel::u64 delta = 1) noexcept {
#if defined(__clang__) || defined(__GNUC__)
        return __atomic_fetch_add(&value, delta, __ATOMIC_RELAXED);
#else
        return std::atomic_ref<keel::u64>(value).fetch_add(delta, std::memory_order_relaxed);
#endif
    }

    [[nodiscard]] keel::u64 LoadRelaxed() const noexcept {
#if defined(__clang__) || defined(__GNUC__)
        return __atomic_load_n(&value, __ATOMIC_RELAXED);
#else
        return std::atomic_ref<const keel::u64>(value).load(std::memory_order_relaxed);
#endif
    }
};

struct AsyncLoadedFeaturePayload {
    keel::u64 layerUid{0};
    keel::u32 featureIndex{0};
    keel::u64 featureUid{0};
    keel::Arc<const geo::DecodedGeometryPayload> payload;
};

struct AsyncLayerTileJob {
    AsyncLayerTileJobKey jobKey{};
    keel::u64 topologyHash{0};
    keel::u64 styleHash{0};
    keel::u64 windowUpdateFrame{0};
    keel::u64 epoch{0};
    guikit::OsmTileCell cell{};
    guikit::MapCrs crs{guikit::MapCrs::EPSG3857_WebMercator};
    guikit::Rect queryBox{};
    bool lodSimplification{true};
    float lodPixelTolerance{1.25f};
    float distSq{0.0f};
    keel::Arc<geo::IFeaturePager> pager;
    keel::Array<AsyncTileCandidate> candidates;
};

struct AsyncLayerTileResult {
    AsyncLayerTileJobKey jobKey{};
    keel::u64 topologyHash{0};
    keel::u64 styleHash{0};
    keel::u64 windowUpdateFrame{0};
    keel::u64 epoch{0};
    guikit::Rect queryBox{};
    keel::Array<keel::u32> pixels;
    bool hasAnyGeometry{false};
    keel::Array<AsyncLoadedFeaturePayload> newlyLoadedPayloads;
};

struct AsyncWindowJob {
    keel::u64 layerUid{0};
    keel::Arc<geo::IFeaturePager> pager;
    geo::SpatialWindowRequest req{};
    keel::u64 epoch{0};
    keel::Arc<WindowGenCounter> activeGenPtr;
};

struct AsyncWindowResult {
    keel::u64 layerUid{0};
    keel::u64 generation{0};
    keel::u64 epoch{0};
    keel::Arc<geo::SpatialWindowSnapshot> snapshot;
};

struct AsyncBasemapJob {
    keel::String sourceId;
    keel::Arc<geo::IRasterTileProvider> provider;
    geo::SlippyTileKey tileKey{};
    float distSq{0.0f};
    keel::u64 epoch{0};
};

struct AsyncBasemapResult {
    keel::String sourceId;
    geo::SlippyTileKey tileKey{};
    keel::u64 epoch{0};
    keel::Arc<const geo::RasterTileImage> image;
};

class TilePixelBufferPool {
public:
    void AcquireFilled(keel::Array<keel::u32>& outBuf, keel::usize numPixels, keel::u32 fillVal) {
        if (outBuf.capacity() != numPixels) {
            if (outBuf.capacity() > 0u) {
                outBuf = keel::Array<keel::u32>{};
            }
            std::scoped_lock lk(m_mutex);
            if (m_activeNumPixels == numPixels && !m_freeBuffers.empty()) {
                outBuf = keel::move(m_freeBuffers.back());
                m_freeBuffers.pop_back();
            }
        }
        outBuf.assign(numPixels, fillVal);
    }

    void Recycle(keel::Array<keel::u32>& buf) noexcept {
        const keel::usize cap = buf.capacity();
        if (cap == 0u) {
            return;
        }
        buf.clear();
        {
            std::scoped_lock lk(m_mutex);
            if (m_activeNumPixels == 0u) {
                m_activeNumPixels = cap;
            }
            if (cap == m_activeNumPixels &&
                m_freeBuffers.size() < kMaxPooledBuffers &&
                (m_freeBuffers.size() + 1u) * cap * sizeof(keel::u32) <= kMaxPooledBytes) {
                m_freeBuffers.push_back(keel::move(buf));
                return;
            }
        }
        buf = keel::Array<keel::u32>{};
    }

    void Clear(keel::usize newNumPixels = 0u) noexcept {
        keel::Array<keel::Array<keel::u32>> toFree;
        {
            std::scoped_lock lk(m_mutex);
            m_activeNumPixels = newNumPixels;
            toFree = keel::move(m_freeBuffers);
            m_freeBuffers = {};
        }
    }

private:
    static constexpr keel::usize kMaxPooledBuffers = 16u;
    static constexpr keel::usize kMaxPooledBytes = 4u * 1024u * 1024u;
    std::mutex m_mutex;
    keel::usize m_activeNumPixels{0};
    keel::Array<keel::Array<keel::u32>> m_freeBuffers;
};

TilePixelBufferPool& GetTilePixelPool() {
    static TilePixelBufferPool s_pool;
    return s_pool;
}

void RasterizeCandidatesToPixels(
    guikit::ICanvas& canvas,
    guikit::VectorPath& fillPath,
    guikit::VectorPath& strokePath,
    keel::Array<guikit::Vec2>& clippedRing,
    keel::Array<guikit::Vec2>& scratchA,
    keel::Array<guikit::Vec2>& scratchB,
    keel::ArrayRef<AsyncTileCandidate> candidates,
    const guikit::OsmTileCell& cell,
    guikit::MapCrs crs,
    keel::u32 tileSizePx,
    const guikit::Rect& queryBox,
    bool lodSimplification,
    float lodPixelTolerance,
    keel::Array<keel::u32>& outPixels,
    bool& outHasAnyGeometry
) {
    const float tileSizeF = static_cast<float>(tileSizePx);
    const CellNorm3857Bounds cb = ComputeCellNorm3857Bounds(cell, crs);
    const float u0 = cb.u0;
    const float v0 = cb.v0;
    const float invSpanU = tileSizeF / cb.spanU;
    const float invSpanV = tileSizeF / cb.spanV;

    outHasAnyGeometry = false;
    auto ensureCanvasTargeted = [&]() {
        if (!outHasAnyGeometry) {
            GetTilePixelPool().AcquireFilled(
                outPixels,
                static_cast<keel::usize>(tileSizePx) * static_cast<keel::usize>(tileSizePx),
                0u
            );
            canvas.SetTarget(outPixels.data(), tileSizePx, tileSizePx, tileSizePx);
            outHasAnyGeometry = true;
        }
    };

    const float zoomDecimateBoost = (cell.id.z <= 10) ? 0.90f : ((cell.id.z == 11) ? 0.65f : 0.45f);
    const float minDecimateSqPx = lodSimplification
        ? (lodPixelTolerance * lodPixelTolerance * zoomDecimateBoost)
        : 0.16f;

    strokePath.Clear();
    guikit::PathPaint activeStrokePaint{};
    keel::u32 activeStrokeRgba = 0;
    keel::u32 activeStrokeWidthBits = 0;
    bool activeStrokeDashed = false;
    keel::usize activeStrokeSegments = 0;

    auto flushActiveStroke = [&]() {
        if (activeStrokeSegments > 0 && !strokePath.Empty()) {
            ensureCanvasTargeted();
            canvas.DrawPath(strokePath, activeStrokePaint);
            strokePath.Clear();
            activeStrokeSegments = 0;
        }
    };

    for (const AsyncTileCandidate& cand : candidates) {
        if (!cand.payload) continue;
        const geo::DecodedGeometryPayload& geom = *cand.payload;

        const bool useLodRings = lodSimplification && (cell.id.z <= 13 || cand.maxDimPx < 96.0f);
        const bool skipTinyHoles = (lodSimplification && (cell.id.z <= 11 && cand.maxDimPx < 14.0f)) || (cand.maxDimPx < 4.0f);
        const ResolvedFeatureStyle& st = cand.style;

        // 1. Polygon Fill (each GeoPolygon gets its own DrawPath call so overlapping polygons never XOR-cancel)
        if (!geom.polygons.empty() && st.renderFill && st.fillColor.a > 0) {
            flushActiveStroke();

            const bool sealAdjacentSeam = !st.renderStroke && (st.fillColor.a == 255) &&
                (cand.maxDimPx >= 4.0f) &&
                (cand.chartClass == geo::ChartFeatureClass::DepthArea ||
                 cand.chartClass == geo::ChartFeatureClass::LandArea);

            for (const geo::GeoPolygon& poly : geom.polygons) {
                if (poly.hasValidBounds && !poly.normBounds.Intersects(queryBox)) {
                    continue;
                }
                const auto& extRing = (useLodRings && poly.normExteriorLod.size() >= 3)
                    ? poly.normExteriorLod
                    : poly.normExterior;
                ClipRingToTileGuardBand(
                    extRing, u0, v0, invSpanU, invSpanV, cell, crs, tileSizeF, minDecimateSqPx,
                    clippedRing, scratchA, scratchB
                );
                if (clippedRing.size() < 3) continue;

                fillPath.Clear();
                fillPath.AddClosedRing(clippedRing);

                bool hasValidHoles = false;
                if (!skipTinyHoles) {
                    for (const auto& holeRing : poly.normInterior) {
                        ClipRingToTileGuardBand(
                            holeRing, u0, v0, invSpanU, invSpanV, cell, crs, tileSizeF, minDecimateSqPx,
                            clippedRing, scratchA, scratchB
                        );
                        if (clippedRing.size() < 3) continue;
                        fillPath.AddClosedRing(clippedRing);
                        hasValidHoles = true;
                    }
                }

                const guikit::PathPaint fillPaint{
                    .fillColor = st.fillColor,
                    .strokeColor = st.fillColor,
                    .strokeWidthPx = sealAdjacentSeam ? 0.65f : 0.0f,
                    .fillRule = hasValidHoles ? guikit::FillRule::EvenOdd : guikit::FillRule::NonZero,
                    .renderFill = true,
                    .renderStroke = sealAdjacentSeam
                };

                ensureCanvasTargeted();
                canvas.DrawPath(fillPath, fillPaint);
            }
        }

        // 2. Polygon Outline & LineStrings (Style-Bucket Compound Path Batching)
        if (st.renderStroke && st.strokeColor.a > 0 && st.strokeWidthPx > 0.0f) {
            const keel::u32 strokeRgba = st.strokeColor.ToUint32();
            keel::u32 strokeWidthBits = 0;
            std::memcpy(&strokeWidthBits, &st.strokeWidthPx, sizeof(float));
            const bool isDashed = (cand.chartClass == geo::ChartFeatureClass::NavigationLine ||
                                   cand.chartClass == geo::ChartFeatureClass::FairwayArea ||
                                   cand.chartClass == geo::ChartFeatureClass::CoverageMask);

            if (activeStrokeSegments > 0 &&
                (activeStrokeRgba != strokeRgba ||
                 activeStrokeWidthBits != strokeWidthBits ||
                 activeStrokeDashed != isDashed ||
                 activeStrokeSegments >= 256)) {
                flushActiveStroke();
            }

            if (activeStrokeSegments == 0) {
                activeStrokePaint = guikit::PathPaint{
                    .strokeColor = st.strokeColor,
                    .strokeWidthPx = st.strokeWidthPx,
                    .renderFill = false,
                    .renderStroke = true,
                    .dashed = isDashed,
                    .dashOnPx = 5.0f,
                    .dashOffPx = 4.0f
                };
                activeStrokeRgba = strokeRgba;
                activeStrokeWidthBits = strokeWidthBits;
                activeStrokeDashed = isDashed;
            }

            keel::usize addedStrokeSegs = 0;
            for (const geo::GeoPolygon& poly : geom.polygons) {
                if (poly.hasValidBounds && !poly.normBounds.Intersects(queryBox)) {
                    continue;
                }
                const auto& extRing = (useLodRings && poly.normExteriorLod.size() >= 3)
                    ? poly.normExteriorLod
                    : poly.normExterior;
                addedStrokeSegs += AppendPolylineToVectorPath(
                    strokePath, extRing, u0, v0, invSpanU, invSpanV, cell, crs, tileSizeF, minDecimateSqPx, /*closed=*/true);
                if (!skipTinyHoles) {
                    for (const auto& holeRing : poly.normInterior) {
                        addedStrokeSegs += AppendPolylineToVectorPath(
                            strokePath, holeRing, u0, v0, invSpanU, invSpanV, cell, crs, tileSizeF, minDecimateSqPx, /*closed=*/true);
                    }
                }
            }

            for (keel::usize li = 0; li < geom.normLines.size(); ++li) {
                const auto& line = (useLodRings && li < geom.normLinesLod.size() && geom.normLinesLod[li].size() >= 2)
                    ? geom.normLinesLod[li]
                    : geom.normLines[li];
                addedStrokeSegs += AppendPolylineToVectorPath(
                    strokePath, line, u0, v0, invSpanU, invSpanV, cell, crs, tileSizeF, minDecimateSqPx, /*closed=*/false);
            }

            activeStrokeSegments += addedStrokeSegs;
        }
    }

    flushActiveStroke();

    if (outHasAnyGeometry) {
        canvas.End();
    } else {
        GetTilePixelPool().Recycle(outPixels);
    }
}

[[nodiscard]] constexpr bool HasBackgroundWorkerThreads() noexcept {
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
    return false;
#else
    return true;
#endif
}

class AsyncChartStreamer {
public:
    AsyncChartStreamer() {
#if !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)
        m_windowWorker = std::thread([this]() { WindowWorkerLoop(); });

        const keel::u32 hw = keel::max(2u, std::thread::hardware_concurrency());
        const keel::usize numTileWorkers = keel::clamp<keel::usize>(hw / 2u, 2u, 4u);
        m_tileWorkers.reserve(numTileWorkers);
        for (keel::usize i = 0; i < numTileWorkers; ++i) {
            m_tileWorkers.emplace_back([this]() { TileWorkerLoop(); });
        }

        for (keel::usize i = 0; i < 2; ++i) {
            m_basemapWorkers.emplace_back([this]() { BasemapWorkerLoop(); });
        }
#endif
    }

    ~AsyncChartStreamer() {
        {
            std::scoped_lock lk(m_windowMutex, m_tileMutex, m_basemapMutex);
            m_shutdown.store(true, std::memory_order_release);
            for (auto [uid, genPtr] : m_layerWindowGenerations) {
                if (genPtr) genPtr->FetchAddRelaxed(1);
            }
            m_pendingWindowJobs.clear();
            m_pendingTileJobs.clear();
            m_pendingBasemapJobs.clear();
        }
        m_windowCv.notify_all();
        m_tileCv.notify_all();
        m_basemapCv.notify_all();

        if (m_windowWorker.joinable()) m_windowWorker.join();
        for (auto& t : m_tileWorkers) {
            if (t.joinable()) t.join();
        }
        for (auto& t : m_basemapWorkers) {
            if (t.joinable()) t.join();
        }
    }

    void ClearAndBumpEpoch() {
        const keel::u64 newEpoch = m_epoch.fetch_add(1, std::memory_order_acq_rel) + 1;
        {
            std::scoped_lock lk(m_windowMutex);
            for (auto [uid, genPtr] : m_layerWindowGenerations) {
                if (genPtr) genPtr->FetchAddRelaxed(1);
            }
            m_pendingWindowJobs.clear();
            m_completedWindowResults.clear();
        }
        {
            std::scoped_lock lk(m_tileMutex);
            m_pendingTileJobs.clear();
            m_inFlightTileKeys.clear();
            for (AsyncLayerTileResult& res : m_completedTileResults) {
                GetTilePixelPool().Recycle(res.pixels);
            }
            m_completedTileResults.clear();
        }
        {
            std::scoped_lock lk(m_basemapMutex);
            m_pendingBasemapJobs.clear();
            m_inFlightBasemapKeys.clear();
            m_completedBasemapResults.clear();
        }
        (void)newEpoch;
    }

    void TrimIdleMemory() noexcept {
        ClearAndBumpEpoch();
        {
            std::scoped_lock lk(m_windowMutex);
            m_pendingWindowJobs = {};
            m_completedWindowResults = {};
        }
        {
            std::scoped_lock lk(m_tileMutex);
            m_pendingTileJobs = {};
            m_inFlightTileKeys = {};
            m_completedTileResults = {};
            m_trimGeneration.fetch_add(1, std::memory_order_acq_rel);
        }
        {
            std::scoped_lock lk(m_basemapMutex);
            m_pendingBasemapJobs = {};
            m_inFlightBasemapKeys = {};
            m_completedBasemapResults = {};
        }
        m_tileCv.notify_all();
    }

    [[nodiscard]] keel::u64 GetCurrentEpoch() const noexcept {
        return m_epoch.load(std::memory_order_acquire);
    }

    void SetActiveTileZoom(keel::i32 z) noexcept {
        m_activeTileZoom.store(z, std::memory_order_relaxed);
    }

    void RequestSpatialWindow(
        const geo::GeoLayer& layer,
        const guikit::GeoBounds& visibleGeo,
        const guikit::Rect& visibleNormBox,
        float tileZoom,
        bool scaleFiltering,
        keel::u64 frameTick
    ) {
        if (!layer.pager) return;
        const keel::u64 layerUid = layer.uid;
        std::scoped_lock lk(m_windowMutex);
        auto& genPtr = m_layerWindowGenerations.get_or_insert(layerUid);
        if (!genPtr) {
            genPtr = keel::make_arc<WindowGenCounter>();
        }

        // Check if an identical window job is already pending or in-flight
        if (const AsyncWindowJob* cur = m_pendingWindowJobs.get(layerUid)) {
            if (std::abs(cur->req.tileZoom - tileZoom) < 0.05f &&
                cur->req.scaleFiltering == scaleFiltering &&
                std::abs(cur->req.viewBounds.nw.lat - visibleGeo.nw.lat) < 1e-6 &&
                std::abs(cur->req.viewBounds.nw.lon - visibleGeo.nw.lon) < 1e-6 &&
                std::abs(cur->req.viewBounds.se.lat - visibleGeo.se.lat) < 1e-6 &&
                std::abs(cur->req.viewBounds.se.lon - visibleGeo.se.lon) < 1e-6) {
                return;
            }
        }
        if (m_inFlightWindowLayerUid == layerUid &&
            std::abs(m_inFlightWindowZoom - tileZoom) < 0.05f &&
            m_inFlightWindowScaleFilter == scaleFiltering &&
            std::abs(m_inFlightWindowGeo.nw.lat - visibleGeo.nw.lat) < 1e-6 &&
            std::abs(m_inFlightWindowGeo.nw.lon - visibleGeo.nw.lon) < 1e-6 &&
            std::abs(m_inFlightWindowGeo.se.lat - visibleGeo.se.lat) < 1e-6 &&
            std::abs(m_inFlightWindowGeo.se.lon - visibleGeo.se.lon) < 1e-6) {
            return;
        }

        const keel::u64 nextGen = genPtr->FetchAddRelaxed(1) + 1;
        geo::SpatialWindowRequest req{};
        req.layerUid = layerUid;
        req.srsId = layer.srsId;
        req.layerBounds = layer.bounds;
        req.hasLayerBounds = layer.hasValidBounds;
        req.dominantClass = layer.dominantChartClass;
        req.chartClassMask = layer.chartClassMask;
        req.savedPointCount = layer.pointFeatureCount;
        req.savedLineCount = layer.lineFeatureCount;
        req.savedPolygonCount = layer.polygonFeatureCount;
        req.viewBounds = visibleGeo;
        req.normViewBox = visibleNormBox;
        req.tileZoom = tileZoom;
        req.scaleFiltering = scaleFiltering;
        req.frameTick = frameTick;
        req.requestGeneration = nextGen;

        m_pendingWindowJobs.put(layerUid, AsyncWindowJob{
            .layerUid = layerUid,
            .pager = layer.pager,
            .req = req,
            .epoch = GetCurrentEpoch(),
            .activeGenPtr = genPtr
        });
        m_windowCv.notify_one();
    }

    void DrainCompletedWindows(keel::Array<AsyncWindowResult>& outResults) {
        outResults.clear();
        const keel::u64 curEpoch = GetCurrentEpoch();
        std::scoped_lock lk(m_windowMutex);
        for (AsyncWindowResult& res : m_completedWindowResults) {
            if (res.epoch != curEpoch || !res.snapshot) continue;
            if (const auto* gPtr = m_layerWindowGenerations.get(res.layerUid); gPtr && *gPtr) {
                if (res.generation < (*gPtr)->LoadRelaxed()) {
                    continue; // Superseded by a newer window request for this layer
                }
            }
            outResults.push_back(keel::move(res));
        }
        m_completedWindowResults.clear();
    }

    [[nodiscard]] keel::usize GetPendingWindowCount() const {
        std::scoped_lock lk(m_windowMutex);
        return m_pendingWindowJobs.size() + m_inFlightWindowCount + m_completedWindowResults.size();
    }

    [[nodiscard]] bool IsLayerWindowPending(keel::u64 layerUid) const {
        std::scoped_lock lk(m_windowMutex);
        if (m_inFlightWindowLayerUid == layerUid && m_inFlightWindowCount > 0) {
            return true;
        }
        if (m_pendingWindowJobs.contains(layerUid)) {
            return true;
        }
        for (const AsyncWindowResult& res : m_completedWindowResults) {
            if (res.layerUid == layerUid) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool IsLayerTilePending(const AsyncLayerTileJobKey& key, keel::u64 styleHash, keel::u64 windowUpdateFrame) const {
        std::scoped_lock lk(m_tileMutex);
        if (const auto* inFlight = m_inFlightTileKeys.get(key)) {
            if (inFlight->styleHash == styleHash && inFlight->windowUpdateFrame == windowUpdateFrame) {
                return true;
            }
        }
        for (const AsyncLayerTileJob& job : m_pendingTileJobs) {
            if (job.jobKey == key && job.styleHash == styleHash && job.windowUpdateFrame == windowUpdateFrame) {
                return true;
            }
        }
        for (const AsyncLayerTileResult& res : m_completedTileResults) {
            if (res.jobKey == key && res.styleHash == styleHash && res.windowUpdateFrame == windowUpdateFrame) {
                return true;
            }
        }
        return false;
    }

    void EnqueueLayerTileJob(AsyncLayerTileJob&& job) {
        static constexpr keel::usize kMaxPendingTileJobs = 192u;
        job.epoch = GetCurrentEpoch();
        {
            std::scoped_lock lk(m_tileMutex);
            if (const auto* inFlight = m_inFlightTileKeys.get(job.jobKey)) {
                if (inFlight->styleHash == job.styleHash && inFlight->windowUpdateFrame == job.windowUpdateFrame) {
                    return;
                }
            }
            for (AsyncLayerTileJob& existing : m_pendingTileJobs) {
                if (existing.jobKey == job.jobKey) {
                    existing = keel::move(job);
                    m_tileCv.notify_one();
                    return;
                }
            }
            if (m_pendingTileJobs.size() >= kMaxPendingTileJobs) {
                const keel::i32 activeZ = m_activeTileZoom.load(std::memory_order_relaxed);
                for (keel::usize i = m_pendingTileJobs.size(); i > 0u; --i) {
                    if (std::abs(m_pendingTileJobs[i - 1u].jobKey.z - activeZ) > 1) {
                        m_pendingTileJobs.swap_remove(i - 1u);
                    }
                }
                if (m_pendingTileJobs.size() >= kMaxPendingTileJobs) {
                    keel::usize worstIdx = 0u;
                    for (keel::usize i = 1u; i < m_pendingTileJobs.size(); ++i) {
                        const bool worstMatchesZ = (m_pendingTileJobs[worstIdx].jobKey.z == activeZ);
                        const bool curMatchesZ = (m_pendingTileJobs[i].jobKey.z == activeZ);
                        if (worstMatchesZ != curMatchesZ) {
                            if (!curMatchesZ) worstIdx = i;
                        } else if (m_pendingTileJobs[i].distSq > m_pendingTileJobs[worstIdx].distSq) {
                            worstIdx = i;
                        }
                    }
                    const bool newMatchesZ = (job.jobKey.z == activeZ);
                    const bool worstMatchesZ = (m_pendingTileJobs[worstIdx].jobKey.z == activeZ);
                    if ((newMatchesZ && !worstMatchesZ) ||
                        (newMatchesZ == worstMatchesZ && job.distSq < m_pendingTileJobs[worstIdx].distSq)) {
                        m_pendingTileJobs[worstIdx] = keel::move(job);
                        m_tileCv.notify_one();
                    }
                    return;
                }
            }
            m_pendingTileJobs.push_back(keel::move(job));
        }
        m_tileCv.notify_one();
    }

    void DrainCompletedLayerTiles(keel::Array<AsyncLayerTileResult>& outResults) {
        outResults.clear();
        const keel::u64 curEpoch = GetCurrentEpoch();
        std::scoped_lock lk(m_tileMutex);
        for (AsyncLayerTileResult& res : m_completedTileResults) {
            if (res.epoch == curEpoch) {
                outResults.push_back(keel::move(res));
            } else {
                GetTilePixelPool().Recycle(res.pixels);
            }
        }
        m_completedTileResults.clear();
    }

    [[nodiscard]] keel::usize GetPendingLayerTileCount() const {
        std::scoped_lock lk(m_tileMutex);
        return m_pendingTileJobs.size() + m_inFlightTileCount + m_completedTileResults.size();
    }

    void EnqueueBasemapTileJob(
        const keel::Arc<geo::IRasterTileProvider>& provider,
        const geo::SlippyTileKey& tileKey,
        float distSq
    ) {
        if (!provider) return;
        const keel::u64 curEpoch = GetCurrentEpoch();
        const keel::StringRef srcId = provider->GetSourceId();
        if (!HasBackgroundWorkerThreads()) {
            auto img = provider->FetchTileBlocking(tileKey);
            if (img && img->IsValid()) {
                std::scoped_lock lk(m_basemapMutex);
                m_completedBasemapResults.push_back(AsyncBasemapResult{
                    .sourceId = keel::String::from(srcId),
                    .tileKey = tileKey,
                    .epoch = curEpoch,
                    .image = keel::move(img)
                });
            }
            return;
        }
        {
            static constexpr keel::usize kMaxPendingBasemapJobs = 128u;
            std::scoped_lock lk(m_basemapMutex);
            if (m_inFlightBasemapKeys.contains(tileKey)) {
                return;
            }
            for (AsyncBasemapJob& existing : m_pendingBasemapJobs) {
                if (existing.tileKey == tileKey && existing.sourceId == srcId) {
                    existing.distSq = keel::min(existing.distSq, distSq);
                    return;
                }
            }
            if (m_pendingBasemapJobs.size() >= kMaxPendingBasemapJobs) {
                const keel::i32 activeZ = m_activeTileZoom.load(std::memory_order_relaxed);
                for (keel::usize i = m_pendingBasemapJobs.size(); i > 0u; --i) {
                    if (std::abs(m_pendingBasemapJobs[i - 1u].tileKey.z - activeZ) > 2) {
                        m_pendingBasemapJobs.swap_remove(i - 1u);
                    }
                }
                if (m_pendingBasemapJobs.size() >= kMaxPendingBasemapJobs) {
                    keel::usize worstIdx = 0u;
                    for (keel::usize i = 1u; i < m_pendingBasemapJobs.size(); ++i) {
                        if (m_pendingBasemapJobs[i].distSq > m_pendingBasemapJobs[worstIdx].distSq) {
                            worstIdx = i;
                        }
                    }
                    if (distSq < m_pendingBasemapJobs[worstIdx].distSq) {
                        m_pendingBasemapJobs[worstIdx] = AsyncBasemapJob{
                            .sourceId = keel::String::from(srcId),
                            .provider = provider,
                            .tileKey = tileKey,
                            .distSq = distSq,
                            .epoch = curEpoch
                        };
                        m_basemapCv.notify_one();
                    }
                    return;
                }
            }
            m_pendingBasemapJobs.push_back(AsyncBasemapJob{
                .sourceId = keel::String::from(srcId),
                .provider = provider,
                .tileKey = tileKey,
                .distSq = distSq,
                .epoch = curEpoch
            });
        }
        m_basemapCv.notify_one();
    }

    void DrainCompletedBasemapTiles(keel::Array<AsyncBasemapResult>& outResults) {
        outResults.clear();
        const keel::u64 curEpoch = GetCurrentEpoch();
        std::scoped_lock lk(m_basemapMutex);
        for (AsyncBasemapResult& res : m_completedBasemapResults) {
            if (res.epoch == curEpoch) {
                outResults.push_back(keel::move(res));
            }
        }
        m_completedBasemapResults.clear();
    }

    [[nodiscard]] keel::usize GetPendingBasemapCount() const {
        std::scoped_lock lk(m_basemapMutex);
        return m_pendingBasemapJobs.size() + m_inFlightBasemapCount + m_completedBasemapResults.size();
    }

    bool WaitForIdle(keel::u32 timeoutMs = 5000) {
        const keel::u64 deadlineMs = keel::monotonic_ms() + static_cast<keel::u64>(timeoutMs);
        for (;;) {
            bool anyActive = false;
            {
                std::scoped_lock lk(m_windowMutex, m_tileMutex, m_basemapMutex);
                anyActive = !m_pendingWindowJobs.empty() || m_inFlightWindowCount > 0 ||
                            !m_pendingTileJobs.empty() || m_inFlightTileCount > 0 ||
                            !m_pendingBasemapJobs.empty() || m_inFlightBasemapCount > 0;
            }
            if (!anyActive) return true;
            if (keel::monotonic_ms() >= deadlineMs) {
                return false;
            }
            std::this_thread::yield();
        }
    }

private:
    void WindowWorkerLoop() {
        for (;;) {
            AsyncWindowJob job{};
            {
                std::unique_lock lk(m_windowMutex);
                m_windowCv.wait(lk, [this]() {
                    return m_shutdown.load(std::memory_order_acquire) || !m_pendingWindowJobs.empty();
                });
                if (m_shutdown.load(std::memory_order_acquire)) {
                    break;
                }
                auto it = m_pendingWindowJobs.begin();
                const keel::u64 kUid = (*it).key;
                job = keel::move((*it).value);
                m_pendingWindowJobs.remove(kUid);
                m_inFlightWindowCount = 1;
                m_inFlightWindowLayerUid = job.layerUid;
                m_inFlightWindowGeo = job.req.viewBounds;
                m_inFlightWindowZoom = job.req.tileZoom;
                m_inFlightWindowScaleFilter = job.req.scaleFiltering;
            }

            keel::Arc<geo::SpatialWindowSnapshot> snapshot;
            if (job.pager && job.epoch == GetCurrentEpoch()) {
                snapshot = job.pager->BuildSpatialWindowSnapshot(
                    job.req,
                    job.activeGenPtr ? &job.activeGenPtr->value : nullptr
                );
            }

            {
                std::scoped_lock lk(m_windowMutex);
                m_inFlightWindowCount = 0;
                m_inFlightWindowLayerUid = 0;
                if (snapshot && snapshot->changed && job.epoch == GetCurrentEpoch()) {
                    // Replace any earlier undrained result for the same layer
                    for (keel::usize ri = m_completedWindowResults.size(); ri > 0; --ri) {
                        if (m_completedWindowResults[ri - 1].layerUid == job.layerUid) {
                            m_completedWindowResults.swap_remove(ri - 1);
                        }
                    }
                    m_completedWindowResults.push_back(AsyncWindowResult{
                        .layerUid = job.layerUid,
                        .generation = job.req.requestGeneration,
                        .epoch = job.epoch,
                        .snapshot = keel::move(snapshot)
                    });
                }
            }
        }
    }

    void TileWorkerLoop() {
        keel::UniquePtr<guikit::ICanvas> workerCanvas = CreateCanvasThreadSafe();
        guikit::VectorPath fillPath;
        guikit::VectorPath strokePath;
        fillPath.Reserve(256, 256);
        strokePath.Reserve(512, 512);
        keel::Array<guikit::Vec2> clippedRing;
        keel::Array<guikit::Vec2> scratchA;
        keel::Array<guikit::Vec2> scratchB;
        keel::u64 localTrimGen = m_trimGeneration.load(std::memory_order_acquire);

        for (;;) {
            AsyncLayerTileJob job{};
            bool shouldTrimWorker = false;
            {
                std::unique_lock lk(m_tileMutex);
                m_tileCv.wait(lk, [this, &localTrimGen]() {
                    return m_shutdown.load(std::memory_order_acquire) ||
                           !m_pendingTileJobs.empty() ||
                           m_trimGeneration.load(std::memory_order_acquire) != localTrimGen;
                });
                if (m_shutdown.load(std::memory_order_acquire)) {
                    break;
                }
                const keel::u64 curTrimGen = m_trimGeneration.load(std::memory_order_acquire);
                if (curTrimGen != localTrimGen) {
                    localTrimGen = curTrimGen;
                    if (m_pendingTileJobs.empty()) {
                        shouldTrimWorker = true;
                    }
                }
                if (!shouldTrimWorker) {
                    const keel::i32 curZ = m_activeTileZoom.load(std::memory_order_relaxed);
                    // Pick the job matching `curZ` and closest to viewport center (`distSq`)
                    keel::usize bestIdx = 0;
                    for (keel::usize i = 1; i < m_pendingTileJobs.size(); ++i) {
                        const bool bestMatchesZ = (m_pendingTileJobs[bestIdx].jobKey.z == curZ);
                        const bool curMatchesZ = (m_pendingTileJobs[i].jobKey.z == curZ);
                        if (curMatchesZ != bestMatchesZ) {
                            if (curMatchesZ) bestIdx = i;
                        } else if (m_pendingTileJobs[i].distSq < m_pendingTileJobs[bestIdx].distSq) {
                            bestIdx = i;
                        }
                    }
                    job = keel::move(m_pendingTileJobs[bestIdx]);
                    m_pendingTileJobs.swap_remove(bestIdx);
                    m_inFlightTileKeys.put(job.jobKey, InFlightTileStamp{job.styleHash, job.windowUpdateFrame});
                    ++m_inFlightTileCount;
                }
            }

            if (shouldTrimWorker) {
                if (workerCanvas) {
                    workerCanvas->TrimCaches();
                }
                fillPath = guikit::VectorPath{};
                strokePath = guikit::VectorPath{};
                clippedRing = {};
                scratchA = {};
                scratchB = {};
#if defined(__GLIBC__)
                ::malloc_trim(0);
#endif
                continue;
            }

            const keel::i32 activeZ = m_activeTileZoom.load(std::memory_order_relaxed);
            const bool obsoleteZoom = (std::abs(job.jobKey.z - activeZ) > 1);
            if (!obsoleteZoom && job.epoch == GetCurrentEpoch()) {
                AsyncLayerTileResult res{};
                res.jobKey = job.jobKey;
                res.topologyHash = job.topologyHash;
                res.styleHash = job.styleHash;
                res.windowUpdateFrame = job.windowUpdateFrame;
                res.epoch = job.epoch;
                res.queryBox = job.queryBox;

                bool aborted = false;
                for (AsyncTileCandidate& cand : job.candidates) {
                    if (job.epoch != GetCurrentEpoch()) {
                        aborted = true;
                        break;
                    }
                    if (!cand.payload && job.pager) {
                        cand.payload = job.pager->GetOrLoadGeometryPayload(
                            cand.sqliteRowId, cand.srsId, 0
                        );
                        if (cand.payload) {
                            res.newlyLoadedPayloads.push_back(AsyncLoadedFeaturePayload{
                                .layerUid = job.jobKey.layerUid,
                                .featureIndex = cand.featureIndex,
                                .featureUid = cand.featureUid,
                                .payload = cand.payload
                            });
                        }
                    }
                }

                if (!workerCanvas) {
                    workerCanvas = CreateCanvasThreadSafe();
                }
                if (!aborted && workerCanvas && job.epoch == GetCurrentEpoch()) {
                    RasterizeCandidatesToPixels(
                        *workerCanvas,
                        fillPath,
                        strokePath,
                        clippedRing,
                        scratchA,
                        scratchB,
                        job.candidates,
                        job.cell,
                        job.crs,
                        job.jobKey.tileSizePx,
                        job.queryBox,
                        job.lodSimplification,
                        job.lodPixelTolerance,
                        res.pixels,
                        res.hasAnyGeometry
                    );
                }

                if (!aborted && job.epoch == GetCurrentEpoch()) {
                    std::scoped_lock lk(m_tileMutex);
                    m_inFlightTileKeys.remove(job.jobKey);
                    if (m_inFlightTileCount > 0) --m_inFlightTileCount;
                    if (m_completedTileResults.size() >= 256u) {
                        GetTilePixelPool().Recycle(m_completedTileResults[0].pixels);
                        m_completedTileResults.swap_remove(0u);
                    }
                    m_completedTileResults.push_back(keel::move(res));
                    continue;
                }
                GetTilePixelPool().Recycle(res.pixels);
            }

            {
                std::scoped_lock lk(m_tileMutex);
                m_inFlightTileKeys.remove(job.jobKey);
                if (m_inFlightTileCount > 0) --m_inFlightTileCount;
            }
        }

        DestroyCanvasThreadSafe(workerCanvas);
    }

    void BasemapWorkerLoop() {
        for (;;) {
            AsyncBasemapJob job{};
            {
                std::unique_lock lk(m_basemapMutex);
                m_basemapCv.wait(lk, [this]() {
                    return m_shutdown.load(std::memory_order_acquire) || !m_pendingBasemapJobs.empty();
                });
                if (m_shutdown.load(std::memory_order_acquire)) {
                    break;
                }

                const keel::i32 curZ = m_activeTileZoom.load(std::memory_order_relaxed);
                keel::usize bestIdx = 0;
                for (keel::usize i = 1; i < m_pendingBasemapJobs.size(); ++i) {
                    const bool bestMatchesZ = (m_pendingBasemapJobs[bestIdx].tileKey.z == curZ);
                    const bool curMatchesZ = (m_pendingBasemapJobs[i].tileKey.z == curZ);
                    if (curMatchesZ != bestMatchesZ) {
                        if (curMatchesZ) bestIdx = i;
                    } else if (m_pendingBasemapJobs[i].distSq < m_pendingBasemapJobs[bestIdx].distSq) {
                        bestIdx = i;
                    }
                }
                job = keel::move(m_pendingBasemapJobs[bestIdx]);
                m_pendingBasemapJobs.swap_remove(bestIdx);
                m_inFlightBasemapKeys.insert(job.tileKey);
                ++m_inFlightBasemapCount;
            }

            keel::Arc<const geo::RasterTileImage> img;
            const keel::i32 activeZ = m_activeTileZoom.load(std::memory_order_relaxed);
            if (job.provider && job.epoch == GetCurrentEpoch() && std::abs(job.tileKey.z - activeZ) <= 2) {
                img = job.provider->FetchTileBlocking(job.tileKey);
            }

            {
                std::scoped_lock lk(m_basemapMutex);
                m_inFlightBasemapKeys.remove(job.tileKey);
                if (m_inFlightBasemapCount > 0) --m_inFlightBasemapCount;
                if (img && img->IsValid() && job.epoch == GetCurrentEpoch()) {
                    m_completedBasemapResults.push_back(AsyncBasemapResult{
                        .sourceId = keel::move(job.sourceId),
                        .tileKey = job.tileKey,
                        .epoch = job.epoch,
                        .image = keel::move(img)
                    });
                }
            }
        }
    }

    std::atomic<bool> m_shutdown{false};
    std::atomic<keel::u64> m_epoch{1};
    std::atomic<keel::u64> m_trimGeneration{0};
    std::atomic<keel::i32> m_activeTileZoom{12};

    mutable std::mutex m_windowMutex;
    std::condition_variable m_windowCv;
    keel::HashMap<keel::u64, keel::Arc<WindowGenCounter>> m_layerWindowGenerations;
    keel::HashMap<keel::u64, AsyncWindowJob> m_pendingWindowJobs;
    keel::Array<AsyncWindowResult> m_completedWindowResults;
    keel::usize m_inFlightWindowCount{0};
    keel::u64 m_inFlightWindowLayerUid{0};
    guikit::GeoBounds m_inFlightWindowGeo{};
    float m_inFlightWindowZoom{-1.0f};
    bool m_inFlightWindowScaleFilter{true};
    std::thread m_windowWorker;

    mutable std::mutex m_tileMutex;
    std::condition_variable m_tileCv;
    keel::Array<AsyncLayerTileJob> m_pendingTileJobs;
    keel::HashMap<AsyncLayerTileJobKey, InFlightTileStamp, AsyncLayerTileJobKeyHasher> m_inFlightTileKeys;
    keel::Array<AsyncLayerTileResult> m_completedTileResults;
    keel::usize m_inFlightTileCount{0};
    keel::Array<std::thread> m_tileWorkers;

    mutable std::mutex m_basemapMutex;
    std::condition_variable m_basemapCv;
    keel::Array<AsyncBasemapJob> m_pendingBasemapJobs;
    keel::HashSet<geo::SlippyTileKey, geo::SlippyTileKeyHash> m_inFlightBasemapKeys;
    keel::Array<AsyncBasemapResult> m_completedBasemapResults;
    keel::usize m_inFlightBasemapCount{0};
    keel::Array<std::thread> m_basemapWorkers;
};

AsyncChartStreamer& GetAsyncStreamer() {
    static AsyncChartStreamer s_streamer;
    return s_streamer;
}

[[nodiscard]] keel::Arc<const geo::DecodedGeometryPayload> GetOrCacheInMemoryFeaturePayload(
    const geo::GeoFeature& feat
) {
    if (feat.sharedGeometry) {
        return feat.sharedGeometry;
    }
    if (!feat.geometryLoaded) {
        return {};
    }
    feat.sharedGeometry = feat.BuildDecodedGeometryPayload();
    return feat.sharedGeometry;
}

class PerLayerTileCacheManager {
public:
    struct TileCandidate {
        const geo::GeoFeature* feat{nullptr};
        keel::u32 featIdx{0};
        keel::i32 painterRank{0};
        float sortDepth{0.0f};
        float maxDimPx{256.0f};
    };

    struct LayerTileStore {
        keel::u64 activeTopologyHash{0};
        keel::u64 lastWindowUpdateFrame{1};
        keel::HashMap<TileCoordKey, CachedVectorTile, TileCoordKeyHasher> tiles{nullptr};
    };

    struct CompositeLayerRef {
        LayerTileStore* store{nullptr};
        keel::u8 opacityByte{255};
    };

    PerLayerTileCacheManager()
        : m_tileCanvas(CreateCanvasThreadSafe()) {
        m_fillPath.Reserve(256, 256);
        m_strokePath.Reserve(512, 512);
    }

    ~PerLayerTileCacheManager() {
        DestroyCanvasThreadSafe(m_tileCanvas);
    }

    void Clear() {
        for (auto [uid, store] : m_layerStores) {
            ClearStoreTiles(store);
        }
        m_layerStores.clear();
        m_pixelLayerTileCount = 0u;
        for (auto [k, comp] : m_compositeTiles) {
            GetTilePixelPool().Recycle(comp.pixels);
        }
        m_compositeTiles.clear();
        m_basemapTiles.clear();
        GetTilePixelPool().Recycle(m_upscaledBasemapScratch);
    }

    void ReleaseMemory() noexcept {
        Clear();
        m_layerStores = {};
        m_compositeTiles = {};
        m_basemapTiles = {};
        m_upscaledBasemapScratch = {};
        m_queryIndices = {};
        m_clippedRing = {};
        m_scratchA = {};
        m_scratchB = {};
        m_candidates = {};
        m_syncAsyncCandidates = {};
        m_fillPath = guikit::VectorPath{};
        m_strokePath = guikit::VectorPath{};
        if (m_tileCanvas) {
            m_tileCanvas->TrimCaches();
        }
    }

    void SyncConfig(const GisRenderConfig& cfg, guikit::MapCrs crs, keel::StringRef basemapSourceId) {
        const keel::u32 newTileSize = ResolveTileSizePx(cfg.tileSizePx);
        if (newTileSize != m_tileSizePx || crs != m_activeCrs) {
            Clear();
            GetTilePixelPool().Clear(static_cast<keel::usize>(newTileSize) * static_cast<keel::usize>(newTileSize));
            m_tileSizePx = newTileSize;
            m_activeCrs = crs;
        }
        if (m_activeBasemapSourceId != basemapSourceId) {
            m_activeBasemapSourceId.assign(basemapSourceId);
            m_basemapTiles.clear();
            ++m_basemapGeneration;
            for (auto [k, comp] : m_compositeTiles) {
                comp.isComplete = false;
                comp.compositeSignature = 0ULL;
            }
        }
        const keel::usize tileAreaScale = (m_tileSizePx > 256u)
            ? static_cast<keel::usize>((m_tileSizePx / 256u) * (m_tileSizePx / 256u))
            : 1u;
        const keel::usize rawMaxLayer = keel::clamp<keel::usize>(cfg.maxLayerTiles, 16u, 2048u);
        const keel::usize rawMaxComp = keel::clamp<keel::usize>(cfg.maxCompositeTiles, 16u, 2048u);
        m_maxLayerTiles = keel::max<keel::usize>(16u, rawMaxLayer / tileAreaScale);
        m_maxCompositeTiles = keel::max<keel::usize>(16u, rawMaxComp / tileAreaScale);
        if (m_compositeTiles.size() > m_maxCompositeTiles) {
            EvictOldestCompositeTiles(m_maxCompositeTiles);
        }
        if (m_pixelLayerTileCount > m_maxLayerTiles) {
            EnforceGlobalLayerPixelBudget(~keel::u64{0});
        }
    }

    [[nodiscard]] keel::u32 GetTileSizePx() const noexcept { return m_tileSizePx; }
    [[nodiscard]] float GetTileSizeF() const noexcept { return static_cast<float>(m_tileSizePx); }
    [[nodiscard]] guikit::MapCrs GetActiveCrs() const noexcept { return m_activeCrs; }

    void GarbageCollectLayers(keel::ArrayRef<geo::GeoLayer> layers) {
        keel::Array<keel::u64> staleUids;
        for (auto [uid, store] : m_layerStores) {
            bool found = false;
            for (const geo::GeoLayer& lyr : layers) {
                if (lyr.uid == uid) {
                    found = true;
                    break;
                }
            }
            if (!found) staleUids.push_back(uid);
        }
        for (keel::u64 uid : staleUids) {
            if (LayerTileStore* st = m_layerStores.get(uid)) {
                ClearStoreTiles(*st);
            }
            m_layerStores.remove(uid);
        }
    }

    [[nodiscard]] LayerTileStore& SyncLayerStore(
        const geo::GeoLayer& layer,
        guikit::MapCrs crs
    ) {
        keel::u64 topoHash = keel::kFnv1aOffsetBasis;
        HashCombineVal(topoHash, crs);
        HashCombineVal(topoHash, layer.uid);
        const keel::usize fCount = layer.isWindowStreamed
            ? keel::max(layer.estimatedTotalFeatures, static_cast<keel::usize>(1u))
            : layer.features.size();
        HashCombineVal(topoHash, fCount);

        LayerTileStore& store = m_layerStores.get_or_insert(layer.uid);
        if (store.activeTopologyHash != topoHash) {
            ClearStoreTiles(store);
            store.activeTopologyHash = topoHash;
            ++store.lastWindowUpdateFrame;
            for (auto [tileKey, comp] : m_compositeTiles) {
                comp.isComplete = false;
                comp.compositeSignature = 0ULL;
            }
        }
        return store;
    }

    keel::usize InvalidateTilesForDirtiedBoxes(
        keel::u64 layerUid,
        keel::ArrayRef<guikit::Rect> dirtiedNormBoxes,
        keel::u64 frameTick = 0
    ) noexcept {
        LayerTileStore* storePtr = m_layerStores.get(layerUid);
        if (storePtr && frameTick > 0) {
            storePtr->lastWindowUpdateFrame = frameTick;
        }
        keel::usize invalidatedCount = 0;
        if (storePtr) {
            LayerTileStore& store = *storePtr;
            keel::Array<TileCoordKey> toErase;
            for (auto [tileKey, tile] : store.tiles) {
                bool shouldInvalidate = !tile.hasAnyGeometry;
                if (!shouldInvalidate) {
                    for (const guikit::Rect& dirtyBox : dirtiedNormBoxes) {
                        if (tile.normQueryBox.Intersects(dirtyBox)) {
                            shouldInvalidate = true;
                            break;
                        }
                    }
                }
                if (shouldInvalidate) {
                    if (CompositeTile* comp = m_compositeTiles.get(tileKey)) {
                        comp->isComplete = false;
                        comp->compositeSignature = 0ULL;
                    }
                    toErase.push_back(tileKey);
                }
            }
            for (const TileCoordKey& tk : toErase) {
                if (CachedVectorTile* t = store.tiles.get(tk)) {
                    ReleaseTilePixels(*t);
                }
                store.tiles.remove(tk);
                ++invalidatedCount;
            }
        }
        for (auto [tileKey, comp] : m_compositeTiles) {
            if (!comp.isComplete) continue;
            if (dirtiedNormBoxes.empty() && frameTick > 0) {
                comp.isComplete = false;
                comp.compositeSignature = 0ULL;
                ++invalidatedCount;
                continue;
            }
            for (const guikit::Rect& dirtyBox : dirtiedNormBoxes) {
                if (comp.normQueryBox.w <= 0.0f || comp.normQueryBox.Intersects(dirtyBox)) {
                    comp.isComplete = false;
                    comp.compositeSignature = 0ULL;
                    ++invalidatedCount;
                    break;
                }
            }
        }
        return invalidatedCount;
    }

    [[nodiscard]] static const CachedVectorTile* FindTile(
        LayerTileStore& store,
        const TileCoordKey& key,
        keel::u64 frameTick
    ) noexcept {
        CachedVectorTile* tile = store.tiles.get(key);
        if (!tile) return nullptr;
        tile->lastUsedFrame = frameTick;
        return tile;
    }

    void MarkTileEmpty(
        LayerTileStore& store,
        const TileCoordKey& key,
        const guikit::Rect& normQueryBox,
        keel::u64 styleHash,
        keel::u64 frameTick
    ) {
        if (store.tiles.size() >= kMaxMetadataTilesPerLayer && !store.tiles.contains(key)) {
            EvictOldestLayerTiles(store, kMaxMetadataTilesPerLayer - 24u, frameTick);
        }
        CachedVectorTile& tile = store.tiles.get_or_insert(key);
        ReleaseTilePixels(tile);
        tile.normQueryBox = normQueryBox;
        tile.styleHash = styleHash;
        tile.lastUsedFrame = frameTick;
        tile.lastRasterizedFrame = frameTick;
        tile.hasAnyGeometry = false;
    }

    bool StoreAsyncLayerTile(AsyncLayerTileResult&& res, keel::u64 frameTick) {
        if (res.jobKey.tileSizePx != m_tileSizePx) {
            GetTilePixelPool().Recycle(res.pixels);
            return false;
        }
        LayerTileStore* storePtr = m_layerStores.get(res.jobKey.layerUid);
        if (!storePtr) {
            GetTilePixelPool().Recycle(res.pixels);
            return false;
        }
        LayerTileStore& store = *storePtr;
        if (store.activeTopologyHash != res.topologyHash ||
            res.windowUpdateFrame < store.lastWindowUpdateFrame) {
            GetTilePixelPool().Recycle(res.pixels);
            return false;
        }

        const TileCoordKey key{res.jobKey.z, res.jobKey.x, res.jobKey.y};
        EnforceGlobalLayerPixelBudget(frameTick);
        if (store.tiles.size() >= kMaxMetadataTilesPerLayer && !store.tiles.contains(key)) {
            EvictOldestLayerTiles(store, kMaxMetadataTilesPerLayer - 24u, frameTick);
        }

        CachedVectorTile& tile = store.tiles.get_or_insert(key);
        ReleaseTilePixels(tile);
        if (res.hasAnyGeometry && !res.pixels.empty()) {
            tile.pixels = keel::move(res.pixels);
            if (tile.pixels.capacity() > 0u) {
                ++m_pixelLayerTileCount;
            }
        } else {
            GetTilePixelPool().Recycle(res.pixels);
        }
        tile.normQueryBox = res.queryBox;
        tile.styleHash = res.styleHash;
        tile.lastUsedFrame = frameTick;
        tile.lastRasterizedFrame = frameTick;
        tile.hasAnyGeometry = res.hasAnyGeometry && !tile.pixels.empty();

        if (CompositeTile* comp = m_compositeTiles.get(key)) {
            comp->isComplete = false;
            comp->compositeSignature = 0ULL;
        }
        return true;
    }

    [[nodiscard]] bool HasBasemapTile(const TileCoordKey& key) const noexcept {
        return m_basemapTiles.contains(key);
    }

    [[nodiscard]] keel::u64 GetBasemapGeneration() const noexcept {
        return m_basemapGeneration;
    }

    bool StoreBasemapTile(
        keel::StringRef sourceId,
        const geo::SlippyTileKey& tileKey,
        keel::Arc<const geo::RasterTileImage> img
    ) {
        if (!img || !img->IsValid() || tileKey.tileSizePx != m_tileSizePx || sourceId != m_activeBasemapSourceId) {
            return false;
        }
        const keel::usize maxBasemapTiles = keel::min<keel::usize>(m_maxCompositeTiles, 32u);
        while (m_basemapTiles.size() >= maxBasemapTiles && !m_basemapTiles.empty()) {
            const TileCoordKey victimKey = (*m_basemapTiles.begin()).key;
            m_basemapTiles.remove(victimKey);
        }
        const TileCoordKey key{tileKey.z, tileKey.x, tileKey.y};
        m_basemapTiles.put(key, keel::move(img));
        ++m_basemapGeneration;

        if (CompositeTile* comp = m_compositeTiles.get(key)) {
            comp->isComplete = false;
            comp->compositeSignature = 0ULL;
        }
        return true;
    }

    [[nodiscard]] CompositeTile* FindCompositeTile(
        const TileCoordKey& key,
        keel::u64 frameTick
    ) noexcept {
        CompositeTile* comp = m_compositeTiles.get(key);
        if (!comp) return nullptr;
        comp->lastUsedFrame = frameTick;
        return comp;
    }

    CompositeTile& BuildCompositeTile(
        const TileCoordKey& key,
        const guikit::Rect& normQueryBox,
        keel::u64 compositeSignature,
        keel::u32 bgPx,
        keel::ArrayRef<CompositeLayerRef> layerRefs,
        keel::u64 frameTick,
        bool useBasemapOverlay = false,
        keel::u8 basemapOpacityByte = 255
    ) {
        if (m_compositeTiles.size() >= m_maxCompositeTiles && !m_compositeTiles.contains(key)) {
            EvictOldestCompositeTiles(m_maxCompositeTiles - keel::min<keel::usize>(24u, m_maxCompositeTiles / 4u));
        }
        const keel::usize numTilePx = static_cast<keel::usize>(m_tileSizePx) * static_cast<keel::usize>(m_tileSizePx);
        CompositeTile& comp = m_compositeTiles.get_or_insert(key);
        GetTilePixelPool().AcquireFilled(comp.pixels, numTilePx, bgPx);
        comp.normQueryBox = normQueryBox;
        comp.compositeSignature = compositeSignature;
        comp.lastUsedFrame = frameTick;
        comp.lastUpdatedFrame = frameTick;
        comp.isComplete = true;

        if (useBasemapOverlay && basemapOpacityByte > 0) {
            const bool exactBasemap = BlendBasemapTileOrAncestor(comp.pixels.data(), key, basemapOpacityByte);
            if (!exactBasemap) {
                comp.isComplete = false;
            }
        }

        for (const CompositeLayerRef& ref : layerRefs) {
            if (!ref.store || ref.opacityByte == 0) continue;
            const CachedVectorTile* lt = FindTile(*ref.store, key, frameTick);
            if (lt && lt->hasAnyGeometry && lt->pixels.size() == numTilePx) {
                BlendPremulTileOntoOpaqueComposite(comp.pixels.data(), lt->pixels.data(), ref.opacityByte, m_tileSizePx);
            }
        }
        return comp;
    }

    // Seeds an uncached [z/x/y] composite tile from parent [z-1]/[z-2] or child [z+1] tiles
    // so zooming in or out immediately displays a full-stack chart preview with zero blanking.
    bool SeedFallbackFromHierarchy(
        const TileCoordKey& key,
        keel::u32 bgPx,
        keel::u64 frameTick
    ) {
        const keel::usize numTilePx = static_cast<keel::usize>(m_tileSizePx) * static_cast<keel::usize>(m_tileSizePx);
        if (CompositeTile* existing = m_compositeTiles.get(key); existing && !existing->pixels.empty()) {
            existing->lastUsedFrame = frameTick;
            return true;
        }

        // 1. Check Parent [z-1] or Grandparent [z-2] and pin its LRU frameTick BEFORE any cache eviction
        keel::i32 ancestorShift = 0;
        TileCoordKey foundParentKey{};
        for (keel::i32 shift = 1; shift <= 2; ++shift) {
            if (key.z - shift < 0) break;
            const TileCoordKey parentKey{key.z - shift, key.x >> shift, key.y >> shift};
            if (CompositeTile* pTile = m_compositeTiles.get(parentKey); pTile && pTile->pixels.size() == numTilePx) {
                pTile->lastUsedFrame = frameTick;
                ancestorShift = shift;
                foundParentKey = parentKey;
                break;
            }
        }

        // 2. Check Children [z+1, 2x+dx, 2y+dy] and pin their LRU frameTick BEFORE any cache eviction
        bool hasChild[2][2]{{false, false}, {false, false}};
        keel::i32 foundChildren = 0;
        if (key.z < 22) {
            for (keel::i32 dy = 0; dy < 2; ++dy) {
                for (keel::i32 dx = 0; dx < 2; ++dx) {
                    const TileCoordKey childKey{key.z + 1, (key.x << 1) + dx, (key.y << 1) + dy};
                    if (CompositeTile* cTile = m_compositeTiles.get(childKey); cTile && cTile->pixels.size() == numTilePx) {
                        cTile->lastUsedFrame = frameTick;
                        hasChild[dy][dx] = true;
                        ++foundChildren;
                    }
                }
            }
        }

        if (ancestorShift == 0 && foundChildren == 0) {
            return false;
        }

        // Perform eviction and map insertion BEFORE taking any pointers into m_compositeTiles!
        if (m_compositeTiles.size() >= m_maxCompositeTiles && !m_compositeTiles.contains(key)) {
            EvictOldestCompositeTiles(m_maxCompositeTiles - keel::min<keel::usize>(24u, m_maxCompositeTiles / 4u));
        }

        CompositeTile& comp = m_compositeTiles.get_or_insert(key);
        GetTilePixelPool().AcquireFilled(comp.pixels, numTilePx, bgPx);
        comp.lastUsedFrame = frameTick;
        comp.isComplete = false;

        const CompositeTile* ancestorTile = nullptr;
        if (ancestorShift > 0) {
            if (const CompositeTile* pTile = m_compositeTiles.get(foundParentKey);
                pTile && pTile->pixels.size() == numTilePx) {
                ancestorTile = pTile;
            }
        }

        if (ancestorTile) {
            const keel::u32 subSpan = m_tileSizePx >> ancestorShift;
            const keel::u32 qx = static_cast<keel::u32>(key.x & ((1 << ancestorShift) - 1)) * subSpan;
            const keel::u32 qy = static_cast<keel::u32>(key.y & ((1 << ancestorShift) - 1)) * subSpan;
            for (keel::u32 y = 0; y < m_tileSizePx; ++y) {
                const keel::u32 sy = keel::min(m_tileSizePx - 1u, qy + (y >> ancestorShift));
                const keel::u32* srcRow = ancestorTile->pixels.data() + static_cast<keel::usize>(sy) * m_tileSizePx;
                keel::u32* dstRow = comp.pixels.data() + static_cast<keel::usize>(y) * m_tileSizePx;
                for (keel::u32 x = 0; x < m_tileSizePx; ++x) {
                    const keel::u32 sx = keel::min(m_tileSizePx - 1u, qx + (x >> ancestorShift));
                    dstRow[x] = srcRow[sx];
                }
            }
        }

        if (foundChildren > 0 && (foundChildren == 4 || ancestorTile != nullptr)) {
            const keel::u32 kHalf = m_tileSizePx >> 1;
            for (keel::i32 dy = 0; dy < 2; ++dy) {
                for (keel::i32 dx = 0; dx < 2; ++dx) {
                    if (!hasChild[dy][dx]) continue;
                    const TileCoordKey childKey{key.z + 1, (key.x << 1) + dx, (key.y << 1) + dy};
                    const CompositeTile* child = m_compositeTiles.get(childKey);
                    if (!child || child->pixels.size() != numTilePx) {
                        continue;
                    }
                    const keel::u32 dstBaseX = static_cast<keel::u32>(dx) * kHalf;
                    const keel::u32 dstBaseY = static_cast<keel::u32>(dy) * kHalf;
                    for (keel::u32 y = 0; y < kHalf; ++y) {
                        const keel::u32* srcRow = child->pixels.data() + static_cast<keel::usize>(y << 1) * m_tileSizePx;
                        keel::u32* dstRow = comp.pixels.data() + static_cast<keel::usize>(dstBaseY + y) * m_tileSizePx + dstBaseX;
                        for (keel::u32 x = 0; x < kHalf; ++x) {
                            dstRow[x] = srcRow[x << 1];
                        }
                    }
                }
            }
        }

        return true;
    }

    const CachedVectorTile* RasterizeOrDispatchLayerTile(
        LayerTileStore& store,
        const TileCoordKey& key,
        keel::u64 styleHash,
        const guikit::OsmTileCell& cell,
        guikit::MapCrs crs,
        const geo::GeoLayer& layer,
        const GisRenderConfig& cfg,
        keel::u64 frameTick,
        float distSqToCenter,
        bool allowAsyncDispatch,
        bool canRunSyncFastPath,
        keel::usize* outPagedInCount = nullptr,
        bool hasDeadline = false,
        keel::u64 deadlineUs = 0,
        keel::usize minPagedBeforeCheck = 96,
        bool* outAbortedOrQueued = nullptr
    ) {
        const float tileSizeF = static_cast<float>(m_tileSizePx);

        const CellNorm3857Bounds cb = ComputeCellNorm3857Bounds(cell, crs);
        const float u0 = cb.u0;
        const float u1 = cb.u1;
        const float v0 = cb.v0;
        const float v1 = cb.v1;
        const float spanU = cb.spanU;
        const float spanV = cb.spanV;
        const float invSpanU = tileSizeF / spanU;
        const float invSpanV = tileSizeF / spanV;

        const float marginU = spanU * (kTileGuardBandPx / tileSizeF);
        const float marginV = spanV * (kTileGuardBandPx / tileSizeF);
        const guikit::Rect queryBox(
            u0 - marginU,
            v0 - marginV,
            (u1 - u0) + 2.0f * marginU,
            (v1 - v0) + 2.0f * marginV
        );

        if (!layer.normBounds.Intersects(queryBox)) {
            MarkTileEmpty(store, key, queryBox, styleHash, frameTick);
            return FindTile(store, key, frameTick);
        }

        const bool layerWindowPending =
            allowAsyncDispatch && layer.isWindowStreamed && GetAsyncStreamer().IsLayerWindowPending(layer.uid);
        if (layer.features.empty()) {
            if (layerWindowPending) {
                if (outAbortedOrQueued) *outAbortedOrQueued = true;
                return nullptr;
            }
            MarkTileEmpty(store, key, queryBox, styleHash, frameTick);
            return FindTile(store, key, frameTick);
        }

        const AsyncLayerTileJobKey asyncKey{
            .layerUid = layer.uid,
            .z = key.z,
            .x = key.x,
            .y = key.y,
            .tileSizePx = m_tileSizePx
        };
        if (allowAsyncDispatch &&
            GetAsyncStreamer().IsLayerTilePending(asyncKey, styleHash, store.lastWindowUpdateFrame)) {
            if (outAbortedOrQueued) *outAbortedOrQueued = true;
            return nullptr;
        }

        m_candidates.clear();
        m_queryIndices.clear();
        layer.spatialIndex.query_box(
            keel::RTreeBox<float>::from_xywh(queryBox.x, queryBox.y, queryBox.w, queryBox.h),
            [&](keel::u32 fIdx) { m_queryIndices.push_back(fIdx); }
        );
        for (keel::u32 fIdx : m_queryIndices) {
            if (fIdx >= layer.features.size()) continue;
            const geo::GeoFeature& feat = layer.features[fIdx];

            if (!feat.HasPolygons() && !feat.HasLines()) continue;
            if (ShouldSkipFeatureAtZoom(feat, cell.id.z, cfg)) continue;

            const float featWpx = feat.normBounds.w * invSpanU;
            const float featHpx = feat.normBounds.h * invSpanV;
            const float maxDimPx = keel::max(featWpx, featHpx);

            const float minFeaturePx = (feat.chartClass == geo::ChartFeatureClass::DepthArea ||
                                        feat.chartClass == geo::ChartFeatureClass::LandArea)
                ? 0.20f
                : ((cfg.lodSimplification || cfg.scaleFiltering) ? 1.35f : 0.65f);
            if (maxDimPx < minFeaturePx) {
                continue;
            }

            if (cfg.lodSimplification || cfg.scaleFiltering) {
                if (cfg.scaleFiltering && cell.id.z <= 11) {
                    if (feat.chartClass == geo::ChartFeatureClass::DepthContour) {
                        const float minCntPx = (cell.id.z <= 10) ? 4.5f : 2.8f;
                        if (maxDimPx < minCntPx) continue;
                    } else if (feat.chartClass == geo::ChartFeatureClass::BuiltUpOrStructure && maxDimPx < 2.8f) {
                        continue;
                    }
                }
            }

            const keel::i32 rank = CartographicPainterRank(feat);
            float depthKey = 0.0f;
            if (feat.chartClass == geo::ChartFeatureClass::DepthArea) {
                const float d = feat.minDepthMeters;
                depthKey = std::isnan(d) ? -100.0f : -d;
            }

            m_candidates.push_back({
                .feat = &feat,
                .featIdx = fIdx,
                .painterRank = rank,
                .sortDepth = depthKey,
                .maxDimPx = maxDimPx
            });
        }

        if (m_candidates.empty()) {
            if (layerWindowPending) {
                if (outAbortedOrQueued) *outAbortedOrQueued = true;
                return nullptr;
            }
            MarkTileEmpty(store, key, queryBox, styleHash, frameTick);
            return FindTile(store, key, frameTick);
        }

        keel::sort(m_candidates,
            [](const TileCandidate& a, const TileCandidate& b) noexcept {
                if (a.painterRank != b.painterRank) return a.painterRank < b.painterRank;
                if (a.sortDepth != b.sortDepth) return a.sortDepth < b.sortDepth;
                return a.featIdx < b.featIdx;
            });

        keel::usize missingDiskCount = 0;
        keel::usize totalCandidateVertices = 0;
        for (const TileCandidate& cand : m_candidates) {
            const geo::GeoFeature& feat = *cand.feat;
            totalCandidateVertices += feat.VertexCount();
            if (!feat.geometryLoaded) {
                if (!feat.TryEnsureGeometryLoadedNonBlocking(frameTick)) {
                    ++missingDiskCount;
                }
            } else {
                feat.TouchAccess(frameTick);
            }
        }

        const bool isLightweightInRamTile =
            (missingDiskCount == 0) &&
            (m_candidates.size() <= 48u) &&
            (totalCandidateVertices <= 3200u);

        (void)canRunSyncFastPath;
        if (allowAsyncDispatch && !isLightweightInRamTile) {
            AsyncLayerTileJob job{};
            job.jobKey = asyncKey;
            job.topologyHash = store.activeTopologyHash;
            job.styleHash = styleHash;
            job.windowUpdateFrame = store.lastWindowUpdateFrame;
            job.cell = cell;
            job.crs = crs;
            job.queryBox = queryBox;
            job.lodSimplification = cfg.lodSimplification;
            job.lodPixelTolerance = cfg.lodPixelTolerance;
            job.distSq = distSqToCenter;
            job.pager = layer.pager;
            job.candidates.reserve(m_candidates.size());

            for (const TileCandidate& cand : m_candidates) {
                const geo::GeoFeature& feat = *cand.feat;
                job.candidates.push_back(AsyncTileCandidate{
                    .featureIndex = cand.featIdx,
                    .featureUid = feat.uid,
                    .sqliteRowId = feat.sqliteRowId,
                    .srsId = feat.srsId,
                    .chartClass = feat.chartClass,
                    .maxDimPx = cand.maxDimPx,
                    .style = ResolveMarineFeatureStyle(layer, feat, cfg, /*bakeFillOpacity=*/false),
                    .payload = GetOrCacheInMemoryFeaturePayload(feat)
                });
            }

            GetAsyncStreamer().EnqueueLayerTileJob(keel::move(job));
            if (outAbortedOrQueued) *outAbortedOrQueued = true;
            return nullptr;
        }

        // Synchronous path (used for lightweight already-in-RAM tiles or when asyncBackgroundStreaming is disabled)
        keel::usize newlyPagedInTile = 0;
        m_syncAsyncCandidates.clear();
        m_syncAsyncCandidates.reserve(m_candidates.size());
        for (const TileCandidate& cand : m_candidates) {
            const geo::GeoFeature& feat = *cand.feat;
            if (!feat.geometryLoaded) {
                if (hasDeadline && newlyPagedInTile >= minPagedBeforeCheck &&
                    (newlyPagedInTile & 7u) == 0u) {
                    if (keel::monotonic_us() >= deadlineUs) {
                        if (outAbortedOrQueued) *outAbortedOrQueued = true;
                        return nullptr;
                    }
                }
                if (feat.EnsureGeometryLoaded(frameTick)) {
                    ++newlyPagedInTile;
                    if (outPagedInCount) {
                        ++(*outPagedInCount);
                    }
                }
            }
            if (feat.geometryLoaded) {
                m_syncAsyncCandidates.push_back(AsyncTileCandidate{
                    .featureIndex = cand.featIdx,
                    .featureUid = feat.uid,
                    .sqliteRowId = feat.sqliteRowId,
                    .srsId = feat.srsId,
                    .chartClass = feat.chartClass,
                    .maxDimPx = cand.maxDimPx,
                    .style = ResolveMarineFeatureStyle(layer, feat, cfg, /*bakeFillOpacity=*/false),
                    .payload = GetOrCacheInMemoryFeaturePayload(feat)
                });
            }
        }

        if (!m_tileCanvas) {
            m_tileCanvas = CreateCanvasThreadSafe();
        }

        EnforceGlobalLayerPixelBudget(frameTick);
        if (store.tiles.size() >= kMaxMetadataTilesPerLayer && !store.tiles.contains(key)) {
            EvictOldestLayerTiles(store, kMaxMetadataTilesPerLayer - 64u, frameTick);
        }

        CachedVectorTile& tile = store.tiles.get_or_insert(key);
        ReleaseTilePixels(tile);
        tile.normQueryBox = queryBox;
        tile.styleHash = styleHash;
        tile.lastUsedFrame = frameTick;
        tile.lastRasterizedFrame = frameTick;
        tile.hasAnyGeometry = false;

        RasterizeCandidatesToPixels(
            *m_tileCanvas,
            m_fillPath,
            m_strokePath,
            m_clippedRing,
            m_scratchA,
            m_scratchB,
            m_syncAsyncCandidates,
            cell,
            crs,
            m_tileSizePx,
            queryBox,
            cfg.lodSimplification,
            cfg.lodPixelTolerance,
            tile.pixels,
            tile.hasAnyGeometry
        );

        if (tile.hasAnyGeometry && !tile.pixels.empty()) {
            if (tile.pixels.capacity() > 0u) {
                ++m_pixelLayerTileCount;
            }
        } else {
            GetTilePixelPool().Recycle(tile.pixels);
            tile.hasAnyGeometry = false;
        }

        return &tile;
    }

    void EnforceGlobalLayerPixelBudget(keel::u64 protectedFrameTick) noexcept {
        if (m_pixelLayerTileCount <= m_maxLayerTiles) {
            return;
        }
        const keel::usize targetCount = m_maxLayerTiles - keel::min<keel::usize>(16u, m_maxLayerTiles / 8u);

        // Pass 1: Prefer evicting Tier-1 pixel buffers whose [z/x/y] already has a complete CompositeTile
        while (m_pixelLayerTileCount > targetCount) {
            LayerTileStore* victimStore = nullptr;
            TileCoordKey victimKey{};
            keel::u64 oldestFrame = ~keel::u64{0};
            for (auto [uid, store] : m_layerStores) {
                for (auto [k, v] : store.tiles) {
                    if (v.pixels.capacity() == 0u || v.lastUsedFrame >= protectedFrameTick) {
                        continue;
                    }
                    const CompositeTile* comp = m_compositeTiles.get(k);
                    if (comp && comp->isComplete && !comp->pixels.empty() && v.lastUsedFrame < oldestFrame) {
                        oldestFrame = v.lastUsedFrame;
                        victimStore = &store;
                        victimKey = k;
                    }
                }
            }
            if (!victimStore) {
                break;
            }
            if (CachedVectorTile* t = victimStore->tiles.get(victimKey)) {
                ReleaseTilePixels(*t);
            }
            victimStore->tiles.remove(victimKey);
        }

        // Pass 2: Fallback global LRU eviction across all older pixel-bearing Tier-1 tiles
        while (m_pixelLayerTileCount > targetCount) {
            LayerTileStore* victimStore = nullptr;
            TileCoordKey victimKey{};
            keel::u64 oldestFrame = ~keel::u64{0};
            for (auto [uid, store] : m_layerStores) {
                for (auto [k, v] : store.tiles) {
                    if (v.pixels.capacity() > 0u &&
                        v.lastUsedFrame < protectedFrameTick &&
                        v.lastUsedFrame < oldestFrame) {
                        oldestFrame = v.lastUsedFrame;
                        victimStore = &store;
                        victimKey = k;
                    }
                }
            }
            if (!victimStore) {
                break;
            }
            if (CachedVectorTile* t = victimStore->tiles.get(victimKey)) {
                ReleaseTilePixels(*t);
            }
            victimStore->tiles.remove(victimKey);
        }
    }

private:
    static constexpr keel::usize kMaxMetadataTilesPerLayer = 96u;

    void ReleaseTilePixels(CachedVectorTile& tile) noexcept {
        if (tile.pixels.capacity() > 0u) {
            if (m_pixelLayerTileCount > 0u) {
                --m_pixelLayerTileCount;
            }
            GetTilePixelPool().Recycle(tile.pixels);
        } else {
            tile.pixels.clear();
        }
    }

    void ClearStoreTiles(LayerTileStore& store) noexcept {
        for (auto [k, tile] : store.tiles) {
            ReleaseTilePixels(tile);
        }
        store.tiles = {};
    }
    bool BlendBasemapTileOrAncestor(
        keel::u32* dstPixels,
        const TileCoordKey& key,
        keel::u8 opacityByte
    ) const {
        const keel::usize numTilePx = static_cast<keel::usize>(m_tileSizePx) * static_cast<keel::usize>(m_tileSizePx);
        if (const auto* exactPtr = m_basemapTiles.get(key);
            exactPtr && *exactPtr && (*exactPtr)->rgbaPixels.size() == numTilePx) {
            BlendStraightRgbaBasemapOntoComposite(
                dstPixels, (*exactPtr)->rgbaPixels.data(), opacityByte, m_tileSizePx
            );
            return true;
        }

        // Check parent [z-1], [z-2], [z-3] for progressive basemap mipmap fallback while exact z downloads
        for (keel::i32 shift = 1; shift <= 3; ++shift) {
            if (key.z - shift < 0) break;
            const TileCoordKey parentKey{key.z - shift, key.x >> shift, key.y >> shift};
            if (const auto* pPtr = m_basemapTiles.get(parentKey);
                pPtr && *pPtr && (*pPtr)->rgbaPixels.size() == numTilePx) {
                const keel::u32* srcData = (*pPtr)->rgbaPixels.data();
                const keel::u32 subSpan = keel::max(1u, m_tileSizePx >> shift);
                const keel::u32 qx = static_cast<keel::u32>(key.x & ((1 << shift) - 1)) * subSpan;
                const keel::u32 qy = static_cast<keel::u32>(key.y & ((1 << shift) - 1)) * subSpan;

                m_upscaledBasemapScratch.resize(numTilePx);
                for (keel::u32 y = 0; y < m_tileSizePx; ++y) {
                    const keel::u32 sy = keel::min(m_tileSizePx - 1u, qy + (y >> shift));
                    const keel::u32* srcRow = srcData + static_cast<keel::usize>(sy) * m_tileSizePx;
                    keel::u32* dstRow = m_upscaledBasemapScratch.data() + static_cast<keel::usize>(y) * m_tileSizePx;
                    for (keel::u32 x = 0; x < m_tileSizePx; ++x) {
                        const keel::u32 sx = keel::min(m_tileSizePx - 1u, qx + (x >> shift));
                        dstRow[x] = srcRow[sx];
                    }
                }
                BlendStraightRgbaBasemapOntoComposite(
                    dstPixels, m_upscaledBasemapScratch.data(), opacityByte, m_tileSizePx
                );
                return false; // Ancestor preview used; exact z tile is still pending
            }
        }
        return false;
    }

    void EvictOldestLayerTiles(LayerTileStore& store, keel::usize targetCount, keel::u64 protectedFrameTick) noexcept {
        while (store.tiles.size() > targetCount && !store.tiles.empty()) {
            bool found = false;
            TileCoordKey oldestKey{};
            keel::u64 oldestFrame = ~keel::u64{0};
            for (auto [k, v] : store.tiles) {
                if (v.lastUsedFrame < protectedFrameTick && v.lastUsedFrame < oldestFrame) {
                    oldestFrame = v.lastUsedFrame;
                    oldestKey = k;
                    found = true;
                }
            }
            if (!found) {
                break;
            }
            if (CachedVectorTile* t = store.tiles.get(oldestKey)) {
                ReleaseTilePixels(*t);
            }
            store.tiles.remove(oldestKey);
        }
    }

    void EvictOldestCompositeTiles(keel::usize targetCount) noexcept {
        while (m_compositeTiles.size() > targetCount && !m_compositeTiles.empty()) {
            TileCoordKey oldestKey = (*m_compositeTiles.begin()).key;
            keel::u64 oldestFrame = (*m_compositeTiles.begin()).value.lastUsedFrame;
            for (auto [k, v] : m_compositeTiles) {
                if (v.lastUsedFrame < oldestFrame) {
                    oldestFrame = v.lastUsedFrame;
                    oldestKey = k;
                }
            }
            if (CompositeTile* victim = m_compositeTiles.get(oldestKey)) {
                GetTilePixelPool().Recycle(victim->pixels);
            }
            m_compositeTiles.remove(oldestKey);
        }
    }

    keel::UniquePtr<guikit::ICanvas> m_tileCanvas;
    guikit::VectorPath m_fillPath;
    guikit::VectorPath m_strokePath;
    keel::u32 m_tileSizePx{256};
    guikit::MapCrs m_activeCrs{guikit::MapCrs::EPSG3857_WebMercator};
    keel::String m_activeBasemapSourceId;
    keel::u64 m_basemapGeneration{1};
    keel::usize m_maxLayerTiles{48};
    keel::usize m_maxCompositeTiles{48};
    keel::usize m_pixelLayerTileCount{0};
    keel::HashMap<keel::u64, LayerTileStore> m_layerStores;
    keel::HashMap<TileCoordKey, CompositeTile, TileCoordKeyHasher> m_compositeTiles;
    keel::HashMap<TileCoordKey, keel::Arc<const geo::RasterTileImage>, TileCoordKeyHasher> m_basemapTiles;
    mutable keel::Array<keel::u32> m_upscaledBasemapScratch;
    keel::Array<keel::u32> m_queryIndices;
    keel::Array<guikit::Vec2> m_clippedRing;
    keel::Array<guikit::Vec2> m_scratchA;
    keel::Array<guikit::Vec2> m_scratchB;
    keel::Array<TileCandidate> m_candidates;
    keel::Array<AsyncTileCandidate> m_syncAsyncCandidates;
};

PerLayerTileCacheManager& GetTileCache() {
    static PerLayerTileCacheManager s_cache;
    return s_cache;
}

[[nodiscard]] keel::Arc<geo::IRasterTileProvider> ResolveActiveBasemapProvider(
    const GisRenderConfig& cfg
) {
    keel::Arc<geo::IRasterTileProvider> resolved;
    if (cfg.customBasemapProvider) {
        resolved = cfg.customBasemapProvider;
    } else if (cfg.showOsmBasemap) {
        if (auto defaultProv = GisRenderEngine::GetDefaultBasemapProvider()) {
            resolved = keel::move(defaultProv);
        } else {
            static keel::Arc<geo::IRasterTileProvider> s_defaultOsmProvider = geo::CreateOsmRasterTileProvider();
            resolved = s_defaultOsmProvider;
        }
    }
    if (resolved) {
        std::scoped_lock lk(GetDefaultBasemapMutex());
        GetLastActiveBasemapStorage() = resolved;
    }
    return resolved;
}

void DrawLoadingTilePlaceholder(
    guikit::SceneScope& scene,
    const guikit::OsmTileCell& cell,
    float zoom,
    float invZ,
    bool darkChartSurface,
    bool hasPreview,
    keel::u64 frameCounter
) {
    const guikit::Rect& r = cell.worldRect;
    const float cellScreenW = r.w * zoom;
    const float cellScreenH = r.h * zoom;
    const float minSidePx = keel::min(cellScreenW, cellScreenH);
    if (minSidePx < 24.0f) {
        return;
    }

    const float x0 = r.x;
    const float y0 = r.y;
    const float x1 = r.Right();
    const float y1 = r.Bottom();
    const float cx = x0 + r.w * 0.5f;
    const float cy = y0 + r.h * 0.5f;

    const bool evenChecker = ((cell.id.x + cell.id.y) & 1) == 0;
    const guikit::Color accentCol = darkChartSurface
        ? guikit::Color(56, 189, 248, hasPreview ? 175 : 225)
        : guikit::Color(2, 132, 199, hasPreview ? 170 : 220);
    const guikit::Color gridLineCol = darkChartSurface
        ? guikit::Color(56, 189, 248, hasPreview ? 28 : 48)
        : guikit::Color(14, 116, 144, hasPreview ? 24 : 42);

    // 1. If no parent/child mipmap preview exists yet, draw a structured nautical blueprint grid cell
    if (!hasPreview) {
        const guikit::Color cellTint = darkChartSurface
            ? (evenChecker ? guikit::Color(13, 28, 46, 220) : guikit::Color(18, 36, 58, 220))
            : (evenChecker ? guikit::Color(214, 230, 244, 225) : guikit::Color(204, 223, 240, 225));
        scene.DrawRect(r, cellTint, 0.0f, gridLineCol, 1.0f * invZ);

        // 2x2 sub-tile quadtree crosshair lines + diagonal survey lines
        scene.DrawLine({x0, cy}, {x1, cy}, gridLineCol, 1.0f * invZ);
        scene.DrawLine({cx, y0}, {cx, y1}, gridLineCol, 1.0f * invZ);
        if (minSidePx >= 64.0f) {
            const guikit::Color diagCol = WithAlpha(gridLineCol, darkChartSurface ? 0.12f : 0.10f);
            scene.DrawLine({x0, y0}, {x1, y1}, diagCol, 0.9f * invZ);
            scene.DrawLine({x1, y0}, {x0, y1}, diagCol, 0.9f * invZ);
        }
    }

    // 2. Crisp L-shaped corner brackets indicating an active background tile job
    const float insetW = keel::min(5.0f * invZ, r.w * 0.12f);
    const float armLenW = keel::clamp(minSidePx * 0.12f, 6.0f, 16.0f) * invZ;
    const float bracketStrokeW = (hasPreview ? 1.35f : 1.65f) * invZ;
    const float bx0 = x0 + insetW;
    const float by0 = y0 + insetW;
    const float bx1 = x1 - insetW;
    const float by1 = y1 - insetW;

    // Top-left bracket
    scene.DrawLine({bx0, by0}, {bx0 + armLenW, by0}, accentCol, bracketStrokeW);
    scene.DrawLine({bx0, by0}, {bx0, by0 + armLenW}, accentCol, bracketStrokeW);
    // Top-right bracket
    scene.DrawLine({bx1, by0}, {bx1 - armLenW, by0}, accentCol, bracketStrokeW);
    scene.DrawLine({bx1, by0}, {bx1, by0 + armLenW}, accentCol, bracketStrokeW);
    // Bottom-left bracket
    scene.DrawLine({bx0, by1}, {bx0 + armLenW, by1}, accentCol, bracketStrokeW);
    scene.DrawLine({bx0, by1}, {bx0, by1 - armLenW}, accentCol, bracketStrokeW);
    // Bottom-right bracket
    scene.DrawLine({bx1, by1}, {bx1 - armLenW, by1}, accentCol, bracketStrokeW);
    scene.DrawLine({bx1, by1}, {bx1, by1 - armLenW}, accentCol, bracketStrokeW);

    // 3. Animated radar/sonar loading spinner + [z/x/y] tile status badge
    const keel::u64 nowMs = keel::monotonic_ms();
    const keel::u32 cellPhase = static_cast<keel::u32>((cell.id.x * 37 + cell.id.y * 17) & 63);
    const float spinAngleDeg = std::fmod(
        static_cast<float>(nowMs % 3600ULL) * 0.30f +
        static_cast<float>(frameCounter % 180ULL) * 4.0f +
        static_cast<float>(cellPhase) * 5.5f,
        360.0f
    );

    const float spinnerRadiusPx = hasPreview
        ? keel::clamp(minSidePx * 0.075f, 6.5f, 11.5f)
        : keel::clamp(minSidePx * 0.11f, 8.5f, 15.5f);
    const float spinnerRadiusW = spinnerRadiusPx * invZ;

    // When a mipmap preview is already visible, anchor a compact badge near the top-right corner
    // so the preview chart in the center remains completely unobstructed; otherwise center it.
    const guikit::Vec2 spinnerCenter = hasPreview
        ? guikit::Vec2{bx1 - (spinnerRadiusPx + 8.0f) * invZ, by0 + (spinnerRadiusPx + 8.0f) * invZ}
        : guikit::Vec2{cx, cy - (minSidePx >= 88.0f ? 8.0f * invZ : 0.0f)};

    const guikit::Color badgeBg = darkChartSurface
        ? guikit::Color(9, 20, 34, hasPreview ? 185 : 215)
        : guikit::Color(248, 250, 252, hasPreview ? 195 : 225);
    const guikit::Color trackCol = darkChartSurface
        ? guikit::Color(56, 189, 248, 52)
        : guikit::Color(2, 132, 199, 48);

    scene.DrawCircle(spinnerCenter, (spinnerRadiusPx + 3.5f) * invZ, badgeBg, trackCol, 1.1f * invZ);
    scene.DrawArc(spinnerCenter, spinnerRadiusW, spinAngleDeg, 115.0f, accentCol, 2.0f * invZ, 12);
    scene.DrawArc(
        spinnerCenter,
        spinnerRadiusW * 0.60f,
        spinAngleDeg + 180.0f,
        80.0f,
        WithAlpha(accentCol, 0.65f),
        1.4f * invZ,
        10
    );
    scene.DrawCircle(spinnerCenter, 1.8f * invZ, accentCol);

    if (!hasPreview && minSidePx >= 84.0f) {
        char labelBuf[48]{};
        const keel::StringRef labelSv = keel::format_buf(
            labelBuf, "LOADING [%d/%d/%d]", cell.id.z, cell.id.x, cell.id.y
        );
        const float pillW = keel::min(r.w * 0.86f, 118.0f * invZ);
        const float pillH = 16.0f * invZ;
        const guikit::Rect pillRect(
            cx - pillW * 0.5f,
            spinnerCenter.y + (spinnerRadiusPx + 6.5f) * invZ,
            pillW,
            pillH
        );
        scene.DrawRect(pillRect, badgeBg, 4.0f * invZ, trackCol, 1.0f * invZ);
        const guikit::Color textCol = darkChartSurface
            ? guikit::Color(186, 230, 253, 235)
            : guikit::Color(3, 105, 161, 235);
        scene.DrawTextBox(
            labelSv,
            pillRect,
            9.5f,
            textCol,
            0.5f,
            0.5f,
            guikit::FontStyle::Bold,
            /*scaleWithZoom=*/false
        );
    }
}

[[nodiscard]] keel::u64 ComputeLayerStyleHash(
    const geo::GeoLayer& lyr,
    const GisRenderConfig& cfg,
    guikit::MapCrs crs
) noexcept {
    keel::u64 h = keel::kFnv1aOffsetBasis;
    const keel::u32 resolvedTileSz = ResolveTileSizePx(cfg.tileSizePx);
    const bool hasBasemapOverlay = (cfg.showOsmBasemap || cfg.customBasemapProvider != nullptr);
    const bool suppressLandFill = hasBasemapOverlay && cfg.hideLandOverBasemap;
    HashCombineVal(h, crs);
    HashCombineVal(h, resolvedTileSz);
    HashCombineVal(h, cfg.chartPalette);
    HashCombineVal(h, cfg.lodSimplification);
    HashCombineVal(h, cfg.lodPixelTolerance);
    HashCombineVal(h, cfg.scaleFiltering);
    HashCombineVal(h, cfg.showDepthContours);
    HashCombineVal(h, cfg.showCoverageBounds);
    HashCombineVal(h, cfg.showCautionAndRestrictedZones);
    HashCombineVal(h, cfg.safetyContourM);
    HashCombineVal(h, cfg.shallowContourM);
    HashCombineVal(h, cfg.deepContourM);
    HashCombineVal(h, suppressLandFill);

    HashCombineVal(h, lyr.uid);
    const keel::usize fCount = lyr.isWindowStreamed
        ? keel::max(lyr.estimatedTotalFeatures, static_cast<keel::usize>(1u))
        : lyr.features.size();
    HashCombineVal(h, fCount);
    HashCombineVal(h, lyr.style.useCustomColor);
    HashCombineVal(h, lyr.style.primaryColor.r);
    HashCombineVal(h, lyr.style.primaryColor.g);
    HashCombineVal(h, lyr.style.primaryColor.b);
    HashCombineVal(h, lyr.style.strokeOpacity);
    HashCombineVal(h, lyr.style.strokeWidthPx);
    HashCombineVal(h, lyr.style.renderFill);
    HashCombineVal(h, lyr.style.renderStroke);
    return h;
}

// ============================================================================
// Pan-Invariant Zoom-Level World-Space Declutter Cache (Pre-Indexed Compact Lists)
// ============================================================================

[[nodiscard]] keel::StringRef ResolveFeatureLabelText(
    const geo::GeoFeature& feat,
    const GisRenderConfig& cfg
) noexcept {
    if (!cfg.preferredLanguage.empty()) {
        const keel::StringRef lang = cfg.preferredLanguage;
        if (lang == "ru" && !feat.labelRu.empty()) {
            return feat.labelRu.as_ref();
        }
        if (lang == "en" && !feat.labelEn.empty()) {
            return feat.labelEn.as_ref();
        }
        for (const auto& [k, v] : feat.properties) {
            if (v.is_string()) {
                const keel::StringRef sv = v.as_string();
                if (!sv.empty()) {
                    const keel::StringRef kRef = k.as_ref();
                    if ((kRef.size() == 5 + lang.size() && kRef.starts_with("name:") && kRef.substr(5) == lang) ||
                        (kRef.size() == 6 + lang.size() && kRef.starts_with("label_") && kRef.substr(6) == lang)) {
                        return sv;
                    }
                }
            }
        }
    }
    if (cfg.preferRussianLabels && !feat.labelRu.empty()) {
        return feat.labelRu.as_ref();
    }
    if (!feat.labelEn.empty()) {
        return feat.labelEn.as_ref();
    }
    if (!feat.labelRu.empty()) {
        return feat.labelRu.as_ref();
    }
    if ((feat.chartClass == geo::ChartFeatureClass::Generic || feat.featureCode == "LNDELV") &&
        !feat.name.empty()) {
        return feat.name.as_ref();
    }
    return {};
}

[[maybe_unused]] [[nodiscard]] keel::usize CountUtf8CodePoints(keel::StringRef s) noexcept {
    keel::usize count = 0;
    for (keel::usize i = 0; i < s.size();) {
        const keel::u8 c = static_cast<keel::u8>(s[i]);
        keel::usize seqLen = 1;
        if ((c & 0x80u) == 0u) seqLen = 1;
        else if ((c & 0xE0u) == 0xC0u) seqLen = 2;
        else if ((c & 0xF0u) == 0xE0u) seqLen = 3;
        else if ((c & 0xF8u) == 0xF0u) seqLen = 4;
        if (i + seqLen > s.size()) break;
        ++count;
        i += seqLen;
    }
    return count;
}

[[nodiscard]] keel::StringRef FormatMapCalloutLabel(
    const geo::GeoFeature& feat,
    const GisRenderConfig& cfg,
    keel::String& scratchBuf,
    keel::usize* outCharCount = nullptr
) {
    if (outCharCount) *outCharCount = 0;

    // 1. Optional filter: suppress callout labels on polygonal area features (keeping point & line labels)
    if (!cfg.showAreaFeatureLabels && feat.HasPolygons() && feat.points.empty()) {
        return {};
    }

    keel::StringRef raw = ResolveFeatureLabelText(feat, cfg);
    if (raw.empty()) return {};

    // 2. Optional S-52 filter: suppress multi-sentence INFORM / TXTDSC cautionary notes (containing '|')
    //    or regulatory caution/restricted area notes from cluttering the map surface
    if (cfg.hideMultiLineNotesOnMap) {
        if (raw.find('|') != keel::StringRef::npos ||
            raw.find('\n') != keel::StringRef::npos ||
            IsCautionOrRestrictedArea(feat)) {
            return {};
        }
    }

    // 3. Configurable character limit (UTF-8 codepoint-aware truncation so Cyrillic never splits mid-character)
    keel::usize charCount = 0;
    keel::usize cutByteOffset = raw.size();
    for (keel::usize i = 0; i < raw.size();) {
        const keel::u8 c = static_cast<keel::u8>(raw[i]);
        keel::usize seqLen = 1;
        if ((c & 0x80u) == 0u) seqLen = 1;
        else if ((c & 0xE0u) == 0xC0u) seqLen = 2;
        else if ((c & 0xF0u) == 0xE0u) seqLen = 3;
        else if ((c & 0xF8u) == 0xF0u) seqLen = 4;
        if (i + seqLen > raw.size()) break;
        ++charCount;
        i += seqLen;
        if (cfg.maxMapLabelChars > 0 && charCount == cfg.maxMapLabelChars && i < raw.size()) {
            cutByteOffset = i;
            break;
        }
    }

    if (cfg.maxMapLabelChars > 0 && cutByteOffset < raw.size()) {
        if (!cfg.truncateLongLabels) {
            return {};
        }
        scratchBuf.assign(raw.substr(0, cutByteOffset));
        for (char& ch : scratchBuf) {
            if (ch == '|' || ch == '\n' || ch == '\r') ch = ' ';
        }
        scratchBuf.append("...");
        if (outCharCount) *outCharCount = charCount + 3;
        return scratchBuf.as_ref();
    }

    if (outCharCount) *outCharCount = charCount;

    if (raw.find('|') != keel::StringRef::npos || raw.find('\n') != keel::StringRef::npos) {
        scratchBuf.assign(raw);
        for (char& ch : scratchBuf) {
            if (ch == '|' || ch == '\n' || ch == '\r') ch = ' ';
        }
        return scratchBuf.as_ref();
    }

    return raw;
}

class WorldDeclutterCache {
public:
    struct AcceptedItem {
        keel::u32 layerIdx{0};
        keel::u32 featIdx{0};
    };

    void Clear() noexcept {
        m_cachedKey = 0;
        m_safeNormBox = guikit::Rect(0.0f, 0.0f, 0.0f, 0.0f);
        m_acceptedSymbols.clear();
        m_acceptedLabels.clear();
        m_acceptedSoundings.clear();
        ClearOccupiedCells();
    }

    void ReleaseMemory() noexcept {
        m_cachedKey = 0;
        m_safeNormBox = guikit::Rect(0.0f, 0.0f, 0.0f, 0.0f);
        m_totalDecluttered = 0;
        m_acceptedSymbols = {};
        m_acceptedLabels = {};
        m_acceptedSoundings = {};
        m_cellKeys = {};
        m_cellEpochs = {};
        m_curEpoch = 1;
        m_occupiedCount = 0;
        m_queryScratch = {};
        m_soundingCands = {};
    }

    void Sync(
        keel::ArrayRef<geo::GeoLayer> layers,
        const GisRenderConfig& cfg,
        keel::i32 tileZoom,
        guikit::MapCrs crs,
        const guikit::Rect& paddedVisNormBox
    ) {
        keel::u64 key = keel::kFnv1aOffsetBasis;
        HashCombineVal(key, tileZoom);
        HashCombineVal(key, crs);
        HashCombineVal(key, cfg.preferRussianLabels);
        for (char ch : cfg.preferredLanguage) {
            HashCombineVal(key, static_cast<keel::u8>(ch));
        }
        HashCombineVal(key, cfg.showSoundings);
        HashCombineVal(key, cfg.showFeatureCallouts);
        HashCombineVal(key, cfg.scaleFiltering);
        HashCombineVal(key, cfg.showCautionAndRestrictedZones);
        HashCombineVal(key, cfg.showAreaFeatureLabels);
        HashCombineVal(key, cfg.hideMultiLineNotesOnMap);
        HashCombineVal(key, cfg.maxMapLabelChars);
        HashCombineVal(key, cfg.truncateLongLabels);

        for (const geo::GeoLayer& lyr : layers) {
            HashCombineVal(key, lyr.uid);
            HashCombineVal(key, lyr.visible);
            HashCombineVal(key, lyr.style.showLabels);
            HashCombineVal(key, lyr.style.pointRadiusPx);
            HashCombineVal(key, lyr.style.renderAsSeamark);
            const keel::usize fCount = lyr.features.size();
            HashCombineVal(key, fCount);
            HashCombineVal(key, lyr.windowGeneration);
        }

        const bool insideSafeBox = (m_safeNormBox.w > 0.0f) &&
            (paddedVisNormBox.x >= m_safeNormBox.x &&
             paddedVisNormBox.y >= m_safeNormBox.y &&
             paddedVisNormBox.Right() <= m_safeNormBox.Right() &&
             paddedVisNormBox.Bottom() <= m_safeNormBox.Bottom());

        if (key == m_cachedKey && insideSafeBox) {
            return;
        }

        m_cachedKey = key;
        const float spanX = keel::max(1e-5f, paddedVisNormBox.w);
        const float spanY = keel::max(1e-5f, paddedVisNormBox.h);
        m_safeNormBox = guikit::Rect(
            paddedVisNormBox.x - spanX * 0.30f,
            paddedVisNormBox.y - spanY * 0.30f,
            spanX * 1.60f,
            spanY * 1.60f
        );
        const guikit::Rect queryNormBox(
            paddedVisNormBox.x - spanX * 0.70f,
            paddedVisNormBox.y - spanY * 0.70f,
            spanX * 2.40f,
            spanY * 2.40f
        );

        m_totalDecluttered = 0;
        m_acceptedSymbols.clear();
        m_acceptedLabels.clear();
        m_acceptedSoundings.clear();
        ClearOccupiedCells();

        const float worldPxScale = static_cast<float>(256ULL << keel::clamp(tileZoom, 0, 22));
        auto toWorldPx = [crs, worldPxScale](guikit::Vec2 norm3857, guikit::GeoCoord geo) noexcept -> guikit::Vec2 {
            if (crs == guikit::MapCrs::EPSG3857_WebMercator || crs == guikit::MapCrs::EPSG3395_WorldMercator) {
                return {norm3857.x * worldPxScale, norm3857.y * worldPxScale};
            }
            if (crs == guikit::MapCrs::EPSG4326_PlateCarree) {
                const float ny = static_cast<float>((90.0 - keel::clamp(geo.lat, -90.0, 90.0)) * (1.0 / 180.0));
                return {norm3857.x * worldPxScale, ny * worldPxScale};
            }
            const guikit::Vec2 n = guikit::MapGrid::GeoToNormalized(geo, crs);
            return {n.x * worldPxScale, n.y * worldPxScale};
        };

        // Pass 1: Buoys, Beacons & Generic Point Features (highest priority — only scan layers with points!)
        for (keel::usize li = 0; li < layers.size(); ++li) {
            const geo::GeoLayer& layer = layers[li];
            if (!layer.visible || layer.pointFeatureCount == 0 || layer.features.empty()) continue;
            if (layer.chartClassMask != 0u &&
                !layer.HasChartClass(geo::ChartFeatureClass::BuoyOrBeacon) &&
                !layer.HasChartClass(geo::ChartFeatureClass::Generic)) {
                continue;
            }
            if (!layer.normBounds.Intersects(queryNormBox)) continue;
            const float ptScale = keel::clamp(layer.style.pointRadiusPx / 6.5f, 0.35f, 3.5f);
            const float boxHalf = 7.0f * ptScale;

            auto processPass1Feat = [&](keel::u32 fi) {
                const geo::GeoFeature& feat = layer.features[fi];
                if (feat.normPoints.empty() || feat.points.empty()) return;

                if (feat.chartClass == geo::ChartFeatureClass::BuoyOrBeacon) {
                    const guikit::Vec2 wpt = toWorldPx(feat.normPoints[0], feat.points[0]);
                    if (TryReserveWorldBox(wpt.x - boxHalf, wpt.y - boxHalf, boxHalf * 2.0f, boxHalf * 2.0f)) {
                        m_acceptedSymbols.push_back({static_cast<keel::u32>(li), fi});
                    } else {
                        ++m_totalDecluttered;
                    }
                } else if (feat.chartClass == geo::ChartFeatureClass::Generic &&
                           !feat.HasPolygons() && !feat.HasLines()) {
                    m_acceptedSymbols.push_back({static_cast<keel::u32>(li), fi});
                }
            };

            if (layer.features.size() > 2048) {
                m_queryScratch.clear();
                layer.spatialIndex.query_box(
                    keel::RTreeBox<float>::from_xywh(queryNormBox.x, queryNormBox.y, queryNormBox.w, queryNormBox.h),
                    [&](keel::u32 fi) { m_queryScratch.push_back(fi); }
                );
                keel::sort(m_queryScratch);
                for (keel::u32 fi : m_queryScratch) {
                    if (fi < layer.features.size()) processPass1Feat(fi);
                }
            } else {
                for (keel::usize fi = 0; fi < layer.features.size(); ++fi) {
                    processPass1Feat(static_cast<keel::u32>(fi));
                }
            }
        }

        // Pass 2: Underwater Hazards, Point Structures (PILPNT / SLCONS), & Land Elevations (LNDELV)
        if (tileZoom >= 11) {
            for (keel::usize li = 0; li < layers.size(); ++li) {
                const geo::GeoLayer& layer = layers[li];
                if (!layer.visible || layer.pointFeatureCount == 0 || layer.features.empty()) continue;
                if (layer.chartClassMask != 0u &&
                    !layer.HasChartClass(geo::ChartFeatureClass::UnderwaterHazard) &&
                    !layer.HasChartClass(geo::ChartFeatureClass::BuiltUpOrStructure) &&
                    !layer.HasChartClass(geo::ChartFeatureClass::LandArea)) {
                    continue;
                }
                if (!layer.normBounds.Intersects(queryNormBox)) continue;
                const float ptScale = keel::clamp(layer.style.pointRadiusPx / 6.5f, 0.35f, 3.5f);

                auto processPass2Feat = [&](keel::u32 fi) {
                    const geo::GeoFeature& feat = layer.features[fi];
                    if (feat.normPoints.empty() || feat.points.empty() ||
                        feat.HasPolygons() || feat.HasLines()) {
                        return;
                    }
                    const bool isHazard = (feat.chartClass == geo::ChartFeatureClass::UnderwaterHazard);
                    const bool isPointStructOrLand = (tileZoom >= 12) &&
                        (feat.chartClass == geo::ChartFeatureClass::BuiltUpOrStructure ||
                         feat.chartClass == geo::ChartFeatureClass::LandArea);
                    if (!isHazard && !isPointStructOrLand) {
                        return;
                    }

                    const guikit::Vec2 wpt = toWorldPx(feat.normPoints[0], feat.points[0]);
                    const float boxHalf = (isHazard ? 6.0f : 4.5f) * ptScale;
                    if (TryReserveWorldBox(wpt.x - boxHalf, wpt.y - boxHalf, boxHalf * 2.0f, boxHalf * 2.0f)) {
                        m_acceptedSymbols.push_back({static_cast<keel::u32>(li), fi});
                    } else {
                        ++m_totalDecluttered;
                    }
                };

                if (layer.features.size() > 2048) {
                    m_queryScratch.clear();
                    layer.spatialIndex.query_box(
                        keel::RTreeBox<float>::from_xywh(queryNormBox.x, queryNormBox.y, queryNormBox.w, queryNormBox.h),
                        [&](keel::u32 fi) { m_queryScratch.push_back(fi); }
                    );
                    keel::sort(m_queryScratch);
                    for (keel::u32 fi : m_queryScratch) {
                        if (fi < layer.features.size()) processPass2Feat(fi);
                    }
                } else {
                    for (keel::usize fi = 0; fi < layer.features.size(); ++fi) {
                        processPass2Feat(static_cast<keel::u32>(fi));
                    }
                }
            }
        }

        // Pass 3: Cartographic Text Labels (`labelRu` / `labelEn` / Named Features)
        if (cfg.showFeatureCallouts) {
            keel::String labelScratch;
            constexpr keel::u32 kNoCalloutMask =
                (1u << static_cast<keel::u8>(geo::ChartFeatureClass::Sounding)) |
                (1u << static_cast<keel::u8>(geo::ChartFeatureClass::DepthArea)) |
                (1u << static_cast<keel::u8>(geo::ChartFeatureClass::CoverageMask));

            for (keel::usize li = 0; li < layers.size(); ++li) {
                const geo::GeoLayer& layer = layers[li];
                if (!layer.visible || !layer.style.showLabels || layer.features.empty()) continue;
                if (layer.IsExclusivelyChartClasses(kNoCalloutMask)) {
                    continue;
                }
                if (!layer.normBounds.Intersects(queryNormBox)) continue;

                auto processPass3Feat = [&](keel::u32 fi) {
                    const geo::GeoFeature& feat = layer.features[fi];
                    if (feat.chartClass == geo::ChartFeatureClass::Sounding ||
                        feat.chartClass == geo::ChartFeatureClass::DepthArea ||
                        feat.chartClass == geo::ChartFeatureClass::CoverageMask) {
                        return;
                    }
                    if (tileZoom < 11 && feat.chartClass != geo::ChartFeatureClass::TextLabel) return;
                    if (ShouldSkipFeatureAtZoom(feat, tileZoom, cfg)) return;

                    keel::usize charCount = 0;
                    const keel::StringRef labelText = FormatMapCalloutLabel(feat, cfg, labelScratch, &charCount);
                    if (labelText.empty()) return;

                    const guikit::Vec2 wpt = toWorldPx(feat.normCentroid, feat.centroid);
                    const float estWidthPx = keel::clamp(static_cast<float>(charCount) * 6.0f + 10.0f, 16.0f, 640.0f);
                    const float estHeightPx = 16.0f;
                    const float offsetX = !feat.points.empty() ? 8.0f : (-estWidthPx * 0.4f);
                    const float offsetY = -7.0f;

                    if (TryReserveWorldBox(wpt.x + offsetX, wpt.y + offsetY - 4.0f, estWidthPx, estHeightPx)) {
                        m_acceptedLabels.push_back({static_cast<keel::u32>(li), fi});
                    } else {
                        ++m_totalDecluttered;
                    }
                };

                if (layer.features.size() > 2048) {
                    m_queryScratch.clear();
                    layer.spatialIndex.query_box(
                        keel::RTreeBox<float>::from_xywh(queryNormBox.x, queryNormBox.y, queryNormBox.w, queryNormBox.h),
                        [&](keel::u32 fi) { m_queryScratch.push_back(fi); }
                    );
                    keel::sort(m_queryScratch);
                    for (keel::u32 fi : m_queryScratch) {
                        if (fi < layer.features.size()) processPass3Feat(fi);
                    }
                } else {
                    for (keel::usize fi = 0; fi < layer.features.size(); ++fi) {
                        processPass3Feat(static_cast<keel::u32>(fi));
                    }
                }
            }
        }

        // Pass 4: Depth Soundings (`SOUNDG`) — sorted shallowest-first (IHO S-52 hydrographic safety rule)
        if (cfg.showSoundings && tileZoom >= 11) {
            const float boxW = (tileZoom >= 14) ? 32.0f : ((tileZoom == 13) ? 42.0f : 54.0f);
            const float boxH = (tileZoom >= 14) ? 18.0f : 24.0f;

            m_soundingCands.clear();

            for (keel::usize li = 0; li < layers.size(); ++li) {
                const geo::GeoLayer& layer = layers[li];
                if (!layer.visible || layer.pointFeatureCount == 0 || layer.features.empty()) continue;
                if (layer.chartClassMask != 0u && !layer.HasChartClass(geo::ChartFeatureClass::Sounding)) {
                    continue;
                }
                if (!layer.normBounds.Intersects(queryNormBox)) continue;

                auto processSoundingFeat = [&](keel::u32 fi) {
                    const geo::GeoFeature& feat = layer.features[fi];
                    if (feat.chartClass != geo::ChartFeatureClass::Sounding ||
                        feat.normPoints.empty() || feat.points.empty()) return;
                    if (std::isnan(feat.depthMeters)) return;

                    const guikit::Vec2 wpt = toWorldPx(feat.normPoints[0], feat.points[0]);
                    m_soundingCands.push_back(SoundingCand{
                        .layerIdx = static_cast<keel::u32>(li),
                        .featIdx = fi,
                        .depth = feat.depthMeters,
                        .wx = wpt.x,
                        .wy = wpt.y
                    });
                };

                if (layer.features.size() > 2048) {
                    m_queryScratch.clear();
                    layer.spatialIndex.query_box(
                        keel::RTreeBox<float>::from_xywh(queryNormBox.x, queryNormBox.y, queryNormBox.w, queryNormBox.h),
                        [&](keel::u32 fi) { m_queryScratch.push_back(fi); }
                    );
                    for (keel::u32 fi : m_queryScratch) {
                        if (fi < layer.features.size()) processSoundingFeat(fi);
                    }
                } else {
                    for (keel::usize fi = 0; fi < layer.features.size(); ++fi) {
                        processSoundingFeat(static_cast<keel::u32>(fi));
                    }
                }
            }

            keel::sort(m_soundingCands,
                [](const SoundingCand& a, const SoundingCand& b) noexcept {
                    if (a.depth != b.depth) return a.depth < b.depth;
                    if (a.layerIdx != b.layerIdx) return a.layerIdx < b.layerIdx;
                    return a.featIdx < b.featIdx;
                });

            for (const SoundingCand& sc : m_soundingCands) {
                if (TryReserveWorldBox(sc.wx - boxW * 0.5f, sc.wy - boxH * 0.5f, boxW, boxH)) {
                    m_acceptedSoundings.push_back({sc.layerIdx, sc.featIdx});
                } else {
                    ++m_totalDecluttered;
                }
            }
        }
    }

    [[nodiscard]] keel::ArrayRef<AcceptedItem> GetAcceptedSymbols() const noexcept { return m_acceptedSymbols; }
    [[nodiscard]] keel::ArrayRef<AcceptedItem> GetAcceptedLabels() const noexcept { return m_acceptedLabels; }
    [[nodiscard]] keel::ArrayRef<AcceptedItem> GetAcceptedSoundings() const noexcept { return m_acceptedSoundings; }
    [[nodiscard]] keel::usize GetTotalDecluttered() const noexcept { return m_totalDecluttered; }

private:
    static constexpr keel::usize kCellTableSize = 131072; // 2^17 slots (~1.5 MB flat table, zero heap allocations)
    static constexpr keel::usize kCellTableMask = kCellTableSize - 1;

    void ClearOccupiedCells() noexcept {
        if (m_cellEpochs.size() != kCellTableSize) {
            m_cellKeys.assign(kCellTableSize, 0ULL);
            m_cellEpochs.assign(kCellTableSize, 0U);
            m_curEpoch = 1U;
        } else {
            ++m_curEpoch;
            if (m_curEpoch == 0U) {
                keel::fill(m_cellEpochs.as_mut_ref(), 0U);
                m_curEpoch = 1U;
            }
        }
        m_occupiedCount = 0;
    }

    [[nodiscard]] static keel::u64 PackCell(keel::i32 cx, keel::i32 cy) noexcept {
        return (static_cast<keel::u64>(static_cast<keel::u32>(cx)) << 32) |
               static_cast<keel::u64>(static_cast<keel::u32>(cy));
    }

    [[nodiscard]] static keel::usize HashCellKey(keel::u64 k) noexcept {
        k ^= k >> 33;
        k *= 0xff51afd7ed558ccdULL;
        k ^= k >> 33;
        return static_cast<keel::usize>(k) & kCellTableMask;
    }

    [[nodiscard]] bool ContainsCell(keel::u64 key) const noexcept {
        keel::usize idx = HashCellKey(key);
        while (m_cellEpochs[idx] == m_curEpoch) {
            if (m_cellKeys[idx] == key) return true;
            idx = (idx + 1) & kCellTableMask;
        }
        return false;
    }

    void InsertCell(keel::u64 key) noexcept {
        if (m_occupiedCount >= (kCellTableSize * 3) / 4) return;
        keel::usize idx = HashCellKey(key);
        while (m_cellEpochs[idx] == m_curEpoch) {
            if (m_cellKeys[idx] == key) return;
            idx = (idx + 1) & kCellTableMask;
        }
        m_cellEpochs[idx] = m_curEpoch;
        m_cellKeys[idx] = key;
        ++m_occupiedCount;
    }

    [[nodiscard]] bool TryReserveWorldBox(float wx, float wy, float w, float h) noexcept {
        const keel::i32 c0 = keel::floor_i32(wx * 0.125f);
        const keel::i32 c1 = keel::floor_i32((wx + w) * 0.125f);
        const keel::i32 r0 = keel::floor_i32(wy * 0.125f);
        const keel::i32 r1 = keel::floor_i32((wy + h) * 0.125f);

        for (keel::i32 r = r0; r <= r1; ++r) {
            for (keel::i32 c = c0; c <= c1; ++c) {
                if (ContainsCell(PackCell(c, r))) {
                    return false;
                }
            }
        }

        for (keel::i32 r = r0; r <= r1; ++r) {
            for (keel::i32 c = c0; c <= c1; ++c) {
                InsertCell(PackCell(c, r));
            }
        }
        return true;
    }

    struct SoundingCand {
        keel::u32 layerIdx;
        keel::u32 featIdx;
        float depth;
        float wx;
        float wy;
    };

    keel::u64 m_cachedKey{0};
    guikit::Rect m_safeNormBox{0.0f, 0.0f, 0.0f, 0.0f};
    keel::usize m_totalDecluttered{0};
    keel::Array<AcceptedItem> m_acceptedSymbols;
    keel::Array<AcceptedItem> m_acceptedLabels;
    keel::Array<AcceptedItem> m_acceptedSoundings;
    keel::Array<keel::u64> m_cellKeys;
    keel::Array<keel::u32> m_cellEpochs;
    keel::u32 m_curEpoch{1};
    keel::usize m_occupiedCount{0};
    keel::Array<keel::u32> m_queryScratch;
    keel::Array<SoundingCand> m_soundingCands;
};

WorldDeclutterCache& GetDeclutterCache() {
    static WorldDeclutterCache s_declutterCache;
    return s_declutterCache;
}

// ============================================================================
// O(log N) R-Tree Geometric Hit-Testing (Points -> Lines -> Polygons with Holes)
// ============================================================================

[[nodiscard]] bool PointInNormRing(guikit::Vec2 pt, keel::ArrayRef<guikit::Vec2> ring) noexcept {
    if (ring.size() < 3) return false;
    bool inside = false;
    for (keel::usize i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
        const guikit::Vec2& vi = ring[i];
        const guikit::Vec2& vj = ring[j];
        if (((vi.y > pt.y) != (vj.y > pt.y)) &&
            (pt.x < (vj.x - vi.x) * (pt.y - vi.y) / (vj.y - vi.y + 1e-12f) + vi.x)) {
            inside = !inside;
        }
    }
    return inside;
}

[[nodiscard]] float PointToSegmentDistSq(guikit::Vec2 p, guikit::Vec2 a, guikit::Vec2 b) noexcept {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float lenSq = dx * dx + dy * dy;
    if (lenSq <= 1e-16f) {
        const float px = p.x - a.x;
        const float py = p.y - a.y;
        return px * px + py * py;
    }
    const float t = keel::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / lenSq, 0.0f, 1.0f);
    const float projX = a.x + t * dx;
    const float projY = a.y + t * dy;
    const float ex = p.x - projX;
    const float ey = p.y - projY;
    return ex * ex + ey * ey;
}

struct HitTestMatch {
    const geo::GeoLayer* layer{nullptr};
    const geo::GeoFeature* feat{nullptr};
};

struct CursorHitCache {
    guikit::Vec2 cursorNorm{-1e9f, -1e9f};
    float pickRadiusNorm{0.0f};
    keel::i32 tileZoom{-1};
    keel::usize layerCount{0};
    bool showCoverageBounds{false};
    bool showCautionAndRestrictedZones{true};
    keel::u64 matchedUid{0};
    bool valid{false};

    void Clear() noexcept { valid = false; matchedUid = 0; }
};

CursorHitCache& GetCursorHitCache() noexcept {
    static CursorHitCache s_hitCache;
    return s_hitCache;
}

[[nodiscard]] HitTestMatch FindFeatureUnderCursor(
    guikit::Vec2 cursorNorm,
    float pickRadiusNorm,
    keel::i32 tileZoom,
    keel::ArrayRef<geo::GeoLayer> layers,
    const GisRenderConfig& cfg,
    keel::u64 frameTick,
    keel::usize* outPagedInCount = nullptr
) {
    CursorHitCache& hc = GetCursorHitCache();
    const float moveDx = cursorNorm.x - hc.cursorNorm.x;
    const float moveDy = cursorNorm.y - hc.cursorNorm.y;
    const float moveThresholdSq = (pickRadiusNorm * 0.08f) * (pickRadiusNorm * 0.08f);
    if (hc.valid &&
        hc.tileZoom == tileZoom &&
        hc.layerCount == layers.size() &&
        hc.showCoverageBounds == cfg.showCoverageBounds &&
        hc.showCautionAndRestrictedZones == cfg.showCautionAndRestrictedZones &&
        std::abs(hc.pickRadiusNorm - pickRadiusNorm) <= pickRadiusNorm * 0.05f &&
        (moveDx * moveDx + moveDy * moveDy) <= moveThresholdSq) {
        if (hc.matchedUid == 0) {
            return {};
        }
        const geo::GeoLayer* hitLyr = nullptr;
        const geo::GeoFeature* hitFeat = geo::FindFeatureByUid(layers, hc.matchedUid, &hitLyr);
        if (hitFeat && hitLyr && hitLyr->visible) {
            hitFeat->TouchAccess(frameTick);
            return {hitLyr, hitFeat};
        }
    }

    const guikit::Rect pickBox(
        cursorNorm.x - pickRadiusNorm,
        cursorNorm.y - pickRadiusNorm,
        pickRadiusNorm * 2.0f,
        pickRadiusNorm * 2.0f
    );
    const float pickRadiusSq = pickRadiusNorm * pickRadiusNorm;
    const float minPickableFeatureDim = pickRadiusNorm * 0.25f;
    const bool useLodHit = (tileZoom <= 11);

    HitTestMatch bestPoint{};
    float bestPointDistSq = pickRadiusSq * 1.6f;

    HitTestMatch bestLine{};
    float bestLineDistSq = pickRadiusSq;

    HitTestMatch bestPoly{};
    float bestPolyArea = 1e30f;

    keel::usize syncHitPagedCount = 0;
    keel::Array<keel::u32> candidates;
    for (const geo::GeoLayer& layer : layers) {
        if (!layer.visible || layer.features.empty()) continue;
        if (!layer.normBounds.Intersects(pickBox)) continue;

        candidates.clear();
        layer.spatialIndex.query_box(
            keel::RTreeBox<float>::from_xywh(pickBox.x, pickBox.y, pickBox.w, pickBox.h),
            [&](keel::u32 fIdx) { candidates.push_back(fIdx); }
        );
        for (keel::u32 fIdx : candidates) {
            if (fIdx >= layer.features.size()) continue;
            const geo::GeoFeature& feat = layer.features[fIdx];
            if (ShouldSkipFeatureAtZoom(feat, tileZoom, cfg)) continue;

            // 1. Point hit-test (highest priority)
            for (const guikit::Vec2& pt : feat.normPoints) {
                const float dx = cursorNorm.x - pt.x;
                const float dy = cursorNorm.y - pt.y;
                const float dSq = dx * dx + dy * dy;
                if (dSq <= bestPointDistSq) {
                    bestPointDistSq = dSq;
                    bestPoint = {&layer, &feat};
                }
            }
            if (bestPoint.feat) continue;

            // Skip sub-pixel lines/polygons at low zoom
            const float maxDimNorm = keel::max(feat.normBounds.w, feat.normBounds.h);
            if (maxDimNorm < minPickableFeatureDim) continue;

            // 2. Line hit-test
            if (feat.HasLines()) {
                const bool wasLoaded = feat.geometryLoaded;
                bool ready = feat.TryEnsureGeometryLoadedNonBlocking(frameTick);
                if (!ready && (!cfg.asyncBackgroundStreaming || syncHitPagedCount < 2)) {
                    ready = feat.EnsureGeometryLoaded(frameTick);
                    if (ready) ++syncHitPagedCount;
                }
                if (!ready) {
                    continue;
                }
                if (!wasLoaded && outPagedInCount) {
                    ++(*outPagedInCount);
                }
                for (keel::usize li = 0; li < feat.normLines.size(); ++li) {
                    const auto& line = (useLodHit && li < feat.normLinesLod.size() && feat.normLinesLod[li].size() >= 2) ? feat.normLinesLod[li] : feat.normLines[li];
                    for (keel::usize i = 1; i < line.size(); ++i) {
                        const float dSq = PointToSegmentDistSq(cursorNorm, line[i - 1], line[i]);
                        if (dSq <= bestLineDistSq) {
                            bestLineDistSq = dSq;
                            bestLine = {&layer, &feat};
                        }
                    }
                }
            }

            // 3. Polygon hit-test (smallest containing polygon wins; fast bbox & area reject BEFORE paging SQLite!)
            if (!feat.HasPolygons() || !feat.normBounds.Contains(cursorNorm)) {
                continue;
            }
            if (feat.chartClass == geo::ChartFeatureClass::CoverageMask && !cfg.showCoverageBounds) {
                continue;
            }
            const float bboxArea = keel::max(1e-12f, feat.normBounds.w * feat.normBounds.h);
            const float priorityWeight = (feat.chartClass == geo::ChartFeatureClass::DepthArea)
                ? 4.0f
                : ((feat.chartClass == geo::ChartFeatureClass::SeabedOrMarineArea) ? 2.5f : 1.0f);
            const float weightedArea = bboxArea * priorityWeight;
            if (weightedArea > bestPolyArea) {
                continue;
            }

            const bool wasLoaded = feat.geometryLoaded;
            bool ready = feat.TryEnsureGeometryLoadedNonBlocking(frameTick);
            if (!ready && (!cfg.asyncBackgroundStreaming || syncHitPagedCount < 2)) {
                ready = feat.EnsureGeometryLoaded(frameTick);
                if (ready) ++syncHitPagedCount;
            }
            if (!ready) {
                continue;
            }
            if (!wasLoaded && outPagedInCount) {
                ++(*outPagedInCount);
            }

            for (const geo::GeoPolygon& poly : feat.polygons) {
                const auto& extRing = (useLodHit && poly.normExteriorLod.size() >= 3)
                    ? poly.normExteriorLod
                    : poly.normExterior;
                if (!PointInNormRing(cursorNorm, extRing)) continue;
                bool inHole = false;
                for (const auto& hole : poly.normInterior) {
                    if (PointInNormRing(cursorNorm, hole)) {
                        inHole = true;
                        break;
                    }
                }
                if (inHole) continue;
                bestPolyArea = weightedArea;
                bestPoly = {&layer, &feat};
            }
        }
    }

    HitTestMatch result = bestPoint.feat ? bestPoint : (bestLine.feat ? bestLine : bestPoly);
    hc.cursorNorm = cursorNorm;
    hc.pickRadiusNorm = pickRadiusNorm;
    hc.tileZoom = tileZoom;
    hc.layerCount = layers.size();
    hc.showCoverageBounds = cfg.showCoverageBounds;
    hc.showCautionAndRestrictedZones = cfg.showCautionAndRestrictedZones;
    hc.matchedUid = result.feat ? result.feat->uid : 0ULL;
    hc.valid = true;
    return result;
}

// ============================================================================
// Direct Feature Highlight & Marine Symbol Helpers
// ============================================================================

[[nodiscard]] guikit::Color ResolveIalaBuoyColor(keel::StringRef lightOrColor, guikit::Color fallback) noexcept {
    if (lightOrColor.empty()) return fallback;
    // Check standard IHO S-57 COLOUR codes, letter abbreviations, and light strings
    if (lightOrColor == "R" || lightOrColor == "3" || lightOrColor == "RW" || lightOrColor == "WR" ||
        lightOrColor == "3,1" || lightOrColor == "1,3" ||
        lightOrColor.contains("Red") ||
        lightOrColor.contains("red")) {
        return guikit::Color(239, 68, 68, 255); // IALA Red (Port / Safe Water)
    }
    if (lightOrColor == "G" || lightOrColor == "4" ||
        lightOrColor == "4,1" || lightOrColor == "1,4" ||
        lightOrColor.contains("Green") ||
        lightOrColor.contains("green")) {
        return guikit::Color(16, 185, 129, 255); // IALA Green (Starboard)
    }
    if (lightOrColor == "Y" || lightOrColor == "6" ||
        lightOrColor == "BY" || lightOrColor == "YB" ||
        lightOrColor == "YBY" || lightOrColor == "BYB" ||
        lightOrColor == "6,2" || lightOrColor == "2,6" ||
        lightOrColor == "6,2,6" || lightOrColor == "2,6,2" ||
        lightOrColor.contains("Yellow") ||
        lightOrColor.contains("yellow")) {
        return guikit::Color(245, 158, 11, 255); // IALA Yellow / Cardinal (Black-Yellow)
    }
    if (lightOrColor == "W" || lightOrColor == "1" ||
        lightOrColor.contains("White") ||
        lightOrColor.contains("white")) {
        return guikit::Color(56, 189, 248, 255); // Lighted Mark / Sector
    }
    return fallback;
}

void DrawWorldRing(
    guikit::MapViewScope& map,
    guikit::SceneScope& scene,
    keel::ArrayRef<guikit::GeoCoord> ring,
    guikit::Color strokeColor,
    float strokeWidthWorld,
    float minSegmentWorldSq,
    bool closed,
    bool drawVertexHandles,
    float handleRadiusWorld,
    guikit::Color handleFill,
    keel::usize& ioRenderedVerts
) {
    if (ring.size() < 2) return;

    const guikit::Rect visW = scene.GetVisibleWorldRect();
    const float padW = keel::max(strokeWidthWorld * 4.0f, keel::max(visW.w, visW.h) * 0.05f);
    const float clipMinX = visW.x - padW;
    const float clipMinY = visW.y - padW;
    const float clipMaxX = visW.Right() + padW;
    const float clipMaxY = visW.Bottom() + padW;

    auto drawClippedWorldSegment = [&](guikit::Vec2 a, guikit::Vec2 b) {
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        float t0 = 0.0f;
        float t1 = 1.0f;
        auto clipTest = [&](float p, float q) noexcept -> bool {
            if (std::abs(p) < 1e-8f) return q >= 0.0f;
            const float r = q / p;
            if (p < 0.0f) {
                if (r > t1) return false;
                if (r > t0) t0 = r;
            } else {
                if (r < t0) return false;
                if (r < t1) t1 = r;
            }
            return true;
        };
        if (!clipTest(-dx, a.x - clipMinX) ||
            !clipTest( dx, clipMaxX - a.x) ||
            !clipTest(-dy, a.y - clipMinY) ||
            !clipTest( dy, clipMaxY - a.y)) {
            return;
        }
        const guikit::Vec2 ca(a.x + t0 * dx, a.y + t0 * dy);
        const guikit::Vec2 cb(a.x + t1 * dx, a.y + t1 * dy);
        scene.DrawLine(ca, cb, strokeColor, strokeWidthWorld);
    };

    guikit::Vec2 firstW = map.GeoToWorld(ring.front());
    guikit::Vec2 prevW = firstW;
    ++ioRenderedVerts;

    if (drawVertexHandles &&
        firstW.x >= clipMinX && firstW.x <= clipMaxX &&
        firstW.y >= clipMinY && firstW.y <= clipMaxY) {
        scene.DrawCircle(firstW, handleRadiusWorld, handleFill, strokeColor, strokeWidthWorld * 0.6f);
    }

    for (keel::usize i = 1; i < ring.size(); ++i) {
        const guikit::Vec2 currW = map.GeoToWorld(ring[i]);
        const float dx = currW.x - prevW.x;
        const float dy = currW.y - prevW.y;
        const float distSq = dx * dx + dy * dy;

        const bool isLast = (i + 1 == ring.size());
        if (!isLast && distSq < minSegmentWorldSq) {
            continue;
        }

        drawClippedWorldSegment(prevW, currW);
        prevW = currW;
        ++ioRenderedVerts;

        if (drawVertexHandles &&
            currW.x >= clipMinX && currW.x <= clipMaxX &&
            currW.y >= clipMinY && currW.y <= clipMaxY) {
            scene.DrawCircle(currW, handleRadiusWorld, handleFill, strokeColor, strokeWidthWorld * 0.6f);
        }
    }

    if (closed && ring.size() >= 3) {
        const float dx = firstW.x - prevW.x;
        const float dy = firstW.y - prevW.y;
        if (dx * dx + dy * dy > 1e-6f) {
            drawClippedWorldSegment(prevW, firstW);
        }
    }
}

void RenderHighlightedFeatureOverlay(
    guikit::MapViewScope& map,
    guikit::SceneScope& scene,
    const geo::GeoFeature& feat,
    guikit::Color accentCol,
    guikit::Color handleFill,
    float invZ,
    float minSegWorldSq,
    bool drawHandles,
    keel::u64 frameTick,
    keel::usize& ioRenderedVerts,
    keel::usize* outPagedInCount = nullptr
) {
    const bool wasLoaded = feat.geometryLoaded;
    if (!feat.EnsureGeometryLoaded(frameTick)) {
        return;
    }
    if (!wasLoaded && outPagedInCount) {
        ++(*outPagedInCount);
    }

    const float strokeW = 2.4f * invZ;
    const float glowW = 5.2f * invZ;
    const float handleRadiusW = 3.2f * invZ;
    const guikit::Color glowCol = WithAlpha(accentCol, 0.30f);
    const bool allowHandles = drawHandles && (feat.VertexCount() <= 160);

    for (const geo::GeoPolygon& poly : feat.polygons) {
        DrawWorldRing(map, scene, poly.exteriorRing, glowCol, glowW, minSegWorldSq, true, false, 0.0f, handleFill, ioRenderedVerts);
        DrawWorldRing(map, scene, poly.exteriorRing, accentCol, strokeW, minSegWorldSq, true, allowHandles, handleRadiusW, handleFill, ioRenderedVerts);
        for (const auto& hole : poly.interiorRings) {
            DrawWorldRing(map, scene, hole, accentCol, strokeW * 0.85f, minSegWorldSq, true, allowHandles, handleRadiusW, handleFill, ioRenderedVerts);
        }
    }

    for (const auto& line : feat.lines) {
        DrawWorldRing(map, scene, line, glowCol, glowW, minSegWorldSq, false, false, 0.0f, handleFill, ioRenderedVerts);
        DrawWorldRing(map, scene, line, accentCol, strokeW, minSegWorldSq, false, allowHandles, handleRadiusW, handleFill, ioRenderedVerts);
    }

    for (const guikit::GeoCoord& pt : feat.points) {
        map.DrawGeoCircle(pt, 9.5f, WithAlpha(accentCol, 0.28f), accentCol, 2.0f);
        map.DrawGeoCircle(pt, 4.5f, accentCol, handleFill, 1.5f);
        ++ioRenderedVerts;
    }
}

} // namespace

keel::usize GisRenderEngine::EnforceFeatureRamBudget(
    keel::ArrayRef<geo::GeoLayer> layers,
    keel::usize maxResidentBytes,
    keel::u64 protectedTick,
    keel::usize* outResidentFeatures,
    keel::usize* outResidentBytes
) {
    keel::usize totalResidentBytes = 0;
    keel::usize totalResidentFeatures = 0;
    keel::usize streamedChunkBytes = 0;

    for (const geo::GeoLayer& layer : layers) {
        if (!layer.pager) continue;
        if (layer.isWindowStreamed) {
            const keel::usize chBytes = layer.pager->GetCachedChunkBytes();
            const keel::usize chFeats = layer.pager->GetCachedFeatureCount();
            totalResidentBytes += chBytes;
            streamedChunkBytes += chBytes;
            totalResidentFeatures += chFeats;
            if (layer.pointFeatureCount == 0) {
                for (const geo::GeoFeature& feat : layer.features) {
                    if (feat.residentHeapBytes > 0) {
                        totalResidentBytes += feat.residentHeapBytes;
                    }
                }
            }
        } else {
            for (const geo::GeoFeature& feat : layer.features) {
                if (feat.residentHeapBytes > 0) {
                    totalResidentBytes += feat.residentHeapBytes;
                    ++totalResidentFeatures;
                }
            }
        }
    }

    keel::usize evictedCount = 0;
    if (totalResidentBytes > maxResidentBytes) {
        const keel::usize targetBytes = (maxResidentBytes * 85u) / 100u;

        // 1. First evict cold decoded WKB feature payloads (polygons/lines) so lightweight chunk metadata stays cached
        {
            struct EvictCandidate {
                const geo::GeoFeature* feat{nullptr};
                keel::u64 lastAccessTick{0};
                keel::u32 bytes{0};
                bool isWindowStreamed{false};
            };
            keel::Array<EvictCandidate> evictPool;
            evictPool.reserve(keel::min<keel::usize>(totalResidentFeatures, 65536u));

            for (const geo::GeoLayer& layer : layers) {
                if (!layer.pager) continue;
                if (layer.isWindowStreamed && layer.pointFeatureCount > 0 &&
                    layer.polygonFeatureCount == 0 && layer.lineFeatureCount == 0) {
                    continue;
                }
                for (const geo::GeoFeature& feat : layer.features) {
                    if (feat.residentHeapBytes > 0 && feat.lastAccessTick < protectedTick) {
                        if (layer.isWindowStreamed && !feat.points.empty() &&
                            feat.polygons.empty() && feat.lines.empty()) {
                            continue;
                        }
                        evictPool.push_back({
                            .feat = &feat,
                            .lastAccessTick = feat.lastAccessTick,
                            .bytes = feat.residentHeapBytes,
                            .isWindowStreamed = layer.isWindowStreamed
                        });
                    }
                }
            }

            // Sort least-recently-used first; break ties by evicting largest allocations first
            keel::sort(evictPool,
                [](const EvictCandidate& a, const EvictCandidate& b) noexcept {
                    if (a.lastAccessTick != b.lastAccessTick) {
                        return a.lastAccessTick < b.lastAccessTick;
                    }
                    return a.bytes > b.bytes;
                });

            for (const EvictCandidate& cand : evictPool) {
                if (totalResidentBytes <= targetBytes) {
                    break;
                }
                const keel::usize beforeBytes = cand.feat->residentHeapBytes;
                cand.feat->UnloadGeometryPayload();
                cand.feat->UnloadPropertiesPayload();
                const keel::usize afterBytes = cand.feat->residentHeapBytes;
                if (beforeBytes > afterBytes) {
                    const keel::usize freed = beforeBytes - afterBytes;
                    totalResidentBytes = (totalResidentBytes > freed) ? (totalResidentBytes - freed) : 0;
                    if (!cand.isWindowStreamed && afterBytes == 0 && totalResidentFeatures > 0) {
                        --totalResidentFeatures;
                    }
                    ++evictedCount;
                }
            }
        }

        // 2. Only evict cold spatial metadata chunks if decoded geometry eviction was not enough
        if (totalResidentBytes > targetBytes && streamedChunkBytes > 0) {
            for (const geo::GeoLayer& layer : layers) {
                if (!layer.pager || !layer.isWindowStreamed) continue;
                const keel::usize lyrBytes = layer.pager->GetCachedChunkBytes();
                if (lyrBytes == 0) continue;
                const keel::usize lyrTarget = (maxResidentBytes == 0)
                    ? 0u
                    : static_cast<keel::usize>((static_cast<double>(targetBytes) * static_cast<double>(lyrBytes)) /
                                          static_cast<double>(keel::max<keel::usize>(1u, totalResidentBytes)));
                evictedCount += layer.pager->EvictChunksToBudget(lyrTarget, protectedTick);
            }

            // Recompute resident totals after chunk eviction
            totalResidentBytes = 0;
            totalResidentFeatures = 0;
            for (const geo::GeoLayer& layer : layers) {
                if (!layer.pager) continue;
                if (layer.isWindowStreamed) {
                    totalResidentBytes += layer.pager->GetCachedChunkBytes();
                    totalResidentFeatures += layer.pager->GetCachedFeatureCount();
                    if (layer.pointFeatureCount == 0) {
                        for (const geo::GeoFeature& feat : layer.features) {
                            if (feat.residentHeapBytes > 0) {
                                totalResidentBytes += feat.residentHeapBytes;
                            }
                        }
                    }
                } else {
                    for (const geo::GeoFeature& feat : layer.features) {
                        if (feat.residentHeapBytes > 0) {
                            totalResidentBytes += feat.residentHeapBytes;
                            ++totalResidentFeatures;
                        }
                    }
                }
            }
        }
    }

    if (outResidentFeatures) *outResidentFeatures = totalResidentFeatures;
    if (outResidentBytes) *outResidentBytes = totalResidentBytes;
    return evictedCount;
}

void GisRenderEngine::InvalidateTileCache() {
    GetAsyncStreamer().ClearAndBumpEpoch();
    GetTileCache().Clear();
    GetTilePixelPool().Clear();
    GetDeclutterCache().Clear();
    GetCursorHitCache().Clear();
}

void GisRenderEngine::TrimInactiveCaches() noexcept {
    GetAsyncStreamer().TrimIdleMemory();
    GetTileCache().ReleaseMemory();
    GetTilePixelPool().Clear();
    GetDeclutterCache().ReleaseMemory();
    GetCursorHitCache().Clear();

    keel::Arc<IRasterTileProvider> defaultProv;
    keel::Arc<IRasterTileProvider> lastProv;
    {
        std::scoped_lock lk(GetDefaultBasemapMutex());
        defaultProv = GetDefaultBasemapStorage();
        lastProv = keel::move(GetLastActiveBasemapStorage());
    }
    if (defaultProv) {
        defaultProv->ClearCache();
    }
    if (lastProv && lastProv.get() != defaultProv.get()) {
        lastProv->ClearCache();
    }
#if defined(__GLIBC__)
    ::malloc_trim(0);
#endif
}

bool GisRenderEngine::WaitForBackgroundStreamingIdle(keel::u32 timeoutMs) {
    return GetAsyncStreamer().WaitForIdle(timeoutMs);
}

Color GisRenderEngine::GetWaterBackgroundColor(ChartPalette palette) noexcept {
    switch (palette) {
        case ChartPalette::MarineNight:
            return Color(6, 15, 28, 255);
        case ChartPalette::MarineDusk:
            return Color(17, 38, 60, 255);
        case ChartPalette::ClassicGis:
            return Color(226, 238, 248, 255);
        case ChartPalette::MarineDay:
        default:
            return Color(226, 238, 248, 255);
    }
}

void GisRenderEngine::SetDefaultBasemapProvider(keel::Arc<IRasterTileProvider> provider) {
    std::scoped_lock lk(GetDefaultBasemapMutex());
    GetDefaultBasemapStorage() = keel::move(provider);
}

keel::Arc<IRasterTileProvider> GisRenderEngine::GetDefaultBasemapProvider() {
    std::scoped_lock lk(GetDefaultBasemapMutex());
    return GetDefaultBasemapStorage();
}

void GisRenderEngine::SetBreadcrumbCallback(BreadcrumbCallback cb) noexcept {
    g_breadcrumbCallback.store(cb, std::memory_order_relaxed);
}

GisRenderStats GisRenderEngine::RenderLayers(
    guikit::MapViewScope& map,
    keel::ArrayRef<geo::GeoLayer> layers,
    const GisRenderConfig& cfg,
    keel::u64& ioSelectedFeatureUid,
    bool lightMode
) {
    static keel::u64 s_frameCounter = 1;
    ++s_frameCounter;

    GisRenderStats stats{};
    guikit::SceneScope& scene = map.GetScene();
    const guikit::MapViewState& viewState = map.GetViewState();
    const guikit::GeoBounds visBounds = map.GetVisibleGeoBounds();
    const bool isPanning = scene.IsPanning();
    const bool useAsyncStreaming = cfg.asyncBackgroundStreaming && HasBackgroundWorkerThreads();

    const float zoom = keel::max(0.0001f, scene.GetZoom());
    const float invZ = 1.0f / zoom;
    const float minSegWorld = cfg.lodSimplification ? (cfg.lodPixelTolerance * invZ) : 0.0f;
    const float minSegWorldSq = minSegWorld * minSegWorld;
    const keel::i32 tileZoom = viewState.tileZoom;
    GetAsyncStreamer().SetActiveTileZoom(tileZoom);

    const keel::u32 configuredTileSizePx = ResolveTileSizePx(cfg.tileSizePx);
    const keel::Arc<geo::IRasterTileProvider> activeBasemapProvider = ResolveActiveBasemapProvider(cfg);
    const keel::StringRef basemapSourceId = activeBasemapProvider
        ? activeBasemapProvider->GetSourceId()
        : keel::StringRef{};
    const keel::u8 basemapOpacityByte = activeBasemapProvider
        ? static_cast<keel::u8>(
              keel::clamp(keel::round_i32(keel::clamp(cfg.basemapOpacity, 0.0f, 1.0f) * 255.0f), 0, 255)
          )
        : 0u;
    const bool useBasemapOverlay = (activeBasemapProvider != nullptr && basemapOpacityByte > 0);

    const bool isNightPalette = (cfg.chartPalette == ChartPalette::MarineNight);
    const bool isDuskPalette  = (cfg.chartPalette == ChartPalette::MarineDusk);
    const bool darkChartSurface = isNightPalette || isDuskPalette || (cfg.chartPalette == ChartPalette::ClassicGis && !lightMode);

    const guikit::Color selAccent = darkChartSurface
        ? guikit::Color(250, 204, 21, 255)  // Crisp Gold
        : guikit::Color(217, 119, 6, 255);  // Deep Amber
    const guikit::Color hovAccent = darkChartSurface
        ? guikit::Color(56, 189, 248, 255)
        : guikit::Color(2, 132, 199, 255);
    const guikit::Color labelTextCol = darkChartSurface
        ? guikit::Color(241, 245, 249, 245)
        : guikit::Color(15, 23, 42, 245);
    const guikit::Color handleFill = darkChartSurface
        ? guikit::Color(15, 23, 42, 245)
        : guikit::Color(255, 255, 255, 245);
    const guikit::Color chartBaseBg = isNightPalette
        ? guikit::Color(6, 15, 28, 255)
        : (isDuskPalette
            ? guikit::Color(17, 38, 60, 255)
            : (cfg.chartPalette == ChartPalette::ClassicGis
                ? (lightMode ? guikit::Color(241, 245, 249, 255) : guikit::Color(15, 23, 42, 255))
                : guikit::Color(226, 238, 248, 255)));

    // Compute normalized Web Mercator visible box for fast O(log N) R-Tree queries
    const guikit::Vec2 visNwNorm = guikit::MapGrid::GeoToNormalized(visBounds.nw, guikit::MapCrs::EPSG3857_WebMercator);
    const guikit::Vec2 visSeNorm = guikit::MapGrid::GeoToNormalized(visBounds.se, guikit::MapCrs::EPSG3857_WebMercator);
    const float visMinX = keel::min(visNwNorm.x, visSeNorm.x);
    const float visMinY = keel::min(visNwNorm.y, visSeNorm.y);
    const float visMaxX = keel::max(visNwNorm.x, visSeNorm.x);
    const float visMaxY = keel::max(visNwNorm.y, visSeNorm.y);
    const guikit::Rect visNormBox(visMinX, visMinY, visMaxX - visMinX, visMaxY - visMinY);

    // Padded query box (80 px screen margin) so labels near screen edges slide smoothly without popping
    const float worldPixelsTotal = keel::max(256.0f, 2048.0f * zoom);
    const float labelPadNorm = 80.0f / worldPixelsTotal;
    const guikit::Rect paddedVisNormBox(
        visMinX - labelPadNorm,
        visMinY - labelPadNorm,
        (visMaxX - visMinX) + labelPadNorm * 2.0f,
        (visMaxY - visMinY) + labelPadNorm * 2.0f
    );

    // Compute union of `visBounds` and all visible slippy tiles (128 / 256 / 512 px, including guard band)
    // so polygon/line layers always have 100% spatial coverage for every visible tile cell.
    guikit::GeoBounds tileUnionGeoBounds = visBounds;
    if (cfg.useVectorTileCache && !viewState.visibleTiles.empty()) {
        double uMinLon = keel::min(visBounds.nw.lon, visBounds.se.lon);
        double uMaxLon = keel::max(visBounds.nw.lon, visBounds.se.lon);
        double uMinLat = keel::min(visBounds.nw.lat, visBounds.se.lat);
        double uMaxLat = keel::max(visBounds.nw.lat, visBounds.se.lat);
        const double kGuardFrac = static_cast<double>(kTileGuardBandPx) / static_cast<double>(configuredTileSizePx);
        for (const guikit::OsmTileCell& cell : viewState.visibleTiles) {
            const double cMinLon = keel::min(cell.geoBounds.nw.lon, cell.geoBounds.se.lon);
            const double cMaxLon = keel::max(cell.geoBounds.nw.lon, cell.geoBounds.se.lon);
            const double cMinLat = keel::min(cell.geoBounds.nw.lat, cell.geoBounds.se.lat);
            const double cMaxLat = keel::max(cell.geoBounds.nw.lat, cell.geoBounds.se.lat);
            const double padLon = (cMaxLon - cMinLon) * kGuardFrac;
            const double padLat = (cMaxLat - cMinLat) * kGuardFrac;
            uMinLon = keel::min(uMinLon, cMinLon - padLon);
            uMaxLon = keel::max(uMaxLon, cMaxLon + padLon);
            uMinLat = keel::min(uMinLat, cMinLat - padLat);
            uMaxLat = keel::max(uMaxLat, cMaxLat + padLat);
        }
        tileUnionGeoBounds.nw = {keel::clamp(uMaxLat, -85.05112878, 85.05112878), keel::clamp(uMinLon, -180.0, 180.0)};
        tileUnionGeoBounds.se = {keel::clamp(uMinLat, -85.05112878, 85.05112878), keel::clamp(uMaxLon, -180.0, 180.0)};
    }

    const keel::u64 frameRenderStartUs = keel::monotonic_us();

    // Stream active spatial window chunks for massive Out-of-Core GeoPackage layers.
    const keel::u64 syncStartUs = keel::monotonic_us();
    keel::Array<guikit::Rect> dirtiedBoxes;
    keel::usize totalInvalidatedTilesThisFrame = 0;

    if (useAsyncStreaming) {
        keel::Array<AsyncWindowResult> completedWindows;
        GetAsyncStreamer().DrainCompletedWindows(completedWindows);
        for (AsyncWindowResult& winRes : completedWindows) {
            if (!winRes.snapshot) continue;
            for (const geo::GeoLayer& layer : layers) {
                if (layer.uid == winRes.layerUid) {
                    const keel::Array<guikit::Rect> snapDirtied = winRes.snapshot->dirtiedNormBoxes.clone();
                    const keel::usize newlyPaged = winRes.snapshot->newlyPagedFeatures;
                    if (const_cast<geo::GeoLayer&>(layer).ApplyWindowSnapshot(keel::move(*winRes.snapshot))) {
                        stats.pagedInFeaturesThisFrame += newlyPaged;
                        totalInvalidatedTilesThisFrame += GetTileCache().InvalidateTilesForDirtiedBoxes(
                            layer.uid, snapDirtied, s_frameCounter
                        );
                        GetCursorHitCache().Clear();
                    }
                    break;
                }
            }
        }
    }

    for (const geo::GeoLayer& layer : layers) {
        if (!layer.visible || !layer.isWindowStreamed || !layer.pager) continue;
        const bool hasPolyOrLine = (layer.polygonFeatureCount > 0 || layer.lineFeatureCount > 0 || layer.pointFeatureCount == 0);
        const guikit::GeoBounds& targetWindowBounds = hasPolyOrLine ? tileUnionGeoBounds : visBounds;

        if (!useAsyncStreaming) {
            dirtiedBoxes.clear();
            const keel::usize prevCachedFeats = layer.pager->GetCachedFeatureCount();
            if (layer.pager->SyncSpatialWindow(
                    const_cast<geo::GeoLayer&>(layer),
                    targetWindowBounds,
                    paddedVisNormBox,
                    static_cast<float>(tileZoom),
                    cfg.scaleFiltering,
                    s_frameCounter,
                    &dirtiedBoxes)) {
                const keel::usize curCachedFeats = layer.pager->GetCachedFeatureCount();
                if (curCachedFeats > prevCachedFeats) {
                    stats.pagedInFeaturesThisFrame += (curCachedFeats - prevCachedFeats);
                }
                if (!dirtiedBoxes.empty()) {
                    totalInvalidatedTilesThisFrame += GetTileCache().InvalidateTilesForDirtiedBoxes(
                        layer.uid, dirtiedBoxes, s_frameCounter
                    );
                }
                GetCursorHitCache().Clear();
            }
        } else if (layer.pager->NeedsSpatialWindowUpdate(
                       layer, targetWindowBounds, static_cast<float>(tileZoom), cfg.scaleFiltering)) {
            GetAsyncStreamer().RequestSpatialWindow(
                layer,
                targetWindowBounds,
                paddedVisNormBox,
                static_cast<float>(tileZoom),
                cfg.scaleFiltering,
                s_frameCounter
            );
        }
    }

    if (useAsyncStreaming) {
        stats.cachedTilesPending += GetAsyncStreamer().GetPendingWindowCount();
    }
    const double syncMs = static_cast<double>(keel::monotonic_us() - syncStartUs) * 0.001;

    keel::usize activeWindowFeatures = 0;
    for (const geo::GeoLayer& layer : layers) {
        if (layer.visible) {
            ++stats.visibleLayers;
            stats.totalFeatures += layer.isWindowStreamed
                ? keel::max(layer.estimatedTotalFeatures, layer.features.size())
                : layer.features.size();
            activeWindowFeatures += layer.features.size();
        }
    }

    if (stats.visibleLayers == 0 && !useBasemapOverlay) {
        return stats;
    }
    if (!useBasemapOverlay && stats.totalFeatures == 0 && activeWindowFeatures == 0 && stats.cachedTilesPending == 0) {
        return stats;
    }

    // ========================================================================
    // 1. O(log N) R-Tree Interactive Hit-Testing under Mouse Cursor
    //    (Suppressed while actively dragging/panning the map)
    // ========================================================================
    EmitRenderBreadcrumb("GisRenderEngine::Stage1_CursorHitTest", static_cast<keel::i32>(s_frameCounter), tileZoom);
    HitTestMatch hoveredMatch{};
    if (cfg.enableFeaturePicking && !isPanning && scene.IsHovered() && activeWindowFeatures > 0) {
        const guikit::Vec2 mouseWorld = scene.GetMouseWorldPos();
        const guikit::GeoCoord cursorGeo = map.WorldToGeo(mouseWorld);
        const guikit::Vec2 cursorNorm = guikit::MapGrid::GeoToNormalized(cursorGeo, guikit::MapCrs::EPSG3857_WebMercator);
        const float pickRadiusNorm = 9.0f / worldPixelsTotal;

        hoveredMatch = FindFeatureUnderCursor(
            cursorNorm, pickRadiusNorm, tileZoom, layers, cfg, s_frameCounter, &stats.pagedInFeaturesThisFrame
        );

        if (hoveredMatch.feat) {
            stats.hoveredFeatureUid = hoveredMatch.feat->uid;
            const float hitR = 12.0f * invZ;
            const guikit::Rect cursorHitRect(mouseWorld.x - hitR, mouseWorld.y - hitR, hitR * 2.0f, hitR * 2.0f);
            char catBuf[160];
            const auto catLabelRef = keel::format_buf(catBuf, "%s [%s]",
                hoveredMatch.layer->name.as_ref(),
                hoveredMatch.feat->featureCode.empty()
                    ? geo::GeometryTypeToString(hoveredMatch.feat->type)
                    : hoveredMatch.feat->featureCode.as_ref());
            auto item = scene.Item({
                .id = hoveredMatch.feat->id.as_ref(),
                .title = hoveredMatch.feat->name.as_ref(),
                .category = catLabelRef,
                .hitRect = cursorHitRect,
                .selectable = true,
                .showSelectionFrame = false
            });
            if (item.Clicked()) {
                stats.clickedFeatureUid = hoveredMatch.feat->uid;
                ioSelectedFeatureUid = hoveredMatch.feat->uid;
            }
        }
    }

    // Locate currently selected feature in O(1) via packed (layerUid << 32 | idx)
    const geo::GeoLayer* selectedLayer = nullptr;
    const geo::GeoFeature* selectedFeat = cfg.enableFeaturePicking
        ? geo::FindFeatureByUid(layers, ioSelectedFeatureUid, &selectedLayer)
        : nullptr;
    if (selectedLayer && !selectedLayer->visible) {
        selectedLayer = nullptr;
        selectedFeat = nullptr;
    }

    // ========================================================================
    // 2. Static Polygons & Lines: Two-Tier Atomic [z/x/y] Vector Tile Compositor
    // ========================================================================
    EmitRenderBreadcrumb("GisRenderEngine::Stage2_VectorTileCompositor", static_cast<keel::i32>(s_frameCounter), static_cast<keel::i32>(viewState.visibleTiles.size()));
    if (!viewState.visibleTiles.empty()) {
        float minTileWorldX = viewState.visibleTiles.front().worldRect.x;
        float minTileWorldY = viewState.visibleTiles.front().worldRect.y;
        float maxTileWorldX = viewState.visibleTiles.front().worldRect.Right();
        float maxTileWorldY = viewState.visibleTiles.front().worldRect.Bottom();
        for (const guikit::OsmTileCell& cell : viewState.visibleTiles) {
            minTileWorldX = keel::min(minTileWorldX, cell.worldRect.x);
            minTileWorldY = keel::min(minTileWorldY, cell.worldRect.y);
            maxTileWorldX = keel::max(maxTileWorldX, cell.worldRect.Right());
            maxTileWorldY = keel::max(maxTileWorldY, cell.worldRect.Bottom());
        }
        scene.DrawRect(
            guikit::Rect(minTileWorldX, minTileWorldY, maxTileWorldX - minTileWorldX, maxTileWorldY - minTileWorldY),
            chartBaseBg
        );
    }

    double rasterMs = 0.0;
    const keel::usize numVisTiles = viewState.visibleTiles.size();
    if (cfg.useVectorTileCache && !viewState.visibleTiles.empty()) {
        const keel::u64 rasterPassStartUs = keel::monotonic_us();
        PerLayerTileCacheManager& cache = GetTileCache();
        cache.SyncConfig(cfg, viewState.crs, basemapSourceId);
        cache.GarbageCollectLayers(layers);

        const keel::u32 tileSizePx = cache.GetTileSizePx();
        const float tileSizeF = cache.GetTileSizeF();

        if (useBasemapOverlay) {
            keel::Array<AsyncBasemapResult> completedBasemapTiles;
            GetAsyncStreamer().DrainCompletedBasemapTiles(completedBasemapTiles);
            for (AsyncBasemapResult& bm : completedBasemapTiles) {
                if (cache.StoreBasemapTile(bm.sourceId.as_ref(), bm.tileKey, keel::move(bm.image))) {
                    ++stats.cachedTilesRendered;
                }
            }
        }

        struct OrderedLayerEntry {
            keel::usize layerIdx{0};
            const geo::GeoLayer* layer{nullptr};
            PerLayerTileCacheManager::LayerTileStore* store{nullptr};
            keel::u64 styleHash{0};
            keel::u8 opacityByte{255};
            keel::i32 cartoRank{90};
            bool isActiveStyle{false};
        };

        keel::Array<OrderedLayerEntry> tileLayers;
        tileLayers.reserve(layers.size());
        for (keel::usize li = 0; li < layers.size(); ++li) {
            const geo::GeoLayer& lyr = layers[li];
            if (!lyr.visible || (lyr.polygonFeatureCount == 0 && lyr.lineFeatureCount == 0)) {
                continue;
            }
            float effectiveOpacity = 1.0f;
            if (lyr.polygonFeatureCount > 0 && lyr.style.renderFill) {
                effectiveOpacity = keel::clamp(lyr.style.fillOpacity, 0.0f, 1.0f);
                if (useBasemapOverlay && lyr.HasChartClass(geo::ChartFeatureClass::DepthArea)) {
                    effectiveOpacity *= keel::clamp(cfg.bathymetryOverBasemapOpacity, 0.05f, 1.0f);
                }
            }
            const keel::u8 layerOpacityByte = static_cast<keel::u8>(
                keel::clamp(keel::round_i32(effectiveOpacity * 255.0f), 0, 255)
            );
            if (layerOpacityByte == 0) continue;

            (void)cache.SyncLayerStore(lyr, viewState.crs);
            tileLayers.push_back(OrderedLayerEntry{
                .layerIdx = li,
                .layer = &lyr,
                .store = nullptr,
                .styleHash = ComputeLayerStyleHash(lyr, cfg, viewState.crs),
                .opacityByte = layerOpacityByte,
                .cartoRank = CartographicLayerRank(lyr),
                .isActiveStyle = (static_cast<keel::i32>(li) == cfg.activeStyleLayerIdx)
            });
        }
        for (OrderedLayerEntry& entry : tileLayers) {
            entry.store = &cache.SyncLayerStore(*entry.layer, viewState.crs);
        }

        // Drain completed background layer tiles AFTER `SyncLayerStore` so `activeTopologyHash` is initialized
        if (useAsyncStreaming) {
            keel::Array<AsyncLayerTileResult> completedLayerTiles;
            GetAsyncStreamer().DrainCompletedLayerTiles(completedLayerTiles);
            for (AsyncLayerTileResult& ltRes : completedLayerTiles) {
                for (const AsyncLoadedFeaturePayload& lp : ltRes.newlyLoadedPayloads) {
                    for (const geo::GeoLayer& lyr : layers) {
                        if (lyr.uid == lp.layerUid && lp.featureIndex < lyr.features.size()) {
                            const geo::GeoFeature& feat = lyr.features[lp.featureIndex];
                            if (feat.uid == lp.featureUid && !feat.geometryLoaded && lp.payload) {
                                feat.sharedGeometry = lp.payload;
                                feat.ApplyDecodedGeometryPayload(*lp.payload);
                                feat.TouchAccess(s_frameCounter);
                                ++stats.pagedInFeaturesThisFrame;
                            }
                            break;
                        }
                    }
                }
                if (cache.StoreAsyncLayerTile(keel::move(ltRes), s_frameCounter)) {
                    ++stats.cachedTilesRendered;
                }
            }
        }

        // Sort back-to-front by cartographic rank for deterministic compositing
        keel::sort(tileLayers,
            [](const OrderedLayerEntry& a, const OrderedLayerEntry& b) noexcept {
                if (a.cartoRank != b.cartoRank) return a.cartoRank < b.cartoRank;
                return a.layerIdx < b.layerIdx;
            });

        const keel::u32 bgPx = PackColorABGR8888(chartBaseBg);
        keel::u64 compositeSignature = keel::kFnv1aOffsetBasis;
        HashCombineVal(compositeSignature, viewState.crs);
        HashCombineVal(compositeSignature, tileSizePx);
        HashCombineVal(compositeSignature, bgPx);
        HashCombineVal(compositeSignature, useBasemapOverlay);
        if (useBasemapOverlay) {
            HashCombineBytes(compositeSignature, basemapSourceId.data(), basemapSourceId.size());
            HashCombineVal(compositeSignature, basemapOpacityByte);
            HashCombineVal(compositeSignature, cache.GetBasemapGeneration());
        }

        keel::Array<PerLayerTileCacheManager::CompositeLayerRef> compositeRefs;
        compositeRefs.reserve(tileLayers.size());
        for (const OrderedLayerEntry& entry : tileLayers) {
            HashCombineVal(compositeSignature, entry.layer->uid);
            HashCombineVal(compositeSignature, entry.styleHash);
            HashCombineVal(compositeSignature, entry.opacityByte);
            compositeRefs.push_back({
                .store = entry.store,
                .opacityByte = entry.opacityByte
            });
        }

        const guikit::Vec2 viewCenterW = map.GeoToWorld(viewState.center);

        struct PendingCellTask {
            keel::usize cellIdx{0};
            keel::u64 lastUpdatedFrame{0};
            float distSq{0.0f};
            bool hasPreview{false};
        };
        keel::Array<PendingCellTask> pendingCells;
        pendingCells.reserve(numVisTiles);

        for (keel::usize ci = 0; ci < numVisTiles; ++ci) {
            const guikit::OsmTileCell& cell = viewState.visibleTiles[ci];
            const TileCoordKey key{cell.id.z, cell.id.x, cell.id.y};

            const float cx = cell.worldRect.x + cell.worldRect.w * 0.5f;
            const float cy = cell.worldRect.y + cell.worldRect.h * 0.5f;
            const float dx = cx - viewCenterW.x;
            const float dy = cy - viewCenterW.y;
            const float cellDistSq = dx * dx + dy * dy;

            if (useBasemapOverlay && !cache.HasBasemapTile(key)) {
                const geo::SlippyTileKey bmKey{cell.id.z, cell.id.x, cell.id.y, tileSizePx};
                auto cachedBm = activeBasemapProvider->TryGetCachedTile(bmKey);
                if (cachedBm && cachedBm->IsValid()) {
                    if (cache.StoreBasemapTile(basemapSourceId, bmKey, keel::move(cachedBm))) {
                        ++stats.cachedTilesRendered;
                    }
                } else {
                    GetAsyncStreamer().EnqueueBasemapTileJob(activeBasemapProvider, bmKey, cellDistSq);
                }
            }

            // Fast-path 1: Tier-2 Composite Tile is already complete and matches current signature
            CompositeTile* comp = cache.FindCompositeTile(key, s_frameCounter);
            if (comp && comp->isComplete && comp->compositeSignature == compositeSignature && !comp->pixels.empty()) {
                ++stats.cachedTilesHit;
                continue;
            }

            // Compute guard-band normalized EPSG:3857 query box for this cell's geographic footprint
            const CellNorm3857Bounds cb = ComputeCellNorm3857Bounds(cell, viewState.crs);
            const float marginU = cb.spanU * (kTileGuardBandPx / tileSizeF);
            const float marginV = cb.spanV * (kTileGuardBandPx / tileSizeF);
            const guikit::Rect cellQueryBox(
                cb.u0 - marginU,
                cb.v0 - marginV,
                (cb.u1 - cb.u0) + 2.0f * marginU,
                (cb.v1 - cb.v0) + 2.0f * marginV
            );

            bool anyLayerNeedsRaster = false;
            for (const OrderedLayerEntry& entry : tileLayers) {
                const CachedVectorTile* lt = PerLayerTileCacheManager::FindTile(*entry.store, key, s_frameCounter);
                if (lt && lt->styleHash == entry.styleHash) {
                    continue;
                }
                if (!entry.layer->normBounds.Intersects(cellQueryBox)) {
                    cache.MarkTileEmpty(*entry.store, key, cellQueryBox, entry.styleHash, s_frameCounter);
                    continue;
                }
                if (entry.layer->features.empty()) {
                    if (useAsyncStreaming && entry.layer->isWindowStreamed &&
                        GetAsyncStreamer().IsLayerWindowPending(entry.layer->uid)) {
                        anyLayerNeedsRaster = true;
                        continue;
                    }
                    cache.MarkTileEmpty(*entry.store, key, cellQueryBox, entry.styleHash, s_frameCounter);
                    continue;
                }
                anyLayerNeedsRaster = true;
            }

            // Fast-path 2: All Tier-1 layer tiles are already cached (e.g. live fillOpacity change, basemap arrival, or layer toggle)
            if (!anyLayerNeedsRaster) {
                if (!comp || comp->pixels.empty() || comp->compositeSignature != compositeSignature) {
                    (void)cache.BuildCompositeTile(
                        key, cellQueryBox, compositeSignature, bgPx, compositeRefs,
                        s_frameCounter, useBasemapOverlay, basemapOpacityByte
                    );
                }
                ++stats.cachedTilesHit;
                continue;
            }

            // Seed fallback preview from parent [z-1]/[z-2] or child [z+1] composite tiles if this cell has no pixels yet
            if (!comp || comp->pixels.empty()) {
                (void)cache.SeedFallbackFromHierarchy(key, bgPx, s_frameCounter);
                comp = cache.FindCompositeTile(key, s_frameCounter);
            }

            pendingCells.push_back(PendingCellTask{
                .cellIdx = ci,
                .lastUpdatedFrame = comp ? comp->lastUpdatedFrame : 0ULL,
                .distSq = cellDistSq,
                .hasPreview = (comp != nullptr && !comp->pixels.empty())
            });
        }

        if (!pendingCells.empty()) {
            const bool isStyleDrag = (cfg.activeStyleLayerIdx >= 0);
            keel::sort(pendingCells,
                [isStyleDrag](const PendingCellTask& a, const PendingCellTask& b) noexcept {
                    if (isStyleDrag) {
                        if (a.lastUpdatedFrame != b.lastUpdatedFrame) {
                            return a.lastUpdatedFrame < b.lastUpdatedFrame;
                        }
                        if (a.distSq != b.distSq) return a.distSq < b.distSq;
                        return a.cellIdx < b.cellIdx;
                    }
                    if (a.hasPreview != b.hasPreview) {
                        return !a.hasPreview; // Tiles without fallback preview get highest priority
                    }
                    if (a.distSq != b.distSq) return a.distSq < b.distSq; // Center-outward radial completion
                    return a.cellIdx < b.cellIdx;
                });

            const float baseBudgetMs = keel::clamp(cfg.tileRenderBudgetMs, 1.0f, 30.0f);
            const float effectiveBudgetMs = isPanning ? keel::min(baseBudgetMs, 4.0f) : baseBudgetMs;
            const keel::u64 deadlineUs = frameRenderStartUs + static_cast<keel::u64>(effectiveBudgetMs * 1000.0f);
            const keel::u64 fastPathDeadlineUs = frameRenderStartUs + static_cast<keel::u64>(isPanning ? 1400 : 2500);
            keel::usize completedCells = 0;

            for (keel::usize pi = 0; pi < pendingCells.size(); ++pi) {
                const keel::usize ci = pendingCells[pi].cellIdx;
                if (!useAsyncStreaming &&
                    (completedCells >= 1 || syncMs >= 3.0) &&
                    keel::monotonic_us() >= deadlineUs) {
                    stats.cachedTilesPending += (pendingCells.size() - pi);
                    break;
                }

                const guikit::OsmTileCell& cell = viewState.visibleTiles[ci];
                const TileCoordKey key{cell.id.z, cell.id.x, cell.id.y};

                const CellNorm3857Bounds cb = ComputeCellNorm3857Bounds(cell, viewState.crs);
                const float marginU = cb.spanU * (kTileGuardBandPx / tileSizeF);
                const float marginV = cb.spanV * (kTileGuardBandPx / tileSizeF);
                const guikit::Rect cellQueryBox(
                    cb.u0 - marginU,
                    cb.v0 - marginV,
                    (cb.u1 - cb.u0) + 2.0f * marginU,
                    (cb.v1 - cb.v0) + 2.0f * marginV
                );

                keel::String layerSummary;
                bool allLayersReady = true;
                for (const OrderedLayerEntry& entry : tileLayers) {
                    const CachedVectorTile* lt = PerLayerTileCacheManager::FindTile(*entry.store, key, s_frameCounter);
                    if (lt && lt->styleHash == entry.styleHash) {
                        if constexpr (kEnableFileDebugLog) {
                            if (lt->hasAnyGeometry) {
                                if (!layerSummary.empty()) layerSummary.append(",");
                                layerSummary.append(entry.layer->tableName.empty() ? entry.layer->name.as_ref() : entry.layer->tableName.as_ref());
                            }
                        }
                        continue;
                    }

                    if (!useAsyncStreaming &&
                        (completedCells >= 1 || stats.pagedInFeaturesThisFrame >= 48) &&
                        keel::monotonic_us() >= deadlineUs) {
                        allLayersReady = false;
                        break;
                    }

                    bool abortedOrQueued = false;
                    const keel::usize minPagedBeforeCheck = (completedCells == 0) ? 48u : 16u;
                    const bool canRunSyncFastPath =
                        !useAsyncStreaming || (keel::monotonic_us() < fastPathDeadlineUs);
                    const CachedVectorTile* rendered = cache.RasterizeOrDispatchLayerTile(
                        *entry.store,
                        key,
                        entry.styleHash,
                        cell,
                        viewState.crs,
                        *entry.layer,
                        cfg,
                        s_frameCounter,
                        pendingCells[pi].distSq,
                        useAsyncStreaming,
                        canRunSyncFastPath,
                        &stats.pagedInFeaturesThisFrame,
                        /*hasDeadline=*/!useAsyncStreaming,
                        deadlineUs,
                        minPagedBeforeCheck,
                        &abortedOrQueued
                    );
                    if (!rendered || abortedOrQueued) {
                        allLayersReady = false;
                        if (!useAsyncStreaming) {
                            break;
                        }
                        continue;
                    }
                    if constexpr (kEnableFileDebugLog) {
                        if (rendered->hasAnyGeometry) {
                            if (!layerSummary.empty()) layerSummary.append(",");
                            layerSummary.append(entry.layer->tableName.empty() ? entry.layer->name.as_ref() : entry.layer->tableName.as_ref());
                        }
                    }
                }

                if (!allLayersReady) {
                    if (!pendingCells[pi].hasPreview && useBasemapOverlay) {
                        CompositeTile& partial = cache.BuildCompositeTile(
                            key, cellQueryBox, compositeSignature, bgPx, compositeRefs,
                            s_frameCounter, useBasemapOverlay, basemapOpacityByte
                        );
                        partial.isComplete = false;
                    }
                    if (!useAsyncStreaming) {
                        stats.cachedTilesPending += (pendingCells.size() - pi);
                        break;
                    }
                    ++stats.cachedTilesPending;
                    continue;
                }

                (void)cache.BuildCompositeTile(
                    key, cellQueryBox, compositeSignature, bgPx, compositeRefs,
                    s_frameCounter, useBasemapOverlay, basemapOpacityByte
                );
                ++completedCells;
                ++stats.cachedTilesRendered;

                if constexpr (kEnableFileDebugLog) {
                    const auto logLine = keel::format(
                        "[RasterTile] frame=%llu [%d/%d/%d] geo=[%.3f..%.3fE, %.3f..%.3fN] drawnLayers=[%s]",
                        s_frameCounter,
                        cell.id.z, cell.id.x, cell.id.y,
                        keel::min(cell.geoBounds.nw.lon, cell.geoBounds.se.lon),
                        keel::max(cell.geoBounds.nw.lon, cell.geoBounds.se.lon),
                        keel::min(cell.geoBounds.nw.lat, cell.geoBounds.se.lat),
                        keel::max(cell.geoBounds.nw.lat, cell.geoBounds.se.lat),
                        layerSummary.empty() ? keel::StringRef("EMPTY") : layerSummary.as_ref()
                    );
                    AppendRenderDebugLog(logLine);
                }
            }
        }

        cache.EnforceGlobalLayerPixelBudget(s_frameCounter);

        if (useAsyncStreaming || useBasemapOverlay) {
            stats.cachedTilesPending += GetAsyncStreamer().GetPendingLayerTileCount() +
                                        GetAsyncStreamer().GetPendingBasemapCount();
        }

        // Single-pass blit: 1 opaque CompositeTile DrawPixels call per visible [z/x/y] cell,
        // plus cartographic loading placeholder overlay for cells still streaming/rasterizing in the background.
        for (const guikit::OsmTileCell& cell : viewState.visibleTiles) {
            const TileCoordKey key{cell.id.z, cell.id.x, cell.id.y};
            const CompositeTile* comp = cache.FindCompositeTile(key, s_frameCounter);
            const bool hasPixels = (comp != nullptr && !comp->pixels.empty());
            const bool isComplete = (hasPixels && comp->isComplete);
            if (hasPixels) {
                scene.DrawPixels(
                    {comp->pixels, tileSizePx, tileSizePx},
                    cell.worldRect,
                    0.0f,
                    255
                );
            }
            if (!isComplete && cfg.showLoadingTilePlaceholders) {
                DrawLoadingTilePlaceholder(
                    scene,
                    cell,
                    zoom,
                    invZ,
                    darkChartSurface,
                    /*hasPreview=*/hasPixels,
                    s_frameCounter
                );
            }
        }
        rasterMs = static_cast<double>(keel::monotonic_us() - rasterPassStartUs) * 0.001;
    } else {
        // Direct immediate-mode vector pass (used when VectorTileCache is toggled off)
        struct OrderedDirectLayer {
            const geo::GeoLayer* layer{nullptr};
            keel::usize layerIdx{0};
            keel::i32 rank{90};
        };
        keel::Array<OrderedDirectLayer> orderedLayers;
        orderedLayers.reserve(layers.size());
        for (keel::usize li = 0; li < layers.size(); ++li) {
            const geo::GeoLayer& layer = layers[li];
            if (!layer.visible || layer.features.empty()) continue;
            if (cfg.frustumCulling && !layer.normBounds.Intersects(visNormBox)) continue;
            orderedLayers.push_back({&layer, li, CartographicLayerRank(layer)});
        }
        keel::sort(orderedLayers,
            [](const OrderedDirectLayer& a, const OrderedDirectLayer& b) noexcept {
                if (a.rank != b.rank) return a.rank < b.rank;
                return a.layerIdx < b.layerIdx;
            });

        keel::Array<keel::u32> visIndices;
        for (keel::i32 pass = 0; pass < 2; ++pass) {
            for (const OrderedDirectLayer& entry : orderedLayers) {
                const geo::GeoLayer& layer = *entry.layer;
                if (pass == 0 && layer.polygonFeatureCount == 0) continue;
                if (pass == 1 && layer.lineFeatureCount == 0 && layer.polygonFeatureCount == 0) continue;

                visIndices.clear();
                layer.spatialIndex.query_box(
                    keel::RTreeBox<float>::from_xywh(visNormBox.x, visNormBox.y, visNormBox.w, visNormBox.h),
                    [&](keel::u32 fi) { visIndices.push_back(fi); }
                );
                if (pass == 0 && layer.HasChartClass(geo::ChartFeatureClass::DepthArea) && visIndices.size() > 1) {
                    keel::sort(visIndices,
                        [&layer](keel::u32 a, keel::u32 b) noexcept {
                            const geo::GeoFeature& fa = layer.features[a];
                            const geo::GeoFeature& fb = layer.features[b];
                            const float da = !std::isnan(fa.minDepthMeters) ? fa.minDepthMeters
                                           : (!std::isnan(fa.depthMeters) ? fa.depthMeters : 10000.0f);
                            const float db = !std::isnan(fb.minDepthMeters) ? fb.minDepthMeters
                                           : (!std::isnan(fb.depthMeters) ? fb.depthMeters : 10000.0f);
                            if (da != db) return da > db;
                            return a < b;
                        });
                }

                for (keel::u32 fIdx : visIndices) {
                    if (fIdx >= layer.features.size()) continue;
                    const geo::GeoFeature& feat = layer.features[fIdx];
                    if (ShouldSkipFeatureAtZoom(feat, tileZoom, cfg)) continue;

                    const ResolvedFeatureStyle st = ResolveMarineFeatureStyle(layer, feat, cfg, /*bakeFillOpacity=*/true);
                    if (pass == 0 && feat.HasPolygons()) {
                        const bool wasLoaded = feat.geometryLoaded;
                        if (!feat.EnsureGeometryLoaded(s_frameCounter)) continue;
                        if (!wasLoaded) ++stats.pagedInFeaturesThisFrame;

                        auto& mutablePolys = const_cast<keel::Array<geo::GeoPolygon>&>(feat.polygons);
                        for (geo::GeoPolygon& poly : mutablePolys) {
                            if (st.renderFill && poly.triangles.empty() && poly.exteriorRing.size() >= 3) {
                                geo::PolygonTriangulator::TriangulatePolygon(poly);
                                feat.RefreshResidentHeapBytes();
                            }
                            if (st.renderFill && !poly.triangles.empty()) {
                                for (const geo::GeoTriangle& tri : poly.triangles) {
                                    scene.DrawTriangle(
                                        map.GeoToWorld(tri.a),
                                        map.GeoToWorld(tri.b),
                                        map.GeoToWorld(tri.c),
                                        st.fillColor
                                    );
                                    ++stats.renderedTriangles;
                                }
                            }
                            if (st.renderStroke) {
                                DrawWorldRing(map, scene, poly.exteriorRing, st.strokeColor, st.strokeWidthPx * invZ,
                                    minSegWorldSq, true, false, 0.0f, handleFill, stats.renderedVertices);
                                for (const auto& hole : poly.interiorRings) {
                                    DrawWorldRing(map, scene, hole, st.strokeColor, st.strokeWidthPx * invZ,
                                        minSegWorldSq, true, false, 0.0f, handleFill, stats.renderedVertices);
                                }
                            }
                        }
                    } else if (pass == 1 && feat.HasLines() && st.renderStroke) {
                        const bool wasLoaded = feat.geometryLoaded;
                        if (!feat.EnsureGeometryLoaded(s_frameCounter)) continue;
                        if (!wasLoaded) ++stats.pagedInFeaturesThisFrame;

                        for (const auto& line : feat.lines) {
                            DrawWorldRing(map, scene, line, st.strokeColor, st.strokeWidthPx * invZ,
                                minSegWorldSq, false, false, 0.0f, handleFill, stats.renderedVertices);
                        }
                    }
                }
            }
        }
    }

    // ========================================================================
    // 3. Pan-Invariant World-Space Decluttered Overlay Pass
    //    (Pre-Indexed Compact Lists: O(accepted) instead of O(totalFeatures))
    // ========================================================================
    EmitRenderBreadcrumb("GisRenderEngine::Stage3_WorldDeclutterOverlay", static_cast<keel::i32>(s_frameCounter), tileZoom);
    const keel::u64 declutterStartUs = keel::monotonic_us();
    WorldDeclutterCache& declutterCache = GetDeclutterCache();
    declutterCache.Sync(layers, cfg, tileZoom, viewState.crs, paddedVisNormBox);
    stats.declutteredLabels = declutterCache.GetTotalDecluttered();
    const double declutterMs = static_cast<double>(keel::monotonic_us() - declutterStartUs) * 0.001;

    if constexpr (kEnableFileDebugLog) {
        if (totalInvalidatedTilesThisFrame > 0 || stats.cachedTilesRendered > 0 || syncMs >= 1.0 || declutterMs >= 1.0) {
            const double totalFrameMs = static_cast<double>(keel::monotonic_us() - frameRenderStartUs) * 0.001;
            const auto logLine = keel::format(
                "[FrameTiles] frame=%llu z=%d vis=[%.3f..%.3fE, %.3f..%.3fN] visTiles=%zu hit=%zu new=%zu pend=%zu invalidated=%zu activeFeats=%zu pagedIn=%zu ms=%.2f (sync=%.2f raster=%.2f declutter=%.2f)",
                s_frameCounter,
                tileZoom,
                keel::min(visBounds.nw.lon, visBounds.se.lon),
                keel::max(visBounds.nw.lon, visBounds.se.lon),
                keel::min(visBounds.nw.lat, visBounds.se.lat),
                keel::max(visBounds.nw.lat, visBounds.se.lat),
                numVisTiles,
                stats.cachedTilesHit,
                stats.cachedTilesRendered,
                stats.cachedTilesPending,
                totalInvalidatedTilesThisFrame,
                activeWindowFeatures,
                stats.pagedInFeaturesThisFrame,
                totalFrameMs,
                syncMs,
                rasterMs,
                declutterMs
            );
            AppendRenderDebugLog(logLine);
        }
    }

    // Pass 3A: Render Accepted Point Symbols (Buoys/Beacons, Underwater Hazards, Generic Points)
    for (const auto& item : declutterCache.GetAcceptedSymbols()) {
        if (item.layerIdx >= layers.size()) continue;
        const geo::GeoLayer& layer = layers[item.layerIdx];
        if (!layer.visible || item.featIdx >= layer.features.size()) continue;
        const geo::GeoFeature& feat = layer.features[item.featIdx];
        if (feat.points.empty() || feat.normPoints.empty()) continue;
        if (cfg.frustumCulling && !paddedVisNormBox.Contains(feat.normPoints[0])) {
            ++stats.culledFeatures;
            continue;
        }
        ++stats.renderedFeatures;

        const float ptScale = keel::clamp(layer.style.pointRadiusPx / 6.5f, 0.35f, 3.5f);
        const bool customCol = layer.style.useCustomColor;
        const bool isSel = (feat.uid == ioSelectedFeatureUid);
        const bool asSeamark = layer.style.renderAsSeamark;

        if (feat.chartClass == geo::ChartFeatureClass::BuoyOrBeacon) {
            const guikit::Color buoyCol = isSel
                ? selAccent
                : (customCol ? layer.style.primaryColor : ResolveIalaBuoyColor(feat.lightOrColor.as_ref(), guikit::Color(232, 121, 249, 255)));
            const bool isLitMark = asSeamark && (feat.hasLight || feat.featureCode == "LIGHTS");
            for (const guikit::GeoCoord& pt : feat.points) {
                if (isLitMark && tileZoom >= 12) {
                    map.DrawGeoCircle(pt, 8.5f * ptScale, WithAlpha(buoyCol, 0.20f), WithAlpha(buoyCol, 0.70f), 1.1f);
                }
                map.DrawGeoCircle(pt, (isSel ? 6.0f : 4.6f) * ptScale, buoyCol, handleFill, 1.5f);
                if (asSeamark) {
                    map.DrawGeoCircle(pt, 1.6f * ptScale, handleFill);
                }
                ++stats.renderedVertices;
            }
        } else if (feat.chartClass == geo::ChartFeatureClass::UnderwaterHazard) {
            if (tileZoom < 11 && !isSel) continue;
            const guikit::Color hazFill = customCol
                ? WithAlpha(layer.style.primaryColor, 0.78f)
                : (isNightPalette
                    ? guikit::Color(14, 116, 144, 190)
                    : guikit::Color(125, 211, 252, 210));
            const guikit::Color hazCross = isSel
                ? selAccent
                : (darkChartSurface ? guikit::Color(248, 113, 113, 245) : guikit::Color(15, 23, 42, 245));
            for (const guikit::GeoCoord& pt : feat.points) {
                map.DrawGeoCircle(pt, 5.2f * ptScale, hazFill, asSeamark ? hazCross : handleFill, 1.1f);
                if (asSeamark) {
                    const guikit::Vec2 wPt = map.GeoToWorld(pt);
                    const float armW = 3.2f * ptScale * invZ;
                    scene.DrawLine({wPt.x - armW, wPt.y}, {wPt.x + armW, wPt.y}, hazCross, 1.3f * invZ);
                    scene.DrawLine({wPt.x, wPt.y - armW}, {wPt.x, wPt.y + armW}, hazCross, 1.3f * invZ);
                }
                ++stats.renderedVertices;
            }
        } else if (feat.chartClass == geo::ChartFeatureClass::BuiltUpOrStructure &&
                   !feat.HasPolygons() && !feat.HasLines()) {
            const guikit::Color structCol = isSel
                ? selAccent
                : (customCol
                    ? layer.style.primaryColor
                    : (isNightPalette ? guikit::Color(148, 128, 96, 235) : guikit::Color(92, 73, 46, 235)));
            for (const guikit::GeoCoord& pt : feat.points) {
                map.DrawGeoCircle(pt, (isSel ? 4.8f : 3.2f) * ptScale, structCol, handleFill, 1.2f);
                ++stats.renderedVertices;
            }
        } else if (feat.chartClass == geo::ChartFeatureClass::LandArea &&
                   !feat.HasPolygons() && !feat.HasLines()) {
            const guikit::Color landPtCol = isSel
                ? selAccent
                : (customCol
                    ? layer.style.primaryColor
                    : (isNightPalette ? guikit::Color(168, 142, 102, 225) : guikit::Color(120, 88, 42, 230)));
            for (const guikit::GeoCoord& pt : feat.points) {
                map.DrawGeoCircle(pt, (isSel ? 4.4f : 2.6f) * ptScale, landPtCol, handleFill, 1.1f);
                ++stats.renderedVertices;
            }
        } else if (feat.chartClass == geo::ChartFeatureClass::Generic && !feat.HasPolygons() && !feat.HasLines()) {
            const guikit::Color ptCol = isSel ? selAccent : layer.style.primaryColor;
            for (const guikit::GeoCoord& pt : feat.points) {
                if (asSeamark) {
                    auto smItem = map.DrawSeamark(pt, {
                        .id = feat.id.as_ref(),
                        .title = layer.style.showLabels ? feat.name.as_ref() : keel::StringRef{},
                        .category = layer.name.as_ref(),
                        .radiusPx = layer.style.pointRadiusPx,
                        .showLightSector = true,
                        .color = ptCol,
                        .selectable = true
                    });
                    if (smItem.Clicked()) {
                        stats.clickedFeatureUid = feat.uid;
                        ioSelectedFeatureUid = feat.uid;
                    }
                } else {
                    map.DrawGeoCircle(pt, layer.style.pointRadiusPx, WithAlpha(ptCol, 0.85f), handleFill, 1.5f);
                }
                ++stats.renderedVertices;
            }
        }
    }

    // Pass 3B: Render Accepted Pan-Invariant Text Labels (`labelRu` / `labelEn` / Named Features)
    if (cfg.showFeatureCallouts) {
        keel::String labelScratch;
        for (const auto& item : declutterCache.GetAcceptedLabels()) {
            if (item.layerIdx >= layers.size()) continue;
            const geo::GeoLayer& layer = layers[item.layerIdx];
            if (!layer.visible || !layer.style.showLabels || item.featIdx >= layer.features.size()) continue;
            const geo::GeoFeature& feat = layer.features[item.featIdx];
            if (cfg.frustumCulling && !paddedVisNormBox.Contains(feat.normCentroid)) {
                continue;
            }

            keel::usize charCount = 0;
            const keel::StringRef labelText = FormatMapCalloutLabel(feat, cfg, labelScratch, &charCount);
            if (labelText.empty()) continue;

            const bool isSel = (feat.uid == ioSelectedFeatureUid);
            const float estWidthPx = keel::clamp(static_cast<float>(charCount) * 6.0f + 10.0f, 16.0f, 640.0f);
            const float offsetX = !feat.points.empty() ? 8.0f : (-estWidthPx * 0.4f);
            const float offsetY = -7.0f;

            const guikit::Color txtCol = isSel ? selAccent : labelTextCol;
            map.DrawGeoText(
                labelText,
                feat.centroid,
                {offsetX, offsetY},
                isSel ? 12.0f : 10.5f,
                txtCol,
                isSel ? guikit::FontStyle::Bold : guikit::FontStyle::Regular
            );
            ++stats.renderedFeatures;
        }
    }

    // Pass 3C: Render Accepted Depth Soundings (`SOUNDG`) with Navionics Subscript Tenths
    if (cfg.showSoundings && tileZoom >= 11) {
        const guikit::Color shallowSndCol = darkChartSurface
            ? guikit::Color(125, 211, 252, 235)
            : guikit::Color(12, 74, 110, 235);
        const guikit::Color deepSndCol = darkChartSurface
            ? guikit::Color(148, 163, 184, 210)
            : guikit::Color(71, 85, 105, 215);

        for (const auto& item : declutterCache.GetAcceptedSoundings()) {
            if (item.layerIdx >= layers.size()) continue;
            const geo::GeoLayer& layer = layers[item.layerIdx];
            if (!layer.visible || item.featIdx >= layer.features.size()) continue;
            const geo::GeoFeature& feat = layer.features[item.featIdx];
            if (feat.points.empty() || feat.normPoints.empty()) continue;
            if (cfg.frustumCulling && !paddedVisNormBox.Contains(feat.normPoints[0])) {
                ++stats.culledFeatures;
                continue;
            }

            const guikit::GeoCoord pt = feat.points[0];
            const float d = keel::max(0.0f, feat.depthMeters);
            const keel::i32 wholeM = keel::floor_i32(d + 0.04f);
            const keel::i32 tenths = keel::round_i32((d - static_cast<float>(wholeM)) * 10.0f);
            const guikit::Color sndCol = (d <= cfg.safetyContourM) ? shallowSndCol : deepSndCol;

            char wholeBuf[16]{};
            const keel::StringRef wholeRef = keel::format_buf(wholeBuf, "%d", wholeM);

            if (d < 12.0f && tenths > 0 && tenths < 10) {
                map.DrawGeoText(wholeRef, pt, {-5.0f, -5.0f}, 9.5f, sndCol, guikit::FontStyle::Bold);
                char tenthBuf[8]{};
                const keel::StringRef tenthRef = keel::format_buf(tenthBuf, "%d", tenths);
                const float subOffX = (wholeM >= 10) ? 6.5f : 1.5f;
                map.DrawGeoText(tenthRef, pt, {subOffX, -2.0f}, 7.8f, sndCol, guikit::FontStyle::Regular);
            } else {
                map.DrawGeoText(wholeRef, pt, {-4.0f, -5.0f}, 9.5f, sndCol, guikit::FontStyle::Regular);
            }

            ++stats.renderedFeatures;
            ++stats.renderedVertices;
        }
    }

    // ========================================================================
    // 4. Interactive Hovered & Selected Feature Vector Highlight Overlay
    // ========================================================================
    if (!isPanning && hoveredMatch.feat && hoveredMatch.feat != selectedFeat) {
        RenderHighlightedFeatureOverlay(
            map, scene, *hoveredMatch.feat,
            hovAccent, handleFill, invZ, minSegWorldSq,
            /*drawHandles=*/false, s_frameCounter,
            stats.renderedVertices, &stats.pagedInFeaturesThisFrame
        );
    }

    if (selectedFeat && selectedLayer) {
        RenderHighlightedFeatureOverlay(
            map, scene, *selectedFeat,
            selAccent, handleFill, invZ, minSegWorldSq,
            /*drawHandles=*/cfg.showVertexDotsOnSelect, s_frameCounter,
            stats.renderedVertices, &stats.pagedInFeaturesThisFrame
        );
    }

    // ========================================================================
    // 5. Out-of-Core LRU Feature RAM Budget Eviction (Throttled when Idle)
    // ========================================================================
    static keel::usize s_cumulativeEvicted = 0;
    static keel::usize s_lastResidentFeatures = 0;
    static keel::usize s_lastResidentBytes = 0;
    static keel::usize s_lastMaxResidentBytes = 0;
    static keel::u64 s_lastEvictCheckFrame = 0;
    static bool s_pendingBudgetCheck = false;
    if (stats.pagedInFeaturesThisFrame > 0) {
        s_pendingBudgetCheck = true;
    }

    const float clampedBudgetMb = keel::clamp(cfg.featureCacheBudgetMb, 1.0f, 1024.0f);
    const keel::usize maxResidentBytes = static_cast<keel::usize>(clampedBudgetMb * 1024.0f * 1024.0f);
    const bool budgetChanged = (s_lastMaxResidentBytes != maxResidentBytes);
    const bool cooldownElapsed = (s_frameCounter - s_lastEvictCheckFrame >= 45u);
    if (!isPanning && stats.cachedTilesPending == 0) {
        for (const geo::GeoLayer& layer : layers) {
            if (layer.pager) continue;
            for (const geo::GeoFeature& feat : layer.features) {
                if (feat.geometryLoaded && feat.sharedGeometry) {
                    feat.sharedGeometry.reset();
                }
            }
        }
    }
    if (!isPanning && stats.cachedTilesPending == 0 &&
        (budgetChanged || s_lastResidentFeatures == 0 || (s_pendingBudgetCheck && cooldownElapsed))) {
        s_lastMaxResidentBytes = maxResidentBytes;
        s_lastEvictCheckFrame = s_frameCounter;
        s_pendingBudgetCheck = false;
        s_cumulativeEvicted += EnforceFeatureRamBudget(
            layers,
            maxResidentBytes,
            s_frameCounter,
            &s_lastResidentFeatures,
            &s_lastResidentBytes
        );
    }
    stats.residentFeatureCount = s_lastResidentFeatures;
    stats.residentFeatureBytes = s_lastResidentBytes;
    stats.evictedFeaturesTotal = s_cumulativeEvicted;

    return stats;
}

} // namespace guikit::geo
