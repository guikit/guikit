#pragma once

#include <cstdint>
#include <optional>
#include <source_location>
#include <span>
#include <string_view>
#include <vector>
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/ContainerScope.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

class Context;
class SceneScope;
struct Node;

/// Visual background pattern for a 2D Scene viewport.
enum class SceneBackground : uint8_t {
    Solid = 0,      ///< Solid theme background / surface fill
    Checkerboard,   ///< Transparency checkerboard (for PNG/SVG asset inspection)
    GridDots,       ///< World-space adaptive dot matrix (for schematics and node graphs)
    GridLines       ///< World-space adaptive coordinate grid lines (for CAD, maps, and plots)
};

/// Configuration options for an interactive world-space item inside a Scene.
struct SceneItemOptions {
    std::string_view id{};                      ///< Unique item identifier within the scene (e.g. "R100", "TR101")
    std::string_view title{};                   ///< Display value or part number (e.g. "220K", "BC 154")
    std::string_view category{};                ///< Component category/type (e.g. "Resistor", "PNP Transistor")
    std::optional<Rect> hitRect{std::nullopt};  ///< Explicit world-space hit box (auto-computed from geometry if nullopt)
    bool selectable{true};                      ///< Whether this item participates in hit-testing and selection
    bool showSelectionFrame{true};              ///< Whether to draw automatic CAD hover/selection box around worldBounds
};

/// Resolved metadata, interaction flags, and world-space bounding box of a recorded SceneItem.
struct SceneItemInfo {
    uint64_t key{0};                ///< Deterministic 64-bit item key
    std::string_view id{};          ///< Frame-arena allocated identifier
    std::string_view title{};       ///< Frame-arena allocated title/value
    std::string_view category{};    ///< Frame-arena allocated category
    Rect worldBounds{};             ///< World-space bounding box of the item's geometry
    int32_t firstCmdIndex{0};       ///< First draw command index belonging to this item
    bool hasExplicitBounds{false};  ///< True if hitRect was explicitly provided
    bool selectable{true};
    bool showSelectionFrame{true};
    bool hovered{false};
    bool selected{false};
    bool clicked{false};
};

/// Persistent camera and interaction state for a 2D Scene viewport.
struct SceneState {
    Vec2 pan{0.0f, 0.0f};       ///< Offset of world origin (0,0) relative to viewport top-left in screen pixels
    float zoom{1.0f};           ///< Active camera zoom factor
    bool initialized{false};    ///< True once initial fit/framing has been computed
    bool pointerDown{false};    ///< True while left mouse button is held after pressing inside viewport
    bool panning{false};        ///< True while dragging the canvas with mouse
    Vec2 panStartMouse{0.0f, 0.0f};
    Vec2 panStartOffset{0.0f, 0.0f};
    bool draggingScrollX{false};
    bool draggingScrollY{false};
    float dragStartScrollX{0.0f};
    float dragStartScrollY{0.0f};
    Vec2 dragStartScrollMouse{0.0f, 0.0f};
    uint8_t hoveredHudBtn{0};   ///< 1 = '-', 2 = 'Fit', 3 = '100%', 4 = '+'

    // Interactive SceneItem selection & marquee state
    uint64_t hoveredItemKey{0};
    uint64_t clickedItemKey{0};
    std::vector<uint64_t> selectedItemKeys{};
    bool boxSelecting{false};
    Vec2 boxSelectStartWorld{0.0f, 0.0f};
    Vec2 boxSelectCurrentWorld{0.0f, 0.0f};
    std::optional<Vec2> pendingCenterWorld{std::nullopt};
    std::optional<float> pendingCenterZoom{std::nullopt};
    Rect lastViewport{};
    bool preUpdatedCamera{false};
    bool preUpdateHudConsumed{false};
    bool preUpdateScrollbarBusy{false};

    [[nodiscard]] bool IsSelected(uint64_t key) const noexcept {
        if (key == 0) return false;
        for (uint64_t k : selectedItemKeys) {
            if (k == key) return true;
        }
        return false;
    }
};

