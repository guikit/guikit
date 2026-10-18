#pragma once

#include <cstdint>
#include <optional>
#include <source_location>
#include <span>
#include <string_view>
#include "guikit/geo/MapGrid.hpp"
#include "guikit/panels/Scene.hpp"

namespace guikit {

class Context;

/// Configuration options for an interactive IALA nautical seamark / waypoint symbol on a MapView.
struct MapSeamarkOptions {
    std::string_view id{};                      ///< Unique identifier for selection & inspector (e.g. "WP-HELGOLAND")
    std::string_view title{};                   ///< Display label rendered next to the seamark
    std::string_view category{"IALA Seamark"};  ///< Category description shown in floating inspector
    float radiusPx{7.0f};                       ///< Screen-space symbol radius in logical pixels (zoom-invariant)
    bool showLightSector{true};                 ///< Render outer IALA light sector arc
    float sectorStartDeg{-65.0f};               ///< Light sector start angle in degrees
    float sectorSweepDeg{130.0f};               ///< Light sector angular sweep in degrees
    std::optional<Color> color{std::nullopt};   ///< Custom symbol color (defaults to chart magenta / cyan when selected)
    bool selectable{true};                      ///< Participate in click & marquee selection
};

/// Configuration options for the autonomous MapView (Slippy Map / OpenSeaMap GIS viewport) container.
struct MapViewOptions {
    // --- Element Companion Header / Footer ---
    UiText label{};
    UiText description{};

    // --- Cartographic & Projection Parameters ---
    MapCrs crs{MapCrs::EPSG3857_WebMercator};
    GeoCoordFormat coordFormat{GeoCoordFormat::NauticalDMM};
    float worldSize{2048.0f};                   ///< Internal world extent (0..worldSize in X and Y)
    int baseZoom{3};                            ///< Tile zoom level corresponding to sceneZoom == 1.0f (2048 / 256 = 8 = 2^3)
    int minTileZoom{1};
    int maxTileZoom{18};
    float zoomBias{0.0f};
    TileSourceCallback tileSource{nullptr};     ///< Optional callback resolving tile [z/x/y] to SVG/PNG/JPG path
    void* tileSourceUserData{nullptr};

    // --- Initial & Programmatic Camera Framing ---
    GeoCoord initialCenter{54.1825, 7.8853};    ///< Default initial center (Helgoland / German Bight)
    float initialTileZoom{7.0f};                ///< Default initial tile zoom level
    std::optional<GeoCoord> requestCenterGeo{std::nullopt}; ///< Programmatic jump to geographic coordinate
    std::optional<float> requestTileZoom{std::nullopt};     ///< Programmatic jump to tile zoom level
    bool requestResetView{false};               ///< Reset camera to initialCenter & initialTileZoom

    // --- Built-in Cartographic Layers & Overlays ---
    bool showTileGrid{true};                    ///< Render OSM [z/x/y] tile borders, hover/selection, and tile labels
    bool showSubTileQuadtree{true};             ///< Render 2x2 sub-tile quadtree division lines (previewing z+1)
    bool showNauticalGraticule{true};           ///< Render OpenSeaMap meridian/parallel graticule (1' = 1 NM)
    bool showBorderRulers{true};                ///< Render top longitude & left latitude degree/minute zebra rulers
    bool showScaleBarOverlay{true};             ///< Render dual Nautical (NM / cab) + Metric (km / m) scale bars

    // --- Layout & Viewport Controls ---
    ElementLayout layout{ElementLayout::Stacked};
    InlineOrder inlineOrder{InlineOrder::LabelFirst};
    Align align{Align::Start};
    StretchMode stretch{StretchMode::Fill};
    SizeMode widthMode{SizeMode::Stretch};
    SizeMode heightMode{SizeMode::Stretch};
    std::optional<float> width{std::nullopt};
    std::optional<float> height{std::nullopt};
    float minHeight{220.0f};
    float minZoom{0.25f};
    float maxZoom{4096.0f};
    float zoomStep{1.2f};
    bool zoomable{true};
    bool pannable{true};
    bool selectableItems{true};
    bool showScrollbars{true};
    bool showControls{true};
    bool clampToWorld{true};
    std::optional<float> borderRadius{8.0f};
    float borderWidth{1.0f};
    bool disabled{false};
    const UiTheme* theme{nullptr};
    std::optional<float> scale{std::nullopt};
    std::optional<bool> debugBounds{std::nullopt};
    std::optional<bool> inspectMode{std::nullopt};
};

/// RAII scope for `ui.MapView(...)` providing geographic (Lat/Lon) drawing, hit-testing, and telemetry.
class [[nodiscard]] MapViewScope {
public:
    MapViewScope(
        SceneScope&& scene,
        const MapViewOptions& opts,
        const MapViewState& viewState,
        bool lightMode
    ) noexcept;

    ~MapViewScope() noexcept;

