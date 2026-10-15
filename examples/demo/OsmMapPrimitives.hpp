#pragma once

#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include "guikit/core/MapGrid.hpp"
#include "guikit/panels/Scene.hpp"

namespace guikit::demo {

struct OsmMapRenderOptions {
    MapCrs crs{MapCrs::EPSG3857_WebMercator};
    GeoCoordFormat coordFormat{GeoCoordFormat::NauticalDMM};
    float worldSize{2048.0f};
    int baseZoom{3};
    bool showTileGrid{true};
    bool showSubTileQuadtree{true};
    bool showNauticalGraticule{true};
    bool showBorderRulers{true};
    bool showSeamarksAndCourses{true};
    bool showScaleBarOverlay{true};
    bool lightMode{false};
    TileSourceCallback tileSource{nullptr};
    void* tileSourceUserData{nullptr};
};

struct MaritimeWaypoint {
    std::string_view id;
    std::string_view name;
    std::string_view seamarkType;
    GeoCoord coord;
    float defaultTileZoom;
};

inline constexpr std::array<MaritimeWaypoint, 5> kMaritimeWaypoints = {{
    {"WP-HELGOLAND", "Helgoland Light (North Sea)", "IALA Major Lighthouse (Fl 5s 28M)", {54.1825, 7.8853},  9.0f},
    {"WP-KIEL",      "Brunsbüttel / Kiel Canal",    "Locks & VTS Reporting Point",       {53.8900, 9.1433},  9.5f},
    {"WP-DOVER",     "Dover Strait TSS",            "Traffic Separation Buoy (Safe Water)", {51.1133, 1.3400}, 8.5f},
    {"WP-GREENWICH", "Greenwich Prime Meridian",    "000° 00.00' E Geodetic Reference",  {51.4779, 0.0000},  9.0f},
    {"WP-GIBRALTAR", "Strait of Gibraltar (Europa)", "Cardinal Pillar & Sector Light",   {36.1095, -5.3453}, 8.5f}
}};

/// Renders the OSM [z/x/y] tile grid, optional tile callback images, OpenSeaMap nautical graticule,
/// border degree/minute rulers, reference seamarks, and dual (Nautical NM + Metric km/m) scale bars.
inline MapViewState RenderOsmSlippyMap(
    SceneScope& scene,
    const OsmMapRenderOptions& opts
) {
    const float z = std::max(0.0001f, scene.GetZoom());
    const float invZ = 1.0f / z;
    const Rect vp = scene.GetViewportRect();
    const Rect visWorld = scene.GetVisibleWorldRect();
    const Vec2 mouseWorld = scene.GetMouseWorldPos();

    const MapGridConfig gridCfg{
        .crs = opts.crs,
        .worldSize = opts.worldSize,
        .baseZoom = opts.baseZoom,
        .minTileZoom = 1,
        .maxTileZoom = 18,
        .tileSource = opts.tileSource,
        .tileSourceUserData = opts.tileSourceUserData
    };

    MapViewState view = MapGrid::ComputeView(visWorld, vp, z, mouseWorld, gridCfg);

    // Palette tuned for clean cartographic / OpenSeaMap inspection in both Dark and Light modes
    const Color tileEvenFill   = opts.lightMode ? Color::Rgba(238, 245, 252, 95)  : Color::Rgba(18, 30, 46, 95);
    const Color tileOddFill    = opts.lightMode ? Color::Rgba(226, 237, 248, 95)  : Color::Rgba(24, 38, 58, 95);
    const Color tileHoverFill  = opts.lightMode ? Color::Rgba(59, 130, 246, 45)   : Color::Rgba(56, 189, 248, 45);
    const Color tileSelectFill = opts.lightMode ? Color::Rgba(37, 99, 235, 70)    : Color::Rgba(56, 189, 248, 75);
    const Color tileBorderCol  = opts.lightMode ? Color::Rgba(100, 135, 175, 150) : Color::Rgba(75, 115, 160, 145);
    const Color subTileCol     = opts.lightMode ? Color::Rgba(100, 135, 175, 55)  : Color::Rgba(90, 135, 185, 50);
    const Color tileIdTextCol  = opts.lightMode ? Color::Rgba(35, 65, 105, 220)   : Color::Rgba(148, 195, 245, 220);
    const Color tileMetaCol    = opts.lightMode ? Color::Rgba(70, 100, 135, 185)  : Color::Rgba(120, 160, 205, 180);

    const Color gratMinorCol   = opts.lightMode ? Color::Rgba(15, 118, 110, 80)   : Color::Rgba(45, 212, 191, 70);
    const Color gratMajorCol   = opts.lightMode ? Color::Rgba(13, 148, 136, 155)  : Color::Rgba(45, 212, 191, 145);
    const Color gratZeroCol    = opts.lightMode ? Color::Rgba(220, 38, 38, 190)   : Color::Rgba(248, 113, 113, 195);

    const Color seamarkMagenta = opts.lightMode ? Color::Rgba(185, 28, 140, 235)  : Color::Rgba(232, 121, 249, 240);
    const Color seamarkCyan    = opts.lightMode ? Color::Rgba(2, 132, 199, 230)   : Color::Rgba(56, 189, 248, 235);

    // -------------------------------------------------------------------------
    // 1. OSM [z/x/y] Tile Pyramid Cells & Universal Tile Source Callback Hook
    // -------------------------------------------------------------------------
    for (const OsmTileCell& cell : view.visibleTiles) {
        const Rect& r = cell.worldRect;
        const float screenTileW = r.w * z;

        char idBuf[40]{};
        std::snprintf(idBuf, sizeof(idBuf), "%d/%d/%d", cell.id.z, cell.id.x, cell.id.y);

        char titleBuf[96]{};
        std::snprintf(
            titleBuf,
            sizeof(titleBuf),
            "NW %.3f°, %.3f° | SE %.3f°, %.3f°",
            cell.geoBounds.nw.lat,
            cell.geoBounds.nw.lon,
            cell.geoBounds.se.lat,
            cell.geoBounds.se.lon
        );

        auto item = scene.Item({
            .id = idBuf,
            .title = titleBuf,
            .category = "OSM Tile [z/x/y]",
            .hitRect = r
        });

        const bool isCheckerOdd = ((cell.id.x + cell.id.y) & 1) != 0;
        Color cellFill = isCheckerOdd ? tileOddFill : tileEvenFill;
        if (item.IsSelected()) {
            cellFill = tileSelectFill;
        } else if (item.IsHovered()) {
            cellFill = tileHoverFill;
        }

        // Optional external tile image callback (Layer 0: Base OSM, Layer 1: OpenSeaMap Seamark overlay)
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
        } else if (item.IsHovered() || item.IsSelected()) {
            scene.DrawRect(r, cellFill);
        }

        if (opts.showTileGrid) {
            // Crisp 2x2 sub-tile quadtree division lines (previewing level z+1)
            if (opts.showSubTileQuadtree && screenTileW >= 110.0f) {
                const float midX = r.x + r.w * 0.5f;
                const float midY = r.y + r.h * 0.5f;
                scene.DrawLine({midX, r.y}, {midX, r.Bottom()}, subTileCol, 1.0f * invZ);
                scene.DrawLine({r.x, midY}, {r.Right(), midY}, subTileCol, 1.0f * invZ);
            }

            // Tile border
            const float borderW = (item.IsSelected() ? 2.2f : (item.IsHovered() ? 1.6f : 1.0f)) * invZ;
            const Color borderC = item.IsSelected() ? seamarkCyan : tileBorderCol;
            scene.DrawRect(r, Color::Transparent(), 0.0f, borderC, borderW);

            // Tile [z/x/y] coordinate stamp in the center of the tile
            if (screenTileW >= 64.0f) {
                char labelBuf[48]{};
                std::snprintf(labelBuf, sizeof(labelBuf), "z=%d  [%d, %d]", cell.id.z, cell.id.x, cell.id.y);
                scene.DrawTextBox(
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

            // Tile URI path & NW corner coordinate stamp when sufficiently zoomed in
            if (screenTileW >= 150.0f) {
                char uriBuf[64]{};
                std::snprintf(uriBuf, sizeof(uriBuf), "/tile/%d/%d/%d.png", cell.id.z, cell.id.x, cell.id.y);
                scene.DrawTextBox(
                    uriBuf,
                    Rect(r.x, r.y + r.h * 0.52f, r.w, r.h * 0.14f),
                    10.5f,
                    tileMetaCol,
                    0.5f,
                    0.5f,
                    FontStyle::Monospace,
                    /*scaleWithZoom=*/false
                );

                const std::string nwText = MapGrid::FormatCoord(cell.geoBounds.nw, opts.coordFormat);
                scene.DrawText(
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

    // -------------------------------------------------------------------------
    // 2. OpenSeaMap Nautical Graticule (Degrees & Nautical Arc-Minutes, 1' = 1 NM)
    // -------------------------------------------------------------------------
    if (opts.showNauticalGraticule) {
        const float vy0 = std::clamp(visWorld.y, 0.0f, opts.worldSize);
        const float vy1 = std::clamp(visWorld.Bottom(), 0.0f, opts.worldSize);
        const float vx0 = std::clamp(visWorld.x, 0.0f, opts.worldSize);
        const float vx1 = std::clamp(visWorld.Right(), 0.0f, opts.worldSize);

        for (const GraticuleLine& m : view.meridians) {
            const Color col = m.isZero ? gratZeroCol : (m.isMajor ? gratMajorCol : gratMinorCol);
            const float w = (m.isZero ? 1.8f : (m.isMajor ? 1.25f : 0.85f)) * invZ;
            scene.DrawLine({m.worldPos, vy0}, {m.worldPos, vy1}, col, w);
        }

        for (const GraticuleLine& p : view.parallels) {
            const Color col = p.isZero ? gratZeroCol : (p.isMajor ? gratMajorCol : gratMinorCol);
            const float w = (p.isZero ? 1.8f : (p.isMajor ? 1.25f : 0.85f)) * invZ;
            scene.DrawLine({vx0, p.worldPos}, {vx1, p.worldPos}, col, w);
        }
    }

    // -------------------------------------------------------------------------
    // 3. Reference Nautical Seamarks & Rhumb-Line (Loxodrome) Course Legs
    // -------------------------------------------------------------------------
    if (opts.showSeamarksAndCourses) {
        // Draw rhumb-line shipping lane connecting Helgoland -> Dover -> Gibraltar
        const Vec2 pHelgoland = MapGrid::GeoToWorld(kMaritimeWaypoints[0].coord, opts.worldSize, opts.crs);
        const Vec2 pKiel      = MapGrid::GeoToWorld(kMaritimeWaypoints[1].coord, opts.worldSize, opts.crs);
        const Vec2 pDover     = MapGrid::GeoToWorld(kMaritimeWaypoints[2].coord, opts.worldSize, opts.crs);
        const Vec2 pGreenwich = MapGrid::GeoToWorld(kMaritimeWaypoints[3].coord, opts.worldSize, opts.crs);
        const Vec2 pGibraltar = MapGrid::GeoToWorld(kMaritimeWaypoints[4].coord, opts.worldSize, opts.crs);

        scene.DrawLine(pKiel, pHelgoland, seamarkMagenta.WithAlpha(185), 1.8f * invZ);
        scene.DrawLine(pHelgoland, pDover, seamarkMagenta.WithAlpha(185), 1.8f * invZ);
        scene.DrawLine(pDover, pGreenwich, seamarkCyan.WithAlpha(175), 1.5f * invZ);
        scene.DrawLine(pDover, pGibraltar, seamarkMagenta.WithAlpha(185), 1.8f * invZ);

        for (const MaritimeWaypoint& wp : kMaritimeWaypoints) {
            const Vec2 wPos = MapGrid::GeoToWorld(wp.coord, opts.worldSize, opts.crs);
            const float r = 7.0f * invZ;
            const Rect hitBox(wPos.x - r * 2.0f, wPos.y - r * 2.0f, r * 4.0f, r * 4.0f);

            auto wpItem = scene.Item({
                .id = wp.id,
                .title = wp.name,
                .category = wp.seamarkType,
                .hitRect = hitBox
            });

            // IALA Nautical Seamark symbol: outer light sector arc + diamond buoy/beacon + center point
            const Color markCol = (wpItem.IsSelected() || wpItem.IsHovered()) ? seamarkCyan : seamarkMagenta;
            scene.DrawArc(wPos, r * 1.9f, -65.0f, 130.0f, markCol, 1.8f * invZ, 16);
            scene.DrawTriangle(
                {wPos.x, wPos.y - r * 1.25f},
                {wPos.x - r * 0.9f, wPos.y},
                {wPos.x + r * 0.9f, wPos.y},
                markCol.WithAlpha(70),
                markCol,
                1.5f * invZ
            );
            scene.DrawTriangle(
                {wPos.x, wPos.y + r * 1.25f},
                {wPos.x - r * 0.9f, wPos.y},
                {wPos.x + r * 0.9f, wPos.y},
                markCol.WithAlpha(70),
                markCol,
                1.5f * invZ
            );
            scene.DrawCircle(wPos, 2.5f * invZ, markCol);

            // Callout label
            scene.DrawText(
                wp.name,
                {wPos.x + 11.0f * invZ, wPos.y - 6.0f * invZ},
                11.0f,
                markCol,
                FontStyle::Bold,
                /*scaleWithZoom=*/false
            );
        }
    }

    // -------------------------------------------------------------------------
    // 4. OpenSeaMap Nautical Border Rulers (Top Longitude & Left Latitude Bars)
    // -------------------------------------------------------------------------
    if (opts.showBorderRulers && opts.showNauticalGraticule) {
        const float rulerThick = 20.0f * invZ;
        const float leftBarW = rulerThick * 3.9f;
        const float topRulerStartX = visWorld.x + leftBarW;
        const float leftRulerStartY = visWorld.y + rulerThick;

        const Color rulerBg = opts.lightMode ? Color::Rgba(248, 250, 252, 236) : Color::Rgba(11, 18, 30, 236);
        const Color rulerBorder = opts.lightMode ? Color::Rgba(148, 163, 184, 200) : Color::Rgba(51, 65, 85, 210);
        const Color rulerText = opts.lightMode ? Color::Rgba(15, 23, 42, 240) : Color::Rgba(226, 232, 240, 240);
        const Color tickAccent = opts.lightMode ? Color::Rgba(13, 148, 136, 220) : Color::Rgba(45, 212, 191, 220);

        // Top longitude ruler bar (to the right of the top-left corner badge)
        const Rect topBar(topRulerStartX, visWorld.y, std::max(0.0f, visWorld.w - leftBarW), rulerThick);
        scene.DrawRect(topBar, rulerBg);
        scene.DrawLine({topRulerStartX, visWorld.y + rulerThick}, {visWorld.Right(), visWorld.y + rulerThick}, rulerBorder, 1.0f * invZ);

        for (size_t i = 0; i < view.meridians.size(); ++i) {
            const GraticuleLine& m = view.meridians[i];
            if (i + 1 < view.meridians.size() && ((std::abs(m.stepIndex) & 1) == 0)) {
                const float nextX = view.meridians[i + 1].worldPos;
                const float x0 = std::max(topRulerStartX, std::min(m.worldPos, nextX));
                const float x1 = std::min(visWorld.Right(), std::max(m.worldPos, nextX));
                if (x1 > x0) {
                    scene.DrawRect(
                        Rect(x0, visWorld.y + rulerThick - 4.0f * invZ, x1 - x0, 4.0f * invZ),
                        tickAccent.WithAlpha(95)
                    );
                }
            }
            if (m.worldPos >= topRulerStartX && m.worldPos <= visWorld.Right()) {
                scene.DrawLine({m.worldPos, visWorld.y}, {m.worldPos, visWorld.y + rulerThick}, tickAccent, 1.3f * invZ);
                if (m.worldPos + 44.0f * invZ <= visWorld.Right()) {
                    const std::string lbl = MapGrid::FormatGraticuleTick(m.degrees, /*isLat=*/false, view.graticuleStepArcMin);
                    scene.DrawText(lbl, {m.worldPos + 4.0f * invZ, visWorld.y + 3.0f * invZ}, 10.0f, rulerText, FontStyle::Monospace, false);
                }
            }
        }

        // Left latitude ruler bar (below the top-left corner badge)
        const Rect leftBar(visWorld.x, leftRulerStartY, leftBarW, std::max(0.0f, visWorld.h - rulerThick));
        scene.DrawRect(leftBar, rulerBg);
        scene.DrawLine({leftBar.Right(), leftBar.y}, {leftBar.Right(), leftBar.Bottom()}, rulerBorder, 1.0f * invZ);

        for (size_t i = 0; i < view.parallels.size(); ++i) {
            const GraticuleLine& p = view.parallels[i];
            if (i + 1 < view.parallels.size() && ((std::abs(p.stepIndex) & 1) == 0)) {
                const float nextY = view.parallels[i + 1].worldPos;
                const float y0 = std::max(leftRulerStartY, std::min(p.worldPos, nextY));
                const float y1 = std::min(visWorld.Bottom(), std::max(p.worldPos, nextY));
                if (y1 > y0) {
                    scene.DrawRect(
                        Rect(leftBar.Right() - 4.0f * invZ, y0, 4.0f * invZ, y1 - y0),
                        tickAccent.WithAlpha(95)
                    );
                }
            }
            if (p.worldPos >= leftRulerStartY && p.worldPos <= visWorld.Bottom()) {
                scene.DrawLine({leftBar.x, p.worldPos}, {leftBar.Right(), p.worldPos}, tickAccent, 1.3f * invZ);
                if (p.worldPos >= leftRulerStartY + 14.0f * invZ) {
                    const std::string lbl = MapGrid::FormatGraticuleTick(p.degrees, /*isLat=*/true, view.graticuleStepArcMin);
                    scene.DrawText(lbl, {leftBar.x + 4.0f * invZ, p.worldPos - 13.0f * invZ}, 10.0f, rulerText, FontStyle::Monospace, false);
                }
            }
        }

        // Top-left CRS & Graticule step corner badge
        char stepBuf[32]{};
        if (view.graticuleStepArcMin >= 60.0) {
            std::snprintf(stepBuf, sizeof(stepBuf), "Grid %.0f°", view.graticuleStepArcMin / 60.0);
        } else {
            std::snprintf(stepBuf, sizeof(stepBuf), "Grid %.1f' NM", view.graticuleStepArcMin);
        }
        scene.DrawRect(Rect(visWorld.x, visWorld.y, leftBarW, rulerThick), rulerBg, 0.0f, rulerBorder, 1.0f * invZ);
        scene.DrawText(stepBuf, {visWorld.x + 4.0f * invZ, visWorld.y + 3.0f * invZ}, 10.0f, tickAccent, FontStyle::Bold, false);
    }

    // -------------------------------------------------------------------------
    // 5. Dual Scale Bar Overlay (Nautical Miles NM/cab + Metric km/m)
    // -------------------------------------------------------------------------
    if (opts.showScaleBarOverlay) {
        const float leftOffset = (opts.showBorderRulers && opts.showNauticalGraticule) ? 90.0f : 14.0f;
        const float boxW = 240.0f * invZ;
        const float boxH = 54.0f * invZ;
        const float boxX = visWorld.x + leftOffset * invZ;
        const float boxY = visWorld.Bottom() - (boxH + 14.0f * invZ);

        const Color hudBg = opts.lightMode ? Color::Rgba(255, 255, 255, 232) : Color::Rgba(11, 18, 30, 232);
        const Color hudBorder = opts.lightMode ? Color::Rgba(148, 163, 184, 210) : Color::Rgba(51, 65, 85, 220);
        const Color hudText = opts.lightMode ? Color::Rgba(15, 23, 42, 245) : Color::Rgba(226, 232, 240, 245);

        scene.DrawRect(Rect(boxX, boxY, boxW, boxH), hudBg, 5.0f * invZ, hudBorder, 1.0f * invZ);

        // Nautical scale bar (top)
        const float nmLenWorld = view.nauticalScale.screenPixels * invZ;
        const float nmY = boxY + 18.0f * invZ;
        const float barStartX = boxX + 10.0f * invZ;
        scene.DrawLine({barStartX, nmY}, {barStartX + nmLenWorld, nmY}, seamarkMagenta, 2.2f * invZ);
        scene.DrawLine({barStartX, nmY - 4.0f * invZ}, {barStartX, nmY + 4.0f * invZ}, seamarkMagenta, 1.8f * invZ);
        scene.DrawLine({barStartX + nmLenWorld, nmY - 4.0f * invZ}, {barStartX + nmLenWorld, nmY + 4.0f * invZ}, seamarkMagenta, 1.8f * invZ);
        scene.DrawText(view.nauticalScale.label, {barStartX + nmLenWorld + 8.0f * invZ, nmY - 7.0f * invZ}, 10.5f, hudText, FontStyle::Monospace, false);

        // Metric scale bar (bottom)
        const float mLenWorld = view.metricScale.screenPixels * invZ;
        const float mY = boxY + 38.0f * invZ;
        scene.DrawLine({barStartX, mY}, {barStartX + mLenWorld, mY}, seamarkCyan, 2.2f * invZ);
        scene.DrawLine({barStartX, mY - 4.0f * invZ}, {barStartX, mY + 4.0f * invZ}, seamarkCyan, 1.8f * invZ);
        scene.DrawLine({barStartX + mLenWorld, mY - 4.0f * invZ}, {barStartX + mLenWorld, mY + 4.0f * invZ}, seamarkCyan, 1.8f * invZ);
        scene.DrawText(view.metricScale.label, {barStartX + mLenWorld + 8.0f * invZ, mY - 7.0f * invZ}, 10.5f, hudText, FontStyle::Monospace, false);
    }

    return view;
}

} // namespace guikit::demo
