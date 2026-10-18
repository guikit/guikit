#include "guikit/panels/MapView.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include "guikit/Context.hpp"

namespace guikit {

namespace {

struct MapViewInternalState {
    bool cameraInitialized{false};
    MapCrs lastCrs{MapCrs::EPSG3857_WebMercator};
    GeoCoord lastCenterGeo{54.1825, 7.8853};
};

void RenderMapBackgroundLayers(
    SceneScope& scene,
    const MapViewOptions& opts,
    const MapViewState& view,
    bool lightMode
) {
    const Color tileEvenFill = lightMode ? Color::Rgba(238, 245, 252, 95) : Color::Rgba(18, 30, 46, 95);
    const Color tileOddFill  = lightMode ? Color::Rgba(226, 237, 248, 95) : Color::Rgba(24, 38, 58, 95);

    // Base OSM [z/x/y] Tile Pyramid Background & Optional Raster Tile Source
    for (const OsmTileCell& cell : view.visibleTiles) {
        const Rect& r = cell.worldRect;
        const bool isCheckerOdd = ((cell.id.x + cell.id.y) & 1) != 0;
        const Color cellFill = isCheckerOdd ? tileOddFill : tileEvenFill;

        bool drewBaseTile = false;
        if (opts.tileSource != nullptr) {
            const std::string_view baseUri = opts.tileSource(cell.id, 0, opts.tileSourceUserData);
            if (!baseUri.empty()) {
                scene.DrawImage(baseUri, r);
                drewBaseTile = true;
            }
            const std::string_view overlayUri = opts.tileSource(cell.id, 1, opts.tileSourceUserData);
            if (!overlayUri.empty()) {
                scene.DrawImage(overlayUri, r);
            }
        }

        if (!drewBaseTile) {
            scene.DrawRect(r, cellFill);
        }
    }
}

} // namespace

MapViewScope::MapViewScope(
    SceneScope&& scene,
    const MapViewOptions& opts,
    const MapViewState& viewState,
    bool lightMode
) noexcept
    : m_scene(std::move(scene)),
      m_opts(opts),
      m_view(viewState),
      m_lightMode(lightMode),
      m_active(static_cast<bool>(m_scene)) {}

MapViewScope::MapViewScope(MapViewScope&& other) noexcept
    : m_scene(std::move(other.m_scene)),
      m_opts(other.m_opts),
      m_view(std::move(other.m_view)),
      m_lightMode(other.m_lightMode),
      m_active(other.m_active) {
    other.m_active = false;
}

MapViewScope::~MapViewScope() noexcept {
    if (m_active && static_cast<bool>(m_scene)) {
        RenderForegroundOverlays();
    }
}

Vec2 MapViewScope::GeoToWorld(GeoCoord coord) const noexcept {
    return MapGrid::GeoToWorld(coord, m_opts.worldSize, m_opts.crs);
}

GeoCoord MapViewScope::WorldToGeo(Vec2 worldPt) const noexcept {
    return MapGrid::WorldToGeo(worldPt, m_opts.worldSize, m_opts.crs);
}

Vec2 MapViewScope::GeoToScreen(GeoCoord coord) const noexcept {
    return m_scene.WorldToScreen(GeoToWorld(coord));
}

GeoCoord MapViewScope::ScreenToGeo(Vec2 screenPt) const noexcept {
    return WorldToGeo(m_scene.ScreenToWorld(screenPt));
}

void MapViewScope::CenterOnGeo(GeoCoord coord, std::optional<float> tileZoom) noexcept {
    std::optional<float> sceneZoom{};
    if (tileZoom.has_value()) {
        sceneZoom = MapGrid::TileZoomToSceneZoom(*tileZoom, m_opts.baseZoom);
    }
    m_scene.CenterOn(GeoToWorld(coord), sceneZoom);
}

void MapViewScope::ResetView() noexcept {
    CenterOnGeo(m_opts.initialCenter, m_opts.initialTileZoom);
}

SceneItemScope MapViewScope::GeoItem(
    GeoCoord coord,
    float hitRadiusPx,
    const SceneItemOptions& opts
) {
    const float invZ = 1.0f / std::max(0.0001f, m_scene.GetZoom());
    const Vec2 wPos = GeoToWorld(coord);
    const float r = std::max(2.0f, hitRadiusPx) * invZ;
    SceneItemOptions itemOpts = opts;
    if (!itemOpts.hitRect.has_value()) {
        itemOpts.hitRect = Rect(wPos.x - r, wPos.y - r, r * 2.0f, r * 2.0f);
    }
    return m_scene.Item(itemOpts);
}

void MapViewScope::DrawGeoLine(
    GeoCoord from,
    GeoCoord to,
    Color color,
    float strokeWidthPx
) {
    const float invZ = 1.0f / std::max(0.0001f, m_scene.GetZoom());
    m_scene.DrawLine(GeoToWorld(from), GeoToWorld(to), color, strokeWidthPx * invZ);
}

void MapViewScope::DrawGeoPolyline(
    std::span<const GeoCoord> coords,
    Color color,
    float strokeWidthPx,
    bool closed
) {
    if (coords.size() < 2) return;
    const float invZ = 1.0f / std::max(0.0001f, m_scene.GetZoom());
    const float w = strokeWidthPx * invZ;
    for (size_t i = 0; i + 1 < coords.size(); ++i) {
        m_scene.DrawLine(GeoToWorld(coords[i]), GeoToWorld(coords[i + 1]), color, w);
    }
    if (closed && coords.size() >= 3) {
        m_scene.DrawLine(GeoToWorld(coords.back()), GeoToWorld(coords.front()), color, w);
    }
}

void MapViewScope::DrawGeoCircle(
    GeoCoord center,
    float radiusPx,
    Color fill,
    Color stroke,
    float strokeWidthPx
) {
    const float invZ = 1.0f / std::max(0.0001f, m_scene.GetZoom());
    m_scene.DrawCircle(GeoToWorld(center), radiusPx * invZ, fill, stroke, strokeWidthPx * invZ);
}

void MapViewScope::DrawGeoArc(
    GeoCoord center,
    float radiusPx,
    float startAngleDeg,
    float sweepAngleDeg,
    Color stroke,
    float strokeWidthPx,
    int segments
) {
    const float invZ = 1.0f / std::max(0.0001f, m_scene.GetZoom());
    m_scene.DrawArc(
        GeoToWorld(center),
        radiusPx * invZ,
        startAngleDeg,
        sweepAngleDeg,
        stroke,
        strokeWidthPx * invZ,
        segments
    );
}

void MapViewScope::DrawGeoText(
    std::string_view text,
    GeoCoord coord,
    Vec2 offsetPx,
    float fontSize,
    Color color,
    FontStyle style
) {
    const float invZ = 1.0f / std::max(0.0001f, m_scene.GetZoom());
    const Vec2 wPos = GeoToWorld(coord);
    m_scene.DrawText(
        text,
        {wPos.x + offsetPx.x * invZ, wPos.y + offsetPx.y * invZ},
        fontSize,
        color,
        style,
        /*scaleWithZoom=*/false
    );
}

SceneItemScope MapViewScope::DrawSeamark(
    GeoCoord coord,
    const MapSeamarkOptions& opts
) {
    const float invZ = 1.0f / std::max(0.0001f, m_scene.GetZoom());
    const Vec2 wPos = GeoToWorld(coord);
    const float r = std::max(2.0f, opts.radiusPx) * invZ;
    const Rect hitBox(wPos.x - r * 2.0f, wPos.y - r * 2.0f, r * 4.0f, r * 4.0f);

    SceneItemScope item = m_scene.Item({
        .id = opts.id,
        .title = opts.title,
        .category = opts.category,
        .hitRect = hitBox,
        .selectable = opts.selectable
    });

    const Color seamarkMagenta = m_lightMode ? Color::Rgba(185, 28, 140, 235) : Color::Rgba(232, 121, 249, 240);
    const Color seamarkCyan    = m_lightMode ? Color::Rgba(2, 132, 199, 230)  : Color::Rgba(56, 189, 248, 235);
    const Color baseCol = opts.color.value_or(seamarkMagenta);
    const Color markCol = (item.IsSelected() || item.IsHovered()) ? seamarkCyan : baseCol;

    if (opts.showLightSector) {
        m_scene.DrawArc(wPos, r * 1.9f, opts.sectorStartDeg, opts.sectorSweepDeg, markCol, 1.8f * invZ, 16);
    }

    // IALA diamond buoy / beacon silhouette
    m_scene.DrawTriangle(
        {wPos.x, wPos.y - r * 1.25f},
        {wPos.x - r * 0.9f, wPos.y},
        {wPos.x + r * 0.9f, wPos.y},
        markCol.WithAlpha(70),
        markCol,
        1.5f * invZ
    );
    m_scene.DrawTriangle(
        {wPos.x, wPos.y + r * 1.25f},
        {wPos.x - r * 0.9f, wPos.y},
        {wPos.x + r * 0.9f, wPos.y},
        markCol.WithAlpha(70),
        markCol,
        1.5f * invZ
    );
    m_scene.DrawCircle(wPos, 2.5f * invZ, markCol);

    if (!opts.title.empty()) {
        m_scene.DrawText(
            opts.title,
            {wPos.x + (opts.radiusPx + 4.0f) * invZ, wPos.y - 6.0f * invZ},
            11.0f,
            markCol,
            FontStyle::Bold,
            /*scaleWithZoom=*/false
        );
    }

    return item;
}

void MapViewScope::RenderForegroundOverlays() {
    const float z = std::max(0.0001f, m_scene.GetZoom());
    const float invZ = 1.0f / z;
    const Rect visWorld = m_scene.GetVisibleWorldRect();

    const Color seamarkMagenta = m_lightMode ? Color::Rgba(185, 28, 140, 235) : Color::Rgba(232, 121, 249, 240);
    const Color seamarkCyan    = m_lightMode ? Color::Rgba(2, 132, 199, 230)  : Color::Rgba(56, 189, 248, 235);

    // 1. OpenSeaMap Nautical Graticule (Degrees & Nautical Arc-Minutes, 1' = 1 NM) — rendered over chart tiles
    if (m_opts.showNauticalGraticule) {
        const Color gratMinorCol = m_lightMode ? Color::Rgba(15, 118, 110, 105)  : Color::Rgba(20, 184, 166, 110);
        const Color gratMajorCol = m_lightMode ? Color::Rgba(13, 148, 136, 175)  : Color::Rgba(45, 212, 191, 175);
        const Color gratZeroCol  = m_lightMode ? Color::Rgba(220, 38, 38, 200)   : Color::Rgba(248, 113, 113, 205);

        const float vy0 = std::clamp(visWorld.y, 0.0f, m_opts.worldSize);
        const float vy1 = std::clamp(visWorld.Bottom(), 0.0f, m_opts.worldSize);
        const float vx0 = std::clamp(visWorld.x, 0.0f, m_opts.worldSize);
        const float vx1 = std::clamp(visWorld.Right(), 0.0f, m_opts.worldSize);

        if (MapGrid::IsCylindricalCrs(m_opts.crs)) {
            for (const GraticuleLine& m : m_view.meridians) {
                const Color col = m.isZero ? gratZeroCol : (m.isMajor ? gratMajorCol : gratMinorCol);
                const float w = (m.isZero ? 1.8f : (m.isMajor ? 1.25f : 0.85f)) * invZ;
                m_scene.DrawLine({m.worldPos, vy0}, {m.worldPos, vy1}, col, w);
            }

            for (const GraticuleLine& p : m_view.parallels) {
                const Color col = p.isZero ? gratZeroCol : (p.isMajor ? gratMajorCol : gratMinorCol);
                const float w = (p.isZero ? 1.8f : (p.isMajor ? 1.25f : 0.85f)) * invZ;
                m_scene.DrawLine({vx0, p.worldPos}, {vx1, p.worldPos}, col, w);
            }
        } else {
            // Curved / converging meridians & parallels for Transverse Mercator (UTM / GSK-2011 / SK-42) and Polar Stereographic
            const double minLat = std::clamp(m_view.visibleBounds.se.lat, -89.9, 89.9);
            const double maxLat = std::clamp(m_view.visibleBounds.nw.lat, -89.9, 89.9);
            const double minLon = std::clamp(m_view.visibleBounds.nw.lon, -180.0, 180.0);
            const double maxLon = std::clamp(m_view.visibleBounds.se.lon, -180.0, 180.0);
            constexpr int kMeridianSegs = 12;
            constexpr int kParallelSegs = 18;

            for (const GraticuleLine& m : m_view.meridians) {
                const Color col = m.isZero ? gratZeroCol : (m.isMajor ? gratMajorCol : gratMinorCol);
                const float w = (m.isZero ? 1.8f : (m.isMajor ? 1.25f : 0.85f)) * invZ;
                Vec2 prevW = GeoToWorld({minLat, m.degrees});
                for (int s = 1; s <= kMeridianSegs; ++s) {
                    const double t = static_cast<double>(s) / static_cast<double>(kMeridianSegs);
                    const double lat = minLat + t * (maxLat - minLat);
                    const Vec2 currW = GeoToWorld({lat, m.degrees});
                    m_scene.DrawLine(prevW, currW, col, w);
                    prevW = currW;
                }
            }

            for (const GraticuleLine& p : m_view.parallels) {
                const Color col = p.isZero ? gratZeroCol : (p.isMajor ? gratMajorCol : gratMinorCol);
                const float w = (p.isZero ? 1.8f : (p.isMajor ? 1.25f : 0.85f)) * invZ;
                Vec2 prevW = GeoToWorld({p.degrees, minLon});
                for (int s = 1; s <= kParallelSegs; ++s) {
                    const double t = static_cast<double>(s) / static_cast<double>(kParallelSegs);
                    const double lon = minLon + t * (maxLon - minLon);
                    const Vec2 currW = GeoToWorld({p.degrees, lon});
                    m_scene.DrawLine(prevW, currW, col, w);
                    prevW = currW;
                }
            }
        }
    }

    // 2. OSM [z/x/y] Tile Pyramid Wireframe & Coordinates — rendered over chart tiles when enabled
    if (m_opts.showTileGrid) {
        const Color tileBorderCol = m_lightMode ? Color::Rgba(59, 130, 246, 150)  : Color::Rgba(56, 189, 248, 145);
        const Color subTileCol    = m_lightMode ? Color::Rgba(59, 130, 246, 65)   : Color::Rgba(56, 189, 248, 60);
        const Color tileIdTextCol = m_lightMode ? Color::Rgba(30, 58, 138, 215)   : Color::Rgba(125, 211, 252, 215);
        const Color tileMetaCol   = m_lightMode ? Color::Rgba(51, 65, 85, 185)    : Color::Rgba(148, 163, 184, 185);

        for (const OsmTileCell& cell : m_view.visibleTiles) {
            const Rect& r = cell.worldRect;
            const float screenTileW = r.w * z;

            if (m_opts.showSubTileQuadtree && screenTileW >= 110.0f) {
                const float midX = r.x + r.w * 0.5f;
                const float midY = r.y + r.h * 0.5f;
                m_scene.DrawLine({midX, r.y}, {midX, r.Bottom()}, subTileCol, 1.0f * invZ);
                m_scene.DrawLine({r.x, midY}, {r.Right(), midY}, subTileCol, 1.0f * invZ);
            }

            m_scene.DrawRect(r, Color::Transparent(), 0.0f, tileBorderCol, 1.0f * invZ);

            if (screenTileW >= 64.0f) {
                char labelBuf[48]{};
                std::snprintf(labelBuf, sizeof(labelBuf), "z=%d  [%d, %d]", cell.id.z, cell.id.x, cell.id.y);
                m_scene.DrawTextBox(
                    labelBuf,
                    Rect(r.x, r.y + r.h * 0.34f, r.w, r.h * 0.18f),
                    12.0f,
                    tileIdTextCol,
                    0.5f,
                    0.5f,
                    FontStyle::Monospace,
                    /*scaleWithZoom=*/false
                );
            }

            if (screenTileW >= 150.0f) {
                char uriBuf[64]{};
                std::snprintf(uriBuf, sizeof(uriBuf), "/tile/%d/%d/%d.png", cell.id.z, cell.id.x, cell.id.y);
                m_scene.DrawTextBox(
                    uriBuf,
                    Rect(r.x, r.y + r.h * 0.52f, r.w, r.h * 0.14f),
                    10.5f,
                    tileMetaCol,
                    0.5f,
                    0.5f,
                    FontStyle::Monospace,
                    /*scaleWithZoom=*/false
                );

                const std::string nwText = MapGrid::FormatCoord(cell.geoBounds.nw, m_opts.coordFormat, m_opts.crs);
                m_scene.DrawText(
                    nwText,
                    {r.x + 6.0f * invZ, r.y + 6.0f * invZ},
                    9.5f,
                    tileMetaCol,
                    FontStyle::Monospace,
                    /*scaleWithZoom=*/false
                );
            }
        }
    }

    // 3. OpenSeaMap Nautical Border Rulers (Top Longitude & Left Latitude Bars)
    if (m_opts.showBorderRulers) {
        const float rulerThick = 20.0f * invZ;
        const float leftBarW = rulerThick * 3.9f;
        const float topRulerStartX = visWorld.x + leftBarW;
        const float leftRulerStartY = visWorld.y + rulerThick;

        const Color rulerBg     = m_lightMode ? Color::Rgba(248, 250, 252, 236) : Color::Rgba(11, 18, 30, 236);
        const Color rulerBorder = m_lightMode ? Color::Rgba(148, 163, 184, 200) : Color::Rgba(51, 65, 85, 210);
        const Color rulerText   = m_lightMode ? Color::Rgba(15, 23, 42, 240)    : Color::Rgba(226, 232, 240, 240);
        const Color tickAccent  = m_lightMode ? Color::Rgba(13, 148, 136, 220)  : Color::Rgba(45, 212, 191, 220);

        const Rect topBar(topRulerStartX, visWorld.y, std::max(0.0f, visWorld.w - leftBarW), rulerThick);
        const Rect leftBar(visWorld.x, leftRulerStartY, leftBarW, std::max(0.0f, visWorld.h - rulerThick));
        m_scene.DrawRect(topBar, rulerBg);
        m_scene.DrawRect(leftBar, rulerBg);

        for (size_t i = 0; i + 1 < m_view.meridians.size(); ++i) {
            const GraticuleLine& m = m_view.meridians[i];
            if ((std::abs(m.stepIndex) & 1) == 0) {
                const float nextX = m_view.meridians[i + 1].worldPos;
                const float x0 = std::max(topRulerStartX, std::min(m.worldPos, nextX));
                const float x1 = std::min(visWorld.Right(), std::max(m.worldPos, nextX));
                if (x1 > x0) {
                    m_scene.DrawRect(
                        Rect(x0, visWorld.y + rulerThick - 4.0f * invZ, x1 - x0, 4.0f * invZ),
                        tickAccent.WithAlpha(95)
                    );
                }
            }
        }

        for (size_t i = 0; i + 1 < m_view.parallels.size(); ++i) {
            const GraticuleLine& p = m_view.parallels[i];
            if ((std::abs(p.stepIndex) & 1) == 0) {
                const float nextY = m_view.parallels[i + 1].worldPos;
                const float y0 = std::max(leftRulerStartY, std::min(p.worldPos, nextY));
                const float y1 = std::min(visWorld.Bottom(), std::max(p.worldPos, nextY));
                if (y1 > y0) {
                    m_scene.DrawRect(
                        Rect(leftBar.Right() - 4.0f * invZ, y0, 4.0f * invZ, y1 - y0),
                        tickAccent.WithAlpha(95)
                    );
                }
            }
        }

        m_scene.DrawLine({topRulerStartX, visWorld.y + rulerThick}, {visWorld.Right(), visWorld.y + rulerThick}, rulerBorder, 1.0f * invZ);
        m_scene.DrawLine({leftBar.Right(), leftBar.y}, {leftBar.Right(), leftBar.Bottom()}, rulerBorder, 1.0f * invZ);

        for (const GraticuleLine& m : m_view.meridians) {
            if (m.worldPos >= topRulerStartX && m.worldPos <= visWorld.Right()) {
                m_scene.DrawLine({m.worldPos, visWorld.y}, {m.worldPos, visWorld.y + rulerThick}, tickAccent, 1.3f * invZ);
            }
        }
        for (const GraticuleLine& p : m_view.parallels) {
            if (p.worldPos >= leftRulerStartY && p.worldPos <= visWorld.Bottom()) {
                m_scene.DrawLine({leftBar.x, p.worldPos}, {leftBar.Right(), p.worldPos}, tickAccent, 1.3f * invZ);
            }
        }

        m_scene.DrawRect(Rect(visWorld.x, visWorld.y, leftBarW, rulerThick), rulerBg, 0.0f, rulerBorder, 1.0f * invZ);

        for (const GraticuleLine& m : m_view.meridians) {
            if (m.worldPos >= topRulerStartX && m.worldPos + 44.0f * invZ <= visWorld.Right()) {
                const std::string lbl = MapGrid::FormatGraticuleTick(m.degrees, /*isLat=*/false, m_view.graticuleStepArcMin);
                m_scene.DrawText(lbl, {m.worldPos + 4.0f * invZ, visWorld.y + 3.0f * invZ}, 10.0f, rulerText, FontStyle::Monospace, false);
            }
        }
        for (const GraticuleLine& p : m_view.parallels) {
            if (p.worldPos >= leftRulerStartY + 14.0f * invZ && p.worldPos <= visWorld.Bottom()) {
                const std::string lbl = MapGrid::FormatGraticuleTick(p.degrees, /*isLat=*/true, m_view.graticuleStepArcMin);
                m_scene.DrawText(lbl, {leftBar.x + 4.0f * invZ, p.worldPos - 13.0f * invZ}, 10.0f, rulerText, FontStyle::Monospace, false);
            }
        }

        char stepBuf[32]{};
        if (m_view.graticuleStepArcMin >= 60.0) {
            std::snprintf(stepBuf, sizeof(stepBuf), "Grid %.0f°", m_view.graticuleStepArcMin / 60.0);
        } else {
            std::snprintf(stepBuf, sizeof(stepBuf), "Grid %.1f' NM", m_view.graticuleStepArcMin);
        }
        m_scene.DrawText(stepBuf, {visWorld.x + 4.0f * invZ, visWorld.y + 3.0f * invZ}, 10.0f, tickAccent, FontStyle::Bold, false);
    }

    // 2. Dual Scale Bar Overlay (Nautical Miles NM/cab + Metric km/m)
    if (m_opts.showScaleBarOverlay) {
        const float leftOffset = m_opts.showBorderRulers ? 90.0f : 14.0f;
        const float boxW = 240.0f * invZ;
        const float boxH = 54.0f * invZ;
        const float boxX = visWorld.x + leftOffset * invZ;
        const float boxY = visWorld.Bottom() - (boxH + 14.0f * invZ);

        const Color hudBg     = m_lightMode ? Color::Rgba(255, 255, 255, 232) : Color::Rgba(11, 18, 30, 232);
        const Color hudBorder = m_lightMode ? Color::Rgba(148, 163, 184, 210) : Color::Rgba(51, 65, 85, 220);
        const Color hudText   = m_lightMode ? Color::Rgba(15, 23, 42, 245)    : Color::Rgba(226, 232, 240, 245);

        m_scene.DrawRect(Rect(boxX, boxY, boxW, boxH), hudBg, 5.0f * invZ, hudBorder, 1.0f * invZ);

        const float nmLenWorld = m_view.nauticalScale.screenPixels * invZ;
        const float nmY = boxY + 18.0f * invZ;
        const float barStartX = boxX + 10.0f * invZ;
        m_scene.DrawLine({barStartX, nmY}, {barStartX + nmLenWorld, nmY}, seamarkMagenta, 2.2f * invZ);
        m_scene.DrawLine({barStartX, nmY - 4.0f * invZ}, {barStartX, nmY + 4.0f * invZ}, seamarkMagenta, 1.8f * invZ);
        m_scene.DrawLine({barStartX + nmLenWorld, nmY - 4.0f * invZ}, {barStartX + nmLenWorld, nmY + 4.0f * invZ}, seamarkMagenta, 1.8f * invZ);
        m_scene.DrawText(m_view.nauticalScale.label, {barStartX + nmLenWorld + 8.0f * invZ, nmY - 7.0f * invZ}, 10.5f, hudText, FontStyle::Monospace, false);

        const float mLenWorld = m_view.metricScale.screenPixels * invZ;
        const float mY = boxY + 38.0f * invZ;
        m_scene.DrawLine({barStartX, mY}, {barStartX + mLenWorld, mY}, seamarkCyan, 2.2f * invZ);
        m_scene.DrawLine({barStartX, mY - 4.0f * invZ}, {barStartX, mY + 4.0f * invZ}, seamarkCyan, 1.8f * invZ);
        m_scene.DrawLine({barStartX + mLenWorld, mY - 4.0f * invZ}, {barStartX + mLenWorld, mY + 4.0f * invZ}, seamarkCyan, 1.8f * invZ);
        m_scene.DrawText(m_view.metricScale.label, {barStartX + mLenWorld + 8.0f * invZ, mY - 7.0f * invZ}, 10.5f, hudText, FontStyle::Monospace, false);
    }
}

MapViewScope MapView::Open(
    Context& ctx,
    const MapViewOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    auto& mapState = ctx.GetOrCreateState<MapViewInternalState>(id);
    auto& sceneState = ctx.GetOrCreateState<SceneState>(id);

    const float worldSize = std::max(256.0f, opts.worldSize);

    // Pre-schedule camera centering before Scene::Open runs UpdateSceneCameraTransform
    if (!mapState.cameraInitialized || !sceneState.initialized || opts.requestResetView) {
        mapState.cameraInitialized = true;
        mapState.lastCrs = opts.crs;
        mapState.lastCenterGeo = opts.initialCenter;
        sceneState.pendingCenterWorld = MapGrid::GeoToWorld(opts.initialCenter, worldSize, opts.crs);
        sceneState.pendingCenterZoom = MapGrid::TileZoomToSceneZoom(opts.initialTileZoom, opts.baseZoom);
    } else if (opts.requestCenterGeo.has_value() || opts.requestTileZoom.has_value()) {
        const GeoCoord targetGeo = opts.requestCenterGeo.value_or(mapState.lastCenterGeo);
        sceneState.pendingCenterWorld = MapGrid::GeoToWorld(targetGeo, worldSize, opts.crs);
        if (opts.requestTileZoom.has_value()) {
            sceneState.pendingCenterZoom = MapGrid::TileZoomToSceneZoom(*opts.requestTileZoom, opts.baseZoom);
        }
        mapState.lastCrs = opts.crs;
        mapState.lastCenterGeo = targetGeo;
    } else if (opts.crs != mapState.lastCrs) {
        // Preserve geographic center across CRS projection switches
        sceneState.pendingCenterWorld = MapGrid::GeoToWorld(mapState.lastCenterGeo, worldSize, opts.crs);
        mapState.lastCrs = opts.crs;
    }

    const SceneOptions sceneOpts{
        .label = opts.label,
        .description = opts.description,
        .worldSize = Vec2{worldSize, worldSize},
        .background = SceneBackground::Solid,
        .layout = opts.layout,
        .inlineOrder = opts.inlineOrder,
        .align = opts.align,
        .stretch = opts.stretch,
        .widthMode = opts.widthMode,
        .heightMode = opts.heightMode,
        .width = opts.width,
        .height = opts.height,
        .minHeight = opts.minHeight,
        .minZoom = opts.minZoom,
        .maxZoom = opts.maxZoom,
        .zoomStep = opts.zoomStep,
        .zoomable = opts.zoomable,
        .pannable = opts.pannable,
        .selectableItems = opts.selectableItems,
        .showScrollbars = opts.showScrollbars,
        .showControls = opts.showControls,
        .clampToWorld = opts.clampToWorld,
        .borderRadius = opts.borderRadius,
        .borderWidth = opts.borderWidth,
        .disabled = opts.disabled,
        .theme = opts.theme,
        .scale = opts.scale,
        .debugBounds = opts.debugBounds,
        .inspectMode = opts.inspectMode
    };

    SceneScope scene = Scene::Open(ctx, sceneOpts, loc);
    const UiTheme& activeTheme = (opts.theme != nullptr) ? *opts.theme : ctx.GetTheme();
    const bool lightMode = (activeTheme.mode == ThemeMode::Light);

    const float z = std::max(0.0001f, scene.GetZoom());
    const Rect vp = scene.GetViewportRect();
    const Rect visWorld = scene.GetVisibleWorldRect();
    const Vec2 mouseWorld = scene.GetMouseWorldPos();

    const MapGridConfig gridCfg{
        .crs = opts.crs,
        .worldSize = worldSize,
        .baseZoom = opts.baseZoom,
        .minTileZoom = opts.minTileZoom,
        .maxTileZoom = opts.maxTileZoom,
        .zoomBias = opts.zoomBias,
        .tileSource = opts.tileSource,
        .tileSourceUserData = opts.tileSourceUserData
    };

    MapViewState view = MapGrid::ComputeView(visWorld, vp, z, mouseWorld, gridCfg);
    if (vp.w > 1.0f && vp.h > 1.0f) {
        mapState.lastCenterGeo = view.center;
    }

    RenderMapBackgroundLayers(scene, opts, view, lightMode);

    return MapViewScope(std::move(scene), opts, view, lightMode);
}

} // namespace guikit
