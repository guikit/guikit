#pragma once

#include "guikit/core/id.hpp"
#include "guikit/style/tokens.hpp"
#include "guikit/core/types.hpp"
#include "keel/array.h"
#include "keel/core.h"
#include "keel/ref.h"
#include "keel/small_array.h"

namespace guikit {

class Context;
class ICanvas;
struct UiTheme;
class HitTestTree;

/// Z-ordering layer for deferred overlay render passes and overlay hit regions.
enum class OverlayLayer : keel::u8 {
    Panel        = 1, ///< Panel chrome overlays (Sidebar / BottomPanel / SplitView sashes & toggle pills) rendered below Modals/Popovers.
    Portal       = 2, ///< Out-of-flow portals (Modal / Popover).
    Popup        = 3, ///< Top-level interactive popups (Select, DatePicker, ColorPicker, ContextMenu, CommandPalette) rendered above Modals/Popovers.
    Notification = 4  ///< Global notifications (Toast) rendered above all popups regardless of host call order.
};

/// Rectangular hit region registered by a deferred overlay (popup, sash, notification).
struct OverlayHitRegion {
    Id id{Id::Null()};
    Rect rect{};
    OverlayLayer layer{OverlayLayer::Popup};
    keel::u8 zLayer{0};
    Id ownerPortalId{Id::Null()};
};

/// Deferred overlay render pass allocated in the frame arena.
struct OverlayPass {
    void* ctx{nullptr};
    void (*invoke)(void*, ICanvas&){nullptr};
    void (*destroy)(void*) noexcept {nullptr};
    Rect clipRect{};     ///< Frame space
    Transform2D space{}; ///< Space the overlay was registered from; it draws in that space
    OverlayLayer layer{OverlayLayer::Popup};
};

/// Deferred overlay hit-region builder executed after layout completes.
struct OverlayHitBuilder {
    void* ctx{nullptr};
    void (*invoke)(void*){nullptr};
    void (*destroy)(void*) noexcept {nullptr};
    Transform2D space{};
    keel::u8 scopeZLayer{0};
    Id ownerPortalId{Id::Null()};
};

/// Active Modal backdrop record in frame space.
struct ModalRecord {
    Id id{Id::Null()};
    Rect backdropRect{};
};

/// Generic end-of-frame hook (used by detached notification/overlay hosts such as Toast).
struct EndFrameHook {
    const void* key{nullptr};
    void (*invoke)(Context& ctx){nullptr};
    bool (*hasActiveContent)(const Context& ctx) noexcept {nullptr};
    bool (*needsRepaint)(const Context& ctx) noexcept {nullptr};
};

/// Summary of overlay damage changes for `DamageTracker`.
struct OverlayDamageSummary {
    keel::SmallArray<Rect, 16> popupDamageRects;
    bool anyOverlayStateChanged{false};
    bool hasActiveFullRepaintOverlay{false};
    bool hasTooltip{false};
};

/// Coordinates popups, modals, tooltips, deferred overlay passes/hit regions, and end-of-frame overlay hooks.
class OverlayManager {
public:
    OverlayManager() = default;
    ~OverlayManager() noexcept;
    OverlayManager(const OverlayManager&) = delete;
    OverlayManager& operator=(const OverlayManager&) = delete;

    void BeginFrame() noexcept;
    void ClearOverlayPasses() noexcept;
    void ClearOverlayHitBuilders() noexcept;

    // --- End-of-frame overlay hooks (e.g. Toast notification host) ---
    void RegisterEndFrameHook(EndFrameHook hook);
    void RunEndFrameHooks(Context& ctx);
    [[nodiscard]] bool HasActiveHookContent(const Context& ctx) const noexcept;
    [[nodiscard]] bool AnyHookNeedsRepaint(const Context& ctx) const noexcept;

    // --- Deferred Overlay Passes & Hit Builders ---
    void AddOverlayPass(OverlayPass pass) { m_overlayPasses.push_back(pass); }
    [[nodiscard]] keel::ArrayRef<OverlayPass> GetOverlayPasses() const noexcept { return m_overlayPasses; }

    void AddOverlayHitBuilder(OverlayHitBuilder builder) { m_overlayHitBuilders.push_back(builder); }
    [[nodiscard]] keel::ArrayRef<OverlayHitBuilder> GetOverlayHitBuilders() const noexcept { return m_overlayHitBuilders; }

    void AddOverlayHitRegion(OverlayHitRegion region) { m_currOverlayHitRegions.push_back(region); }
    void CommitOverlayHitRegions();
    [[nodiscard]] keel::ArrayRef<OverlayHitRegion> GetPrevOverlayHitRegions() const noexcept { return m_prevOverlayHitRegions; }

    // --- Popup State ---
    [[nodiscard]] static Rect ExpandPopupDamageRect(Rect r) noexcept {
        return Rect(r.x - 12.0f, r.y - 12.0f, r.w + 24.0f, r.h + 32.0f);
    }

    /// Opens or updates the active popup in frame coordinates; appends any invalidated rects to `outInvalidated`.
    void OpenPopup(Id id, Rect frameBounds, Rect anchorFrameBounds, keel::SmallArray<Rect, 16>& outInvalidated);
    void AddOpenPopupRect(Rect frameBounds);
    [[nodiscard]] bool IsPointInOpenPopup(Vec2 framePos) const noexcept;
    /// Closes the active popup; returns true if a popup was open and appends invalidated rects to `outInvalidated`.
    bool ClosePopup(keel::SmallArray<Rect, 16>& outInvalidated);