/// Configuration options for a 2D Scene camera viewport.
struct SceneOptions {
    UiText label{};                                     ///< Optional external heading/label
    UiText description{};                               ///< Optional external caption/description
    std::optional<Vec2> worldSize{std::nullopt};        ///< Bounded world dimensions (0..W, 0..H), or nullopt for infinite world (OSM maps, graphs)
    ObjectFit initialFit{ObjectFit::Contain};           ///< Initial camera framing when worldSize is set
    SceneBackground background{SceneBackground::Checkerboard};
    float gridStep{32.0f};                              ///< Base world-space step in units for GridDots / GridLines
    ElementLayout layout{ElementLayout::Stacked};
    InlineOrder inlineOrder{InlineOrder::LabelFirst};
    Align elementAlign{Align::Start};
    StretchMode stretch{StretchMode::Fill};
    SizeMode widthMode{SizeMode::Stretch};
    SizeMode heightMode{SizeMode::Stretch};
    std::optional<float> width{std::nullopt};           ///< Explicit width in px or % (in Fill mode)
    std::optional<float> height{std::nullopt};          ///< Explicit height in px
    float minHeight{220.0f};                            ///< Minimum fallback height when placed inside a Fit/ScrollArea container
    float minZoom{0.1f};
    float maxZoom{16.0f};
    float zoomStep{1.2f};
    bool zoomable{true};                                ///< Allow mouse wheel zooming towards cursor
    bool pannable{true};                                ///< Allow mouse drag panning
    bool selectableItems{true};                         ///< Enable interactive hit-testing, click selection, and Shift+Drag marquee selection
    bool showScrollbars{true};                          ///< Render 2D (X/Y) scrollbars when worldSize overflows viewport
    bool showControls{true};                            ///< Render floating Zoom HUD (-, Fit, 100%, +)
    bool clampToWorld{true};                            ///< Clamp panning to worldSize bounds when worldSize is finite
    std::optional<float> borderRadius{6.0f};
    float borderWidth{1.0f};
    Color backgroundColor{0, 0, 0, 0};
    Color borderColor{0, 0, 0, 0};
    std::optional<UiSize> size{std::nullopt};
    bool disabled{false};
    const UiTheme* theme{nullptr};
    std::optional<float> scale{std::nullopt};
    std::optional<bool> debugBounds{std::nullopt};
    std::optional<bool> inspectMode{std::nullopt};
};

/// Recorded world-space drawing command executed during Scene rendering.
struct SceneDrawCmd {
    enum class Type : uint8_t {
        Image = 0,
        Pixels,
        Rect,
        Line,
        Triangle,
        Text,
        TextBox
    };
    Type type{Type::Rect};
    int32_t itemIndex{-1};      ///< Owning SceneItemInfo index in ScenePayload::items (-1 if unowned)
    std::string_view textOrSource{};
    const uint32_t* pixels{nullptr};
    uint32_t pixelW{0};
    uint32_t pixelH{0};
    Rect rect{};
    Vec2 p1{};
    Vec2 p2{};
    Vec2 p3{};
    Color fill{};
    Color stroke{};
    float radius{0.0f};
    float strokeWidth{0.0f};
    float fontSize{14.0f};
    float alignX{0.5f};
    float alignY{0.5f};
    float angleDeg{0.0f};
    FontStyle fontStyle{FontStyle::Regular};
    uint8_t opacity{255};
    bool scaleFontWithZoom{true};
};

/// Internal per-frame payload stored on NodeType::Scene.
struct ScenePayload {
    SceneOptions opts{};
    SceneState* state{nullptr};
    Rect openViewport{};
    std::vector<SceneDrawCmd> drawCmds{};
    std::vector<SceneItemInfo> items{};
    int32_t activeItemIndex{-1};
};

