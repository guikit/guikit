#pragma once

#include <cmath>
#include "keel/arena.h"
#include "keel/array.h"
#include "keel/core.h"
#include "keel/format.h"
#include "keel/function.h"
#include "keel/hash_map.h"
#include "keel/optional.h"
#include "keel/ref.h"
#include "keel/small_array.h"
#include "keel/string.h"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/core/id.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"
#include "guikit/core/solver.hpp"
#include "guikit/core/zone.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/container_scope.hpp"
#include "guikit/core/element.hpp"
#include "guikit/layout/row.hpp"
#include "guikit/layout/column.hpp"
#include "guikit/layout/wrap.hpp"
#include "guikit/layout/grid.hpp"
#include "guikit/layout/separator.hpp"
#include "guikit/layout/spacer.hpp"
#include "guikit/panels/accordion.hpp"
#include "guikit/panels/activity_bar.hpp"
#include "guikit/panels/bottom_panel.hpp"
#include "guikit/panels/box.hpp"
#include "guikit/panels/card.hpp"
#include "guikit/panels/carousel.hpp"
#include "guikit/panels/command_palette.hpp"
#if !defined(GUIKIT_HAS_SCENES)
#define GUIKIT_HAS_SCENES 0
#endif

#include "guikit/panels/scroll_area.hpp"
#include "guikit/panels/scene.hpp"
#if GUIKIT_HAS_SCENES
#include "guikit/panels/map_view.hpp"
#endif
#include "guikit/panels/context_menu.hpp"
#include "guikit/panels/modal.hpp"
#include "guikit/panels/popover.hpp"
#include "guikit/panels/sidebar.hpp"
#include "guikit/panels/split_view.hpp"
#include "guikit/panels/status_bar.hpp"
#include "guikit/panels/table.hpp"
#include "guikit/panels/toolbar.hpp"
#include "guikit/panels/tree_view.hpp"
#include "guikit/widgets/breadcrumb.hpp"
#include "guikit/widgets/button.hpp"
#include "guikit/widgets/checkbox.hpp"
#include "guikit/widgets/color_picker.hpp"
#include "guikit/widgets/date_picker.hpp"
#include "guikit/widgets/heading.hpp"
#include "guikit/widgets/image.hpp"
#include "guikit/widgets/input_field.hpp"
#include "guikit/widgets/knob.hpp"
#include "guikit/widgets/level_meter.hpp"
#include "guikit/widgets/markdown.hpp"
#include "guikit/widgets/pagination.hpp"
#include "guikit/widgets/radio.hpp"
#include "guikit/widgets/segmented.hpp"
#include "guikit/widgets/select.hpp"
#include "guikit/widgets/sidebar_menu.hpp"
#include "guikit/widgets/slider.hpp"
#include "guikit/widgets/spin_box.hpp"
#include "guikit/widgets/switch.hpp"
#include "guikit/widgets/tab_bar.hpp"
#include "guikit/widgets/tag_input.hpp"
#include "guikit/widgets/text.hpp"
#include "guikit/widgets/text_area.hpp"
#include "guikit/widgets/toast.hpp"
#include "guikit/graphics/colormap.hpp"
#include "guikit/widgets/histogram.hpp"
#include "guikit/widgets/image_view.hpp"
#if GUIKIT_HAS_SCENES
#include "guikit/widgets/signal_view.hpp"
#include "guikit/widgets/sonar_view.hpp"
#include "guikit/widgets/waterfall_view.hpp"
#include "guikit/widgets/attitude_view.hpp"
#include "guikit/widgets/bpmn_editor.hpp"
#include "guikit/widgets/car_cluster_view.hpp"
#endif

namespace guikit {

class Context;

/// C++20 concept for any autonomous widget providing a static Run method.
template <typename T, typename... Args>
concept WidgetRunnable = requires(Context& ctx, Args&&... args) {
    { T::Run(ctx, keel::forward<Args>(args)...) };
};

/// C++20 concept for any autonomous layout or panel container providing a static Open method.
template <typename T, typename... Args>
concept ContainerOpenable = requires(Context& ctx, Args&&... args) {
    { T::Open(ctx, keel::forward<Args>(args)...) };
};

/// Zero-header concept for non-const duck-typed string_view-like targets (e.g. external `std::string_view`).
template <typename SV>
concept MutableForeignStringView =
    !keel::IsConst<keel::RemoveReference<SV>> &&
    !keel::IsSame<keel::RemoveCvRef<SV>, keel::StringRef> &&
    !keel::WritableString<SV> &&
    keel::IsTriviallyCopyable<keel::RemoveCvRef<SV>> &&
    requires(SV& sv, const char* d, keel::usize n) {
        typename keel::RemoveCvRef<SV>::traits_type;
        { sv.data() } -> keel::detail::ConvertibleTo<const char*>;
        { sv.size() } -> keel::detail::ConvertibleTo<keel::usize>;
        sv = keel::RemoveCvRef<SV>(d, n);
    };

template <typename T>
concept SpinBoxNumeric =
    !keel::IsConst<keel::RemoveReference<T>> &&
    !keel::IsSame<keel::Decay<T>, bool> &&
    (keel::IsIntegral<keel::Decay<T>> || keel::IsFloatingPoint<keel::Decay<T>>);

template <typename T>
concept SelectTarget =
    !keel::IsConst<keel::RemoveReference<T>> &&
    (IndexLike<T> ||
     keel::WritableString<T> ||
     keel::IsSame<keel::RemoveCvRef<T>, keel::StringRef> ||
     MutableForeignStringView<T>);

template <typename T>
concept SliderNumeric =
    !keel::IsConst<keel::RemoveReference<T>> &&
    !keel::IsSame<keel::Decay<T>, bool> &&
    (keel::IsIntegral<keel::Decay<T>> || keel::IsFloatingPoint<keel::Decay<T>>);

namespace detail {

struct StateKey {
    Id id;
    keel::TypeId type;

    constexpr bool operator==(const StateKey& other) const noexcept {
        return id == other.id && type == other.type;
    }
};

struct StateKeyHash {
    keel::u64 operator()(const StateKey& k) const noexcept {
        const keel::u64 h1 = keel::Hash<Id>{}(k.id);
        const keel::u64 h2 = keel::Hash<keel::TypeId>{}(k.type);
        return keel::hash_u64(h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2)));
    }
};

struct StoredState {
    void* ptr{nullptr};
    void (*destroy)(void*) noexcept {nullptr};
    void (*trim)(void*) noexcept {nullptr};
    keel::u64 lastFrameActive{0};
    double lastTimeActiveSec{0.0};
    bool persistent{false};
    bool cachesTrimmed{false};

    StoredState() noexcept = default;

    template <typename T>
    static StoredState Create(T&& val, keel::u64 frameIndex, double timeSec, bool isPersistent = false) {
        using Decayed = keel::Decay<T>;
        Decayed* p = keel::default_allocator()->create<Decayed>(keel::forward<T>(val));
        StoredState s;
        s.ptr = p;
        s.destroy = [](void* raw) noexcept {
            keel::default_allocator()->destroy(static_cast<Decayed*>(raw));
        };
        if constexpr (requires(Decayed& obj) { obj.TrimInactiveCaches(); }) {
            s.trim = [](void* raw) noexcept {
                static_cast<Decayed*>(raw)->TrimInactiveCaches();
            };
        }
        s.lastFrameActive = frameIndex;
        s.lastTimeActiveSec = timeSec;
        s.persistent = isPersistent;
        return s;
    }

    ~StoredState() noexcept {
        if (ptr && destroy) {
            destroy(ptr);
        }
    }

    StoredState(const StoredState&) = delete;
    StoredState& operator=(const StoredState&) = delete;

    StoredState(StoredState&& other) noexcept
        : ptr(other.ptr)
        , destroy(other.destroy)
        , trim(other.trim)
        , lastFrameActive(other.lastFrameActive)
        , lastTimeActiveSec(other.lastTimeActiveSec)
        , persistent(other.persistent)
        , cachesTrimmed(other.cachesTrimmed) {
        other.ptr = nullptr;
        other.destroy = nullptr;
        other.trim = nullptr;
    }

    StoredState& operator=(StoredState&& other) noexcept {
        if (this != &other) {
            if (ptr && destroy) {
                destroy(ptr);
            }
            ptr = other.ptr;
            destroy = other.destroy;
            trim = other.trim;
            lastFrameActive = other.lastFrameActive;
            lastTimeActiveSec = other.lastTimeActiveSec;
            persistent = other.persistent;
            cachesTrimmed = other.cachesTrimmed;
            other.ptr = nullptr;
            other.destroy = nullptr;
            other.trim = nullptr;
        }
        return *this;
    }
};

} // namespace detail

/// Primary immediate mode context coordinating input, layout, and rendering.
class Context {
public:
    Context();
    ~Context();

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    // --- Frame Lifecycle ---
    /// Starts a frame and enters its root zone.
    ///
    /// `viewportSize` is in physical framebuffer pixels. The frame's single dp -> pixel factor is
    /// `displayScale * rootZoom`: `displayScale` is the OS / monitor scale and `rootZoom` is the user's UI zoom
    /// (the shell zoom). Layout, hit-testing and widget code work in dp and the canvas applies the factor
    /// uniformly, so text, strokes, spacing and input always scale together. A container with
    /// `.scale = ui.ScaleForZoom(z)` renders at `z` of `displayScale` and therefore does not follow `rootZoom`.
    void BeginFrame(Vec2 viewportSize, float displayScale = 1.0f, float rootZoom = 1.0f);
    void EndFrame(ICanvas& canvas);

    // --- Input Handling ---
    /// `pos` is in physical framebuffer pixels (same space as `BeginFrame`'s `viewportSize`);
    /// it is divided by the frame's pixels-per-dp factor to produce the dp mouse position, and is
    /// re-derived when that factor changes so the pointer stays under the cursor after a zoom.
    void OnMouseMove(Vec2 pos);
    void OnMouseDown(keel::i32 button);
    void OnMouseUp(keel::i32 button);
    void OnTextInput(keel::StringRef utf8Text);
    void OnKeyDown(Key key, KeyModifiers mods = {});
    void OnMouseWheel(float x, float y);

    /// Sets the requested OS mouse cursor shape for the current frame.
    void SetCursor(CursorType cursor) noexcept { m_cursor = cursor; }

    /// Returns the OS mouse cursor shape requested during the current frame.
    [[nodiscard]] CursorType GetCursor() const noexcept { return m_cursor; }

    // --- Reactive Repaint & Idle Management ---
    /// Request the event loop to repaint a given number of frames (e.g. 3 frames to settle hover/click).
    void RequestRepaint(keel::i32 frames = 3) noexcept;

    /// Returns the number of pending repaint frames left in the counter.
    [[nodiscard]] keel::i32 GetRepaintFramesLeft() const noexcept { return m_repaintFrames; }

    /// Returns true if the UI requires a repaint (pending frames, mouse held down, or active edit/popup).
    [[nodiscard]] bool NeedsRepaint() const noexcept;

    /// Sets the idle heartbeat interval in milliseconds (e.g. 500ms). Set to -1 or 0 to disable periodic heartbeat (deep sleep).
    void SetIdleHeartbeatMs(keel::i32 timeoutMs) noexcept { m_idleHeartbeatMs = timeoutMs; }

    /// Returns the configured idle heartbeat interval in milliseconds.
    [[nodiscard]] keel::i32 GetIdleHeartbeatMs() const noexcept { return m_idleHeartbeatMs; }

