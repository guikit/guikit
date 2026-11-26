#include "guikit/core/overlay_manager.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/core/text_util.hpp"
#include "guikit/style/style_painter.hpp"
#include "guikit/style/theme.hpp"
#include "keel/math.h"

namespace guikit {

namespace {

template <typename T, typename Pred>
void EraseIf(keel::Array<T>& arr, Pred&& pred) noexcept {
    keel::usize writeIdx = 0;
    for (keel::usize i = 0; i < arr.size(); ++i) {
        if (!pred(arr[i])) {
            if (writeIdx != i) {
                arr[writeIdx] = keel::move(arr[i]);
            }
            ++writeIdx;
        }
    }
    arr.resize(writeIdx);
}

} // namespace

OverlayManager::~OverlayManager() noexcept {
    ClearOverlayPasses();
    ClearOverlayHitBuilders();
}

void OverlayManager::ClearOverlayPasses() noexcept {
    for (auto& pass : m_overlayPasses) {
        if (pass.destroy && pass.ctx) {
            pass.destroy(pass.ctx);
        }
    }
    m_overlayPasses.clear();
}

void OverlayManager::ClearOverlayHitBuilders() noexcept {
    for (auto& builder : m_overlayHitBuilders) {
        if (builder.destroy && builder.ctx) {
            builder.destroy(builder.ctx);
        }
    }
    m_overlayHitBuilders.clear();
}

void OverlayManager::BeginFrame() noexcept {
    ClearOverlayPasses();
    ClearOverlayHitBuilders();
    m_currOverlayHitRegions.clear();
    m_currFrameModalOverlay = false;
    m_openPopupVisualHash = 0;
    m_currFrameModals.clear();
    m_activeModalStack.clear();
    m_modalClosedThisFrame = false;
    m_popupDismissedThisFrame = false;
    m_activeTooltip = {};
    m_activeTooltipAnchor = {};
    m_activeTooltipViewport = {};
}

void OverlayManager::RegisterEndFrameHook(EndFrameHook hook) {
    if (!hook.key || !hook.invoke) return;
    for (auto& existing : m_endFrameHooks) {
        if (existing.key == hook.key) {
            existing = hook;
            return;
        }
    }
    m_endFrameHooks.push_back(hook);
}

void OverlayManager::RunEndFrameHooks(Context& ctx) {
    for (const auto& hook : m_endFrameHooks) {
        if (hook.invoke) {
            hook.invoke(ctx);
        }
    }
}

bool OverlayManager::HasActiveHookContent(const Context& ctx) const noexcept {
    for (const auto& hook : m_endFrameHooks) {
        if (hook.hasActiveContent && hook.hasActiveContent(ctx)) {
            return true;
        }
    }
    return false;
}

bool OverlayManager::AnyHookNeedsRepaint(const Context& ctx) const noexcept {
    for (const auto& hook : m_endFrameHooks) {
        if (hook.needsRepaint && hook.needsRepaint(ctx)) {
            return true;
        }
    }
    return false;
}

void OverlayManager::CommitOverlayHitRegions() {
    m_prevOverlayHitRegions = keel::move(m_currOverlayHitRegions);
    m_currOverlayHitRegions.clear();
}

void OverlayManager::OpenPopup(
    Id id,
    Rect frameBounds,
    Rect anchorFrameBounds,
    keel::SmallArray<Rect, 16>& outInvalidated,
    Id portalId
) {
    if (m_openPopupId != id || m_openPopupPortalId != portalId || m_openPopupRect != frameBounds || m_openPopupAnchorRect != anchorFrameBounds) {
        if (m_openPopupId.IsValid() && m_openPopupRect.w > 0.0f && m_openPopupRect.h > 0.0f) {
            outInvalidated.push_back(ExpandPopupDamageRect(m_openPopupRect));
        }
        for (const Rect& er : m_openPopupExtraRects) {
            if (er.w > 0.0f && er.h > 0.0f) {
                outInvalidated.push_back(ExpandPopupDamageRect(er));
            }
        }
        m_openPopupId = id;
        m_openPopupPortalId = portalId;
        m_openPopupRect = frameBounds;
        m_openPopupAnchorRect = anchorFrameBounds;
        if (m_openPopupRect.w > 0.0f && m_openPopupRect.h > 0.0f) {
            outInvalidated.push_back(ExpandPopupDamageRect(m_openPopupRect));
        }
    }
    m_openPopupExtraRects.clear();
}

void OverlayManager::UpdateOpenPopupRect(Rect frameBounds) noexcept {
    if (m_openPopupId.IsValid() && frameBounds.w > 0.0f && frameBounds.h > 0.0f) {
        m_openPopupRect = frameBounds;
    }
}

void OverlayManager::AddOpenPopupRect(Rect frameBounds) {
    if (!m_openPopupId.IsValid() || frameBounds.w <= 0.0f || frameBounds.h <= 0.0f) {
        return;
    }
    m_openPopupExtraRects.push_back(frameBounds);
}

bool OverlayManager::IsPointInOpenPopup(Vec2 framePos) const noexcept {
    if (!m_openPopupId.IsValid()) {
        return false;
    }
    if (m_openPopupRect.Contains(framePos)) {
        return true;
    }
    for (const Rect& er : m_openPopupExtraRects) {
        if (er.Contains(framePos)) {
            return true;
        }
    }
    return false;
}

bool OverlayManager::ClosePopup(keel::SmallArray<Rect, 16>& outInvalidated) {
    if (!m_openPopupId.IsValid()) {
        return false;
    }
    const Id closedId = m_openPopupId;
    if (m_openPopupRect.w > 0.0f && m_openPopupRect.h > 0.0f) {
        outInvalidated.push_back(ExpandPopupDamageRect(m_openPopupRect));
    }
    for (const Rect& er : m_openPopupExtraRects) {
        if (er.w > 0.0f && er.h > 0.0f) {
            outInvalidated.push_back(ExpandPopupDamageRect(er));
        }
    }
    m_openPopupId = Id::Null();
    m_openPopupPortalId = Id::Null();
    m_openPopupRect = Rect{};
    m_openPopupAnchorRect = Rect{};
    m_openPopupExtraRects.clear();
    m_openPopupVisualHash = 0;
    m_popupDismissedThisFrame = true;
    EraseIf(m_prevOverlayHitRegions, [closedId](const OverlayHitRegion& r) { return r.id == closedId; });
    EraseIf(m_currOverlayHitRegions, [closedId](const OverlayHitRegion& r) { return r.id == closedId; });
    return true;
}

void OverlayManager::PushModal(Id id, Rect frameBackdrop) noexcept {
    m_activeModalStack.push_back(id);
    for (auto& rec : m_currFrameModals) {
        if (rec.id == id) {
            rec.backdropRect = frameBackdrop;
            return;
        }
    }
    m_currFrameModals.push_back(ModalRecord{id, frameBackdrop});
}

Id OverlayManager::PopModal(bool stillOpen, Id id) noexcept {
    Id closedId = id;
    if (!m_activeModalStack.empty()) {
        if (!closedId.IsValid()) {
            closedId = m_activeModalStack.back();
        }
        m_activeModalStack.pop_back();
    }
    if (!stillOpen) {
        m_modalClosedThisFrame = true;
        ClearModalBlock(closedId);
    }
    return closedId;
}

void OverlayManager::ClearModalBlock(Id id) noexcept {
    if (!id.IsValid()) {
        m_currFrameModals.clear();
        m_prevFrameModals.clear();
        return;
    }
    for (keel::usize i = 0; i < m_currFrameModals.size(); ) {
        if (m_currFrameModals[i].id == id) {
            m_currFrameModals.remove_at(i);
        } else {
            ++i;
        }
    }
    for (keel::usize i = 0; i < m_prevFrameModals.size(); ) {
        if (m_prevFrameModals[i].id == id) {
            m_prevFrameModals.remove_at(i);
        } else {
            ++i;
        }
    }
}

bool OverlayManager::HasPrevFrameModal(Id id) const noexcept {
    if (!id.IsValid()) return false;
    for (const auto& m : m_prevFrameModals) {
        if (m.id == id) return true;
    }
    return false;
}

bool OverlayManager::IsTopModal(Id id) const noexcept {
    if (!id.IsValid()) return false;
    for (keel::usize i = 0; i < m_prevFrameModals.size(); ++i) {
        if (m_prevFrameModals[i].id == id) {
            return (i + 1 == m_prevFrameModals.size());
        }
    }
    for (keel::usize i = 0; i < m_currFrameModals.size(); ++i) {
        if (m_currFrameModals[i].id == id) {
            return (i + 1 == m_currFrameModals.size());
        }
    }
    return true;
}

bool OverlayManager::HasModalAboveCurrentScope() const noexcept {
    if (m_activeModalStack.empty()) {
        return !m_currFrameModals.empty() || !m_prevFrameModals.empty();
    }
    return !IsTopModal(m_activeModalStack.back());
}

bool OverlayManager::IsModalBlockingInFrame(Vec2 pos, Id exemptModalId) const noexcept {
    const Id activeModalId = exemptModalId.IsValid()
        ? exemptModalId
        : (!m_activeModalStack.empty() ? m_activeModalStack.back() : Id::Null());

    auto checkList = [&](const keel::SmallArray<ModalRecord, 4>& modals) -> bool {
        if (modals.empty()) return false;
        if (!activeModalId.IsValid()) {
            for (const auto& m : modals) {
                if (m.backdropRect.Contains(pos)) return true;
            }
            return false;
        }
        keel::usize activeIdx = keel::kInvalidIndex;
        for (keel::usize i = 0; i < modals.size(); ++i) {
            if (modals[i].id == activeModalId) {
                activeIdx = i;
                break;
            }
        }
        if (activeIdx == keel::kInvalidIndex) {
            return false;
        }
        for (keel::usize i = activeIdx + 1; i < modals.size(); ++i) {
            if (modals[i].backdropRect.Contains(pos)) return true;
        }
        return false;
    };

    return checkList(m_currFrameModals) || checkList(m_prevFrameModals);
}

Id OverlayManager::FindModalIdInPrevOrCurr(Id id) const noexcept {
    if (!id.IsValid()) return Id::Null();
    for (const auto& m : m_prevFrameModals) {
        if (m.id == id) return id;
    }
    for (const auto& m : m_currFrameModals) {
        if (m.id == id) return id;
    }
    return Id::Null();
}

void OverlayManager::SetTooltip(UiText tooltip, Rect frameAnchor, Rect frameViewport) noexcept {
    if (!tooltip.empty()) {
        m_activeTooltip = tooltip;
        m_activeTooltipAnchor = frameAnchor;
        m_activeTooltipViewport = frameViewport;
    }
}

void OverlayManager::RenderTooltipOverlay(
    ICanvas& canvas,
    Vec2 logicalSize,
    Vec2 mousePos,
    const UiTheme& theme
) const {
    if (m_activeTooltip.empty()) return;

    const keel::f32 fontSize = theme.ResolveFontSize(m_activeTooltip, TextRole::LabelSmall, UiSize::Sm);
    const keel::f32 lineH = canvas.GetLineHeight(fontSize);

    const keel::f32 maxTextW = 280.0f;
    const keel::f32 natW = TextUtil::MeasureUiTextWidth(m_activeTooltip, fontSize, &canvas);
    const keel::f32 contentW = keel::min(natW, maxTextW);
    const keel::f32 contentH = TextUtil::MeasureUiTextHeight(
        m_activeTooltip, keel::max(1.0f, contentW), fontSize, lineH, &canvas);

    const keel::f32 padX = 10.0f;
    const keel::f32 padY = 6.0f;
    const keel::f32 boxW = keel::max(28.0f, contentW + padX * 2.0f);
    const keel::f32 boxH = keel::max(24.0f, contentH + padY * 2.0f);

    const Rect vp = (m_activeTooltipViewport.w > 1.0f && m_activeTooltipViewport.h > 1.0f)
        ? m_activeTooltipViewport
        : Rect(0.0f, 0.0f, logicalSize.x, logicalSize.y);

    keel::f32 bx = mousePos.x + 12.0f;
    keel::f32 by = mousePos.y + 18.0f;
    if (m_activeTooltipAnchor.w > 0.0f && m_activeTooltipAnchor.h > 0.0f) {
        bx = m_activeTooltipAnchor.x + (m_activeTooltipAnchor.w - boxW) * 0.5f;
        by = m_activeTooltipAnchor.y - boxH - 6.0f;
        if (by < vp.y + 6.0f) {
            by = m_activeTooltipAnchor.y + m_activeTooltipAnchor.h + 6.0f;
        }
    }

    bx = keel::clamp(bx, vp.x + 6.0f, keel::max(vp.x + 6.0f, vp.Right() - boxW - 6.0f));
    by = keel::clamp(by, vp.y + 6.0f, keel::max(vp.y + 6.0f, vp.Bottom() - boxH - 6.0f));

    canvas.PushClip(vp);
    const Rect boxRect(bx, by, boxW, boxH);
    const keel::f32 tipRadius = theme.GetRadius(UiSize::Md);
    theme.Painter().DrawSurface(canvas, theme, SurfacePaintParams{
        .rect = boxRect,
        .role = SurfaceRole::Popup,
        .radius = tipRadius,
        .bg = theme.palette.surfaceElevated.WithAlpha(248u),
        .border = theme.palette.border,
        .borderWidth = 1.0f,
        .elevation = Elevation::Sm
    });

    const Color textColor = theme.ResolveTextColor(m_activeTooltip, theme.palette.textPrimary);

    TextUtil::DrawUiText(
        canvas, m_activeTooltip,
        Rect(bx + padX, by + padY, contentW + 2.0f, contentH),
        fontSize, lineH, textColor, 0.0f, 0.5f);
    canvas.PopClip();
}

OverlayDamageSummary OverlayManager::FinalizeFrameState(const Context& ctx, Vec2 mousePos) {
    OverlayDamageSummary summary{};

    bool popupExtraChanged = (m_openPopupExtraRects.size() != m_prevOpenPopupExtraRects.size());
    if (!popupExtraChanged) {
        for (keel::usize i = 0; i < m_openPopupExtraRects.size(); ++i) {
            if (m_openPopupExtraRects[i] != m_prevOpenPopupExtraRects[i]) {
                popupExtraChanged = true;
                break;
            }
        }
    }
    if (m_openPopupId != m_prevOpenPopupId ||
        m_openPopupRect != m_prevOpenPopupRect ||
        popupExtraChanged ||
        (m_openPopupId.IsValid() && m_openPopupVisualHash != m_prevOpenPopupVisualHash)) {
        if (m_prevOpenPopupId.IsValid() && m_prevOpenPopupRect.w > 0.0f && m_prevOpenPopupRect.h > 0.0f) {
            summary.popupDamageRects.push_back(ExpandPopupDamageRect(m_prevOpenPopupRect));
        }
        for (const Rect& er : m_prevOpenPopupExtraRects) {
            if (er.w > 0.0f && er.h > 0.0f) {
                summary.popupDamageRects.push_back(ExpandPopupDamageRect(er));
            }
        }
        if (m_openPopupId.IsValid() && m_openPopupRect.w > 0.0f && m_openPopupRect.h > 0.0f) {
            summary.popupDamageRects.push_back(ExpandPopupDamageRect(m_openPopupRect));
        }
        for (const Rect& er : m_openPopupExtraRects) {
            if (er.w > 0.0f && er.h > 0.0f) {
                summary.popupDamageRects.push_back(ExpandPopupDamageRect(er));
            }
        }
    }
    m_prevOpenPopupId = m_openPopupId;
    m_prevOpenPopupRect = m_openPopupRect;
    m_prevOpenPopupExtraRects.clear();
    for (const Rect& er : m_openPopupExtraRects) {
        m_prevOpenPopupExtraRects.push_back(er);
    }
    m_prevOpenPopupVisualHash = m_openPopupVisualHash;

    const bool hasTooltip = !m_activeTooltip.empty();
    const keel::u64 tooltipHash = hasTooltip
        ? detail::HashArgs(
              m_activeTooltip,
              m_activeTooltipAnchor,
              m_activeTooltipViewport,
              m_activeTooltipAnchor.w <= 0.0f ? mousePos.x : 0.0f,
              m_activeTooltipAnchor.w <= 0.0f ? mousePos.y : 0.0f
          )
        : 0;
    const bool tooltipChanged = (hasTooltip != m_hadTooltipLastFrame) ||
                                (hasTooltip && tooltipHash != m_prevTooltipHash);

    const bool hasModal = !m_currFrameModals.empty();
    const bool modalChanged = (hasModal != m_hadModalLastFrame) || m_modalClosedThisFrame;

    const bool hasHookContent = HasActiveHookContent(ctx);
    const bool hookContentChanged = (hasHookContent != m_hadHookContentLastFrame);

    const bool modalOverlayChanged = (m_currFrameModalOverlay != m_hadModalOverlayLastFrame);

    summary.hasTooltip = hasTooltip;
    summary.anyOverlayStateChanged =
        modalChanged || hookContentChanged || modalOverlayChanged || tooltipChanged;
    summary.hasActiveFullRepaintOverlay =
        hasModal || hasHookContent || m_currFrameModalOverlay || hasTooltip;

    m_hadTooltipLastFrame = hasTooltip;
    m_prevTooltipHash = tooltipHash;
    m_hadHookContentLastFrame = hasHookContent;
    m_hadModalLastFrame = hasModal || m_modalClosedThisFrame;
    m_hadModalOverlayLastFrame = m_currFrameModalOverlay;
    m_prevFrameModals = keel::move(m_currFrameModals);
    m_currFrameModals.clear();

    return summary;
}

bool OverlayManager::DoesOverlayRegionOcclude(
    const OverlayHitRegion& ov,
    Id targetId,
    keel::u8 targetZLayer,
    Id targetPortalId,
    Vec2 framePos
) noexcept {
    if (ov.id == targetId || !ov.rect.Contains(framePos)) {
        return false;
    }
    if (keel::as<keel::u8>(ov.layer) >= keel::as<keel::u8>(OverlayLayer::Popup)) {
        return true;
    }
    if (ov.zLayer > targetZLayer) {
        return true;
    }
    if (ov.zLayer == targetZLayer && ov.ownerPortalId == targetPortalId) {
        return true;
    }
    return false;
}

} // namespace guikit