/// RAII scope that groups world-space draw commands into an interactive, selectable SceneItem.
class SceneItemScope {
public:
    SceneItemScope(
        SceneScope* scene,
        int32_t itemIndex,
        int32_t prevItemIndex,
        bool hovered,
        bool selected,
        bool clicked
    ) noexcept;
    ~SceneItemScope() noexcept;

    SceneItemScope(const SceneItemScope&) = delete;
    SceneItemScope& operator=(const SceneItemScope&) = delete;
    SceneItemScope(SceneItemScope&& other) noexcept;
    SceneItemScope& operator=(SceneItemScope&&) = delete;

    [[nodiscard]] explicit operator bool() const noexcept { return true; }
    [[nodiscard]] bool IsHovered() const noexcept { return m_hovered; }
    [[nodiscard]] bool IsSelected() const noexcept { return m_selected; }
    [[nodiscard]] bool Clicked() const noexcept { return m_clicked; }
    [[nodiscard]] int32_t GetItemIndex() const noexcept { return m_itemIndex; }

private:
    SceneScope* m_scene{nullptr};
    int32_t m_itemIndex{-1};
    int32_t m_prevItemIndex{-1};
    bool m_hovered{false};
    bool m_selected{false};
    bool m_clicked{false};
};

/// RAII scope for a 2D Scene container providing World <-> Screen transforms and world-space drawing.
class SceneScope : public ContainerScope {
public:
    SceneScope(Context* ctx, Node* node, ScenePayload* payload, Rect prevViewport) noexcept;

    /// Declares an interactive world-space item whose bounding box is automatically computed from enclosed geometry.
    [[nodiscard]] SceneItemScope Item(const SceneItemOptions& opts);

    /// Returns all interactive items recorded in the scene during this frame.
    [[nodiscard]] std::span<const SceneItemInfo> GetItems() const noexcept;

    /// Returns the currently hovered item (or nullptr if none).
    [[nodiscard]] const SceneItemInfo* GetHoveredItem() const noexcept;

    /// Returns the number of currently selected items.
    [[nodiscard]] size_t GetSelectedCount() const noexcept;

    /// Clears all selected items in the scene.
    void ClearSelection() noexcept;

    /// Selects an item by its string identifier.
    void SelectItem(std::string_view id, bool addToSelection = false) noexcept;

    /// Returns the current camera zoom multiplier.
    [[nodiscard]] float GetZoom() const noexcept;

    /// Returns the effective screen-pixels-per-world-unit scale factor (zoom * themeScale).
    [[nodiscard]] float GetWorldScale() const noexcept;

    /// Sets the camera zoom multiplier (clamped to [minZoom, maxZoom]).
    void SetZoom(float zoom) noexcept;

    /// Centers the camera on a specific world-space coordinate with an optional new zoom multiplier.
    void CenterOn(Vec2 worldPoint, std::optional<float> zoom = std::nullopt) noexcept;

    /// Resets camera to initial fit framing.
    void ResetView() noexcept;

    /// Returns the current screen-space pan offset of the world origin (0,0).
    [[nodiscard]] Vec2 GetPan() const noexcept;

    /// Returns true if the user is actively dragging/panning the scene viewport.
    [[nodiscard]] bool IsPanning() const noexcept;

    /// Returns the screen-space viewport rectangle.
    [[nodiscard]] Rect GetViewportRect() const noexcept { return m_viewport; }

    /// Returns the currently visible region in World Coordinates (ideal for tile culling in OSM maps).
    [[nodiscard]] Rect GetVisibleWorldRect() const noexcept;

    /// Transforms a point from World Space to Screen Space.
    [[nodiscard]] Vec2 WorldToScreen(Vec2 worldPt) const noexcept;

    /// Transforms a point from Screen Space to World Space.
    [[nodiscard]] Vec2 ScreenToWorld(Vec2 screenPt) const noexcept;

    /// Transforms a rectangle from World Space to Screen Space.
    [[nodiscard]] Rect WorldToScreen(Rect worldRect) const noexcept;

