#pragma once

#include "keel/arena.h"
#include "keel/array.h"
#include "keel/core.h"
#include "keel/format.h"
#include "keel/function.h"
#include "keel/hash_map.h"
#include "keel/math.h"
#include "keel/optional.h"
#include "keel/ref.h"
#include "keel/small_array.h"
#include "keel/string.h"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/core/damage_tracker.hpp"
#include "guikit/core/debug_inspector.hpp"
#include "guikit/core/focus_manager.hpp"
#include "guikit/core/hit_test_tree.hpp"
#include "guikit/core/id.hpp"
#include "guikit/core/overlay_manager.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/state_store.hpp"
#include "guikit/core/text_buffer.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"
#include "guikit/core/solver.hpp"
#include "guikit/core/zone.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/container_scope.hpp"

namespace guikit {

class Context;

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
    void BeginFrame(Vec2 viewportSize, keel::f32 displayScale = 1.0f, keel::f32 rootZoom = 1.0f);
    void EndFrame(ICanvas& canvas);

    // --- Input Handling ---
    /// `pos` is in physical framebuffer pixels (same space as `BeginFrame`'s `viewportSize`);
    /// it is divided by the frame's pixels-per-dp factor to produce the dp mouse position, and is
    /// re-derived when that factor changes so the pointer stays under the cursor after a zoom.
    void OnMouseMove(Vec2 pos);
    void OnMouseDown(keel::i32 button);
    void OnMouseUp(keel::i32 button);
    void OnTextInput(keel::StringRef utf8Text);
    void OnTextComposition(keel::StringRef preeditText, keel::usize cursor = 0, keel::usize selectionLength = 0);
    void OnTextCompositionEnd(keel::StringRef committedText = {});
    void OnKeyDown(Key key, KeyModifiers mods = {});
    void OnMouseWheel(keel::f32 x, keel::f32 y);
    void OnFilesDropped(keel::ArrayRef<keel::StringRef> paths, Vec2 posPx);

    // --- IME Composition & OS File Drop Queries ---
    [[nodiscard]] const ImeCompositionState& GetImeComposition() const noexcept { return m_ime; }
    [[nodiscard]] bool IsImeComposing() const noexcept { return m_ime.active && !m_ime.text.empty(); }
    void SetImeTargetRect(Rect rect) noexcept { m_imeTargetRect = CurrentSpace().Apply(rect); }
    [[nodiscard]] Rect GetImeTargetRect() const noexcept { return m_imeTargetRect; }
    [[nodiscard]] bool WantsTextInput() const noexcept {
        return m_focus.GetFocusedId().IsValid() && m_editState.id == m_focus.GetFocusedId() && !m_editState.readOnly;
    }

    [[nodiscard]] bool HasDroppedFiles() const noexcept { return !m_droppedFiles.empty(); }
    [[nodiscard]] keel::ArrayRef<keel::String> GetDroppedFiles() const noexcept { return m_droppedFiles; }
    [[nodiscard]] Vec2 GetDroppedFilesPos() const noexcept { return CurrentSpace().Invert(m_droppedFilesPos); }
    void ConsumeDroppedFiles() noexcept { m_droppedFiles.clear(); }

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
    void SetDirtyRectEnabled(bool enabled) noexcept { m_damage.SetDirtyRectEnabled(enabled); }

    /// Returns whether dirty damage rect tracking is currently enabled.
    [[nodiscard]] bool IsDirtyRectEnabled() const noexcept { return m_damage.IsDirtyRectEnabled(); }

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
    void SetDensity(keel::f32 density) noexcept {
        m_rootDensity = (density > 0.0f) ? density : 1.0f;
        m_rootZone.density = m_rootDensity;
        InvalidateAll();
    }

    /// Returns the active spacing/padding density multiplier in the current container zone.
    [[nodiscard]] keel::f32 GetDensity() const noexcept {
        return GetZone()->density;
    }

