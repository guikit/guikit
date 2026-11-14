#pragma once

#include "guikit/backend/i_canvas.hpp"
#include "guikit/core/container_scope.hpp"
#include "guikit/core/id.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "keel/arena.h"
#include "keel/array.h"
#include "keel/core.h"
#include "keel/optional.h"
#include "keel/ref.h"

namespace guikit {

class Context;
class SceneScope;
struct Node;

/// Visual background pattern for a 2D Scene viewport.
enum class SceneBackground : keel::u8 {
    Solid = 0,      ///< Solid theme background / surface fill
    Checkerboard,   ///< Transparency checkerboard (for PNG/SVG asset inspection)
    GridDots,       ///< World-space adaptive dot matrix (for schematics and node graphs)
    GridLines       ///< World-space adaptive coordinate grid lines (for CAD, maps, and plots)
};

/// Configuration options for an interactive world-space item inside a Scene.
struct SceneItemOptions {
    keel::StringRef id{};                       ///< Unique item identifier within the scene (e.g. "R100", "TR101")
    keel::StringRef title{};                    ///< Display value or part number (e.g. "220K", "BC 154")
    keel::StringRef category{};                 ///< Component category/type (e.g. "Resistor", "PNP Transistor")
    keel::Optional<Rect> hitRect{keel::kNullOpt}; ///< Explicit world-space hit box (auto-computed from geometry if nullopt)
    bool selectable{true};                      ///< Whether this item participates in hit-testing and selection
    bool showSelectionFrame{true};              ///< Whether to draw automatic CAD hover/selection box around worldBounds
    bool draggable{false};                      ///< Pressing the item drags it (SceneItemScope::GetDrag) instead of panning the scene; hit-tested even when SceneOptions::selectableItems is false
    UiText tooltip{};                           ///< Floating tooltip while the item is hovered (not while dragging)
};

/// Resolved metadata, interaction flags, and world-space bounding box of a recorded SceneItem.
struct SceneItemInfo {
    keel::u64 key{0};                ///< Deterministic 64-bit item key
    keel::StringRef id{};           ///< Frame-arena allocated identifier
    keel::StringRef title{};        ///< Frame-arena allocated title/value
    keel::StringRef category{};     ///< Frame-arena allocated category
    Rect worldBounds{};             ///< World-space bounding box of the item's geometry
    keel::i32 firstCmdIndex{0};       ///< First draw command index belonging to this item
    bool hasExplicitBounds{false};  ///< True if hitRect was explicitly provided
    bool selectable{true};
    bool showSelectionFrame{true};
    bool draggable{false};
    UiText tooltip{};               ///< Frame-arena allocated tooltip
    bool hovered{false};
    bool selected{false};
    bool clicked{false};
};

/// Pointer drag of a draggable SceneItem. The scene reports where the pointer is; the caller owns the
/// dragged value: remember it on `started`, apply `Delta()` while `active`, restore it on `canceled`.
struct SceneItemDrag {
    bool started{false};    ///< Pointer pressed on the item this frame (drag begins; `active` is also set)
    bool active{false};     ///< Pointer held: the item follows it
    bool ended{false};      ///< Pointer released this frame: commit
    bool canceled{false};   ///< Escape pressed while dragging: restore the value saved on `started`
    Vec2 startWorld{};      ///< Pointer world position at press
    Vec2 currentWorld{};    ///< Pointer world position now (last one on ended / canceled)

    [[nodiscard]] Vec2 Delta() const noexcept { return currentWorld - startWorld; }
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
    keel::u8 hoveredHudBtn{0};   ///< 1 = '-', 2 = 'Fit', 3 = '100%', 4 = '+'