    /// Returns the current mouse cursor position in World Space.
    [[nodiscard]] Vec2 GetMouseWorldPos() const noexcept;

    /// Queues a vector or raster image (SVG, PNG, JPG) in World Coordinates.
    void DrawImage(
        std::string_view source,
        Rect worldRect,
        float radius = 0.0f,
        uint8_t opacity = 255
    );

    /// Queues an in-memory ABGR8888 pixel buffer in World Coordinates.
    void DrawPixels(
        const uint32_t* pixels,
        uint32_t width,
        uint32_t height,
        Rect worldRect,
        float radius = 0.0f,
        uint8_t opacity = 255
    );

    /// Queues a filled/stroked rectangle in World Coordinates.
    void DrawRect(
        Rect worldRect,
        Color fill,
        float radius = 0.0f,
        Color stroke = Color::Transparent(),
        float strokeWidth = 0.0f
    );

    /// Queues a filled/stroked circle in World Coordinates.
    void DrawCircle(
        Vec2 worldCenter,
        float worldRadius,
        Color fill,
        Color stroke = Color::Transparent(),
        float strokeWidth = 0.0f
    );

    /// Queues a filled/stroked triangle in World Coordinates.
    void DrawTriangle(
        Vec2 worldP1,
        Vec2 worldP2,
        Vec2 worldP3,
        Color fill,
        Color stroke = Color::Transparent(),
        float strokeWidth = 0.0f
    );

    /// Queues a circular arc in World Coordinates (angles in degrees, 0 = +X, 90 = +Y).
    void DrawArc(
        Vec2 worldCenter,
        float worldRadius,
        float startAngleDeg,
        float sweepAngleDeg,
        Color color,
        float strokeWidth = 1.0f,
        int segments = 14
    );

    /// Queues a line segment in World Coordinates.
    void DrawLine(Vec2 worldP1, Vec2 worldP2, Color color, float strokeWidth = 1.0f);

    /// Queues a text label in World Coordinates.
    void DrawText(
        std::string_view text,
        Vec2 worldPos,
        float fontSize,
        Color color,
        FontStyle style = FontStyle::Regular,
        bool scaleWithZoom = true,
        float angleDeg = 0.0f
    );

    /// Queues aligned text inside a World Coordinate bounding box (with optional rotation angle in degrees).
    void DrawTextBox(
        std::string_view text,
        Rect worldRect,
        float fontSize,
        Color color,
        float alignX = 0.5f,
        float alignY = 0.5f,
        FontStyle style = FontStyle::Regular,
        bool scaleWithZoom = true,
        float angleDeg = 0.0f
    );

private:
    friend class SceneItemScope;
    void FinalizeItem(int32_t itemIndex, int32_t prevItemIndex) noexcept;

    ScenePayload* m_payload{nullptr};
    Rect m_viewport{};
};

/// 2D Camera and World Viewport container implementation.
class Scene {
public:
    /// Opens an immediate mode 2D Scene container.
    [[nodiscard]] static SceneScope Open(
        Context& ctx,
        const SceneOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Returns the inner viewport clip rectangle for a Scene node.
    [[nodiscard]] static Rect GetViewportRect(const Node* node, ICanvas* canvas, const UiTheme& theme) noexcept;

    /// Updates camera initialization, mouse wheel zoom, drag panning, HUD buttons, and 2D scrollbars.
    static void Update(Context& ctx, Node* node);

    /// Measures the Scene node (including external label/description via ElementComposer).
    static Vec2 Measure(
        const SceneOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    );

    /// Renders background grid/checkerboard and world-space draw commands before child UI nodes.
    static void RenderBackgroundAndWorld(
        const Node* node,
        ICanvas& canvas,
        const UiTheme& theme
    );

    /// Renders 2D scrollbars, floating Zoom HUD, and border after child UI nodes.
    static void RenderOverlays(
        const Node* node,
        ICanvas& canvas,
        const UiTheme& theme
    );
};

} // namespace guikit