    // --- Developer Debug & Inspector Tools ---
    /// Sets whether 1px color-coded debug bounding boxes and padding overlays are rendered.
    void SetDebugBounds(bool enabled) noexcept;
    [[nodiscard]] bool IsDebugBounds() const noexcept { return m_inspector.IsDebugBounds(); }

    /// Sets whether the interactive element inspector (Chrome DevTools style) is active.
    void SetInspectMode(bool enabled) noexcept;
    [[nodiscard]] bool IsInspectMode() const noexcept { return m_inspector.IsInspectMode(); }

    /// Returns true if this frame produced any visual damage requiring rasterization / blitting.
    [[nodiscard]] bool HasDirtyDamage() const noexcept { return m_damage.HasDirtyDamage(); }

    /// Returns true if the entire frame required a full repaint (e.g. window resize or tab switch).
    [[nodiscard]] bool IsFullRepaint() const noexcept { return m_damage.IsFullRepaint(); }

    /// Returns the active bounding rectangle of the dirty damage region for this frame (in logical points).
    [[nodiscard]] Rect GetDirtyRect() const noexcept { return m_damage.GetDirtyRect(); }

    /// Returns the list of mutually disjoint dirty damage rectangles for this frame (in logical points).
    [[nodiscard]] keel::ArrayRef<Rect> GetDirtyRects() const noexcept { return m_damage.GetDirtyRects(); }

    /// Returns the active bounding rectangle of the dirty damage region in physical pixels for GPU blitting.
    [[nodiscard]] Rect GetPhysicalDirtyRect() const noexcept {
        return m_damage.GetPhysicalDirtyRect(m_pixelsPerDp);
    }

    /// OS / monitor scale of this frame (100% = 1.0), as passed to `BeginFrame`.
    [[nodiscard]] keel::f32 GetDisplayScale() const noexcept { return m_displayScale; }

    /// User UI zoom of the frame's root zone (the shell zoom), as passed to `BeginFrame`.
    [[nodiscard]] keel::f32 GetRootZoom() const noexcept { return m_rootZoom; }

    /// The frame's single dp -> pixel factor: `GetDisplayScale() * GetRootZoom()`.
    [[nodiscard]] keel::f32 GetPixelsPerDp() const noexcept { return m_pixelsPerDp; }

    /// The `.scale` a container opened in the current space needs so that its content renders at `zoom` relative
    /// to the display scale, independent of the root (shell) zoom and of any scaled ancestor.
    [[nodiscard]] keel::f32 ScaleForZoom(keel::f32 zoom) const noexcept { return zoom / (m_rootZoom * GetZone()->scale); }

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

    // --- Environment Zone & Identity Scope Wrappers ---
    /// Opens a transparent environment scope overriding `theme`, `scale`, `density`, `debugBounds`, or `inspectMode`.
    [[nodiscard]] ZoneScope Zone(const ZoneOptions& opts = {});

    void PushZone(const ZoneOptions& opts);
    void PopZone() noexcept;

    /// Opens a transparent RAII identity scope mixing a stable key into all descendant widget IDs
    /// (prevents focus/state shifts when inserting, removing, or reordering dynamic list items).
    [[nodiscard]] ScopedId IdScope(keel::StringRef key);
    [[nodiscard]] ScopedId IdScope(keel::u64 key);
    [[nodiscard]] ScopedId IdScope(keel::i64 key);
    [[nodiscard]] ScopedId IdScope(const void* ptr);