    /// Recommended timeout in milliseconds for SDL_WaitEventTimeout in reactive mode.
    [[nodiscard]] keel::i32 GetRecommendedSleepTimeoutMs() const noexcept;

    // --- Dirty Rect & Damage Tracking ---
    /// Enables or disables dirty damage rect tracking (enabled by default).
    void SetDirtyRectEnabled(bool enabled) noexcept { m_dirtyRectEnabled = enabled; }

    /// Returns whether dirty damage rect tracking is currently enabled.
    [[nodiscard]] bool IsDirtyRectEnabled() const noexcept { return m_dirtyRectEnabled; }

    // --- Window Live Resize Policy ---
    /// Enables or disables live UI reflow and redraw during interactive OS window border dragging (enabled by default).
    /// When false (deferred resize), the UI updates after the OS resize gesture step completes.
    void SetLiveResize(bool enabled) noexcept { m_liveResizeEnabled = enabled; }

    /// Returns whether live window resize is currently enabled.
    [[nodiscard]] bool IsLiveResizeEnabled() const noexcept { return m_liveResizeEnabled; }

    /// Sets the active color theme mode (Dark or Light) and adapts clear color & palette.
    void SetThemeMode(ThemeMode mode) noexcept;

    /// Returns the active theme mode (respects active `ui.Zone(...)` theme overrides).
    [[nodiscard]] ThemeMode GetThemeMode() const noexcept { return GetTheme().mode; }

    /// Returns true if dark mode is active in the current zone.
    [[nodiscard]] bool IsDarkMode() const noexcept { return GetTheme().mode == ThemeMode::Dark; }

    /// Returns the active theme palette tokens (respects active `ui.Zone(...)` theme overrides).
    [[nodiscard]] const ThemePalette& GetThemePalette() const noexcept { return GetTheme().palette; }

    /// Sets the full UI theme (palette, variants, metrics, typography, etc.).
    void SetTheme(const UiTheme& theme) noexcept {
        m_theme = theme;
        m_clearColor = m_theme.palette.background;
        InvalidateAll();
    }

    /// Returns the active UI theme: the root theme, or the theme of the innermost container zone that replaced it.
    [[nodiscard]] const UiTheme& GetTheme() const noexcept {
        const UiTheme* zt = GetZone()->theme;
        return (zt != nullptr) ? *zt : m_theme;
    }
    [[nodiscard]] UiTheme& GetTheme() noexcept {
        UiTheme* zt = GetZone()->theme;
        return (zt != nullptr) ? *zt : m_theme;
    }

    /// Sets the clear color used by EndFrame when clearing dirty damage regions.
    void SetClearColor(Color color) noexcept { m_clearColor = color; }

    /// Returns the current clear color.
    [[nodiscard]] Color GetClearColor() const noexcept { return m_clearColor; }

    /// Sets the root layout spacing/padding density multiplier (1.0f = standard).
    void SetDensity(float density) noexcept {
        m_rootDensity = (density > 0.0f) ? density : 1.0f;
        m_rootZone.density = m_rootDensity;
        InvalidateAll();
    }

    /// Returns the active spacing/padding density multiplier in the current container zone.
    [[nodiscard]] float GetDensity() const noexcept {
        return GetZone()->density;
    }

    // --- Developer Debug & Inspector Tools ---
    /// Sets whether 1px color-coded debug bounding boxes and padding overlays are rendered.
    void SetDebugBounds(bool enabled) noexcept;
    [[nodiscard]] bool IsDebugBounds() const noexcept { return m_debugBoundsEnabled; }

    /// Sets whether the interactive element inspector (Chrome DevTools style) is active.
    void SetInspectMode(bool enabled) noexcept;
    [[nodiscard]] bool IsInspectMode() const noexcept { return m_inspectModeEnabled; }

    /// Returns true if this frame produced any visual damage requiring rasterization / blitting.
    [[nodiscard]] bool HasDirtyDamage() const noexcept { return m_hasDirtyDamage; }

    /// Returns true if the entire frame required a full repaint (e.g. window resize or tab switch).
    [[nodiscard]] bool IsFullRepaint() const noexcept { return m_isFullRepaint; }

    /// Returns the active bounding rectangle of the dirty damage region for this frame (in logical points).
    [[nodiscard]] Rect GetDirtyRect() const noexcept { return m_dirtyRect; }

    /// Returns the list of mutually disjoint dirty damage rectangles for this frame (in logical points).
    [[nodiscard]] keel::ArrayRef<Rect> GetDirtyRects() const noexcept { return m_dirtyRects; }

    /// Returns the active bounding rectangle of the dirty damage region in physical pixels for GPU blitting.
    [[nodiscard]] Rect GetPhysicalDirtyRect() const noexcept {
        const float x1 = std::floor(m_dirtyRect.x * m_pixelsPerDp);
        const float y1 = std::floor(m_dirtyRect.y * m_pixelsPerDp);
        const float x2 = std::ceil(m_dirtyRect.Right() * m_pixelsPerDp);
        const float y2 = std::ceil(m_dirtyRect.Bottom() * m_pixelsPerDp);
        return Rect(x1, y1, keel::max(0.0f, x2 - x1), keel::max(0.0f, y2 - y1));
    }

    /// OS / monitor scale of this frame (100% = 1.0), as passed to `BeginFrame`.
    [[nodiscard]] float GetDisplayScale() const noexcept { return m_displayScale; }

    /// User UI zoom of the frame's root zone (the shell zoom), as passed to `BeginFrame`.
    [[nodiscard]] float GetRootZoom() const noexcept { return m_rootZoom; }

    /// The frame's single dp -> pixel factor: `GetDisplayScale() * GetRootZoom()`.
    [[nodiscard]] float GetPixelsPerDp() const noexcept { return m_pixelsPerDp; }

    /// The `.scale` a container opened in the current space needs so that its content renders at `zoom` relative
    /// to the display scale, independent of the root (shell) zoom and of any scaled ancestor.
    [[nodiscard]] float ScaleForZoom(float zoom) const noexcept { return zoom / (m_rootZoom * GetZone()->scale); }

    /// Coordinate spaces: a container with a scale gives its subtree a local coordinate system. Layout, widget code
    /// and every coordinate this API takes or returns (mouse position, previous-frame bounds, tooltip anchors, popup
    /// and overlay rects, damage rects) are in the space of the innermost open container; hit testing and damage
    /// tracking happen in frame space. This is the transform from the current space to frame space.
    [[nodiscard]] Transform2D CurrentSpace() const noexcept;

    /// The transform from the space a node was laid out in (its own local space) to frame space, as of the
    /// previous frame. Identity for nodes outside any scaled container.
    [[nodiscard]] Transform2D GetNodeSpace(Id id) const noexcept;

    /// Returns the current logical viewport size used for layout.
    [[nodiscard]] Vec2 GetLogicalViewport() const noexcept {
        return m_viewportSize / m_pixelsPerDp;
    }

    /// Returns the active popup/overlay viewport rectangle, intersected with any enclosing clipping Box container.
    [[nodiscard]] Rect GetPopupViewportRect() const noexcept;

    /// Explicitly forces the next frame to perform a full screen redraw.
    void InvalidateAll() noexcept;

    /// Explicitly invalidates a rectangular region to be repainted in the next frame.
    void InvalidateRect(Rect rect) noexcept;

    // --- Generic Autonomous Widget & Container Dispatch (Open-Closed Platform) ---
    /// Dispatches execution to any autonomous widget component satisfying WidgetRunnable.
    template <typename WidgetT, typename... Args>
        requires WidgetRunnable<WidgetT, Args...>
    decltype(auto) Run(Args&&... args) {
        struct ExecGuard {
            Context& c;
            ~ExecGuard() noexcept { c.EndWidgetExecution(); }
        } guard{*this};
        BeginWidgetExecution();
        return WidgetT::Run(*this, keel::forward<Args>(args)...);
    }

    /// Dispatches opening to any autonomous layout or panel container satisfying ContainerOpenable.
    template <typename ContainerT, typename... Args>
        requires ContainerOpenable<ContainerT, Args...>
    [[nodiscard]] decltype(auto) Open(Args&&... args) {
        return ContainerT::Open(*this, keel::forward<Args>(args)...);
    }