    // Interactive SceneItem selection & marquee state
    keel::u64 hoveredItemKey{0};
    keel::u64 clickedItemKey{0};
    keel::Array<keel::u64> selectedItemKeys{};
    keel::u64 hoveredDraggableKey{0};    ///< hoveredItemKey when that item is draggable (press starts a drag)
    keel::u64 draggedItemKey{0};         ///< Item being dragged (0 = none)
    keel::u64 dragStartedKey{0};         ///< One-frame: drag started on this item
    keel::u64 dragEndedKey{0};           ///< One-frame: drag of this item ended (released or canceled)
    keel::u64 dragClickKey{0};           ///< Drag released without moving: reported as a click in Update
    bool dragCanceled{false};           ///< One-frame: the ended drag was canceled with Escape
    Vec2 dragStartWorld{0.0f, 0.0f};
    Vec2 dragCurrentWorld{0.0f, 0.0f};
    Vec2 dragStartMouse{0.0f, 0.0f};
    bool boxSelecting{false};
    Vec2 boxSelectStartWorld{0.0f, 0.0f};
    Vec2 boxSelectCurrentWorld{0.0f, 0.0f};
    keel::Optional<Vec2> pendingCenterWorld{keel::kNullOpt};
    keel::Optional<float> pendingCenterZoom{keel::kNullOpt};
    Rect lastViewport{};
    keel::Optional<Rect> lastWorldBounds{keel::kNullOpt};
    Padding lastFitPadding{};
    bool preUpdatedCamera{false};
    bool preUpdateHudConsumed{false};
    bool preUpdateScrollbarBusy{false};
    bool cameraModifiedByInput{false};