    void PushId(keel::StringRef key);
    void PushId(keel::u64 key);
    void PushId(keel::i64 key);
    void PushId(const void* ptr);
    void PopId() noexcept;

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
    void RequestFocus(Id id, FocusReason reason = FocusReason::Auto);
    [[nodiscard]] Id GetFocusedId() const noexcept { return m_focus.GetFocusedId(); }
    [[nodiscard]] InputModality GetInputModality() const noexcept { return m_focus.GetInputModality(); }
    [[nodiscard]] bool IsFocused(Id id) const noexcept;
    /// Returns true when `id` has focus AND the focus ring should be drawn (`:focus-visible` semantics:
    /// true for keyboard navigation, false after mouse/pointer click on non-text controls).
    [[nodiscard]] bool IsFocusVisible(Id id) const noexcept {
        return m_focus.IsFocusVisible() && IsFocused(id);
    }
    [[nodiscard]] bool HasActiveTextEdit(Id id) const noexcept { return m_editState.id == id && id.IsValid(); }
    void ClearFocus() noexcept {
        if (m_editState.id == m_focus.GetFocusedId()) {
            FlushActiveEditHistory();
            m_editState.id = Id::Null();
        }
        m_focus.SetFocusedId(Id::Null());
        m_focus.SetFocusVisible(false);
    }

    [[nodiscard]] Id GetActiveId() const noexcept { return m_focus.GetActiveId(); }
    void SetActiveId(Id id) noexcept { m_focus.SetActiveId(id); }
    [[nodiscard]] keel::u64 GetFrameIndex() const noexcept { return m_frameIndex; }
    [[nodiscard]] keel::usize GetCustomStateCount() const noexcept { return m_stateStore.Size(); }
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
    void SetCurrentTime(keel::f64 timeSeconds) noexcept;
    void AdvanceTime(keel::f64 deltaSeconds) noexcept;
    [[nodiscard]] keel::f64 GetCurrentTime() const noexcept { return m_currentTimeSec; }
    [[nodiscard]] keel::f64 GetCurrentTimeSec() const noexcept { return m_currentTimeSec; }

    // --- Popup, Tooltip, Modal & Overlay Management ---
    void RegisterEndFrameHook(EndFrameHook hook) { m_overlays.RegisterEndFrameHook(hook); }
    void OpenPopup(Id id, Rect bounds, Rect anchorRect = Rect{});
    void AddOpenPopupRect(Rect bounds);
    void ClosePopup();
    [[nodiscard]] bool IsPopupOpen(Id id) const noexcept { return m_overlays.IsPopupOpen(id); }
    [[nodiscard]] Id GetOpenPopupId() const noexcept { return m_overlays.GetOpenPopupId(); }
    [[nodiscard]] Rect GetOpenPopupRect() const noexcept { return CurrentSpace().Invert(m_overlays.GetOpenPopupRect()); }
    [[nodiscard]] bool IsPointInOpenPopup(Vec2 framePos) const noexcept;
    void SetOpenPopupVisualHash(keel::u64 hash) noexcept { m_overlays.SetOpenPopupVisualHash(hash); }
    template <typename F>
        requires requires(keel::Decay<F>& f, ICanvas& c) { f(c); }
    void RegisterOverlay(F&& drawFn, OverlayLayer layer = OverlayLayer::Popup) {
        using Decayed = keel::Decay<F>;
        if constexpr (keel::IsPointer<Decayed>) {
            if (!drawFn) return;
        } else if constexpr (requires(const Decayed& f) { f.operator bool(); }) {
            if (!static_cast<bool>(drawFn)) return;
        }
        Decayed* stored = m_arena.create<Decayed>(keel::forward<F>(drawFn));
        m_overlays.AddOverlayPass(OverlayPass{
            .ctx = stored,
            .invoke = +[](void* p, ICanvas& c) { (*static_cast<Decayed*>(p))(c); },
            .destroy = keel::IsTriviallyDestructible<Decayed>
                ? nullptr
                : +[](void* p) noexcept { static_cast<Decayed*>(p)->~Decayed(); },
            .clipRect = PopupViewportInFrame(),
            .space = CurrentSpace(),
            .layer = layer
        });
    }
    void MarkModalOverlayActive() noexcept { m_overlays.MarkModalOverlayActive(); }
    void RegisterOverlayHitRegion(Id id, Rect rect, OverlayLayer layer = OverlayLayer::Popup);
    template <typename F>
        requires requires(keel::Decay<F>& f) { f(); }
    void RegisterOverlayHitBuilder(F&& builderFn) {
        using Decayed = keel::Decay<F>;
        if constexpr (keel::IsPointer<Decayed> || requires(const Decayed& f) { static_cast<bool>(f); }) {
            if (!static_cast<bool>(builderFn)) return;
        }
        Decayed* stored = m_arena.create<Decayed>(keel::forward<F>(builderFn));
        m_overlays.AddOverlayHitBuilder(OverlayHitBuilder{
            .ctx = stored,
            .invoke = +[](void* p) { (*static_cast<Decayed*>(p))(); },
            .destroy = keel::IsTriviallyDestructible<Decayed>
                ? nullptr
                : +[](void* p) noexcept { static_cast<Decayed*>(p)->~Decayed(); },
            .space = CurrentSpace(),
            .scopeZLayer = CurrentScopeZLayer(),
            .ownerPortalId = CurrentPortalId()
        });
    }
    /// RAII scope guard that enters an overlay's hit-testing context at `layer` and restores the previous context on exit.
    class [[nodiscard]] OverlayHitCheckScope {
    public:
        OverlayHitCheckScope(Context& ctx, Id id, OverlayLayer layer = OverlayLayer::Popup) noexcept
            : m_ctx(&ctx)
            , m_prevId(ctx.m_activeOverlayHitCheckId)
            , m_prevLayer(ctx.m_activeOverlayHitCheckLayer) {
            ctx.m_activeOverlayHitCheckId = id;
            ctx.m_activeOverlayHitCheckLayer = layer;
        }
        OverlayHitCheckScope(OverlayHitCheckScope&& other) noexcept
            : m_ctx(other.m_ctx)
            , m_prevId(other.m_prevId)
            , m_prevLayer(other.m_prevLayer) {
            other.m_ctx = nullptr;
        }
        OverlayHitCheckScope(const OverlayHitCheckScope&) = delete;
        OverlayHitCheckScope& operator=(const OverlayHitCheckScope&) = delete;
        OverlayHitCheckScope& operator=(OverlayHitCheckScope&&) = delete;
        ~OverlayHitCheckScope() noexcept {
            if (m_ctx) {
                m_ctx->m_activeOverlayHitCheckId = m_prevId;
                m_ctx->m_activeOverlayHitCheckLayer = m_prevLayer;
            }
        }
    private:
        Context* m_ctx{nullptr};
        Id m_prevId{Id::Null()};
        OverlayLayer m_prevLayer{OverlayLayer::Popup};
    };