    // --- Layout Scopes ---
    [[nodiscard]] ContainerScope Row(
        RowOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] ContainerScope Column(
        ColumnOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] ContainerScope Wrap(
        WrapOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] GridScope Grid(
        GridOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    void Spacer(
        float flex = 1.0f,
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    void Gap(
        float sizeDp = 8.0f,
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    void Separator(
        float extentRatio = 0.0f,
        Color colorOverride = Color::Transparent(),
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    void Separator(
        Color colorOverride,
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    void Separator(
        Orientation lineOrientation,
        float extentRatio = 0.0f,
        Color colorOverride = Color::Transparent(),
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    // --- Panel Scopes ---
    [[nodiscard]] AccordionScope Accordion(
        AccordionOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] AccordionScope Accordion(
        keel::StringRef label,
        AccordionOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] AccordionScope Accordion(
        bool& open,
        AccordionOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    template <typename T, typename U>
        requires (!keel::IsSame<keel::Decay<T>, bool> && requires(T& a, const U& b) {
            static_cast<bool>(a == b);
            a = static_cast<T>(b);
        })
    [[nodiscard]] AccordionScope Accordion(
        T& activeValue,
        const U& sectionValue,
        AccordionOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        bool isOpen = static_cast<bool>(activeValue == sectionValue);
        auto scope = guikit::Accordion::Open(*this, &isOpen, opts, loc);
        if (scope.response.changed && isOpen) {
            activeValue = static_cast<T>(sectionValue);
        }
        return scope;
    }

    [[nodiscard]] ContainerScope Box(
        BoxOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] ContainerScope Card(
        CardOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] ContainerScope ScrollArea(
        ScrollAreaOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] ContainerScope Carousel(
        CarouselOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] SidebarScope Sidebar(
        SidebarOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] SidebarScope Sidebar(
        SidebarMode& mode,
        SidebarOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] SidebarScope Sidebar(
        bool& open,
        SidebarOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] ActivityBarScope ActivityBar(
        ActivityBarOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return guikit::ActivityBar::Open(*this, opts, loc);
    }

    [[nodiscard]] ActivityBarScope ActivityBar(
        SidebarMode& linkedSidebarMode,
        ActivityBarOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return guikit::ActivityBar::Open(*this, linkedSidebarMode, keel::move(opts), loc);
    }

    template <IndexLike IndexT>
        requires (!keel::IsConst<IndexT>)
    Response ActivityBar(
        IndexT& activeIndex,
        keel::ArrayRef<ActivityBarItem> items,
        ActivityBarOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        struct ExecGuard {
            Context& c;
            ~ExecGuard() noexcept { c.EndWidgetExecution(); }
        } guard{*this};
        BeginWidgetExecution();
        if constexpr (keel::IsSame<keel::Decay<IndexT>, keel::i32>) {
            return guikit::ActivityBar::Run(*this, activeIndex, items, opts, loc);
        } else {
            keel::i32 idx = static_cast<keel::i32>(activeIndex);
            Response r = guikit::ActivityBar::Run(*this, idx, items, opts, loc);
            if (r.changed && idx >= 0) {
                activeIndex = static_cast<IndexT>(idx);
            }
            return r;
        }
    }

    [[nodiscard]] BottomPanelScope BottomPanel(
        BottomPanelOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return guikit::BottomPanel::Open(*this, opts, loc);
    }

    [[nodiscard]] BottomPanelScope BottomPanel(
        BottomPanelMode& mode,
        BottomPanelOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return guikit::BottomPanel::Open(*this, &mode, opts, loc);
    }

    [[nodiscard]] BottomPanelScope BottomPanel(
        BottomPanelMode* mode,
        BottomPanelOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return guikit::BottomPanel::Open(*this, mode, opts, loc);
    }

    [[nodiscard]] BottomPanelScope BottomPanel(
        bool& open,
        BottomPanelOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return guikit::BottomPanel::Open(*this, &open, opts, loc);
    }

    [[nodiscard]] BottomPanelScope BottomPanel(
        bool* open,
        BottomPanelOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return guikit::BottomPanel::Open(*this, open, opts, loc);
    }

    [[nodiscard]] StatusBarScope StatusBar(
        StatusBarOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return guikit::StatusBar::Open(*this, opts, loc);
    }

    [[nodiscard]] SplitViewScope SplitView(
        const SplitViewOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return guikit::SplitView::Open(*this, opts, loc);
    }

    [[nodiscard]] SplitViewScope SplitView(
        float& ratio,
        const SplitViewOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return guikit::SplitView::Open(*this, ratio, opts, loc);
    }

    [[nodiscard]] SceneScope Scene(
        SceneOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

#if GUIKIT_HAS_SCENES
    [[nodiscard]] MapViewScope MapView(
        MapViewOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return Open<guikit::MapView>(opts, loc);
    }
#endif

    [[nodiscard]] TableScope Table(
        keel::ArrayRef<TableColumn> columns,
        const TableOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return guikit::Table::Open(*this, columns, opts, loc);
    }

    [[nodiscard]] TreeViewScope TreeView(
        TreeViewOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] TreeViewScope TreeView(
        keel::ArrayRef<TableColumn> columns,
        TreeViewOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        opts.columns = columns;
        return TreeView(keel::move(opts), loc);
    }

    [[nodiscard]] TreeItemScope TreeItem(
        TreeItemOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] TreeItemScope TreeItem(
        keel::StringRef label,
        TreeItemOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] TreeItemScope TreeItem(
        bool& open,
        TreeItemOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] TreeItemScope TreeItem(
        bool& open,
        keel::StringRef label,
        TreeItemOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    // --- Environment Zone Wrapper & Queries ---
    /// Opens a transparent environment scope overriding `theme`, `scale`, `density`, `debugBounds`, or `inspectMode`.
    [[nodiscard]] ZoneScope Zone(const ZoneOptions& opts = {});

    void PushZone(const ZoneOptions& opts);
    void PopZone() noexcept;

    [[nodiscard]] const guikit::Zone* GetZone() const noexcept {
        if (!m_zoneStack.empty() && m_zoneStack.back() != nullptr) {
            return m_zoneStack.back();
        }
        if (!m_containerStack.empty() && m_containerStack.back() != nullptr) {
            return m_containerStack.back()->zone;
        }
        return &m_rootZone;
    }

    [[nodiscard]] bool IsDisabled() const noexcept {
        return (!m_containerStack.empty() && m_containerStack.back() != nullptr && m_containerStack.back()->disabled) || m_rowDisabled;
    }

    // --- Widgets ---
    Response Button(
        ButtonOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    Response Text(
        UiText label,
        TextOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    // --- Element / Heading Widgets ---
    ElementScope Element(
        ElementOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    Response Heading(
        HeadingOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    // --- Image Widget ---
    Response Image(
        keel::StringRef source,
        ImageOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    Response Image(
        ImageOptions opts,
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    // --- Markdown Document Viewer Widget ---
    MarkdownResponse Markdown(
        keel::StringRef markdownText,
        const MarkdownOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

#if GUIKIT_HAS_SCENES
    // --- SignalView (Waveform + Spectrogram DAW Analyzer) Widget ---
    SignalViewResponse SignalView(
        keel::ArrayRef<float> samples,
        const SignalViewOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return Run<guikit::SignalView>(samples, opts, loc);
    }

    // --- SonarView (Polar Heatmap / Diver Detection Sonar / PPI Radar) Widget ---
    SonarViewResponse SonarView(
        keel::ArrayRef<float> polarMatrix,
        const SonarViewOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return Run<guikit::SonarView>(polarMatrix, opts, loc);
    }

    // --- WaterfallView (RTSA Spectrum Analyzer & DOA / BTR Target Tracker) Widget ---
    WaterfallViewResponse WaterfallView(
        keel::ArrayRef<float> latestSliceDb,
        const WaterfallViewOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return Run<guikit::WaterfallView>(latestSliceDb, opts, loc);
    }
#endif

    // --- Histogram (Single-Channel & Multi-Channel RGB+Lum Window/Level) Widget ---
    HistogramResponse Histogram(
        const HistogramData& data,
        ChannelTransferParams& transfer,
        const HistogramOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return Run<guikit::Histogram>(data, transfer, opts, loc);
    }

    // --- ImageView (Single-Channel 16-bit/Float & Multi-Channel RGB Workstation) Widget ---
    ImageViewResponse ImageView(
        const ImageViewData& data,
        ImageViewParams& params,
        const ImageViewOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return Run<guikit::ImageView>(data, params, opts, loc);
    }

    ImageViewResponse ImageView(
        const ImageViewData& data,
        const ImageViewOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return Run<guikit::ImageView>(data, opts, loc);
    }

#if GUIKIT_HAS_SCENES
    // --- AttitudeView (Avionics Artificial Horizon & Primary Flight Display) Widget ---
    AttitudeViewResponse AttitudeView(
        const AttitudeViewOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return Run<guikit::AttitudeView>(opts, loc);
    }

    // --- CarClusterView (Automotive Digital Instrument Cluster & ADAS Cockpit) Widget ---
    CarClusterViewResponse CarClusterView(
        const CarClusterViewOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return Run<guikit::CarClusterView>(opts, loc);
    }

    // --- BpmnEditor (BPMN 2.0 Diagram Modeler & Token Simulation) Widget ---
    BpmnEditorResponse BpmnEditor(
        BpmnDiagram& diagram,
        const BpmnEditorOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return Run<guikit::BpmnEditor>(diagram, opts, loc);
    }
#endif

    // --- Switch Widget ---
    Response Switch(
        bool& value,
        SwitchOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    Response Switch(
        const bool& checked,
        SwitchOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    Response Switch(const bool*, SwitchOptions = {}, keel::SourceLocation = keel::SourceLocation::current()) = delete;

    // --- Checkbox & Radio Widgets ---
    Response Checkbox(
        bool& value,
        CheckboxOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    Response Checkbox(
        const bool& checked,
        CheckboxOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    Response Checkbox(const bool*, CheckboxOptions = {}, keel::SourceLocation = keel::SourceLocation::current()) = delete;

    template <typename T, typename U = T>
        requires requires(const T& a, const U& b) { static_cast<bool>(a == b); }
    Response Radio(
        T& currentValue,
        const U& optionValue,
        RadioOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        const bool isSelected = static_cast<bool>(currentValue == optionValue);
        bool state = isSelected;
        Response r = Run<guikit::Radio>(&state, isSelected, opts, loc);
        if (r.clicked && !isSelected) {
            currentValue = static_cast<T>(optionValue);
            r.changed = true;
        } else {
            r.changed = false;
        }
        return r;
    }

    Response Radio(
        bool& value,
        RadioOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    Response Radio(
        const bool& checked,
        RadioOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    Response Radio(const bool*, RadioOptions = {}, keel::SourceLocation = keel::SourceLocation::current()) = delete;

    template <typename T, typename U = T>
        requires requires(const T& a, const U& b) { static_cast<bool>(a == b); }
    Response RadioButton(
        T& currentValue,
        const U& optionValue,
        RadioOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return Radio(currentValue, optionValue, keel::move(opts), loc);
    }

    Response RadioButton(
        bool& checked,
        RadioOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return Radio(checked, keel::move(opts), loc);
    }

    // --- InputField & SpinBox Widgets ---
    template <keel::WritableString S>
    Response InputField(
        S& value,
        InputFieldOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return Run<guikit::InputField>(keel::Optional<keel::StringSink>(keel::StringSink(value)), keel::StringRef{}, opts, loc);
    }

    Response InputField(
        keel::StringRef value,
        InputFieldOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    template <SpinBoxNumeric T>
    Response SpinBox(
        T& value,
        SpinBoxOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        using Decayed = keel::Decay<T>;
        if constexpr (keel::IsSame<Decayed, float>) {
            return Run<guikit::SpinBox>(&value, nullptr, nullptr, opts, loc);
        } else if constexpr (keel::IsSame<Decayed, double>) {
            return Run<guikit::SpinBox>(nullptr, &value, nullptr, opts, loc);
        } else if constexpr (keel::IsFloatingPoint<Decayed>) {
            double tmp = static_cast<double>(value);
            Response r = Run<guikit::SpinBox>(nullptr, &tmp, nullptr, opts, loc);
            if (r.changed) {
                value = static_cast<Decayed>(tmp);
            }
            return r;
        } else if constexpr (keel::IsSame<Decayed, keel::i64>) {
            return Run<guikit::SpinBox>(nullptr, nullptr, &value, opts, loc);
        } else {
            constexpr keel::i64 kInt64Max = static_cast<keel::i64>((~keel::u64{0}) >> 1);
            constexpr keel::i64 kInt64Min = -kInt64Max - 1;
            constexpr keel::usize kBits = sizeof(Decayed) * 8;
            constexpr keel::i64 kTypeMin = keel::IsUnsigned<Decayed>
                ? keel::i64{0}
                : ((sizeof(Decayed) >= sizeof(keel::i64)) ? kInt64Min : -(keel::i64{1} << (kBits - 1)));
            constexpr keel::i64 kTypeMax = keel::IsUnsigned<Decayed>
                ? ((sizeof(Decayed) >= sizeof(keel::u64))
                       ? kInt64Max
                       : static_cast<keel::i64>(static_cast<Decayed>(~Decayed{0})))
                : ((sizeof(Decayed) >= sizeof(keel::i64))
                       ? kInt64Max
                       : ((keel::i64{1} << (kBits - 1)) - 1));
            if constexpr (keel::IsUnsigned<Decayed>) {
                if (!opts.min.has_value()) {
                    opts.min = 0.0;
                } else {
                    opts.min = keel::max(0.0, *opts.min);
                }
            }
            keel::i64 tmp = (keel::IsUnsigned<Decayed> && static_cast<keel::u64>(value) > static_cast<keel::u64>(kTypeMax))
                ? kTypeMax
                : static_cast<keel::i64>(value);
            Response r = Run<guikit::SpinBox>(nullptr, nullptr, &tmp, opts, loc);
            if (r.changed) {
                value = static_cast<Decayed>(keel::clamp(tmp, kTypeMin, kTypeMax));
            }
            return r;
        }
    }

    template <SpinBoxNumeric T>
    Response SpinBox(
        T& value,
        double minVal,
        double maxVal,
        double stepVal = 1.0,
        SpinBoxOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        if (!opts.min.has_value()) opts.min = minVal;
        if (!opts.max.has_value()) opts.max = maxVal;
        if (!opts.step.has_value()) opts.step = stepVal;
        return SpinBox(value, keel::move(opts), loc);
    }

    // --- TextArea & CodeEditor Widgets ---
    template <keel::WritableString S>
    TextAreaResponse TextArea(
        S& value,
        const TextAreaOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return Run<guikit::TextArea>(keel::Optional<keel::StringSink>(keel::StringSink(value)), keel::StringRef{}, opts, loc);
    }

    TextAreaResponse TextArea(
        keel::StringRef value,
        const TextAreaOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return Run<guikit::TextArea>(keel::Optional<keel::StringSink>{}, value, opts, loc);
    }

    template <keel::WritableString S>
    CodeEditorResponse CodeEditor(
        S& value,
        const CodeEditorOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return TextArea(value, opts, loc);
    }

    CodeEditorResponse CodeEditor(
        keel::StringRef value,
        const CodeEditorOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return TextArea(value, opts, loc);
    }

    // --- Select (Dropdown) Widget ---
    template <SelectTarget T>
    Response Select(
        T& selected,
        keel::ArrayRef<SelectItem> items,
        SelectOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        if constexpr (keel::IsSame<keel::Decay<T>, keel::usize>) {
            return Run<guikit::Select>(&selected, nullptr, keel::Optional<keel::StringSink>{}, nullptr, items, opts, loc);
        } else if constexpr (keel::IsSame<keel::Decay<T>, keel::i32>) {
            return Run<guikit::Select>(nullptr, &selected, keel::Optional<keel::StringSink>{}, nullptr, items, opts, loc);
        } else if constexpr (IndexLike<T>) {
            constexpr bool isSignedIndex = []() constexpr {
                if constexpr (keel::IsEnum<keel::Decay<T>>) {
                    return keel::IsSigned<keel::UnderlyingType<keel::Decay<T>>>;
                } else {
                    return keel::IsSigned<keel::Decay<T>>;
                }
            }();
            if constexpr (isSignedIndex) {
                keel::i32 idx = static_cast<keel::i32>(selected);
                Response r = Run<guikit::Select>(nullptr, &idx, keel::Optional<keel::StringSink>{}, nullptr, items, opts, loc);
                if (r.changed) {
                    selected = static_cast<T>(idx);
                }
                return r;
            } else {
                keel::usize idx = static_cast<keel::usize>(selected);
                Response r = Run<guikit::Select>(&idx, nullptr, keel::Optional<keel::StringSink>{}, nullptr, items, opts, loc);
                if (r.changed) {
                    selected = static_cast<T>(idx);
                }
                return r;
            }
        } else if constexpr (keel::WritableString<T>) {
            return Run<guikit::Select>(nullptr, nullptr, keel::Optional<keel::StringSink>(keel::StringSink(selected)), nullptr, items, opts, loc);
        } else if constexpr (keel::IsSame<keel::RemoveCvRef<T>, keel::StringRef>) {
            return Run<guikit::Select>(nullptr, nullptr, keel::Optional<keel::StringSink>{}, &selected, items, opts, loc);
        } else if constexpr (MutableForeignStringView<T>) {
            keel::StringRef sr(selected.data(), static_cast<keel::usize>(selected.size()));
            Response r = Run<guikit::Select>(nullptr, nullptr, keel::Optional<keel::StringSink>{}, &sr, items, opts, loc);
            if (r.changed) {
                selected = keel::RemoveCvRef<T>(sr.data(), sr.size());
            }
            return r;
        }
    }

    template <SelectTarget T>
    Response Select(
        T& selected,
        keel::ArrayRef<keel::StringRef> items,
        SelectOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        if constexpr (keel::IsSame<keel::Decay<T>, keel::usize>) {
            return Run<guikit::Select>(&selected, nullptr, keel::Optional<keel::StringSink>{}, nullptr, items, opts, loc);
        } else if constexpr (keel::IsSame<keel::Decay<T>, keel::i32>) {
            return Run<guikit::Select>(nullptr, &selected, keel::Optional<keel::StringSink>{}, nullptr, items, opts, loc);
        } else if constexpr (IndexLike<T>) {
            constexpr bool isSignedIndex = []() constexpr {
                if constexpr (keel::IsEnum<keel::Decay<T>>) {
                    return keel::IsSigned<keel::UnderlyingType<keel::Decay<T>>>;
                } else {
                    return keel::IsSigned<keel::Decay<T>>;
                }
            }();
            if constexpr (isSignedIndex) {
                keel::i32 idx = static_cast<keel::i32>(selected);
                Response r = Run<guikit::Select>(nullptr, &idx, keel::Optional<keel::StringSink>{}, nullptr, items, opts, loc);
                if (r.changed) {
                    selected = static_cast<T>(idx);
                }
                return r;
            } else {
                keel::usize idx = static_cast<keel::usize>(selected);
                Response r = Run<guikit::Select>(&idx, nullptr, keel::Optional<keel::StringSink>{}, nullptr, items, opts, loc);
                if (r.changed) {
                    selected = static_cast<T>(idx);
                }
                return r;
            }
        } else if constexpr (keel::WritableString<T>) {
            return Run<guikit::Select>(nullptr, nullptr, keel::Optional<keel::StringSink>(keel::StringSink(selected)), nullptr, items, opts, loc);
        } else if constexpr (keel::IsSame<keel::RemoveCvRef<T>, keel::StringRef>) {
            return Run<guikit::Select>(nullptr, nullptr, keel::Optional<keel::StringSink>{}, &selected, items, opts, loc);
        } else if constexpr (MutableForeignStringView<T>) {
            keel::StringRef sr(selected.data(), static_cast<keel::usize>(selected.size()));
            Response r = Run<guikit::Select>(nullptr, nullptr, keel::Optional<keel::StringSink>{}, &sr, items, opts, loc);
            if (r.changed) {
                selected = keel::RemoveCvRef<T>(sr.data(), sr.size());
            }
            return r;
        }
    }

    // --- ComboBox (Searchable Dropdown) & Autocomplete Widgets ---
    template <SelectTarget T>
    Response ComboBox(
        T& selected,
        keel::ArrayRef<SelectItem> items,
        SelectOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        opts.searchable = true;
        return Select(selected, items, keel::move(opts), loc);
    }

    template <SelectTarget T>
    Response ComboBox(
        T& selected,
        keel::ArrayRef<keel::StringRef> items,
        SelectOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        opts.searchable = true;
        return Select(selected, items, keel::move(opts), loc);
    }

    template <keel::WritableString S>
    Response Autocomplete(
        S& value,
        keel::ArrayRef<SelectItem> items,
        SelectOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        opts.searchable = true;
        opts.editable = true;
        return Select(value, items, keel::move(opts), loc);
    }

    template <keel::WritableString S>
    Response Autocomplete(
        S& value,
        keel::ArrayRef<keel::StringRef> items,
        SelectOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        opts.searchable = true;
        opts.editable = true;
        return Select(value, items, keel::move(opts), loc);
    }

    template <WritableIndexList Arr>
    Response MultiSelect(
        Arr& selectedIndices,
        keel::ArrayRef<SelectItem> items,
        SelectOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        struct ExecGuard {
            Context& c;
            ~ExecGuard() noexcept { c.EndWidgetExecution(); }
        } guard{*this};
        BeginWidgetExecution();
        return guikit::Select::RunMulti(*this, IndexListSink(selectedIndices), items, opts, loc);
    }

    template <WritableIndexList Arr>
    Response MultiSelect(
        Arr& selectedIndices,
        keel::ArrayRef<keel::StringRef> items,
        SelectOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        struct ExecGuard {
            Context& c;
            ~ExecGuard() noexcept { c.EndWidgetExecution(); }
        } guard{*this};
        BeginWidgetExecution();
        return guikit::Select::RunMulti(*this, IndexListSink(selectedIndices), items, opts, loc);
    }

    // --- TabBar Component ---
    template <IndexLike IndexT>
        requires (!keel::IsConst<IndexT>)
    TabBarResponse TabBar(
        IndexT& activeIndex,
        keel::ArrayRef<TabItem> tabs,
        TabBarOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        struct ExecGuard {
            Context& c;
            ~ExecGuard() noexcept { c.EndWidgetExecution(); }
        } guard{*this};
        BeginWidgetExecution();
        keel::i32 idx = static_cast<keel::i32>(activeIndex);
        TabBarResponse r = TabBar::Run(*this, idx, tabs, opts, loc);
        if (r.changed && idx >= 0) activeIndex = static_cast<IndexT>(idx);
        return r;
    }

    // --- SidebarMenu Component ---
    [[nodiscard]] SidebarMenuScope SidebarMenu(
        SidebarMenuOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    template <IndexLike IndexT>
        requires (!keel::IsConst<IndexT>)
    Response SidebarMenu(
        IndexT& activeIndex,
        keel::ArrayRef<SidebarMenuItem> items,
        SidebarMenuOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        struct ExecGuard {
            Context& c;
            ~ExecGuard() noexcept { c.EndWidgetExecution(); }
        } guard{*this};
        BeginWidgetExecution();
        keel::i32 idx = static_cast<keel::i32>(activeIndex);
        Response r = SidebarMenu::Run(*this, idx, items, opts, loc);
        if (r.changed && idx >= 0) activeIndex = static_cast<IndexT>(idx);
        return r;
    }

    // --- Slider, StepSlider, RangeSlider & ProgressBar Widgets ---
    template <SliderNumeric T>
    Response Slider(
        T& value,
        T min,
        T max,
        SliderOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        if constexpr (keel::IsSame<keel::Decay<T>, double>) {
            return Run<guikit::Slider>(&value, nullptr, nullptr, min, max, opts, loc);
        } else if constexpr (keel::IsSame<keel::Decay<T>, float>) {
            return Run<guikit::Slider>(nullptr, &value, nullptr, static_cast<double>(min), static_cast<double>(max), opts, loc);
        } else {
            keel::i64 val64 = static_cast<keel::i64>(value);
            Response res = Run<guikit::Slider>(nullptr, nullptr, &val64, static_cast<double>(min), static_cast<double>(max), opts, loc);
            if (res.changed) {
                value = static_cast<T>(val64);
            }
            return res;
        }
    }

    template <SliderNumeric T>
    Response RangeSlider(
        T& lowValue,
        T& highValue,
        T min,
        T max,
        RangeSliderOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        if constexpr (keel::IsSame<keel::Decay<T>, double>) {
            return Run<guikit::RangeSlider>(&lowValue, &highValue, nullptr, nullptr, nullptr, nullptr, min, max, opts, loc);
        } else if constexpr (keel::IsSame<keel::Decay<T>, float>) {
            return Run<guikit::RangeSlider>(nullptr, nullptr, &lowValue, &highValue, nullptr, nullptr, static_cast<double>(min), static_cast<double>(max), opts, loc);
        } else {
            keel::i64 low64 = static_cast<keel::i64>(lowValue);
            keel::i64 high64 = static_cast<keel::i64>(highValue);
            Response res = Run<guikit::RangeSlider>(nullptr, nullptr, nullptr, nullptr, &low64, &high64, static_cast<double>(min), static_cast<double>(max), opts, loc);
            if (res.changed) {
                lowValue = static_cast<T>(low64);
                highValue = static_cast<T>(high64);
            }
            return res;
        }
    }

    Response ProgressBar(
        double value,
        double min,
        double max,
        ProgressBarOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return Run<guikit::ProgressBar>(value, min, max, opts, loc);
    }

    Response ProgressBar(
        double percent0to100,
        ProgressBarOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return Run<guikit::ProgressBar>(percent0to100, 0.0, 100.0, opts, loc);
    }

    template <IndexLike IndexT>
        requires (!keel::IsConst<IndexT>)
    Response StepSlider(
        IndexT& index,
        keel::usize totalSteps,
        StepSliderOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        if constexpr (keel::IsSame<keel::Decay<IndexT>, keel::usize>) {
            return Run<guikit::StepSlider>(&index, nullptr, totalSteps, opts, loc);
        } else if constexpr (keel::IsSame<keel::Decay<IndexT>, keel::i32>) {
            return Run<guikit::StepSlider>(nullptr, &index, totalSteps, opts, loc);
        } else {
            keel::i32 idxInt = static_cast<keel::i32>(index);
            Response res = Run<guikit::StepSlider>(nullptr, &idxInt, totalSteps, opts, loc);
            if (res.changed) {
                index = static_cast<IndexT>(idxInt);
            }
            return res;
        }
    }

    template <IndexLike IndexT>
        requires (!keel::IsConst<IndexT>)
    Response StepSlider(
        IndexT& index,
        keel::ArrayRef<keel::StringRef> labels,
        StepSliderOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        opts.stepLabels = labels;
        return StepSlider(index, labels.size(), opts, loc);
    }

    Response StepSlider(
        UiSize& size,
        StepSliderOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    Response StepSlider(
        UiVariant& variant,
        StepSliderOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    // --- Knob Widget ---
    template <SliderNumeric T>
    Response Knob(
        T& value,
        T min,
        T max,
        KnobOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        if constexpr (keel::IsSame<keel::Decay<T>, double>) {
            return Run<guikit::Knob>(&value, nullptr, nullptr, min, max, opts, loc);
        } else if constexpr (keel::IsSame<keel::Decay<T>, float>) {
            return Run<guikit::Knob>(nullptr, &value, nullptr, static_cast<double>(min), static_cast<double>(max), opts, loc);
        } else if constexpr (keel::IsSame<keel::Decay<T>, keel::i64>) {
            return Run<guikit::Knob>(nullptr, nullptr, &value, static_cast<double>(min), static_cast<double>(max), opts, loc);
        } else {
            keel::i64 valInt = static_cast<keel::i64>(value);
            Response res = Run<guikit::Knob>(nullptr, nullptr, &valInt, static_cast<double>(min), static_cast<double>(max), opts, loc);
            if (res.changed) {
                value = static_cast<T>(valInt);
            }
            return res;
        }
    }

    // --- Segmented Control Widget ---
    template <IndexLike IndexT>
        requires (!keel::IsConst<IndexT>)
    Response Segmented(
        IndexT& selectedIndex,
        keel::ArrayRef<SegmentedItem> items,
        SegmentedOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        if constexpr (keel::IsSame<keel::Decay<IndexT>, keel::usize>) {
            return Run<guikit::Segmented>(&selectedIndex, nullptr, nullptr, items, opts, loc);
        } else {
            keel::i32 idx = static_cast<keel::i32>(selectedIndex);
            Response r = Run<guikit::Segmented>(nullptr, &idx, nullptr, items, opts, loc);
            if (r.changed && idx >= 0) {
                selectedIndex = static_cast<IndexT>(idx);
            }
            return r;
        }
    }

    template <IndexLike IndexT>
        requires (!keel::IsConst<IndexT>)
    Response Segmented(
        IndexT& selectedIndex,
        keel::ArrayRef<keel::StringRef> items,
        SegmentedOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        if constexpr (keel::IsSame<keel::Decay<IndexT>, keel::usize>) {
            return Run<guikit::Segmented>(&selectedIndex, nullptr, nullptr, items, opts, loc);
        } else {
            keel::i32 idx = static_cast<keel::i32>(selectedIndex);
            Response r = Run<guikit::Segmented>(nullptr, &idx, nullptr, items, opts, loc);
            if (r.changed && idx >= 0) {
                selectedIndex = static_cast<IndexT>(idx);
            }
            return r;
        }
    }

    Response SegmentedMask(
        keel::u32& selectedMask,
        keel::ArrayRef<SegmentedItem> items,
        SegmentedOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return Run<guikit::Segmented>(nullptr, nullptr, &selectedMask, items, opts, loc);
    }

    Response SegmentedMask(
        keel::u32& selectedMask,
        keel::ArrayRef<keel::StringRef> items,
        SegmentedOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return Run<guikit::Segmented>(nullptr, nullptr, &selectedMask, items, opts, loc);
    }

    // --- ColorPicker Widget ---
    Response ColorPicker(
        Color& value,
        ColorPickerOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return Run<guikit::ColorPicker>(value, opts, loc);
    }

    // --- DatePicker Widget ---
    Response DatePicker(
        DateTime& value,
        DatePickerOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return Run<guikit::DatePicker>(value, opts, loc);
    }

    // --- LevelMeter Widget ---
    Response LevelMeter(
        double value,
        double min = -60.0,
        double max = 0.0,
        LevelMeterOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return Run<guikit::LevelMeter>(value, min, max, opts, loc);
    }

    Response LevelMeter(
        keel::ArrayRef<float> channelValues,
        float min = -60.0f,
        float max = 0.0f,
        LevelMeterOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return Run<guikit::LevelMeter>(channelValues, min, max, opts, loc);
    }

    // --- TagInput Widget ---
    template <WritableTagStringList Arr>
    Response TagInput(
        Arr& tags,
        TagInputOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return Run<guikit::TagInput>(StringTagListSink(tags), opts, loc);
    }

    template <WritableTagItemList Arr>
    Response TagInput(
        Arr& tags,
        TagInputOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return Run<guikit::TagInput>(TagItemListSink(tags), opts, loc);
    }

    // --- Breadcrumb & Pagination Navigation Widgets ---
    template <IndexLike IndexT>
        requires (!keel::IsConst<IndexT>)
    Response Breadcrumb(
        IndexT& activeIndex,
        keel::ArrayRef<BreadcrumbItem> items,
        BreadcrumbOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        if constexpr (keel::IsSame<keel::Decay<IndexT>, keel::usize>) {
            return Run<guikit::Breadcrumb>(activeIndex, items, opts, loc);
        } else {
            keel::i32 idx = static_cast<keel::i32>(activeIndex);
            Response r = Run<guikit::Breadcrumb>(idx, items, opts, loc);
            if (r.changed && idx >= 0) activeIndex = static_cast<IndexT>(idx);
            return r;
        }
    }

    template <IndexLike IndexT>
        requires (!keel::IsConst<IndexT>)
    Response Breadcrumb(
        IndexT& activeIndex,
        keel::ArrayRef<keel::StringRef> items,
        BreadcrumbOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        if constexpr (keel::IsSame<keel::Decay<IndexT>, keel::usize>) {
            return Run<guikit::Breadcrumb>(activeIndex, items, opts, loc);
        } else {
            keel::i32 idx = static_cast<keel::i32>(activeIndex);
            Response r = Run<guikit::Breadcrumb>(idx, items, opts, loc);
            if (r.changed && idx >= 0) activeIndex = static_cast<IndexT>(idx);
            return r;
        }
    }

    template <IndexLike IndexT, IndexLike CountT>
        requires (!keel::IsConst<IndexT>)
    Response Pagination(
        IndexT& currentPage,
        CountT totalPages,
        PaginationOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        if constexpr (keel::IsSame<keel::Decay<IndexT>, keel::usize>) {
            return Run<guikit::Pagination>(currentPage, static_cast<keel::usize>(totalPages), opts, loc);
        } else {
            keel::i32 page = static_cast<keel::i32>(currentPage);
            Response r = Run<guikit::Pagination>(page, static_cast<keel::i32>(totalPages), opts, loc);
            if (r.changed && page >= 0) currentPage = static_cast<IndexT>(page);
            return r;
        }
    }

    // --- CommandPalette Overlay Component ---
    CommandPaletteResponse CommandPalette(
        bool& isOpen,
        keel::ArrayRef<CommandPaletteItem> items,
        const CommandPaletteOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return Run<guikit::CommandPalette>(isOpen, items, opts, loc);
    }

    // --- Toolbar Component ---
    [[nodiscard]] ToolbarScope Toolbar(
        ToolbarOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    Response ToolButton(
        ToolButtonOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    // --- Modal Dialog Container ---
    [[nodiscard]] ModalScope Modal(
        bool& open,
        const ModalOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return guikit::Modal::Open(*this, open, opts, loc);
    }

    // --- Floating Popover Container ---
    [[nodiscard]] PopoverScope Popover(
        bool& open,
        const PopoverOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return guikit::Popover::Open(*this, open, opts, loc);
    }

    // --- ContextMenu Popup Component ---
    ContextMenuResponse ContextMenu(
        const Response& triggerTarget,
        keel::ArrayRef<ContextMenuItem> items,
        const ContextMenuOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return guikit::ContextMenu::Run(*this, nullptr, keel::kNullOpt, triggerTarget.rect, items, opts, loc);
    }

    ContextMenuResponse ContextMenu(
        Rect triggerRect,
        keel::ArrayRef<ContextMenuItem> items,
        const ContextMenuOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return guikit::ContextMenu::Run(*this, nullptr, keel::kNullOpt, triggerRect, items, opts, loc);
    }

    ContextMenuResponse ContextMenu(
        keel::ArrayRef<ContextMenuItem> items,
        const ContextMenuOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        const Id parentId = m_containerStack.empty() ? Id::Null() : m_containerStack.back()->id;
        const Rect parentRect = GetPrevFrameBounds(parentId);
        return guikit::ContextMenu::Run(*this, nullptr, keel::kNullOpt, parentRect, items, opts, loc);
    }

    ContextMenuResponse ContextMenu(
        bool& open,
        keel::ArrayRef<ContextMenuItem> items,
        const ContextMenuOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        const Id parentId = m_containerStack.empty() ? Id::Null() : m_containerStack.back()->id;
        const Rect parentRect = GetPrevFrameBounds(parentId);
        return guikit::ContextMenu::Run(*this, &open, keel::kNullOpt, parentRect, items, opts, loc);
    }

    ContextMenuResponse ContextMenu(
        bool& open,
        Vec2 anchorPos,
        keel::ArrayRef<ContextMenuItem> items,
        const ContextMenuOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return guikit::ContextMenu::Run(*this, &open, anchorPos, Rect{}, items, opts, loc);
    }

    // --- Toast Notification System ---
    keel::u64 ShowToast(ToastOptions opts);
    void DismissToast(keel::u64 id) noexcept;
    void ClearToasts() noexcept;
    [[nodiscard]] keel::usize GetActiveToastCount() const noexcept { return m_toasts.size(); }
    [[nodiscard]] keel::Array<ToastEntry>& GetToastEntries() noexcept { return m_toasts; }
    [[nodiscard]] keel::ArrayRef<ToastEntry> GetToastEntries() const noexcept { return m_toasts; }
    void MarkToastHostVisited() noexcept { m_toastHostVisitedThisFrame = true; }
    void SetToastHostOptions(const ToastHostOptions& opts) noexcept { m_defaultToastHostOpts = opts; }
    [[nodiscard]] const ToastHostOptions& GetToastHostOptions() const noexcept { return m_defaultToastHostOpts; }
    void ToastHost(const ToastHostOptions& opts = {}) {
        guikit::Toast::RunHost(*this, opts);
    }

    // --- String & Formatting Services ---
    [[nodiscard]] keel::StringRef String(keel::StringRef text);
    [[nodiscard]] keel::StringRef AllocString(keel::StringRef text);
    [[nodiscard]] UiText AllocText(UiText t) {
        if (!t.text.empty()) {
            t.text = AllocString(t.text);
        }
        if (!t.variants.empty()) {
            const keel::usize count = t.variants.size();
            auto* arr = m_arena.alloc_array<keel::StringRef>(count);
            keel::usize i = 0;
            for (keel::StringRef v : t.variants) {
                arr[i++] = v.empty() ? keel::StringRef{} : AllocString(v);
            }
            t.variants = keel::ArrayRef<keel::StringRef>(arr, count);
        }
        return t;
    }

    [[nodiscard]] Icon AllocIcon(Icon ic) {
        if (!ic.GetPath().empty()) {
            return Icon::File(AllocString(ic.GetPath()));
        }
        return ic;
    }

    [[nodiscard]] UiAffix AllocAffix(UiAffix a) {
        a.text = AllocText(a.text);
        a.icon = AllocIcon(a.icon);
        return a;
    }

    template <typename... Args>
    [[nodiscard]] keel::StringRef Format(keel::StringRef fmt, const Args&... args) {
        return keel::format_arena(m_arena, fmt, args...);
    }

    // --- Core Engine Services for Widgets ---
    Id MakeChildId(keel::SourceLocation loc);
    Id MakeChildIdForParent(Id parentId, keel::SourceLocation loc);
    Response Interact(Id id);
    Node* CreateNode(NodeType type, Id id, bool disabled = false);

    /// Allocates and constructs arbitrary widget payload data in the per-frame scratch arena.
    template <typename T, typename... Args>
    [[nodiscard]] T* AllocPayload(Args&&... args) {
        return m_arena.create<T>(keel::forward<Args>(args)...);
    }

    [[nodiscard]] keel::Arena& GetArena() noexcept { return m_arena; }

    void AdaptChildToContainer(Node* node);
    [[nodiscard]] Rect GetPrevFrameBounds(Id id) const;
    [[nodiscard]] Rect GetPrevUnclippedBounds(Id id) const;
    [[nodiscard]] Rect GetParentPrevFrameBounds() const;

    // --- Internal Hierarchy & Scope Management ---
    /// Makes `node` the innermost container. Its children inherit its resolved zone and disabled state,
    /// and `PopContainer()` restores the enclosing container.
    void PushContainer(Node* node);

    void PopContainer(Node* node);

    /// Number of open containers, including the frame's root container.
    [[nodiscard]] keel::usize GetContainerDepth() const noexcept { return m_containerStack.size(); }

    /// Converts scaled outer sizing fields into parent units and pushes `node` onto the container stack.
    void FinishContainer(Node* node) {
        if (node != nullptr) {
            node->SetOuterScale(node->GetRelativeScale());
        }
        PushContainer(node);
    }

    [[nodiscard]] Node* GetCurrentContainer() const noexcept {
        return m_containerStack.empty() ? nullptr : m_containerStack.back();
    }

    // Imperative actions requested from Response
    void RequestFocus(Id id);
    [[nodiscard]] Id GetFocusedId() const noexcept { return m_focusedId; }
    [[nodiscard]] bool IsFocused(Id id) const noexcept {
        if (!id.IsValid() || m_focusedId != id) return false;
        if (m_openPopupId.IsValid() && m_openPopupId != id) return false;
        if (m_modalScopeDepth == 0 && (m_currFrameModalId.IsValid() || m_prevFrameModalId.IsValid())) return false;
        return true;
    }
    [[nodiscard]] bool HasActiveTextEdit(Id id) const noexcept { return m_editState.id == id && id.IsValid(); }
    void ClearFocus() noexcept {
        if (m_editState.id == m_focusedId) {
            FlushActiveEditHistory();
            m_editState.id = Id::Null();
        }
        m_focusedId = Id::Null();
    }

    [[nodiscard]] Id GetActiveId() const noexcept { return m_activeId; }
    void SetActiveId(Id id) noexcept { m_activeId = id; }
    [[nodiscard]] keel::u64 GetFrameIndex() const noexcept { return m_frameIndex; }
    [[nodiscard]] keel::usize GetCustomStateCount() const noexcept { return m_customStates.size(); }
    [[nodiscard]] ICanvas* GetCanvas() const noexcept { return m_lastCanvas; }

    // --- Clipboard & Host Services ---
    using ClipboardGetFn = keel::Function<keel::String()>;
    using ClipboardSetFn = keel::Function<void(keel::StringRef)>;
    using OpenUrlFn = keel::Function<void(keel::StringRef)>;

    void SetClipboardCallbacks(ClipboardGetFn getFn, ClipboardSetFn setFn);
    void SetClipboardText(keel::StringRef text);
    [[nodiscard]] keel::String GetClipboardText() const;

    void SetOpenUrlCallback(OpenUrlFn fn) { m_openUrlFn = keel::move(fn); }
    void OpenUrl(keel::StringRef url) const {
        if (!url.empty() && m_openUrlFn) {
            m_openUrlFn(url);
        }
    }

    keel::StringRef EnsureTextEdit(
        Id id,
        keel::StringRef initialText,
        bool multiline = false,
        bool readOnly = false
    );

    keel::StringRef GetOrStartTextEdit(
        Id id,
        keel::StringRef initialText,
        keel::usize& outCursor,
        bool& outChanged,
        bool& outSubmitted,
        bool multiline = false
    );

    keel::StringRef GetOrStartTextEdit(
        Id id,
        keel::StringRef initialText,
        keel::usize& outCursor,
        keel::usize& outSelectionAnchor,
        bool& outChanged,
        bool& outSubmitted,
        bool multiline = false,
        bool readOnly = false
    );

    void SetTextEditBuffer(Id id, keel::StringRef text);
    void SetTextEditCursor(Id id, keel::usize cursor, keel::usize selectionAnchor) noexcept;
    void SetTextEditAutoClose(Id id, bool enabled) noexcept;
    void SetTextEditIndentation(Id id, keel::u8 tabSize, bool insertSpaces) noexcept;
    void ApplyTextEditChange(Id id, keel::StringRef newText, keel::usize newCursor, keel::usize newSelectionAnchor);
    bool UndoTextEdit(Id id);
    bool RedoTextEdit(Id id);
    void SetSuspendDirectEditInput(bool suspend) noexcept { m_suspendDirectEditInput = suspend; }
    [[nodiscard]] bool GetTextEditSelection(Id id, keel::usize& outStart, keel::usize& outEnd) const noexcept;
    [[nodiscard]] keel::usize GetTextEditAnchor(Id id) const noexcept {
        return (m_editState.id == id && id.IsValid()) ? m_editState.selectionAnchor : 0;
    }

    // --- Cursor Configuration & State ---
    void SetCursorConfig(const CursorConfig& config) noexcept { m_cursorConfig = config; }
    [[nodiscard]] const CursorConfig& GetCursorConfig() const noexcept { return m_cursorConfig; }
    [[nodiscard]] bool IsCursorVisible(keel::Optional<CursorBlinkMode> modeOverride = keel::kNullOpt) const noexcept;
    void ResetCursorBlink() noexcept;

#ifdef GetCurrentTime
#undef GetCurrentTime
#endif
    // --- Deterministic Time Control (for unit tests / animations) ---
    void SetCurrentTime(double timeSeconds) noexcept;
    void AdvanceTime(double deltaSeconds) noexcept;
    [[nodiscard]] double GetCurrentTime() const noexcept { return m_currentTimeSec; }
    [[nodiscard]] double GetCurrentTimeSec() const noexcept { return m_currentTimeSec; }

    // --- Popup, Tooltip, Modal & Overlay Management ---
    struct OverlayHitRegion {
        Id id{Id::Null()};
        Rect rect{};
    };

    void OpenPopup(Id id, Rect bounds);
    void ClosePopup();
    [[nodiscard]] bool IsPopupOpen(Id id) const noexcept { return m_openPopupId == id && id.IsValid(); }
    [[nodiscard]] Id GetOpenPopupId() const noexcept { return m_openPopupId; }
    [[nodiscard]] Rect GetOpenPopupRect() const noexcept { return CurrentSpace().Invert(m_openPopupRect); }
    void SetOpenPopupVisualHash(keel::u64 hash) noexcept { m_openPopupVisualHash = hash; }
    template <typename F>
        requires requires(keel::Decay<F>& f, ICanvas& c) { f(c); }
    void RegisterOverlay(F&& drawFn) {
        using Decayed = keel::Decay<F>;
        if constexpr (keel::IsPointer<Decayed> || requires(const Decayed& f) { static_cast<bool>(f); }) {
            if (!static_cast<bool>(drawFn)) return;
        }
        Decayed* stored = m_arena.create<Decayed>(keel::forward<F>(drawFn));
        m_overlayPasses.push_back(OverlayPass{
            .ctx = stored,
            .invoke = +[](void* p, ICanvas& c) { (*static_cast<Decayed*>(p))(c); },
            .destroy = keel::IsTriviallyDestructible<Decayed>
                ? nullptr
                : +[](void* p) noexcept { static_cast<Decayed*>(p)->~Decayed(); },
            .clipRect = PopupViewportInFrame(),
            .space = CurrentSpace()
        });
    }
    void MarkModalOverlayActive() noexcept { m_currFrameModalOverlay = true; }
    void RegisterOverlayHitRegion(Id id, Rect rect);
    template <typename F>
        requires requires(keel::Decay<F>& f) { f(); }
    void RegisterOverlayHitBuilder(F&& builderFn) {
        using Decayed = keel::Decay<F>;
        if constexpr (keel::IsPointer<Decayed> || requires(const Decayed& f) { static_cast<bool>(f); }) {
            if (!static_cast<bool>(builderFn)) return;
        }
        Decayed* stored = m_arena.create<Decayed>(keel::forward<F>(builderFn));
        m_overlayHitBuilders.push_back(OverlayHitBuilder{
            .ctx = stored,
            .invoke = +[](void* p) { (*static_cast<Decayed*>(p))(); },
            .destroy = keel::IsTriviallyDestructible<Decayed>
                ? nullptr
                : +[](void* p) noexcept { static_cast<Decayed*>(p)->~Decayed(); },
            .space = CurrentSpace()
        });
    }
    /// RAII scope guard that enters an overlay's hit-testing context and restores the previous overlay ID on exit.
    class [[nodiscard]] OverlayHitCheckScope {
    public:
        OverlayHitCheckScope(Context& ctx, Id id) noexcept
            : m_ctx(&ctx), m_prevId(ctx.m_activeOverlayHitCheckId) {
            ctx.m_activeOverlayHitCheckId = id;
        }
        OverlayHitCheckScope(OverlayHitCheckScope&& other) noexcept
            : m_ctx(other.m_ctx), m_prevId(other.m_prevId) {
            other.m_ctx = nullptr;
        }
        OverlayHitCheckScope(const OverlayHitCheckScope&) = delete;
        OverlayHitCheckScope& operator=(const OverlayHitCheckScope&) = delete;
        OverlayHitCheckScope& operator=(OverlayHitCheckScope&&) = delete;
        ~OverlayHitCheckScope() noexcept {
            if (m_ctx) {
                m_ctx->m_activeOverlayHitCheckId = m_prevId;
            }
        }
    private:
        Context* m_ctx{nullptr};
        Id m_prevId{Id::Null()};
    };

    [[nodiscard]] OverlayHitCheckScope ScopedOverlayHitCheck(Id id) noexcept {
        return OverlayHitCheckScope(*this, id);
    }
    void BeginWidgetExecution() noexcept { ++m_widgetExecDepth; }
    void EndWidgetExecution() noexcept { if (m_widgetExecDepth > 0) --m_widgetExecDepth; }
    [[nodiscard]] bool IsMouseOccluded(Id id, Vec2 pos) const noexcept;
    [[nodiscard]] bool IsRectHovered(Id id, const Rect& rect) const noexcept;
    [[nodiscard]] bool IsMouseBlockedForCurrentScope() const noexcept;
    void SetTooltip(UiText tooltip, Rect anchorRect);
    /// Tooltip requested during the current frame (empty if none).
    [[nodiscard]] const UiText& GetActiveTooltip() const noexcept { return m_activeTooltip; }
    void RegisterFocusable(Id id, bool disabled = false);

    void BeginModalScope(Id id, Rect backdropRect) noexcept {
        if (m_modalScopeDepth == 0 && m_focusedId.IsValid()) {
            bool focusedBeforeModal = false;
            for (const Id& fid : m_currFocusableIds) {
                if (fid == m_focusedId) {
                    focusedBeforeModal = true;
                    break;
                }
            }
            if (focusedBeforeModal) {
                if (!m_savedFocusBeforeModal.IsValid()) {
                    m_savedFocusBeforeModal = m_focusedId;
                }
                if (m_editState.id == m_focusedId) {
                    FlushActiveEditHistory();
                    m_editState.id = Id::Null();
                }
                m_focusedId = Id::Null();
                m_suspendDirectEditInput = false;
                m_pendingTextInput.clear();
            }
        }
        ++m_modalScopeDepth;
        m_currFrameModalId = id;
        m_currFrameModalBackdropRect = CurrentSpace().Apply(backdropRect);
    }

    void EndModalScope(bool stillOpen = true) noexcept {
        if (m_modalScopeDepth > 0) {
            --m_modalScopeDepth;
        }
        if (!stillOpen && m_modalScopeDepth == 0) {
            m_modalClosedThisFrame = true;
            ClearModalBlock();
            InvalidateAll();
        }
    }

    void ClearModalBlock() noexcept {
        m_currFrameModalId = Id::Null();
        m_currFrameModalBackdropRect = Rect{};
        m_prevFrameModalId = Id::Null();
        m_prevFrameModalBackdropRect = Rect{};
        m_currModalFocusableIds.clear();
    }

    [[nodiscard]] bool IsInModalScope() const noexcept { return m_modalScopeDepth > 0; }
    [[nodiscard]] Id GetPrevFrameModalId() const noexcept { return m_prevFrameModalId; }

    /// `pos` is in the current space.
    [[nodiscard]] bool IsModalBlocking(Vec2 pos) const noexcept { return IsModalBlockingInFrame(CurrentSpace().Apply(pos)); }

    /// Frame-space variant used by the context's own hit testing.
    [[nodiscard]] bool IsModalBlockingInFrame(Vec2 pos) const noexcept {
        if (m_modalScopeDepth > 0) return false;
        if (m_currFrameModalId.IsValid() && m_currFrameModalBackdropRect.Contains(pos)) return true;
        if (m_prevFrameModalId.IsValid() && m_prevFrameModalBackdropRect.Contains(pos)) return true;
        return false;
    }

    // --- Input Queries & Consumption ---
    /// Mouse position in the current space (see `CurrentSpace()`).
    [[nodiscard]] Vec2 GetMousePos() const noexcept { return CurrentSpace().Invert(m_mousePos); }
    [[nodiscard]] bool IsMouseDown() const noexcept { return m_mouseDown && !IsMouseBlockedForCurrentScope(); }
    [[nodiscard]] bool IsMouseClicked() const noexcept { return m_mouseClicked && !IsMouseBlockedForCurrentScope(); }
    [[nodiscard]] bool IsMouseReleased() const noexcept { return m_mouseReleased; }
    [[nodiscard]] bool IsMouseDoubleClicked() const noexcept { return m_mouseDoubleClicked && !IsMouseBlockedForCurrentScope(); }
    [[nodiscard]] bool IsRightMouseDown() const noexcept { return m_rightMouseDown && !IsMouseBlockedForCurrentScope(); }
    [[nodiscard]] bool IsRightMouseClicked() const noexcept { return m_rightMouseClicked && !IsMouseBlockedForCurrentScope(); }
    [[nodiscard]] bool IsRightMouseReleased() const noexcept { return m_rightMouseReleased; }
    [[nodiscard]] bool IsMiddleMouseDown() const noexcept { return m_middleMouseDown && !IsMouseBlockedForCurrentScope(); }
    [[nodiscard]] bool IsMiddleMouseClicked() const noexcept { return m_middleMouseClicked && !IsMouseBlockedForCurrentScope(); }
    [[nodiscard]] bool IsMiddleMouseReleased() const noexcept { return m_middleMouseReleased; }
    void ConsumeMouseClick() noexcept { m_mouseClicked = false; m_mouseReleased = false; m_mouseDoubleClicked = false; m_rightMouseClicked = false; m_rightMouseReleased = false; }
    void SetKeyModifiers(KeyModifiers mods) noexcept { m_keyMods = mods; }
    [[nodiscard]] KeyModifiers GetKeyModifiers() const noexcept { return m_keyMods; }
    [[nodiscard]] Vec2 GetViewportSize() const noexcept { return m_viewportSize; }
    [[nodiscard]] Vec2 GetMouseWheel() const noexcept {
        return IsMouseBlockedForCurrentScope() ? Vec2{0.0f, 0.0f} : m_mouseWheel;
    }
    void ConsumeMouseWheel() noexcept { m_mouseWheel = {0.0f, 0.0f}; }
    bool ConsumeKey(Key key);
    bool ConsumeKey(Key key, KeyModifiers& outMods);
    bool ConsumeShortcut(Key key, bool ctrl, bool shift = false, bool alt = false);

    // --- Semantic Keymap & Action Routing ---
    [[nodiscard]] Keymap& GetKeymap() noexcept { return m_keymap; }
    [[nodiscard]] const Keymap& GetKeymap() const noexcept { return m_keymap; }
    void SetKeymap(const Keymap& keymap) noexcept { m_keymap = keymap; }
    void BindAction(KeyAction action, KeyChord primary, KeyChord secondary = {}) noexcept {
        m_keymap.Bind(action, primary, secondary);
    }
    [[nodiscard]] bool MatchesAction(KeyAction action, Key key, KeyModifiers mods) const noexcept {
        return m_keymap.Matches(action, key, mods);
    }
    [[nodiscard]] KeyAction ResolveAction(Key key, KeyModifiers mods) const noexcept {
        return m_keymap.Resolve(key, mods);
    }
    bool ConsumeAction(KeyAction action);

    [[nodiscard]] keel::Array<keel::String> ConsumePendingTextInput();
    void ScrollToId(Id id);

    // --- Generic Persistent State Storage ---
    /// Typed state storage across frames for widgets by deterministic Id and Type (garbage-collected if unused for 600+ frames).
    template <typename T>
    [[nodiscard]] T& GetOrCreateState(Id id, T defaultVal = T{}) {
        detail::StateKey key{id, keel::type_id<T>()};
        detail::StoredState* found = m_customStates.get(key);
        if (!found) {
            detail::StoredState& inserted = m_customStates.get_or_insert(
                key,
                detail::StoredState::Create<T>(keel::move(defaultVal), m_frameIndex, m_currentTimeSec, false)
            );
            return *static_cast<T*>(inserted.ptr);
        }
        found->lastFrameActive = m_frameIndex;
        found->lastTimeActiveSec = m_currentTimeSec;
        found->cachesTrimmed = false;
        return *static_cast<T*>(found->ptr);
    }

    /// Long-lived typed state storage that survives tab switches, collapsed trees, and hidden panels without 600-frame eviction.
    template <typename T>
    [[nodiscard]] T& GetOrCreatePersistentState(Id id, T defaultVal = T{}) {
        detail::StateKey key{id, keel::type_id<T>()};
        detail::StoredState* found = m_customStates.get(key);
        if (!found) {
            detail::StoredState& inserted = m_customStates.get_or_insert(
                key,
                detail::StoredState::Create<T>(keel::move(defaultVal), m_frameIndex, m_currentTimeSec, true)
            );
            return *static_cast<T*>(inserted.ptr);
        }
        found->lastFrameActive = m_frameIndex;
        found->lastTimeActiveSec = m_currentTimeSec;
        found->persistent = true;
        found->cachesTrimmed = false;
        return *static_cast<T*>(found->ptr);
    }

    /// Finds persistent typed state associated with a unique widget Id, or nullptr if not found.
    template <typename T>
    [[nodiscard]] const T* FindState(Id id) const {
        detail::StateKey key{id, keel::type_id<T>()};
        const detail::StoredState* found = m_customStates.get(key);
        if (!found) {
            return nullptr;
        }
        return static_cast<const T*>(found->ptr);
    }

    template <typename T>
    [[nodiscard]] T* FindMutableState(Id id) {
        detail::StateKey key{id, keel::type_id<T>()};
        detail::StoredState* found = m_customStates.get(key);
        if (!found) {
            return nullptr;
        }
        found->lastFrameActive = m_frameIndex;
        found->lastTimeActiveSec = m_currentTimeSec;
        found->cachesTrimmed = false;
        return static_cast<T*>(found->ptr);
    }

private:
    friend class TableScope;
    friend class TableRowScope;

    // Frame-space variants used by the context's own hit testing and damage tracking
    [[nodiscard]] bool IsMouseOccludedInFrame(Id id, Vec2 framePos) const noexcept;
    [[nodiscard]] Rect PopupViewportInFrame() const noexcept;
    void InvalidateFrameRect(Rect frameRect) noexcept;

    [[nodiscard]] bool PushRowDisabled(bool disabled) noexcept {
        const bool prev = m_rowDisabled;
        m_rowDisabled = prev || disabled;
        return prev;
    }
    void PopRowDisabled(bool prevDisabled) noexcept {
        m_rowDisabled = prevDisabled;
    }

    void FlushActiveEditHistory() noexcept;
    [[nodiscard]] bool IsDirectEditAllowed() const noexcept;
    void RenderNodeRecursive(Node* node, ICanvas& canvas, const Rect& dirtyRect, const UiTheme& parentTheme);
    void RenderModalsRecursive(Node* node, ICanvas& canvas, const Rect& dirtyRect, const UiTheme& parentTheme);
    bool UpdateScrollAreasRecursive(Node* node);

    // Widget snapshot for frame-to-frame change detection
    struct WidgetSnapshot {
        Rect rect{};
        keel::u64 visualHash{0};
        keel::u32 flags{0}; // bits: 0=hovered, 1=pressed, 2=disabled, 3=checked, 4=cursorVisible, 5=activeEdit, 6=tabActive, 7=focused, 8..11=scrollbar
        keel::usize cursor{0};
        keel::usize selectionAnchor{0};
        keel::u32 bgColor{0};
        keel::u32 borderColor{0};

        bool operator==(const WidgetSnapshot& o) const noexcept {
            return rect == o.rect &&
                   visualHash == o.visualHash &&
                   flags == o.flags &&
                   cursor == o.cursor &&
                   selectionAnchor == o.selectionAnchor &&
                   bgColor == o.bgColor &&
                   borderColor == o.borderColor;
        }

        bool operator!=(const WidgetSnapshot& o) const noexcept {
            return !(*this == o);
        }
    };

    keel::HashMap<Id, WidgetSnapshot> m_prevSnapshots;
    keel::Array<Rect> m_explicitDamageRects;
    keel::Array<Rect> m_dirtyRects;
    Rect m_dirtyRect{0.0f, 0.0f, 0.0f, 0.0f};
    UiTheme m_theme{UiTheme::Dark()};
    Color m_clearColor{11, 15, 25, 255}; // Tailwind Slate-950 #0B0F19
    bool m_hasDirtyDamage{true};
    bool m_isFullRepaint{true};
    bool m_forceFullRepaint{true};
    bool m_dirtyRectEnabled{true};
    bool m_liveResizeEnabled{true};
    bool m_debugBoundsEnabled{false};
    bool m_inspectModeEnabled{false};
    bool m_anyScopedInspectActive{false};

    keel::Arena m_arena;
    guikit::Zone m_rootZone{&m_theme, 1.0f, 1.0f};
    keel::SmallArray<const guikit::Zone*, 16> m_zoneStack;
    keel::SmallArray<Node*, 16> m_containerStack;
    float m_rootDensity{1.0f};
    bool m_rowDisabled{false};
    Node* m_root{nullptr};
    ICanvas* m_lastCanvas{nullptr};

    Vec2 m_viewportSize{800.0f, 600.0f};
    float m_displayScale{1.0f};
    float m_rootZoom{1.0f};
    float m_pixelsPerDp{1.0f}; ///< m_displayScale * m_rootZoom

    // Time & Cursor animation state
    CursorConfig m_cursorConfig{};
    keel::u64 m_startTimeNs{0};
    double m_currentTimeSec{0.0};
    double m_lastInputTimeSec{0.0};
    bool m_manualTime{false};

    // Input state
    Vec2 m_mousePos{-10000.0f, -10000.0f};   ///< dp
    Vec2 m_mousePosPx{-10000.0f, -10000.0f}; ///< physical pixels, source of truth across scale changes
    bool m_mouseDown{false};
    bool m_mouseClicked{false};
    bool m_mouseReleased{false};
    bool m_mouseDoubleClicked{false};
    double m_lastClickTimeSec{-1.0};
    Vec2 m_lastClickPos{-10000.0f, -10000.0f};
    bool m_rightMouseDown{false};
    bool m_rightMouseClicked{false};
    bool m_rightMouseReleased{false};
    bool m_middleMouseDown{false};
    bool m_middleMouseClicked{false};
    bool m_middleMouseReleased{false};
    KeyModifiers m_keyMods{};
    Keymap m_keymap{Keymap::Default()};
    keel::i32 m_repaintFrames{3};
    keel::i32 m_idleHeartbeatMs{500};

    // Clipboard & host state
    ClipboardGetFn m_clipboardGetFn{};
    ClipboardSetFn m_clipboardSetFn{};
    OpenUrlFn m_openUrlFn{};
    keel::String m_clipboardText{};

    Id m_hoveredId{Id::Null()};
    Id m_activeId{Id::Null()};
    Id m_focusedId{Id::Null()};
    CursorType m_cursor{CursorType::Default};

    // Active text edit state
    struct ActiveEditState {
        struct EditSnapshot {
            keel::String text;
            keel::usize cursor{0};
            keel::usize selectionAnchor{0};
        };

        Id id{Id::Null()};
        keel::String text;
        keel::usize cursor{0};
        keel::usize selectionAnchor{0};
        keel::Array<EditSnapshot> undoStack;
        keel::Array<EditSnapshot> redoStack;
        keel::Optional<keel::usize> preferredColumn;
        bool changed{false};
        bool submitted{false};
        bool multiline{false};
        bool readOnly{false};
        bool autoCloseBrackets{false};
        keel::u8 tabSize{4};
        bool insertSpaces{true};

        [[nodiscard]] bool HasSelection() const noexcept { return cursor != selectionAnchor; }
        [[nodiscard]] keel::usize SelectionStart() const noexcept { return keel::min(cursor, selectionAnchor); }
        [[nodiscard]] keel::usize SelectionEnd() const noexcept { return keel::max(cursor, selectionAnchor); }

        [[nodiscard]] keel::StringRef SelectedText() const noexcept {
            if (!HasSelection()) return {};
            const keel::usize s = keel::min(SelectionStart(), text.size());
            const keel::usize e = keel::min(SelectionEnd(), text.size());
            return text.substr(s, e - s);
        }

        void SaveUndo() {
            if (undoStack.empty() || undoStack.back().text != text) {
                if (undoStack.size() >= 64) {
                    undoStack.remove_at(0);
                }
                undoStack.push_back(EditSnapshot{text.clone(), cursor, selectionAnchor});
                redoStack.clear();
            }
        }

        bool Undo() {
            while (!undoStack.empty()) {
                EditSnapshot snap = keel::move(undoStack.back());
                undoStack.pop_back();
                if (snap.text != text) {
                    redoStack.push_back(EditSnapshot{text.clone(), cursor, selectionAnchor});
                    text = keel::move(snap.text);
                    cursor = keel::min(snap.cursor, text.size());
                    selectionAnchor = keel::min(snap.selectionAnchor, text.size());
                    preferredColumn.reset();
                    changed = true;
                    return true;
                }
            }
            return false;
        }

        bool Redo() {
            while (!redoStack.empty()) {
                EditSnapshot snap = keel::move(redoStack.back());
                redoStack.pop_back();
                if (snap.text != text) {
                    undoStack.push_back(EditSnapshot{text.clone(), cursor, selectionAnchor});
                    text = keel::move(snap.text);
                    cursor = keel::min(snap.cursor, text.size());
                    selectionAnchor = keel::min(snap.selectionAnchor, text.size());
                    preferredColumn.reset();
                    changed = true;
                    return true;
                }
            }
            return false;
        }

        void SelectAll() noexcept {
            selectionAnchor = 0;
            cursor = text.size();
            preferredColumn.reset();
        }

        bool DeleteSelection();
        void Insert(keel::StringRef utf8Text);
        bool HandleKey(Context& ctx, Key key, KeyModifiers mods, bool& outLostFocus);
    };
    ActiveEditState m_editState;
    struct TextEditHistory {
        keel::String lastText;
        keel::Array<ActiveEditState::EditSnapshot> undoStack;
        keel::Array<ActiveEditState::EditSnapshot> redoStack;
    };
    keel::HashMap<Id, TextEditHistory> m_editHistories;
    Id m_savedFocusBeforeModal{Id::Null()};
    bool m_suspendDirectEditInput{false};

    struct QueuedKeyEvent {
        Key key;
        KeyModifiers mods;
    };
    keel::Array<keel::String> m_pendingTextInput;
    keel::Array<QueuedKeyEvent> m_pendingKeyEvents;

    // Previous frame widget bounds for instantaneous hit testing
    struct HitNodeRecord {
        Id id{Id::Null()};
        Rect rect{};
        keel::u32 preIndex{0};
        keel::u32 postIndex{0};
        NodeType type{NodeType::Container};
        keel::u8 zLayer{0};
        bool blocksHit{false};
    };

    keel::HashMap<Id, Rect> m_prevFrameBounds;
    keel::HashMap<Id, Rect> m_currFrameBounds;
    keel::HashMap<Id, Rect> m_prevUnclippedBounds;
    keel::HashMap<Id, Rect> m_currUnclippedBounds;
    keel::HashMap<Id, Transform2D> m_prevNodeSpaces; ///< Only nodes inside a scaled container (others: identity)
    keel::HashMap<Id, Transform2D> m_currNodeSpaces;
    const Transform2D* m_spaceOverride{nullptr};     ///< Space of the overlay hit builder that is running
    keel::Array<HitNodeRecord> m_prevHitNodes;
    keel::Array<HitNodeRecord> m_currHitNodes;
    keel::HashMap<Id, keel::usize> m_prevHitNodeMap;
    struct OverlayHitBuilder {
        void* ctx{nullptr};
        void (*invoke)(void*){nullptr};
        void (*destroy)(void*) noexcept{nullptr};
        Transform2D space{}; ///< Space the builder was registered from; current while it runs
    };
    keel::Array<OverlayHitRegion> m_prevOverlayHitRegions;
    keel::Array<OverlayHitRegion> m_currOverlayHitRegions;
    keel::SmallArray<OverlayHitBuilder, 8> m_overlayHitBuilders;
    Id m_activeOverlayHitCheckId{Id::Null()};
    keel::i32 m_widgetExecDepth{0};
    keel::Array<Id> m_prevFocusableIds;
    keel::Array<Id> m_currFocusableIds;
    keel::Array<Id> m_prevModalFocusableIds;
    keel::Array<Id> m_currModalFocusableIds;

    // Floating popup, tooltip, modal & toast overlay state
    Id m_openPopupId{Id::Null()};
    Rect m_openPopupRect{0.0f, 0.0f, 0.0f, 0.0f};
    keel::u64 m_openPopupVisualHash{0};
    Id m_prevOpenPopupId{Id::Null()};
    Rect m_prevOpenPopupRect{0.0f, 0.0f, 0.0f, 0.0f};
    keel::u64 m_prevOpenPopupVisualHash{0};
    Id m_prevFrameModalId{Id::Null()};
    Rect m_prevFrameModalBackdropRect{0.0f, 0.0f, 0.0f, 0.0f};
    Id m_currFrameModalId{Id::Null()};
    Rect m_currFrameModalBackdropRect{0.0f, 0.0f, 0.0f, 0.0f};
    keel::i32 m_modalScopeDepth{0};
    bool m_hadModalLastFrame{false};
    bool m_modalClosedThisFrame{false};
    UiText m_activeTooltip{};
    Rect m_activeTooltipAnchor{0.0f, 0.0f, 0.0f, 0.0f};
    Rect m_activeTooltipViewport{0.0f, 0.0f, 0.0f, 0.0f};
    keel::u64 m_prevTooltipHash{0};
    bool m_hadTooltipLastFrame{false};
    keel::Array<ToastEntry> m_toasts;
    ToastHostOptions m_defaultToastHostOpts{};
    keel::u64 m_nextToastId{0};
    bool m_toastHostVisitedThisFrame{false};
    bool m_hadToastsLastFrame{false};
    struct OverlayPass {
        void* ctx{nullptr};
        void (*invoke)(void*, ICanvas&){nullptr};
        void (*destroy)(void*) noexcept{nullptr};
        Rect clipRect{};     ///< Frame space
        Transform2D space{}; ///< Space the overlay was registered from; it draws in that space
    };
    keel::SmallArray<OverlayPass, 8> m_overlayPasses;
    void ClearOverlayPasses() noexcept;
    void ClearOverlayHitBuilders() noexcept;
    bool m_currFrameModalOverlay{false};
    bool m_hadModalOverlayLastFrame{false};
    bool m_hadDebugBoundsLastFrame{false};
    bool m_hadInspectModeLastFrame{false};
    Vec2 m_mouseWheel{0.0f, 0.0f};
    keel::HashMap<const char*, keel::u64> m_fileHashCache;
    keel::HashMap<keel::u64, keel::u32> m_childIdCounts;
    keel::HashMap<detail::StateKey, detail::StoredState, detail::StateKeyHash> m_customStates;
    keel::u64 m_frameIndex{0};
    double m_lastCacheTrimSweepSec{0.0};
    double m_lastTransientPruneSec{0.0};
    bool m_hasUntrimmedInactiveStates{false};

    void RenderDebugBoundsRecursive(Node* node, ICanvas& canvas, bool inheritedDebugBounds);
    void RenderTooltipOverlay(ICanvas& canvas, Vec2 logicalSize);
    Node* FindDeepestNodeAt(Node* root, Vec2 pos, Rect clipRect);
    void RenderInspectOverlay(Node* root, ICanvas& canvas, Vec2 logicalSize);
};

} // namespace guikit