    [[nodiscard]] bool IsSelected(keel::u64 key) const noexcept {
        if (key == 0) return false;
        for (keel::u64 k : selectedItemKeys) {
            if (k == key) return true;
        }
        return false;
    }
};

/// Options for positioning, anchor-aligning, and styling world-space text labels in a Scene.
struct SceneTextOptions {
    FontStyle style{FontStyle::Regular};
    Vec2 align{0.0f, 0.0f};     ///< Anchor on text box: {0,0}=TopLeft, {0.5,0}=TopCenter, {0.5,1}=BottomCenter, {1,0.5}=MiddleRight
    Vec2 offset{0.0f, 0.0f};    ///< Screen-space offset in dp from worldPos (invariant to camera zoom)
    bool scaleWithZoom{false};  ///< false = constant readable UI font size in dp; true = scales with world zoom
    float angleDeg{0.0f};       ///< Rotation angle in degrees
};

/// Configuration options for a 2D Scene camera viewport.
struct SceneOptions {
    keel::Optional<Vec2> worldSize{keel::kNullOpt};     ///< Bounded world dimensions (0..W, 0..H), or nullopt for infinite world (OSM maps, graphs)
    keel::Optional<Rect> worldBounds{keel::kNullOpt};   ///< Arbitrary world coordinate rectangle (minX, minY, width, height) to frame in viewport
    Padding fitPadding{};                               ///< Screen-space inset padding in dp reserved around fitted worldBounds / worldSize
    ObjectFit initialFit{ObjectFit::Contain};           ///< Initial camera framing when worldSize / worldBounds is set
    SceneBackground backgroundStyle{SceneBackground::Checkerboard};
    float gridStep{32.0f};                              ///< Base world-space step in units for GridDots / GridLines
    Align align{Align::Start};
    StretchMode stretch{StretchMode::Fill};
    SizeMode widthMode{SizeMode::Stretch};
    SizeMode heightMode{SizeMode::Stretch};
    keel::Optional<float> width{keel::kNullOpt};        ///< Explicit width in dp (or legacy ratio 0.0..1.0 in Fill mode)
    keel::Optional<float> widthRatio{keel::kNullOpt};   ///< Normalized fill ratio 0.0..1.0 of bounds in Fill mode
    keel::Optional<float> height{keel::kNullOpt};       ///< Explicit height in px
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
    keel::Optional<bool> scaleStrokeWithZoom{keel::kNullOpt}; ///< Scale line/rect strokeWidth with zoom (defaults to false when worldBounds is set, true otherwise)
    StateColor background{};
    Border border{.width = 1.0f, .radius = 6.0f};
    UiText tooltip{};                                   ///< Floating tooltip while the scene is hovered and no item with its own tooltip is under the pointer
    bool disabled{false};
    const UiTheme* theme{nullptr};
    keel::Optional<float> scale{keel::kNullOpt};
    keel::Optional<bool> debugBounds{keel::kNullOpt};
    keel::Optional<bool> inspectMode{keel::kNullOpt};
};

/// Recorded world-space drawing command executed during Scene rendering.
struct SceneDrawCmd {
    enum class Type : keel::u8 {
        Image = 0,
        Pixels,
        Rect,
        Line,
        Triangle,
        Text,
        TextBox
    };
    Type type{Type::Rect};
    keel::i32 itemIndex{-1};      ///< Owning SceneItemInfo index in ScenePayload::items (-1 if unowned)
    keel::StringRef textOrSource{};
    const keel::u32* pixels{nullptr};
    keel::u32 pixelW{0};
    keel::u32 pixelH{0};
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
    keel::u8 opacity{255};
    bool scaleFontWithZoom{true};
};

/// Internal per-frame payload stored on NodeType::Scene.
struct ScenePayload {
    SceneOptions opts{};
    SceneState* state{nullptr};
    Rect openViewport{};
    keel::ArenaArray<SceneDrawCmd> drawCmds{};
    keel::ArenaArray<SceneItemInfo> items{};
    keel::i32 activeItemIndex{-1};
    keel::u64 contentHash{0};        ///< Running fingerprint of drawCmds (incl. text and pixel content) for damage tracking
};

/// RAII scope that groups world-space draw commands into an interactive, selectable SceneItem.
class SceneItemScope {
public:
    SceneItemScope(
        SceneScope* scene,
        keel::i32 itemIndex,
        keel::i32 prevItemIndex,
        bool hovered,
        bool selected,
        bool clicked,
        SceneItemDrag drag = {}
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
    /// True while the pointer drags this item (SceneItemOptions::draggable).
    [[nodiscard]] bool IsDragging() const noexcept { return m_drag.active; }
    /// Drag lifecycle and pointer positions for this frame (all false unless the item is draggable).
    [[nodiscard]] const SceneItemDrag& GetDrag() const noexcept { return m_drag; }
    [[nodiscard]] keel::i32 GetItemIndex() const noexcept { return m_itemIndex; }

private:
    SceneScope* m_scene{nullptr};
    keel::i32 m_itemIndex{-1};
    keel::i32 m_prevItemIndex{-1};
    bool m_hovered{false};
    bool m_selected{false};
    bool m_clicked{false};
    SceneItemDrag m_drag{};
};

/// RAII scope for a 2D Scene container providing World <-> Screen transforms and world-space drawing.
class SceneScope : public ContainerScope {
public:
    SceneScope(Context* ctx, Node* node, ScenePayload* payload, Rect prevViewport) noexcept;

    /// Declares an interactive world-space item whose bounding box is automatically computed from enclosed geometry.
    [[nodiscard]] SceneItemScope Item(const SceneItemOptions& opts);

    /// Returns all interactive items recorded in the scene during this frame.
    [[nodiscard]] keel::ArrayRef<SceneItemInfo> GetItems() const noexcept;

    /// Returns the currently hovered item (or nullptr if none).
    [[nodiscard]] const SceneItemInfo* GetHoveredItem() const noexcept;

    /// Returns the number of currently selected items.
    [[nodiscard]] keel::usize GetSelectedCount() const noexcept;

    /// Clears all selected items in the scene.
    void ClearSelection() noexcept;

    /// Selects an item by its string identifier.
    void SelectItem(keel::StringRef id, bool addToSelection = false) noexcept;

    /// Returns the current camera zoom multiplier.
    [[nodiscard]] float GetZoom() const noexcept;

    /// Returns the effective screen-pixels-per-world-unit scale factor (zoom * themeScale).
    [[nodiscard]] float GetWorldScale() const noexcept;

    /// Sets the camera zoom multiplier (clamped to [minZoom, maxZoom]).
    void SetZoom(float zoom) noexcept;