    [[nodiscard]] OverlayHitCheckScope ScopedOverlayHitCheck(
        Id id,
        OverlayLayer layer = OverlayLayer::Popup
    ) noexcept {
        return OverlayHitCheckScope(*this, id, layer);
    }
    void BeginWidgetExecution() noexcept { ++m_widgetExecDepth; }
    void EndWidgetExecution() noexcept { if (m_widgetExecDepth > 0) --m_widgetExecDepth; }
    [[nodiscard]] bool IsMouseOccluded(Id id, Vec2 pos) const noexcept;
    /// Returns true if any overlay/portal strictly above `layer` (or another blocking overlay on `layer`) occludes `pos`.
    [[nodiscard]] bool IsOverlayOccluded(
        Id id,
        Vec2 pos,
        OverlayLayer layer = OverlayLayer::Panel
    ) const noexcept {
        return IsOverlayOccludedInFrame(id, CurrentSpace().Apply(pos), layer);
    }
    [[nodiscard]] bool IsRectHovered(Id id, const Rect& rect) const noexcept;
    [[nodiscard]] bool IsMouseBlockedForCurrentScope() const noexcept;
    void SetTooltip(UiText tooltip, Rect anchorRect);
    /// Tooltip requested during the current frame (empty if none).
    [[nodiscard]] const UiText& GetActiveTooltip() const noexcept { return m_overlays.GetActiveTooltip(); }
    void RegisterFocusable(Id id, bool disabled = false);