    MapViewScope(const MapViewScope&) = delete;
    MapViewScope& operator=(const MapViewScope&) = delete;
    MapViewScope(MapViewScope&& other) noexcept;
    MapViewScope& operator=(MapViewScope&&) = delete;

    [[nodiscard]] explicit operator bool() const noexcept {
        return static_cast<bool>(m_scene);
    }

    // --- Cartographic Telemetry & Coordinate Queries ---
    [[nodiscard]] const MapViewState& GetViewState() const noexcept { return m_view; }
    [[nodiscard]] GeoCoord GetCenterGeo() const noexcept { return m_view.center; }
    [[nodiscard]] GeoCoord GetCursorGeo() const noexcept { return m_view.cursor; }
    [[nodiscard]] GeoBounds GetVisibleGeoBounds() const noexcept { return m_view.visibleBounds; }
    [[nodiscard]] int GetTileZoom() const noexcept { return m_view.tileZoom; }
    [[nodiscard]] float GetContinuousZoom() const noexcept { return m_view.continuousZoom; }
    [[nodiscard]] double GetMetersPerPixel() const noexcept { return m_view.metersPerPixel; }
    [[nodiscard]] double GetGraticuleStepArcMin() const noexcept { return m_view.graticuleStepArcMin; }

    // --- Geographic <-> World / Screen Projections ---
    [[nodiscard]] Vec2 GeoToWorld(GeoCoord coord) const noexcept;
    [[nodiscard]] GeoCoord WorldToGeo(Vec2 worldPt) const noexcept;
    [[nodiscard]] Vec2 GeoToScreen(GeoCoord coord) const noexcept;
    [[nodiscard]] GeoCoord ScreenToGeo(Vec2 screenPt) const noexcept;

    // --- Imperative Camera Control ---
    void CenterOnGeo(GeoCoord coord, std::optional<float> tileZoom = std::nullopt) noexcept;
    void ResetView() noexcept;

    // --- Interactive Geographic Items & Drawing Primitives ---
    /// Declares an interactive, selectable item anchored at a geographic coordinate with a screen-space hit radius.
    [[nodiscard]] SceneItemScope GeoItem(
        GeoCoord coord,
        float hitRadiusPx,
        const SceneItemOptions& opts
    );

    /// Draws a rhumb-line (loxodrome) course segment between two geographic coordinates.
    void DrawGeoLine(
        GeoCoord from,
        GeoCoord to,
        Color color,
        float strokeWidthPx = 1.8f
    );

    /// Draws a connected multi-leg geographic polyline or closed polygon outline.
    void DrawGeoPolyline(
        std::span<const GeoCoord> coords,
        Color color,
        float strokeWidthPx = 1.8f,
        bool closed = false
    );

    /// Draws a zoom-invariant circle anchored at a geographic coordinate.
    void DrawGeoCircle(
        GeoCoord center,
        float radiusPx,
        Color fill,
        Color stroke = Color::Transparent(),
        float strokeWidthPx = 0.0f
    );

    /// Draws a zoom-invariant circular arc (e.g. lighthouse sector) anchored at a geographic coordinate.
    void DrawGeoArc(
        GeoCoord center,
        float radiusPx,
        float startAngleDeg,
        float sweepAngleDeg,
        Color stroke,
        float strokeWidthPx = 1.8f,
        int segments = 16
    );

    /// Draws a zoom-invariant text label anchored at a geographic coordinate with an optional pixel offset.
    void DrawGeoText(
        std::string_view text,
        GeoCoord coord,
        Vec2 offsetPx = {0.0f, 0.0f},
        float fontSize = 11.0f,
        Color color = Color(255, 255, 255, 255),
        FontStyle style = FontStyle::Regular
    );

    /// Renders a complete interactive IALA Nautical Seamark (diamond beacon, light sector arc, and callout label).
    SceneItemScope DrawSeamark(
        GeoCoord coord,
        const MapSeamarkOptions& opts
    );

    // --- Underlying Scene Access ---
    [[nodiscard]] SceneScope& GetScene() noexcept { return m_scene; }
    [[nodiscard]] const SceneScope& GetScene() const noexcept { return m_scene; }
    [[nodiscard]] std::span<const SceneItemInfo> GetItems() const noexcept { return m_scene.GetItems(); }
    [[nodiscard]] const SceneItemInfo* GetHoveredItem() const noexcept { return m_scene.GetHoveredItem(); }
    [[nodiscard]] size_t GetSelectedCount() const noexcept { return m_scene.GetSelectedCount(); }

private:
    void RenderForegroundOverlays();

    SceneScope m_scene;
    MapViewOptions m_opts{};
    MapViewState m_view{};
    bool m_lightMode{false};
    bool m_active{false};
};

/// Autonomous Slippy Map / OpenSeaMap GIS viewport component built on Scene + MapGrid.
class MapView {
public:
    [[nodiscard]] static MapViewScope Open(
        Context& ctx,
        const MapViewOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );
};

} // namespace guikit