    [[nodiscard]] Id GetOpenPopupId() const noexcept { return m_openPopupId; }
    [[nodiscard]] bool IsPopupOpen(Id id) const noexcept { return m_openPopupId == id && id.IsValid(); }
    [[nodiscard]] Rect GetOpenPopupRect() const noexcept { return m_openPopupRect; }
    [[nodiscard]] Rect GetOpenPopupAnchorRect() const noexcept { return m_openPopupAnchorRect; }
    void SetOpenPopupVisualHash(keel::u64 hash) noexcept { m_openPopupVisualHash = hash; }

    // --- Modal Stack & Queries ---
    void PushModal(Id id, Rect frameBackdrop) noexcept;
    Id PopModal(bool stillOpen, Id id) noexcept;
    void ClearModalBlock(Id id) noexcept;
    void ClearActiveModalStack() noexcept { m_activeModalStack.clear(); }

    [[nodiscard]] bool IsInModalScope() const noexcept { return !m_activeModalStack.empty(); }
    [[nodiscard]] Id GetActiveModalId() const noexcept {
        return m_activeModalStack.empty() ? Id::Null() : m_activeModalStack.back();
    }
    [[nodiscard]] Id GetPrevFrameModalId() const noexcept {
        return m_prevFrameModals.empty() ? Id::Null() : m_prevFrameModals.back().id;
    }
    [[nodiscard]] Id GetCurrFrameModalId() const noexcept {
        return m_currFrameModals.empty() ? Id::Null() : m_currFrameModals.back().id;
    }
    [[nodiscard]] bool HasCurrFrameModals() const noexcept { return !m_currFrameModals.empty(); }
    [[nodiscard]] bool HasPrevFrameModal(Id id) const noexcept;
    [[nodiscard]] bool IsTopModal(Id id) const noexcept;
    [[nodiscard]] bool HasModalAboveCurrentScope() const noexcept;
    [[nodiscard]] bool IsModalBlockingInFrame(Vec2 pos, Id exemptModalId = Id::Null()) const noexcept;
    [[nodiscard]] Id FindModalIdInPrevOrCurr(Id id) const noexcept;

    void MarkModalOverlayActive() noexcept { m_currFrameModalOverlay = true; }
    [[nodiscard]] bool IsModalOverlayActive() const noexcept { return m_currFrameModalOverlay; }

    // --- Tooltip ---
    void SetTooltip(UiText tooltip, Rect frameAnchor, Rect frameViewport) noexcept;
    [[nodiscard]] const UiText& GetActiveTooltip() const noexcept { return m_activeTooltip; }
    void RenderTooltipOverlay(ICanvas& canvas, Vec2 logicalSize, Vec2 mousePos, const UiTheme& theme) const;

    // --- Frame Commit & Damage Summary ---
    /// Evaluates popup/tooltip/modal/hook state transitions for `DamageTracker` and commits prev-frame state.
    [[nodiscard]] OverlayDamageSummary FinalizeFrameState(
        const Context& ctx,
        Vec2 mousePos
    );

    [[nodiscard]] static bool DoesOverlayRegionOcclude(
        const OverlayHitRegion& ov,
        Id targetId,
        keel::u8 targetZLayer,
        Id targetPortalId,
        Vec2 framePos
    ) noexcept;

private:
    keel::Array<OverlayHitRegion> m_prevOverlayHitRegions;
    keel::Array<OverlayHitRegion> m_currOverlayHitRegions;
    keel::SmallArray<OverlayHitBuilder, 8> m_overlayHitBuilders;
    keel::SmallArray<OverlayPass, 8> m_overlayPasses;
    keel::SmallArray<EndFrameHook, 4> m_endFrameHooks;

    Id m_openPopupId{Id::Null()};
    Rect m_openPopupRect{0.0f, 0.0f, 0.0f, 0.0f};
    Rect m_openPopupAnchorRect{0.0f, 0.0f, 0.0f, 0.0f};
    keel::SmallArray<Rect, 8> m_openPopupExtraRects;
    keel::u64 m_openPopupVisualHash{0};

    Id m_prevOpenPopupId{Id::Null()};
    Rect m_prevOpenPopupRect{0.0f, 0.0f, 0.0f, 0.0f};
    keel::SmallArray<Rect, 8> m_prevOpenPopupExtraRects;
    keel::u64 m_prevOpenPopupVisualHash{0};

    keel::SmallArray<ModalRecord, 4> m_prevFrameModals;
    keel::SmallArray<ModalRecord, 4> m_currFrameModals;
    keel::SmallArray<Id, 4> m_activeModalStack;
    bool m_hadModalLastFrame{false};
    bool m_modalClosedThisFrame{false};

    UiText m_activeTooltip{};
    Rect m_activeTooltipAnchor{0.0f, 0.0f, 0.0f, 0.0f};
    Rect m_activeTooltipViewport{0.0f, 0.0f, 0.0f, 0.0f};
    keel::u64 m_prevTooltipHash{0};
    bool m_hadTooltipLastFrame{false};

    bool m_currFrameModalOverlay{false};
    bool m_hadModalOverlayLastFrame{false};
    bool m_hadHookContentLastFrame{false};
};

} // namespace guikit