    void BeginModalScope(Id id, Rect backdropRect) noexcept;
    void EndModalScope(bool stillOpen = true, Id id = Id::Null()) noexcept;
    void ClearModalBlock(Id id = Id::Null()) noexcept;
    void ClearPopoverBlock(Id id) noexcept;

    [[nodiscard]] bool IsInModalScope() const noexcept { return m_overlays.IsInModalScope(); }
    [[nodiscard]] Id GetPrevFrameModalId() const noexcept { return m_overlays.GetPrevFrameModalId(); }
    [[nodiscard]] bool HasPrevFrameModal(Id id) const noexcept;
    [[nodiscard]] bool IsTopModal(Id id) const noexcept;
    [[nodiscard]] bool HasModalAboveCurrentScope() const noexcept;
    [[nodiscard]] bool HasPrevFramePopoverOver(Id modalId) const noexcept;

    /// `pos` is in the current space.
    [[nodiscard]] bool IsModalBlocking(Vec2 pos) const noexcept { return IsModalBlockingInFrame(CurrentSpace().Apply(pos)); }

    /// Frame-space variant used by the context's own hit testing.
    [[nodiscard]] bool IsModalBlockingInFrame(Vec2 pos, Id exemptModalId = Id::Null()) const noexcept;

    /// Returns true if an open `Popover` sits above the current scope and covers `pos` (in current space).
    [[nodiscard]] bool IsPopoverBlocking(Vec2 pos) const noexcept { return IsPopoverBlockingInFrame(CurrentSpace().Apply(pos)); }

    /// Frame-space variant used by the context's own hit testing.
    [[nodiscard]] bool IsPopoverBlockingInFrame(Vec2 pos, Id ownerPortalOverride = Id::Null()) const noexcept;

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
        return m_stateStore.GetOrCreate<T>(id, keel::move(defaultVal), m_frameIndex, m_currentTimeSec, false);
    }

    /// Long-lived typed state storage that survives tab switches, collapsed trees, and hidden panels without 600-frame eviction.
    template <typename T>
    [[nodiscard]] T& GetOrCreatePersistentState(Id id, T defaultVal = T{}) {
        return m_stateStore.GetOrCreate<T>(id, keel::move(defaultVal), m_frameIndex, m_currentTimeSec, true);
    }

    /// Finds persistent typed state associated with a unique widget Id, or nullptr if not found.
    template <typename T>
    [[nodiscard]] const T* FindState(Id id) const {
        return m_stateStore.Find<T>(id);
    }

    template <typename T>
    [[nodiscard]] T* FindMutableState(Id id) {
        return m_stateStore.FindMutable<T>(id, m_frameIndex, m_currentTimeSec);
    }