    /// Centers the camera on a specific world-space coordinate with an optional new zoom multiplier.
    void CenterOn(Vec2 worldPoint, keel::Optional<float> zoom = keel::kNullOpt) noexcept;

    /// Resets camera to initial fit framing.
    void ResetView() noexcept;

    /// Returns the current screen-space pan offset of the world origin (0,0).
    [[nodiscard]] Vec2 GetPan() const noexcept;

    /// Returns true if the user is actively dragging/panning the scene viewport.
    [[nodiscard]] bool IsPanning() const noexcept;

    /// Returns true if user input (mouse wheel zoom, drag pan, HUD controls, or scrollbars) modified camera zoom/pan in this frame.
    [[nodiscard]] bool WasCameraModified() const noexcept;

    /// Returns true if the mouse cursor is currently inside the scene viewport and not occluded by any overlay, panel, or child widget.
    [[nodiscard]] bool IsHovered() const noexcept;

    /// Returns true if the given mouse button (1=Left, 2=Middle, 3=Right) was clicked on the unoccluded scene canvas in this frame.
    [[nodiscard]] bool WasClicked(keel::i32 button = 1) const noexcept;

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
        keel::StringRef source,
        Rect worldRect,
        float radius = 0.0f,
        keel::u8 opacity = 255
    );

    /// Queues an in-memory ABGR8888 pixel buffer in World Coordinates.
    /// The buffer content takes part in damage tracking: it is hashed every frame unless
    /// `contentVersion` (non-zero, changed whenever the pixels change) is given.
    void DrawPixels(
        const keel::u32* pixels,
        keel::u32 width,
        keel::u32 height,
        Rect worldRect,
        float radius = 0.0f,
        keel::u8 opacity = 255,
        keel::u64 contentVersion = 0
    );

    /// Queues an in-memory ABGR8888 pixel slice in World Coordinates.
    void DrawPixels(
        keel::ArrayRef<keel::u32> pixels,
        keel::u32 width,
        keel::u32 height,
        Rect worldRect,
        float radius = 0.0f,
        keel::u8 opacity = 255,
        keel::u64 contentVersion = 0
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
        keel::i32 segments = 14
    );

    /// Queues a line segment in World Coordinates.
    void DrawLine(Vec2 worldP1, Vec2 worldP2, Color color, float strokeWidth = 1.0f);

    /// Queues a connected polyline (or closed polygon outline) in World Coordinates.
    void DrawPolyline(
        keel::ArrayRef<Vec2> points,
        Color color,
        float strokeWidth = 1.0f,
        bool closed = false
    );

    /// Queues a filled/stroked convex polygon in World Coordinates.
    void DrawPolygon(
        keel::ArrayRef<Vec2> points,
        Color fill,
        Color stroke = Color::Transparent(),
        float strokeWidth = 0.0f
    );

    /// Queues a text label in World Coordinates.
    void DrawText(
        keel::StringRef text,
        Vec2 worldPos,
        float fontSize,
        Color color,
        FontStyle style = FontStyle::Regular,
        bool scaleWithZoom = true,
        float angleDeg = 0.0f
    );

    /// Queues an anchor-aligned text label at a world coordinate with optional screen-space dp offset.
    void DrawText(
        keel::StringRef text,
        Vec2 worldPos,
        float fontSize,
        Color color,
        const SceneTextOptions& opts
    );

    /// Queues aligned text inside a World Coordinate bounding box (with optional rotation angle in degrees).
    void DrawTextBox(
        keel::StringRef text,
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
    void FinalizeItem(keel::i32 itemIndex, keel::i32 prevItemIndex) noexcept;
    /// Records a draw command and folds it into the content fingerprint: a scene whose drawing
    /// changes is repainted even when the camera does not move.
    void PushCmd(const SceneDrawCmd& cmd) noexcept;

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
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Opens an immediate mode 2D Scene container using an already-resolved widget ID (e.g. MapView wrapper).
    [[nodiscard]] static SceneScope OpenWithId(
        Context& ctx,
        Id explicitId,
        const SceneOptions& opts = {}
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