private:
    friend class TableScope;
    friend class TableRowScope;
    friend class TextBuffer;

    // Frame-space variants used by the context's own hit testing and damage tracking
    [[nodiscard]] keel::u8 CurrentScopeZLayer() const noexcept;
    [[nodiscard]] Id CurrentPortalId() const noexcept;
    [[nodiscard]] bool IsMouseOccludedInFrame(Id id, Vec2 framePos) const noexcept;
    [[nodiscard]] bool IsOverlayOccludedInFrame(Id id, Vec2 framePos, OverlayLayer layer) const noexcept;
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

    // Core decomposed subsystems
    StateStore m_stateStore;
    DamageTracker m_damage;
    HitTestTree m_hitTree;
    OverlayManager m_overlays;
    FocusManager m_focus;
    DebugInspector m_inspector;

    UiTheme m_theme{UiTheme::Dark()};
    Color m_clearColor{11, 15, 25, 255}; // Tailwind Slate-950 #0B0F19
    bool m_liveResizeEnabled{true};

    keel::Arena m_arena;
    guikit::Zone m_rootZone{&m_theme, 1.0f, 1.0f};
    keel::SmallArray<const guikit::Zone*, 16> m_zoneStack;
    keel::SmallArray<keel::u64, 16> m_idStack;
    keel::SmallArray<Node*, 16> m_containerStack;
    keel::f32 m_rootDensity{1.0f};
    bool m_rowDisabled{false};
    Node* m_root{nullptr};
    ICanvas* m_lastCanvas{nullptr};

    Vec2 m_viewportSize{800.0f, 600.0f};
    keel::f32 m_displayScale{1.0f};
    keel::f32 m_rootZoom{1.0f};
    keel::f32 m_pixelsPerDp{1.0f}; ///< m_displayScale * m_rootZoom

    // Time & Cursor animation state
    CursorConfig m_cursorConfig{};
    keel::u64 m_startTimeNs{0};
    keel::f64 m_currentTimeSec{0.0};
    keel::f64 m_lastInputTimeSec{0.0};
    bool m_manualTime{false};

    // Input state
    Vec2 m_mousePos{-10000.0f, -10000.0f};   ///< dp
    Vec2 m_mousePosPx{-10000.0f, -10000.0f}; ///< physical pixels, source of truth across scale changes
    bool m_mouseDown{false};
    bool m_mouseClicked{false};
    bool m_mouseReleased{false};
    bool m_mouseDoubleClicked{false};
    keel::f64 m_lastClickTimeSec{-1.0};
    Vec2 m_lastClickPos{-10000.0f, -10000.0f};
    bool m_rightMouseDown{false};
    bool m_rightMouseClicked{false};
    bool m_rightMouseReleased{false};
    bool m_middleMouseDown{false};
    bool m_middleMouseClicked{false};
    bool m_middleMouseReleased{false};
    Vec2 m_mouseWheel{0.0f, 0.0f};
    KeyModifiers m_keyMods{};
    Keymap m_keymap{Keymap::Default()};
    keel::i32 m_repaintFrames{3};
    keel::i32 m_idleHeartbeatMs{500};

    // Clipboard & host state
    ClipboardGetFn m_clipboardGetFn{};
    ClipboardSetFn m_clipboardSetFn{};
    OpenUrlFn m_openUrlFn{};
    keel::String m_clipboardText{};
    CursorType m_cursor{CursorType::Default};

    // Active text editing buffer, diff undo/redo histories, IME & OS file drop state
    TextBuffer m_editState;
    keel::HashMap<Id, TextEditHistory> m_editHistories;
    ImeCompositionState m_ime;
    Rect m_imeTargetRect{0.0f, 0.0f, 0.0f, 0.0f};
    keel::Array<keel::String> m_droppedFiles;
    Vec2 m_droppedFilesPos{0.0f, 0.0f};
    bool m_suspendDirectEditInput{false};

    struct QueuedKeyEvent {
        Key key;
        KeyModifiers mods;
    };
    keel::Array<keel::String> m_pendingTextInput;
    keel::Array<QueuedKeyEvent> m_pendingKeyEvents;

    // Active overlay hit-builder / scope tracking
    const Transform2D* m_spaceOverride{nullptr};
    bool m_runningOverlayHitBuilder{false};
    keel::u8 m_overlayBuilderZLayer{0};
    Id m_overlayBuilderPortalId{Id::Null()};
    Id m_activeOverlayHitCheckId{Id::Null()};
    OverlayLayer m_activeOverlayHitCheckLayer{OverlayLayer::Popup};
    keel::i32 m_widgetExecDepth{0};

    keel::HashMap<const char*, keel::u64> m_fileHashCache;
    keel::HashMap<keel::u64, keel::u32> m_childIdCounts;
    keel::u64 m_frameIndex{0};
};

/// RAII scope guard marking active widget execution (used by free-function widget entry points).
class WidgetExecGuard {
public:
    explicit WidgetExecGuard(Context& ui) noexcept : m_ui(ui) { m_ui.BeginWidgetExecution(); }
    ~WidgetExecGuard() noexcept { m_ui.EndWidgetExecution(); }
    WidgetExecGuard(const WidgetExecGuard&) = delete;
    WidgetExecGuard& operator=(const WidgetExecGuard&) = delete;
private:
    Context& m_ui;
};

} // namespace guikit
