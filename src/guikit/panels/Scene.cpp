#include "guikit/panels/Scene.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <format>
#include <string>
#include "guikit/Context.hpp"
#include "guikit/core/Element.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"
#include "guikit/panels/ScrollArea.hpp"
#include "guikit/widgets/Image.hpp"

namespace guikit {

namespace {

ElementSpec MakeSceneSpec(const SceneOptions& opts, UiSize size, const UiTheme& theme, bool disabled) noexcept {
    const bool stretchH = (opts.heightMode == SizeMode::Stretch) && !opts.height.has_value();
    return ElementSpec{
        .label = opts.label,
        .description = opts.description,
        .layout = opts.layout,
        .inlineOrder = opts.inlineOrder,
        .elementAlign = opts.elementAlign,
        .stretch = opts.stretch,
        .width = opts.width,
        .widgetSize = size,
        .labelGap = theme.Scale(6.0f),
        .descGap = theme.Scale(4.0f),
        .inlineGap = theme.Scale(12.0f),
        .stretchableControl = true,
        .stretchHeightWhenStandalone = stretchH,
        .disabled = disabled
    };
}

Vec2 ComputeSceneControlSize(
    const SceneOptions& opts,
    const UiTheme& theme,
    float availableWidth
) noexcept {
    const float defW = opts.worldSize.has_value() ? theme.Scale(opts.worldSize->x) : theme.Scale(360.0f);
    const float defH = opts.worldSize.has_value() ? theme.Scale(opts.worldSize->y) : theme.Scale(opts.minHeight);
    const float aspect = (defH > 0.0f) ? (defW / defH) : (16.0f / 9.0f);

    float ctrlW = defW;
    if (opts.stretch == StretchMode::Fill && availableWidth > 0.0f) {
        const float pct = std::clamp(opts.width.value_or(100.0f), 1.0f, 100.0f);
        ctrlW = availableWidth * (pct * 0.01f);
    } else if (opts.width.has_value() && *opts.width > 0.0f) {
        ctrlW = theme.Scale(*opts.width);
    }

    float ctrlH = theme.Scale(opts.minHeight);
    if (opts.height.has_value() && *opts.height > 0.0f) {
        ctrlH = theme.Scale(*opts.height);
    } else if (opts.heightMode == SizeMode::Fit && opts.worldSize.has_value() && aspect > 0.0f) {
        ctrlH = std::max(theme.Scale(opts.minHeight), ctrlW / aspect);
    }

    return { std::max(40.0f, ctrlW), std::max(40.0f, ctrlH) };
}

void ApplyInitialFit(
    SceneState& state,
    const SceneOptions& opts,
    Rect vp,
    const UiTheme& theme
) noexcept {
    if (vp.w <= 1.0f || vp.h <= 1.0f) return;

    if (!opts.worldSize.has_value() || opts.worldSize->x <= 0.0f || opts.worldSize->y <= 0.0f) {
        state.zoom = 1.0f;
        state.pan = { vp.w * 0.5f, vp.h * 0.5f };
        state.initialized = true;
        return;
    }

    const float s = theme.Scale(1.0f);
    const Vec2 scaledWorld = { opts.worldSize->x * s, opts.worldSize->y * s };
    const Rect fitted = ComputeBoxFit(opts.initialFit, scaledWorld, Rect(0.0f, 0.0f, vp.w, vp.h));
    const float fitZoom = (scaledWorld.x > 0.0f) ? (fitted.w / scaledWorld.x) : 1.0f;
    state.zoom = std::clamp(fitZoom, opts.minZoom, opts.maxZoom);
    state.pan = {
        (vp.w - scaledWorld.x * state.zoom) * 0.5f,
        (vp.h - scaledWorld.y * state.zoom) * 0.5f
    };
    state.initialized = true;
}

void ClampCameraToWorld(
    SceneState& state,
    const SceneOptions& opts,
    Rect vp,
    const UiTheme& theme
) noexcept {
    if (!opts.clampToWorld || !opts.worldSize.has_value() || vp.w <= 0.0f || vp.h <= 0.0f) {
        return;
    }
    const float s = theme.Scale(1.0f);
    const float worldScreenW = opts.worldSize->x * s * state.zoom;
    const float worldScreenH = opts.worldSize->y * s * state.zoom;

    if (worldScreenW <= vp.w) {
        state.pan.x = (vp.w - worldScreenW) * 0.5f;
    } else {
        state.pan.x = std::clamp(state.pan.x, vp.w - worldScreenW, 0.0f);
    }

    if (worldScreenH <= vp.h) {
        state.pan.y = (vp.h - worldScreenH) * 0.5f;
    } else {
        state.pan.y = std::clamp(state.pan.y, vp.h - worldScreenH, 0.0f);
    }
}

struct HudLayout {
    Rect barRect{};
    Rect minusBtn{};
    Rect fitBtn{};
    Rect oneBtn{};
    Rect plusBtn{};
    Rect badgeRect{};
};

HudLayout ComputeHudLayout(Rect vp, const UiTheme& theme, bool hasHScroll, bool hasVScroll, UiSize size) noexcept {
    HudLayout hud{};
    const float s = theme.Scale(1.0f);
    const ScrollbarMetrics& sm = theme.GetScrollbar(size);
    const float marginX = 10.0f * s + (hasVScroll ? (sm.trackWidth + sm.margin) : 0.0f);
    const float marginY = 10.0f * s + (hasHScroll ? (sm.trackWidth + sm.margin) : 0.0f);

    const float btnH = 24.0f * s;
    const float sqW = 24.0f * s;
    const float fitW = 34.0f * s;
    const float oneW = 40.0f * s;
    const float badgeW = 46.0f * s;
    const float pad = 3.0f * s;
    const float gap = 2.0f * s;

    const float totalW = pad * 2.0f + sqW + fitW + oneW + sqW + badgeW + gap * 4.0f;
    const float totalH = btnH + pad * 2.0f;

    const float x = std::max(vp.x + 4.0f * s, vp.Right() - totalW - marginX);
    const float y = std::max(vp.y + 4.0f * s, vp.Bottom() - totalH - marginY);

    hud.barRect = Rect(x, y, totalW, totalH);
    float cx = x + pad;
    const float cy = y + pad;

    hud.minusBtn = Rect(cx, cy, sqW, btnH); cx += sqW + gap;
    hud.fitBtn   = Rect(cx, cy, fitW, btnH); cx += fitW + gap;
    hud.oneBtn   = Rect(cx, cy, oneW, btnH); cx += oneW + gap;
    hud.plusBtn  = Rect(cx, cy, sqW, btnH); cx += sqW + gap;
    hud.badgeRect = Rect(cx, cy, badgeW, btnH);
    return hud;
}

} // namespace

SceneItemScope::SceneItemScope(
    SceneScope* scene,
    int32_t itemIndex,
    int32_t prevItemIndex,
    bool hovered,
    bool selected,
    bool clicked
) noexcept
    : m_scene(scene)
    , m_itemIndex(itemIndex)
    , m_prevItemIndex(prevItemIndex)
    , m_hovered(hovered)
    , m_selected(selected)
    , m_clicked(clicked)
{
}

SceneItemScope::~SceneItemScope() noexcept {
    if (m_scene && m_itemIndex >= 0) {
        m_scene->FinalizeItem(m_itemIndex, m_prevItemIndex);
    }
}

SceneItemScope::SceneItemScope(SceneItemScope&& other) noexcept
    : m_scene(other.m_scene)
    , m_itemIndex(other.m_itemIndex)
    , m_prevItemIndex(other.m_prevItemIndex)
    , m_hovered(other.m_hovered)
    , m_selected(other.m_selected)
    , m_clicked(other.m_clicked)
{
    other.m_scene = nullptr;
    other.m_itemIndex = -1;
}

SceneScope::SceneScope(Context* ctx, Node* node, ScenePayload* payload, Rect prevViewport) noexcept
    : ContainerScope(ctx, node)
    , m_payload(payload)
    , m_viewport(prevViewport)
{
}

SceneItemScope SceneScope::Item(const SceneItemOptions& opts) {
    if (!m_ctx || !m_payload || !m_payload->state) {
        return SceneItemScope(nullptr, -1, -1, false, false, false);
    }

    const int32_t prevIdx = m_payload->activeItemIndex;
    const int32_t newIdx = static_cast<int32_t>(m_payload->items.size());

    uint64_t baseKey = detail::HashStringView(opts.id.empty() ? opts.title : opts.id);
    if (baseKey == 0) {
        baseKey = detail::HashArgs(static_cast<uint32_t>(newIdx + 1));
    }
    // Disambiguate if another item in the same scene shares the same base string key
    uint64_t key = baseKey;
    uint32_t dupCount = 0;
    for (const SceneItemInfo& existing : m_payload->items) {
        if (existing.key == key) {
            ++dupCount;
            key = detail::HashArgs(baseKey, dupCount);
        }
    }

    const SceneState& state = *m_payload->state;
    const bool isHov = (key != 0 && state.hoveredItemKey == key);
    const bool isSel = state.IsSelected(key);
    const bool isClk = (key != 0 && state.clickedItemKey == key);

    SceneItemInfo info{};
    info.key = key;
    info.id = m_ctx->AllocString(opts.id);
    info.title = m_ctx->AllocString(opts.title);
    info.category = m_ctx->AllocString(opts.category);
    info.firstCmdIndex = static_cast<int32_t>(m_payload->drawCmds.size());
    info.hasExplicitBounds = opts.hitRect.has_value();
    info.worldBounds = opts.hitRect.value_or(Rect{});
    info.selectable = opts.selectable;
    info.hovered = isHov;
    info.selected = isSel;
    info.clicked = isClk;

    m_payload->items.push_back(info);
    m_payload->activeItemIndex = newIdx;

    return SceneItemScope(this, newIdx, prevIdx, isHov, isSel, isClk);
}

void SceneScope::FinalizeItem(int32_t itemIndex, int32_t prevItemIndex) noexcept {
    if (!m_payload) return;
    m_payload->activeItemIndex = prevItemIndex;

    if (itemIndex < 0 || itemIndex >= static_cast<int32_t>(m_payload->items.size())) {
        return;
    }

    SceneItemInfo& item = m_payload->items[static_cast<size_t>(itemIndex)];
    if (item.hasExplicitBounds) {
        return;
    }

    float minX = 1e30f, minY = 1e30f;
    float maxX = -1e30f, maxY = -1e30f;
    bool hasGeom = false;

    auto expandPt = [&](Vec2 p) {
        minX = std::min(minX, p.x);
        minY = std::min(minY, p.y);
        maxX = std::max(maxX, p.x);
        maxY = std::max(maxY, p.y);
        hasGeom = true;
    };
    auto expandRect = [&](Rect r) {
        if (r.w <= 0.0f && r.h <= 0.0f) return;
        expandPt(r.Pos());
        expandPt({r.Right(), r.Bottom()});
    };

    const int32_t cmdCount = static_cast<int32_t>(m_payload->drawCmds.size());
    for (int32_t i = item.firstCmdIndex; i < cmdCount; ++i) {
        const SceneDrawCmd& cmd = m_payload->drawCmds[static_cast<size_t>(i)];
        if (cmd.itemIndex != itemIndex) continue;

        switch (cmd.type) {
            case SceneDrawCmd::Type::Image:
            case SceneDrawCmd::Type::Rect:
                expandRect(cmd.rect);
                break;
            case SceneDrawCmd::Type::Line:
                expandPt(cmd.p1);
                expandPt(cmd.p2);
                break;
            case SceneDrawCmd::Type::Triangle:
                expandPt(cmd.p1);
                expandPt(cmd.p2);
                expandPt(cmd.p3);
                break;
            case SceneDrawCmd::Type::Text:
            case SceneDrawCmd::Type::TextBox:
                // Excluded from primary geometry pass so external text callouts don't inflate component hitboxes
                break;
        }
    }

    // Fallback if the item consists solely of text commands
    if (!hasGeom) {
        for (int32_t i = item.firstCmdIndex; i < cmdCount; ++i) {
            const SceneDrawCmd& cmd = m_payload->drawCmds[static_cast<size_t>(i)];
            if (cmd.itemIndex != itemIndex) continue;
            if (cmd.type == SceneDrawCmd::Type::TextBox) {
                expandRect(cmd.rect);
            } else if (cmd.type == SceneDrawCmd::Type::Text) {
                expandRect(Rect(cmd.p1.x, cmd.p1.y - cmd.fontSize, cmd.fontSize * 3.0f, cmd.fontSize));
            }
        }
    }

    if (hasGeom) {
        // Ensure thin single-line items have a minimum 4-unit world AABB thickness
        if (maxX - minX < 4.0f) {
            const float cx = 0.5f * (minX + maxX);
            minX = cx - 2.0f;
            maxX = cx + 2.0f;
        }
        if (maxY - minY < 4.0f) {
            const float cy = 0.5f * (minY + maxY);
            minY = cy - 2.0f;
            maxY = cy + 2.0f;
        }
        item.worldBounds = Rect(minX, minY, maxX - minX, maxY - minY);
    }
}

std::span<const SceneItemInfo> SceneScope::GetItems() const noexcept {
    if (!m_payload) return {};
    return m_payload->items;
}

const SceneItemInfo* SceneScope::GetHoveredItem() const noexcept {
    if (!m_payload) return nullptr;
    for (const SceneItemInfo& item : m_payload->items) {
        if (item.hovered) return &item;
    }
    return nullptr;
}

size_t SceneScope::GetSelectedCount() const noexcept {
    return (m_payload && m_payload->state) ? m_payload->state->selectedItemKeys.size() : 0;
}

void SceneScope::ClearSelection() noexcept {
    if (!m_payload || !m_payload->state) return;
    m_payload->state->selectedItemKeys.clear();
    for (SceneItemInfo& item : m_payload->items) {
        item.selected = false;
    }
}

void SceneScope::SelectItem(std::string_view id, bool addToSelection) noexcept {
    if (!m_payload || !m_payload->state || id.empty()) return;
    const uint64_t key = detail::HashStringView(id);
    if (!addToSelection) {
        m_payload->state->selectedItemKeys.clear();
    }
    if (!m_payload->state->IsSelected(key)) {
        m_payload->state->selectedItemKeys.push_back(key);
    }
    for (SceneItemInfo& item : m_payload->items) {
        item.selected = m_payload->state->IsSelected(item.key);
    }
}

float SceneScope::GetZoom() const noexcept {
    return (m_payload && m_payload->state) ? m_payload->state->zoom : 1.0f;
}

float SceneScope::GetWorldScale() const noexcept {
    if (!m_ctx || !m_payload || !m_payload->state) return 1.0f;
    return std::max(0.0001f, m_ctx->GetTheme().Scale(1.0f) * m_payload->state->zoom);
}

void SceneScope::SetZoom(float zoom) noexcept {
    if (!m_payload || !m_payload->state) return;
    m_payload->state->zoom = std::clamp(zoom, m_payload->opts.minZoom, m_payload->opts.maxZoom);
}

void SceneScope::CenterOn(Vec2 worldPoint, std::optional<float> zoom) noexcept {
    if (!m_payload || !m_payload->state) return;
    SceneState& st = *m_payload->state;
    if (zoom.has_value()) {
        st.zoom = std::clamp(*zoom, m_payload->opts.minZoom, m_payload->opts.maxZoom);
    }
    if (m_ctx && m_viewport.w > 1.0f && m_viewport.h > 1.0f) {
        const UiTheme& theme = m_ctx->GetTheme();
        const float s = theme.Scale(1.0f) * st.zoom;
        st.pan.x = m_viewport.w * 0.5f - worldPoint.x * s;
        st.pan.y = m_viewport.h * 0.5f - worldPoint.y * s;
        ClampCameraToWorld(st, m_payload->opts, m_viewport, theme);
        st.initialized = true;
        st.pendingCenterWorld.reset();
        st.pendingCenterZoom.reset();
    } else {
        st.pendingCenterWorld = worldPoint;
        st.pendingCenterZoom = zoom;
    }
}

void SceneScope::ResetView() noexcept {
    if (!m_payload || !m_payload->state) return;
    m_payload->state->initialized = false;
    m_payload->state->pendingCenterWorld.reset();
    m_payload->state->pendingCenterZoom.reset();
}

Vec2 SceneScope::GetPan() const noexcept {
    return (m_payload && m_payload->state) ? m_payload->state->pan : Vec2{0.0f, 0.0f};
}

Vec2 SceneScope::WorldToScreen(Vec2 worldPt) const noexcept {
    if (!m_ctx || !m_payload || !m_payload->state) return worldPt;
    const float s = m_ctx->GetTheme().Scale(1.0f) * m_payload->state->zoom;
    return {
        m_viewport.x + m_payload->state->pan.x + worldPt.x * s,
        m_viewport.y + m_payload->state->pan.y + worldPt.y * s
    };
}

Vec2 SceneScope::ScreenToWorld(Vec2 screenPt) const noexcept {
    if (!m_ctx || !m_payload || !m_payload->state) return screenPt;
    const float s = std::max(0.0001f, m_ctx->GetTheme().Scale(1.0f) * m_payload->state->zoom);
    return {
        (screenPt.x - m_viewport.x - m_payload->state->pan.x) / s,
        (screenPt.y - m_viewport.y - m_payload->state->pan.y) / s
    };
}

Rect SceneScope::WorldToScreen(Rect worldRect) const noexcept {
    const Vec2 tl = WorldToScreen(worldRect.Pos());
    const float s = (m_ctx && m_payload && m_payload->state)
        ? (m_ctx->GetTheme().Scale(1.0f) * m_payload->state->zoom)
        : 1.0f;
    return { tl.x, tl.y, worldRect.w * s, worldRect.h * s };
}

Rect SceneScope::GetVisibleWorldRect() const noexcept {
    if (m_viewport.w <= 0.0f || m_viewport.h <= 0.0f) return {};
    const Vec2 tl = ScreenToWorld(m_viewport.Pos());
    const Vec2 br = ScreenToWorld({m_viewport.Right(), m_viewport.Bottom()});
    return { tl.x, tl.y, std::max(0.0f, br.x - tl.x), std::max(0.0f, br.y - tl.y) };
}

Vec2 SceneScope::GetMouseWorldPos() const noexcept {
    if (!m_ctx) return {};
    return ScreenToWorld(m_ctx->GetMousePos());
}

void SceneScope::DrawImage(
    std::string_view source,
    Rect worldRect,
    float radius,
    uint8_t opacity
) {
    if (!m_ctx || !m_payload || source.empty()) return;
    SceneDrawCmd cmd{};
    cmd.type = SceneDrawCmd::Type::Image;
    cmd.itemIndex = m_payload->activeItemIndex;
    cmd.textOrSource = m_ctx->AllocString(source);
    cmd.rect = worldRect;
    cmd.radius = radius;
    cmd.opacity = opacity;
    m_payload->drawCmds.push_back(cmd);
}

void SceneScope::DrawRect(
    Rect worldRect,
    Color fill,
    float radius,
    Color stroke,
    float strokeWidth
) {
    if (!m_payload) return;
    SceneDrawCmd cmd{};
    cmd.type = SceneDrawCmd::Type::Rect;
    cmd.itemIndex = m_payload->activeItemIndex;
    cmd.rect = worldRect;
    cmd.fill = fill;
    cmd.radius = radius;
    cmd.stroke = stroke;
    cmd.strokeWidth = strokeWidth;
    m_payload->drawCmds.push_back(cmd);
}

void SceneScope::DrawCircle(
    Vec2 worldCenter,
    float worldRadius,
    Color fill,
    Color stroke,
    float strokeWidth
) {
    if (worldRadius <= 0.0f) return;
    DrawRect(
        Rect(worldCenter.x - worldRadius, worldCenter.y - worldRadius, worldRadius * 2.0f, worldRadius * 2.0f),
        fill,
        worldRadius,
        stroke,
        strokeWidth
    );
}

void SceneScope::DrawTriangle(
    Vec2 worldP1,
    Vec2 worldP2,
    Vec2 worldP3,
    Color fill,
    Color stroke,
    float strokeWidth
) {
    if (!m_payload) return;
    SceneDrawCmd cmd{};
    cmd.type = SceneDrawCmd::Type::Triangle;
    cmd.itemIndex = m_payload->activeItemIndex;
    cmd.p1 = worldP1;
    cmd.p2 = worldP2;
    cmd.p3 = worldP3;
    cmd.fill = fill;
    cmd.stroke = stroke;
    cmd.strokeWidth = strokeWidth;
    m_payload->drawCmds.push_back(cmd);
}

void SceneScope::DrawArc(
    Vec2 worldCenter,
    float worldRadius,
    float startAngleDeg,
    float sweepAngleDeg,
    Color color,
    float strokeWidth,
    int segments
) {
    if (!m_payload || worldRadius <= 0.0f || segments < 2 || std::abs(sweepAngleDeg) < 0.01f) return;
    constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
    const float startRad = startAngleDeg * kDegToRad;
    const float stepRad = (sweepAngleDeg * kDegToRad) / static_cast<float>(segments);

    Vec2 prev{
        worldCenter.x + std::cos(startRad) * worldRadius,
        worldCenter.y + std::sin(startRad) * worldRadius
    };
    for (int i = 1; i <= segments; ++i) {
        const float ang = startRad + stepRad * static_cast<float>(i);
        const Vec2 cur{
            worldCenter.x + std::cos(ang) * worldRadius,
            worldCenter.y + std::sin(ang) * worldRadius
        };
        DrawLine(prev, cur, color, strokeWidth);
        prev = cur;
    }
}

void SceneScope::DrawLine(Vec2 worldP1, Vec2 worldP2, Color color, float strokeWidth) {
    if (!m_payload) return;
    SceneDrawCmd cmd{};
    cmd.type = SceneDrawCmd::Type::Line;
    cmd.itemIndex = m_payload->activeItemIndex;
    cmd.p1 = worldP1;
    cmd.p2 = worldP2;
    cmd.fill = color;
    cmd.strokeWidth = strokeWidth;
    m_payload->drawCmds.push_back(cmd);
}

void SceneScope::DrawText(
    std::string_view text,
    Vec2 worldPos,
    float fontSize,
    Color color,
    FontStyle style,
    bool scaleWithZoom,
    float angleDeg
) {
    if (!m_ctx || !m_payload || text.empty()) return;
    SceneDrawCmd cmd{};
    cmd.type = SceneDrawCmd::Type::Text;
    cmd.itemIndex = m_payload->activeItemIndex;
    cmd.textOrSource = m_ctx->AllocString(text);
    cmd.p1 = worldPos;
    cmd.fontSize = fontSize;
    cmd.fill = color;
    cmd.fontStyle = style;
    cmd.scaleFontWithZoom = scaleWithZoom;
    cmd.angleDeg = angleDeg;
    m_payload->drawCmds.push_back(cmd);
}

void SceneScope::DrawTextBox(
    std::string_view text,
    Rect worldRect,
    float fontSize,
    Color color,
    float alignX,
    float alignY,
    FontStyle style,
    bool scaleWithZoom,
    float angleDeg
) {
    if (!m_ctx || !m_payload || text.empty() || worldRect.w <= 0.0f || worldRect.h <= 0.0f) return;
    SceneDrawCmd cmd{};
    cmd.type = SceneDrawCmd::Type::TextBox;
    cmd.itemIndex = m_payload->activeItemIndex;
    cmd.textOrSource = m_ctx->AllocString(text);
    cmd.rect = worldRect;
    cmd.fontSize = fontSize;
    cmd.fill = color;
    cmd.alignX = alignX;
    cmd.alignY = alignY;
    cmd.fontStyle = style;
    cmd.scaleFontWithZoom = scaleWithZoom;
    cmd.angleDeg = angleDeg;
    m_payload->drawCmds.push_back(cmd);
}

namespace {

void UpdateSceneCameraTransform(
    Context& ctx,
    Node* node,
    ScenePayload* payload,
    Rect vp,
    const UiTheme& theme
) {
    SceneState& state = *payload->state;
    const SceneOptions& opts = payload->opts;

    if (!state.initialized) {
        ApplyInitialFit(state, opts, vp, theme);
    }
    if (state.pendingCenterWorld.has_value()) {
        if (state.pendingCenterZoom.has_value()) {
            state.zoom = std::clamp(*state.pendingCenterZoom, opts.minZoom, opts.maxZoom);
        }
        const float s = theme.Scale(1.0f) * state.zoom;
        state.pan.x = vp.w * 0.5f - state.pendingCenterWorld->x * s;
        state.pan.y = vp.h * 0.5f - state.pendingCenterWorld->y * s;
        ClampCameraToWorld(state, opts, vp, theme);
        state.initialized = true;
        state.pendingCenterWorld.reset();
        state.pendingCenterZoom.reset();
    }

    state.preUpdateHudConsumed = false;
    state.preUpdateScrollbarBusy = false;
    if (node->disabled) {
        ClampCameraToWorld(state, opts, vp, theme);
        return;
    }

    const float s = theme.Scale(1.0f);
    const float worldScale = std::max(0.0001f, state.zoom * s);
    const Vec2 mousePos = ctx.GetMousePos();
    const bool insideVp = vp.Contains(mousePos);
    const bool mouseClicked = ctx.IsMouseClicked();
    const bool mouseDown = ctx.IsMouseDown();
    const KeyModifiers mods = ctx.GetKeyModifiers();

    const bool hasWorld = opts.worldSize.has_value() && opts.worldSize->x > 0.0f && opts.worldSize->y > 0.0f;
    const float worldScreenW = hasWorld ? (opts.worldSize->x * s * state.zoom) : 0.0f;
    const float worldScreenH = hasWorld ? (opts.worldSize->y * s * state.zoom) : 0.0f;
    const bool showVScroll = opts.showScrollbars && hasWorld && (worldScreenH > vp.h + 0.5f);
    const bool showHScroll = opts.showScrollbars && hasWorld && (worldScreenW > vp.w + 0.5f);

    // 1. Floating Zoom HUD interaction
    bool hudConsumed = false;
    state.hoveredHudBtn = 0;
    if (opts.showControls) {
        const HudLayout hud = ComputeHudLayout(vp, theme, showHScroll, showVScroll, node->uiSize);
        if (hud.barRect.Contains(mousePos)) {
            hudConsumed = true;
            if (hud.minusBtn.Contains(mousePos)) state.hoveredHudBtn = 1;
            else if (hud.fitBtn.Contains(mousePos)) state.hoveredHudBtn = 2;
            else if (hud.oneBtn.Contains(mousePos)) state.hoveredHudBtn = 3;
            else if (hud.plusBtn.Contains(mousePos)) state.hoveredHudBtn = 4;

            if (mouseClicked && state.hoveredHudBtn != 0) {
                const Vec2 centerRel = { vp.w * 0.5f, vp.h * 0.5f };
                const Vec2 worldCenter = {
                    (centerRel.x - state.pan.x) / worldScale,
                    (centerRel.y - state.pan.y) / worldScale
                };
                if (state.hoveredHudBtn == 1) {
                    state.zoom = std::clamp(state.zoom / opts.zoomStep, opts.minZoom, opts.maxZoom);
                    state.pan = centerRel - worldCenter * (state.zoom * s);
                } else if (state.hoveredHudBtn == 2) {
                    ApplyInitialFit(state, opts, vp, theme);
                } else if (state.hoveredHudBtn == 3) {
                    state.zoom = std::clamp(1.0f, opts.minZoom, opts.maxZoom);
                    state.pan = centerRel - worldCenter * (state.zoom * s);
                } else if (state.hoveredHudBtn == 4) {
                    state.zoom = std::clamp(state.zoom * opts.zoomStep, opts.minZoom, opts.maxZoom);
                    state.pan = centerRel - worldCenter * (state.zoom * s);
                }
                ctx.RequestRepaint(2);
            }
        }
    }

    // 2. 2D Scrollbar interaction (when worldSize is finite)
    bool hovY = false, actY = false, hovX = false, actX = false;
    if (opts.showScrollbars && hasWorld) {
        const ScrollbarMetrics& sm = theme.GetScrollbar(node->uiSize);
        const float cornerReserve = (showVScroll && showHScroll) ? (sm.trackWidth + sm.margin) : 0.0f;

        if (showVScroll) {
            float scrollY = std::clamp(-state.pan.y, 0.0f, std::max(0.0f, worldScreenH - vp.h));
            ScrollArea::UpdateAxisScrollbar(
                ctx,
                node->id,
                vp,
                worldScreenH,
                scrollY,
                state.draggingScrollY,
                state.dragStartScrollY,
                state.dragStartScrollMouse,
                hovY,
                actY,
                false,
                node->uiSize,
                theme,
                cornerReserve
            );
            state.pan.y = -scrollY;
        } else {
            state.draggingScrollY = false;
        }

        if (showHScroll) {
            float scrollX = std::clamp(-state.pan.x, 0.0f, std::max(0.0f, worldScreenW - vp.w));
            ScrollArea::UpdateAxisScrollbar(
                ctx,
                node->id,
                vp,
                worldScreenW,
                scrollX,
                state.draggingScrollX,
                state.dragStartScrollX,
                state.dragStartScrollMouse,
                hovX,
                actX,
                true,
                node->uiSize,
                theme,
                cornerReserve
            );
            state.pan.x = -scrollX;
        } else {
            state.draggingScrollX = false;
        }
    }
    node->scrollbarHovered = hovY;
    node->scrollbarActive = actY;
    node->scrollbarHoveredX = hovX;
    node->scrollbarActiveX = actX;

    const bool scrollbarBusy = hovY || actY || hovX || actX;

    // 3. Mouse wheel focal-point zoom (around cursor position)
    if (opts.zoomable && insideVp && !hudConsumed) {
        const Vec2 wheel = ctx.GetMouseWheel();
        if (wheel.y != 0.0f) {
            const float oldZoom = state.zoom;
            const float factor = std::pow(opts.zoomStep, wheel.y);
            const float newZoom = std::clamp(oldZoom * factor, opts.minZoom, opts.maxZoom);
            if (std::abs(newZoom - oldZoom) > 0.0001f) {
                const Vec2 mouseRel = mousePos - vp.Pos();
                const Vec2 worldUnderMouse = {
                    (mouseRel.x - state.pan.x) / (oldZoom * s),
                    (mouseRel.y - state.pan.y) / (oldZoom * s)
                };
                state.zoom = newZoom;
                state.pan = mouseRel - worldUnderMouse * (newZoom * s);
                ctx.RequestRepaint(2);
            }
        }
    }

    const Vec2 mouseWorld = {
        (mousePos.x - vp.x - state.pan.x) / std::max(0.0001f, state.zoom * s),
        (mousePos.y - vp.y - state.pan.y) / std::max(0.0001f, state.zoom * s)
    };

    // 4. Pointer Press & Active Drag-to-Pan / Marquee Box Update
    if (!hudConsumed && !scrollbarBusy && insideVp && mouseClicked) {
        if (opts.selectableItems && mods.shift) {
            state.boxSelecting = true;
            state.boxSelectStartWorld = mouseWorld;
            state.boxSelectCurrentWorld = mouseWorld;
            ctx.SetActiveId(node->id);
            ctx.RequestRepaint(2);
        } else {
            state.pointerDown = true;
            state.panning = false;
            state.panStartMouse = mousePos;
            state.panStartOffset = state.pan;
            ctx.SetActiveId(node->id);
        }
    }

    if (state.boxSelecting && mouseDown) {
        state.boxSelectCurrentWorld = mouseWorld;
        ctx.RequestRepaint(2);
    } else if (state.pointerDown && mouseDown) {
        const Vec2 delta = mousePos - state.panStartMouse;
        if (!state.panning && opts.pannable && (std::abs(delta.x) >= 3.5f || std::abs(delta.y) >= 3.5f)) {
            state.panning = true;
        }
        if (state.panning) {
            state.pan = state.panStartOffset + delta;
            ctx.RequestRepaint(2);
        }
    }

    ClampCameraToWorld(state, opts, vp, theme);
    state.preUpdateHudConsumed = hudConsumed;
    state.preUpdateScrollbarBusy = scrollbarBusy;
}

} // namespace

SceneScope Scene::Open(
    Context& ctx,
    const SceneOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size.value_or(ctx.GetCurrentSize());
    const bool activeDisabled = opts.disabled || ctx.IsDisabled();
    const float activeScale = opts.scale.value_or(ctx.GetCurrentScale());
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);

    const UiTheme& theme = ctx.GetTheme();
    auto& state = ctx.GetOrCreateState<SceneState>(id);

    Node* node = ctx.CreateNode(NodeType::Scene, id);
    node->theme = pushedTheme;
    node->debugBoundsOverride = opts.debugBounds;
    node->inspectModeOverride = opts.inspectMode;
    node->direction = FlexDirection::Column;
    node->clipChildren = true;
    node->uiSize = activeSize;
    node->disabled = activeDisabled;

    auto* payload = ctx.AllocPayload<ScenePayload>();
    payload->opts = opts;
    payload->opts.label = ctx.AllocText(opts.label);
    payload->opts.description = ctx.AllocText(opts.description);
    payload->state = &state;
    node->SetPayload(payload);

    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& th) noexcept -> Vec2 {
        const auto* p = n->As<ScenePayload>();
        if (!p) return {0.0f, 0.0f};
        return Scene::Measure(p->opts, n->uiSize, canvas, th, availableWidth);
    };
    node->renderFn = &Scene::RenderBackgroundAndWorld;
    node->postRenderFn = &Scene::RenderOverlays;

    const bool hasExt = opts.label.HasContentOrReserve() || opts.description.HasContentOrReserve();
    node->widthMode = (opts.stretch != StretchMode::Compact) ? SizeMode::Stretch : opts.widthMode;
    node->heightMode = opts.heightMode;
    if (opts.width.has_value() && opts.stretch != StretchMode::Fill && !hasExt) {
        node->widthMode = SizeMode::Fixed;
        node->fixedWidth = theme.Scale(*opts.width);
    }
    if (opts.height.has_value() && *opts.height > 0.0f && !hasExt) {
        node->heightMode = SizeMode::Fixed;
        node->fixedHeight = theme.Scale(*opts.height);
    }
    node->minHeight = opts.height.has_value() ? theme.Scale(*opts.height) : theme.Scale(opts.minHeight);

    Rect prevBounds = ctx.GetPrevFrameBounds(id);
    Rect prevVp = prevBounds;
    if (prevBounds.w > 0.0f && prevBounds.h > 0.0f && hasExt) {
        const ElementSpec spec = MakeSceneSpec(opts, activeSize, theme, activeDisabled);
        const Vec2 ctrlSize = ComputeSceneControlSize(opts, theme, prevBounds.w);
        prevVp = ElementComposer::Resolve(prevBounds, ctrlSize, spec, nullptr, theme).controlRect;
    }

    // Eagerly update camera pan & zoom before SceneScope body runs so GetVisibleWorldRect() and GetZoom() have zero 1-frame lag!
    state.preUpdatedCamera = false;
    if (prevVp.w > 1.0f && prevVp.h > 1.0f) {
        UpdateSceneCameraTransform(ctx, node, payload, prevVp, theme);
        state.preUpdatedCamera = true;
    }

    node->visualHash = detail::HashArgs(
        payload->opts.label,
        payload->opts.description,
        opts.worldSize.has_value() ? opts.worldSize->x : 0.0f,
        opts.worldSize.has_value() ? opts.worldSize->y : 0.0f,
        static_cast<uint8_t>(opts.initialFit),
        static_cast<uint8_t>(opts.background),
        opts.gridStep,
        opts.backgroundColor.ToUint32(),
        static_cast<uint8_t>(opts.layout),
        static_cast<uint8_t>(opts.stretch),
        static_cast<uint8_t>(opts.heightMode),
        opts.width.value_or(0.0f),
        opts.height.value_or(0.0f),
        opts.showScrollbars,
        opts.showControls,
        state.zoom,
        state.pan.x,
        state.pan.y,
        state.hoveredHudBtn,
        state.hoveredItemKey,
        state.selectedItemKeys.size(),
        state.boxSelecting,
        state.boxSelectCurrentWorld.x,
        state.boxSelectCurrentWorld.y,
        activeSize,
        activeDisabled
    );

    ctx.PushScale(activeScale);
    ctx.PushSize(activeSize);
    ctx.PushDisabled(activeDisabled);
    ctx.PushContainer(node);
    return SceneScope(&ctx, node, payload, prevVp);
}

Rect Scene::GetViewportRect(const Node* node, ICanvas* canvas, const UiTheme& theme) noexcept {
    if (!node) return {};
    const auto* payload = node->As<ScenePayload>();
    if (!payload) return node->computedRect;

    const ElementSpec spec = MakeSceneSpec(payload->opts, node->uiSize, theme, node->disabled);
    const Vec2 ctrlSize = ComputeSceneControlSize(payload->opts, theme, node->computedRect.w);
    return ElementComposer::Resolve(node->computedRect, ctrlSize, spec, canvas, theme).controlRect;
}

Vec2 Scene::Measure(
    const SceneOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float availableWidth
) {
    const ElementSpec spec = MakeSceneSpec(opts, size, theme, opts.disabled);
    const Vec2 ctrlSize = ComputeSceneControlSize(opts, theme, availableWidth);
    return ElementComposer::Measure(ctrlSize, spec, canvas, theme, availableWidth);
}

void Scene::Update(Context& ctx, Node* node) {
    if (!node || !node->id.IsValid()) return;
    auto* payload = node->As<ScenePayload>();
    if (!payload || !payload->state) return;

    SceneState& state = *payload->state;
    const SceneOptions& opts = payload->opts;
    state.clickedItemKey = 0;

    const UiTheme* resolvedTheme = &ctx.GetTheme();
    for (const Node* curr = node; curr != nullptr; curr = curr->parent) {
        if (curr->theme != nullptr) {
            resolvedTheme = curr->theme;
            break;
        }
    }
    const UiTheme& theme = *resolvedTheme;
    const Rect vp = GetViewportRect(node, nullptr, theme);
    if (vp.w <= 1.0f || vp.h <= 1.0f) return;

    if (!state.preUpdatedCamera) {
        UpdateSceneCameraTransform(ctx, node, payload, vp, theme);
    } else {
        ClampCameraToWorld(state, opts, vp, theme);
    }

    if (node->disabled) return;

    const float s = theme.Scale(1.0f);
    const Vec2 mousePos = ctx.GetMousePos();
    const bool insideVp = vp.Contains(mousePos);
    const bool mouseDown = ctx.IsMouseDown();
    const KeyModifiers mods = ctx.GetKeyModifiers();
    const bool hudConsumed = state.preUpdateHudConsumed;
    const bool scrollbarBusy = state.preUpdateScrollbarBusy;

    const bool hasWorld = opts.worldSize.has_value() && opts.worldSize->x > 0.0f && opts.worldSize->y > 0.0f;
    const float worldScreenW = hasWorld ? (opts.worldSize->x * s * state.zoom) : 0.0f;
    const float worldScreenH = hasWorld ? (opts.worldSize->y * s * state.zoom) : 0.0f;

    const Vec2 mouseWorld = {
        (mousePos.x - vp.x - state.pan.x) / std::max(0.0001f, state.zoom * s),
        (mousePos.y - vp.y - state.pan.y) / std::max(0.0001f, state.zoom * s)
    };

    // Spatial Hit-Testing for SceneItems (smallest-area / topmost priority)
    const uint64_t prevHoveredKey = state.hoveredItemKey;
    state.hoveredItemKey = 0;
    if (opts.selectableItems && insideVp && !hudConsumed && !scrollbarBusy && !state.panning && !state.boxSelecting) {
        const float worldHitPad = (3.5f * s) / std::max(0.0001f, state.zoom * s);
        float bestArea = 1e30f;

        for (const SceneItemInfo& item : payload->items) {
            if (!item.selectable || item.worldBounds.w <= 0.0f || item.worldBounds.h <= 0.0f) {
                continue;
            }
            const Rect hitBox(
                item.worldBounds.x - worldHitPad,
                item.worldBounds.y - worldHitPad,
                item.worldBounds.w + worldHitPad * 2.0f,
                item.worldBounds.h + worldHitPad * 2.0f
            );
            if (hitBox.Contains(mouseWorld)) {
                const float area = item.worldBounds.Area();
                if (area <= bestArea * 1.05f) {
                    bestArea = area;
                    state.hoveredItemKey = item.key;
                }
            }
        }
    }
    if (state.hoveredItemKey != prevHoveredKey) {
        ctx.RequestRepaint(2);
    }

    // Complete Marquee Box Selection or Click-to-Select on mouse button release
    if (state.boxSelecting && !mouseDown) {
        const float x1 = std::min(state.boxSelectStartWorld.x, state.boxSelectCurrentWorld.x);
        const float y1 = std::min(state.boxSelectStartWorld.y, state.boxSelectCurrentWorld.y);
        const float x2 = std::max(state.boxSelectStartWorld.x, state.boxSelectCurrentWorld.x);
        const float y2 = std::max(state.boxSelectStartWorld.y, state.boxSelectCurrentWorld.y);
        const Rect marquee(x1, y1, x2 - x1, y2 - y1);

        if (!mods.ctrl) {
            state.selectedItemKeys.clear();
        }
        if (marquee.w > 1.0f || marquee.h > 1.0f) {
            for (const SceneItemInfo& item : payload->items) {
                if (item.selectable && item.worldBounds.Intersects(marquee)) {
                    if (!state.IsSelected(item.key)) {
                        state.selectedItemKeys.push_back(item.key);
                    }
                }
            }
        }
        state.boxSelecting = false;
        if (ctx.GetActiveId() == node->id) {
            ctx.SetActiveId(Id::Null());
        }
        ctx.RequestRepaint(2);
    } else if (state.pointerDown && !mouseDown) {
        if (!state.panning && opts.selectableItems) {
            if (state.hoveredItemKey != 0) {
                state.clickedItemKey = state.hoveredItemKey;
                if (mods.ctrl) {
                    auto it = std::find(state.selectedItemKeys.begin(), state.selectedItemKeys.end(), state.hoveredItemKey);
                    if (it != state.selectedItemKeys.end()) {
                        state.selectedItemKeys.erase(it);
                    } else {
                        state.selectedItemKeys.push_back(state.hoveredItemKey);
                    }
                } else {
                    state.selectedItemKeys.clear();
                    state.selectedItemKeys.push_back(state.hoveredItemKey);
                }
            } else if (!mods.ctrl) {
                state.selectedItemKeys.clear();
            }
            ctx.RequestRepaint(2);
        }
        state.pointerDown = false;
        state.panning = false;
        if (ctx.GetActiveId() == node->id) {
            ctx.SetActiveId(Id::Null());
        }
    }

    // Sync current-frame item flags and visualHash so rendering & damage tracking are immediate
    uint64_t selHash = 0;
    for (uint64_t k : state.selectedItemKeys) {
        selHash = detail::HashArgs(selHash, k);
    }
    for (SceneItemInfo& item : payload->items) {
        item.hovered = (item.key != 0 && item.key == state.hoveredItemKey);
        item.selected = state.IsSelected(item.key);
        item.clicked = (item.key != 0 && item.key == state.clickedItemKey);
    }

    ClampCameraToWorld(state, opts, vp, theme);

    node->visualHash = detail::HashArgs(
        node->visualHash,
        state.zoom,
        state.pan.x,
        state.pan.y,
        state.hoveredHudBtn,
        state.hoveredItemKey,
        selHash,
        state.boxSelecting,
        state.boxSelectCurrentWorld.x,
        state.boxSelectCurrentWorld.y
    );

    node->scrollContentSize = { worldScreenW, worldScreenH };
    node->scrollOffset = { std::max(0.0f, -state.pan.x), std::max(0.0f, -state.pan.y) };
}

void Scene::RenderBackgroundAndWorld(
    const Node* node,
    ICanvas& canvas,
    const UiTheme& theme
) {
    if (!node) return;
    const auto* payload = node->As<ScenePayload>();
    if (!payload || !payload->state) return;

    const SceneOptions& opts = payload->opts;
    const SceneState& state = *payload->state;

    const ElementSpec spec = MakeSceneSpec(opts, node->uiSize, theme, node->disabled);
    const Vec2 ctrlSize = ComputeSceneControlSize(opts, theme, node->computedRect.w);
    const ElementGeometry geom = ElementComposer::Resolve(node->computedRect, ctrlSize, spec, &canvas, theme);
    ElementComposer::RenderText(canvas, geom, spec, theme);

    const Rect vp = geom.controlRect;
    if (vp.w <= 0.0f || vp.h <= 0.0f) return;

    const float rad = opts.borderRadius.has_value() ? theme.Scale(*opts.borderRadius) : 0.0f;
    const Color bgCol = (opts.backgroundColor.a > 0) ? opts.backgroundColor : theme.palette.surface;

    // 1. Render Background Pattern
    if (opts.background == SceneBackground::Checkerboard) {
        Image::DrawCheckerboard(canvas, vp, rad, theme, 12.0f);
    } else {
        canvas.DrawRoundRect(vp, rad, bgCol);
    }

    canvas.PushClip(vp.Inset(1.0f));

    const float s = theme.Scale(1.0f);
    const float worldScale = state.zoom * s;

    // Adaptive world-space coordinate grid (GridDots or GridLines)
    if ((opts.background == SceneBackground::GridDots || opts.background == SceneBackground::GridLines) && worldScale > 0.001f) {
        float worldStep = std::max(4.0f, opts.gridStep);
        while (worldStep * worldScale < 16.0f * s) worldStep *= 2.0f;
        while (worldStep * worldScale > 160.0f * s) worldStep *= 0.5f;

        const float screenStep = worldStep * worldScale;
        const float startX = vp.x + std::fmod(state.pan.x, screenStep) - (state.pan.x < 0.0f ? 0.0f : screenStep);
        const float startY = vp.y + std::fmod(state.pan.y, screenStep) - (state.pan.y < 0.0f ? 0.0f : screenStep);
        const Color gridCol = theme.palette.borderSubtle.WithAlpha(130);

        if (opts.background == SceneBackground::GridLines) {
            for (float x = startX; x <= vp.Right(); x += screenStep) {
                if (x >= vp.x) canvas.DrawLine({x, vp.y}, {x, vp.Bottom()}, gridCol, 1.0f);
            }
            for (float y = startY; y <= vp.Bottom(); y += screenStep) {
                if (y >= vp.y) canvas.DrawLine({vp.x, y}, {vp.Right(), y}, gridCol, 1.0f);
            }
            // Highlight world axes (X=0, Y=0)
            const float originX = vp.x + state.pan.x;
            const float originY = vp.y + state.pan.y;
            const Color axisCol = theme.palette.accent.WithAlpha(110);
            if (originX >= vp.x && originX <= vp.Right()) {
                canvas.DrawLine({originX, vp.y}, {originX, vp.Bottom()}, axisCol, 1.5f);
            }
            if (originY >= vp.y && originY <= vp.Bottom()) {
                canvas.DrawLine({vp.x, originY}, {vp.Right(), originY}, axisCol, 1.5f);
            }
        } else {
            const float dotSize = std::max(2.0f, 2.0f * s);
            for (float y = startY; y <= vp.Bottom(); y += screenStep) {
                if (y < vp.y) continue;
                for (float x = startX; x <= vp.Right(); x += screenStep) {
                    if (x < vp.x) continue;
                    canvas.DrawRect(Rect(x - dotSize * 0.5f, y - dotSize * 0.5f, dotSize, dotSize), gridCol);
                }
            }
        }
    }

    auto worldRectToScreen = [&](Rect wr) noexcept -> Rect {
        return Rect(
            vp.x + state.pan.x + wr.x * worldScale,
            vp.y + state.pan.y + wr.y * worldScale,
            wr.w * worldScale,
            wr.h * worldScale
        );
    };

    auto blendColor = [](Color c, Color target, float t) noexcept -> Color {
        if (c.a == 0) return c;
        return Color(
            static_cast<uint8_t>(std::round(c.r + (target.r - c.r) * t)),
            static_cast<uint8_t>(std::round(c.g + (target.g - c.g) * t)),
            static_cast<uint8_t>(std::round(c.b + (target.b - c.b) * t)),
            c.a
        );
    };

    const Color accentCol = theme.palette.accent;
    const float boxPad = 4.0f * s;

    // 2. Render subtle backdrop glow under hovered/selected SceneItems
    for (const SceneItemInfo& item : payload->items) {
        if ((!item.hovered && !item.selected) || item.worldBounds.w <= 0.0f || item.worldBounds.h <= 0.0f) {
            continue;
        }
        const Rect sr = worldRectToScreen(item.worldBounds);
        const Rect halo(sr.x - boxPad, sr.y - boxPad, sr.w + boxPad * 2.0f, sr.h + boxPad * 2.0f);
        if (halo.Intersects(vp)) {
            canvas.DrawRoundRect(halo, 3.0f * s, accentCol.WithAlpha(item.selected ? 34 : 18));
        }
    }

    // 3. Execute recorded world-space draw commands (with automatic SceneItem tinting)
    const int32_t itemCount = static_cast<int32_t>(payload->items.size());
    for (const SceneDrawCmd& cmd : payload->drawCmds) {
        const SceneItemInfo* owner = (cmd.itemIndex >= 0 && cmd.itemIndex < itemCount)
            ? &payload->items[static_cast<size_t>(cmd.itemIndex)]
            : nullptr;
        const bool isSel = owner && owner->selected;
        const bool isHov = owner && owner->hovered && !isSel;
        const float tintFactor = isSel ? 1.0f : (isHov ? 0.72f : 0.0f);
        const float strokeBoost = isSel ? 1.25f : (isHov ? 1.12f : 1.0f);

        switch (cmd.type) {
            case SceneDrawCmd::Type::Image: {
                const Rect dst = worldRectToScreen(cmd.rect);
                if (dst.Intersects(vp)) {
                    canvas.DrawImage(cmd.textOrSource, dst, cmd.radius * state.zoom, cmd.opacity);
                }
                break;
            }
            case SceneDrawCmd::Type::Rect: {
                const Rect dst = worldRectToScreen(cmd.rect);
                if (dst.Intersects(vp)) {
                    const float sw = (cmd.strokeWidth > 0.0f)
                        ? std::max(0.75f, cmd.strokeWidth * worldScale * strokeBoost)
                        : 0.0f;
                    Color fill = cmd.fill;
                    Color stroke = cmd.stroke;
                    if (tintFactor > 0.0f) {
                        if (cmd.strokeWidth > 0.0f && stroke.a > 0) {
                            stroke = blendColor(stroke, accentCol, tintFactor);
                        } else if (cmd.strokeWidth == 0.0f && fill.a > 0) {
                            fill = blendColor(fill, accentCol, tintFactor);
                        }
                    }
                    canvas.DrawRoundRect(dst, cmd.radius * worldScale, fill, stroke, sw);
                }
                break;
            }
            case SceneDrawCmd::Type::Line: {
                const Vec2 sp1 = {
                    vp.x + state.pan.x + cmd.p1.x * worldScale,
                    vp.y + state.pan.y + cmd.p1.y * worldScale
                };
                const Vec2 sp2 = {
                    vp.x + state.pan.x + cmd.p2.x * worldScale,
                    vp.y + state.pan.y + cmd.p2.y * worldScale
                };
                const Color col = (tintFactor > 0.0f) ? blendColor(cmd.fill, accentCol, tintFactor) : cmd.fill;
                canvas.DrawLine(sp1, sp2, col, std::max(0.75f, cmd.strokeWidth * worldScale * strokeBoost));
                break;
            }
            case SceneDrawCmd::Type::Triangle: {
                const Vec2 sp1 = {
                    vp.x + state.pan.x + cmd.p1.x * worldScale,
                    vp.y + state.pan.y + cmd.p1.y * worldScale
                };
                const Vec2 sp2 = {
                    vp.x + state.pan.x + cmd.p2.x * worldScale,
                    vp.y + state.pan.y + cmd.p2.y * worldScale
                };
                const Vec2 sp3 = {
                    vp.x + state.pan.x + cmd.p3.x * worldScale,
                    vp.y + state.pan.y + cmd.p3.y * worldScale
                };
                const float sw = (cmd.strokeWidth > 0.0f)
                    ? std::max(0.75f, cmd.strokeWidth * worldScale * strokeBoost)
                    : 0.0f;
                const Color fill = (tintFactor > 0.0f) ? blendColor(cmd.fill, accentCol, tintFactor) : cmd.fill;
                const Color stroke = (tintFactor > 0.0f) ? blendColor(cmd.stroke, accentCol, tintFactor) : cmd.stroke;
                canvas.DrawTriangle(sp1, sp2, sp3, fill, stroke, sw);
                break;
            }
            case SceneDrawCmd::Type::Text: {
                const Vec2 sp = {
                    vp.x + state.pan.x + cmd.p1.x * worldScale,
                    vp.y + state.pan.y + cmd.p1.y * worldScale
                };
                const float fs = cmd.scaleFontWithZoom
                    ? (cmd.fontSize * worldScale)
                    : theme.Scale(cmd.fontSize);
                if (fs >= 2.2f) {
                    const Color col = (tintFactor > 0.0f) ? blendColor(cmd.fill, accentCol, tintFactor) : cmd.fill;
                    canvas.DrawTextStyled(cmd.textOrSource, sp, fs, col, cmd.fontStyle, cmd.angleDeg);
                }
                break;
            }
            case SceneDrawCmd::Type::TextBox: {
                const Rect dst = worldRectToScreen(cmd.rect);
                if (dst.Intersects(vp)) {
                    const float fs = cmd.scaleFontWithZoom
                        ? (cmd.fontSize * worldScale)
                        : theme.Scale(cmd.fontSize);
                    if (fs >= 2.2f) {
                        const Color col = (tintFactor > 0.0f) ? blendColor(cmd.fill, accentCol, tintFactor) : cmd.fill;
                        const Vec2 ts = canvas.MeasureTextStyled(cmd.textOrSource, fs, cmd.fontStyle);
                        const float opticalH = fs * 1.18f;
                        if (std::abs(cmd.angleDeg + 90.0f) < 1.0f) {
                            const float tx = dst.x + (dst.w - opticalH) * cmd.alignX;
                            const float ty = dst.y + ts.x + (dst.h - ts.x) * cmd.alignY;
                            canvas.DrawTextStyled(cmd.textOrSource, {tx, ty}, fs, col, cmd.fontStyle, cmd.angleDeg);
                        } else {
                            const float tx = dst.x + (dst.w - ts.x) * cmd.alignX;
                            const float ty = dst.y + (dst.h - opticalH) * cmd.alignY;
                            canvas.DrawTextStyled(cmd.textOrSource, {tx, ty}, fs, col, cmd.fontStyle, cmd.angleDeg);
                        }
                    }
                }
                break;
            }
        }
    }

    // 4. Render CAD Selection & Hover Frames with Corner Grips
    const float gripSz = 4.5f * s;
    const float halfGrip = gripSz * 0.5f;
    for (const SceneItemInfo& item : payload->items) {
        if ((!item.hovered && !item.selected) || item.worldBounds.w <= 0.0f || item.worldBounds.h <= 0.0f) {
            continue;
        }
        const Rect sr = worldRectToScreen(item.worldBounds);
        const Rect frame(sr.x - boxPad, sr.y - boxPad, sr.w + boxPad * 2.0f, sr.h + boxPad * 2.0f);
        if (!frame.Intersects(vp)) continue;

        if (item.selected) {
            canvas.DrawRoundRect(frame, 2.0f * s, Color::Transparent(), accentCol, 1.5f * s);
            const Vec2 corners[4] = {
                {frame.x, frame.y},
                {frame.Right(), frame.y},
                {frame.x, frame.Bottom()},
                {frame.Right(), frame.Bottom()}
            };
            for (const Vec2& c : corners) {
                canvas.DrawRoundRect(
                    Rect(c.x - halfGrip, c.y - halfGrip, gripSz, gripSz),
                    1.0f,
                    accentCol,
                    theme.palette.surfaceElevated,
                    1.0f
                );
            }
        } else if (item.hovered) {
            canvas.DrawRoundRect(frame, 3.0f * s, Color::Transparent(), accentCol.WithAlpha(165), 1.0f * s);
        }
    }

    // 5. Render Shift+Drag Marquee Selection Box
    if (state.boxSelecting) {
        const float wx1 = std::min(state.boxSelectStartWorld.x, state.boxSelectCurrentWorld.x);
        const float wy1 = std::min(state.boxSelectStartWorld.y, state.boxSelectCurrentWorld.y);
        const float wx2 = std::max(state.boxSelectStartWorld.x, state.boxSelectCurrentWorld.x);
        const float wy2 = std::max(state.boxSelectStartWorld.y, state.boxSelectCurrentWorld.y);
        const Rect mScreen = worldRectToScreen(Rect(wx1, wy1, wx2 - wx1, wy2 - wy1));
        if (mScreen.w > 1.0f || mScreen.h > 1.0f) {
            canvas.DrawRoundRect(mScreen, 2.0f * s, accentCol.WithAlpha(35), accentCol, 1.5f * s);
        }
    }

    canvas.PopClip();
}

void Scene::RenderOverlays(
    const Node* node,
    ICanvas& canvas,
    const UiTheme& theme
) {
    if (!node) return;
    const auto* payload = node->As<ScenePayload>();
    if (!payload || !payload->state) return;

    const SceneOptions& opts = payload->opts;
    const SceneState& state = *payload->state;
    const Rect vp = GetViewportRect(node, &canvas, theme);
    if (vp.w <= 0.0f || vp.h <= 0.0f) return;

    const float s = theme.Scale(1.0f);
    const bool hasWorld = opts.worldSize.has_value() && opts.worldSize->x > 0.0f && opts.worldSize->y > 0.0f;
    const float worldScreenW = hasWorld ? (opts.worldSize->x * s * state.zoom) : 0.0f;
    const float worldScreenH = hasWorld ? (opts.worldSize->y * s * state.zoom) : 0.0f;
    const bool showVScroll = opts.showScrollbars && hasWorld && (worldScreenH > vp.h + 0.5f);
    const bool showHScroll = opts.showScrollbars && hasWorld && (worldScreenW > vp.w + 0.5f);
    const ScrollbarMetrics& sm = theme.GetScrollbar(node->uiSize);
    const float cornerReserve = (showVScroll && showHScroll) ? (sm.trackWidth + sm.margin) : 0.0f;

    // 1. Render 2D Scrollbars via shared ScrollArea primitive
    if (showVScroll) {
        ScrollArea::RenderAxisScrollbar(
            canvas,
            vp,
            worldScreenH,
            std::max(0.0f, -state.pan.y),
            false,
            node->scrollbarHovered,
            node->scrollbarActive,
            node->uiSize,
            theme,
            cornerReserve
        );
    }
    if (showHScroll) {
        ScrollArea::RenderAxisScrollbar(
            canvas,
            vp,
            worldScreenW,
            std::max(0.0f, -state.pan.x),
            true,
            node->scrollbarHoveredX,
            node->scrollbarActiveX,
            node->uiSize,
            theme,
            cornerReserve
        );
    }

    // 2. Render Floating Zoom HUD (Bottom-Right) and CAD Selection Inspector Pill (Bottom-Left)
    if (opts.showControls && vp.w > 180.0f * s && vp.h > 60.0f * s) {
        const HudLayout hud = ComputeHudLayout(vp, theme, showHScroll, showVScroll, node->uiSize);
        const Color hudBg = theme.palette.surfaceElevated.WithAlpha(235);
        const Color hudBorder = theme.palette.borderHover;
        canvas.DrawRoundRect(hud.barRect, 6.0f * s, hudBg, hudBorder, 1.0f);

        const float fs = theme.GetFontSize(TextRole::LabelSmall);
        auto drawHudBtn = [&](Rect r, std::string_view text, uint8_t btnId) {
            if (state.hoveredHudBtn == btnId) {
                canvas.DrawRoundRect(r, 4.0f * s, theme.palette.accent.WithAlpha(45));
            }
            const Color tc = (state.hoveredHudBtn == btnId) ? theme.palette.accent : theme.palette.textPrimary;
            canvas.DrawTextBox(text, r, fs, tc, 0.5f, 0.5f);
        };

        drawHudBtn(hud.minusBtn, "-", 1);
        drawHudBtn(hud.fitBtn, "Fit", 2);
        drawHudBtn(hud.oneBtn, "100%", 3);
        drawHudBtn(hud.plusBtn, "+", 4);

        char zoomBuf[24];
        std::snprintf(zoomBuf, sizeof(zoomBuf), "%.0f%%", state.zoom * 100.0f);
        canvas.DrawTextBox(zoomBuf, hud.badgeRect, fs, theme.palette.textSecondary, 0.5f, 0.5f);

        // CAD Component Inspector Pill (Bottom-Left) when an item is hovered or selected
        const SceneItemInfo* hovItem = nullptr;
        const SceneItemInfo* primarySelItem = nullptr;
        for (const SceneItemInfo& item : payload->items) {
            if (item.hovered) hovItem = &item;
            if (item.selected) primarySelItem = &item;
        }

        const size_t selCount = state.selectedItemKeys.size();
        const SceneItemInfo* activeInfo = hovItem ? hovItem : primarySelItem;
        if (activeInfo || selCount > 0) {
            std::string badgeText;
            std::string detailText;
            if (selCount > 1 && !hovItem) {
                badgeText = "MULTI-SELECT";
                detailText = std::format("{} components selected  (Ctrl+Click or Shift+Drag)", selCount);
            } else if (activeInfo) {
                badgeText = activeInfo->category.empty()
                    ? (activeInfo->selected ? "SELECTED" : "HOVER")
                    : std::string(activeInfo->category);
                detailText = std::string(activeInfo->id);
                if (!activeInfo->title.empty()) {
                    detailText += "  —  ";
                    detailText += activeInfo->title;
                }
                if (selCount > 1) {
                    detailText += std::format("  (+{} selected)", selCount - 1);
                }
            }

            if (!detailText.empty()) {
                const float pad = 4.0f * s;
                const float badgeTextW = canvas.MeasureText(badgeText, fs).x + 14.0f * s;
                const float detailTextW = canvas.MeasureText(detailText, fs).x + 16.0f * s;
                const float pillW = std::min(hud.barRect.x - (vp.x + 16.0f * s), pad * 2.0f + badgeTextW + detailTextW);
                if (pillW > 80.0f * s) {
                    const Rect pillRect(vp.x + 10.0f * s, hud.barRect.y, pillW, hud.barRect.h);
                    canvas.DrawRoundRect(pillRect, 6.0f * s, hudBg, hudBorder, 1.0f);

                    const Rect catRect(pillRect.x + pad, pillRect.y + pad, badgeTextW, pillRect.h - pad * 2.0f);
                    canvas.DrawRoundRect(catRect, 4.0f * s, theme.palette.accent.WithAlpha(40));
                    canvas.DrawTextBox(badgeText, catRect, fs, theme.palette.accent, 0.5f, 0.5f);

                    const Rect txtRect(catRect.Right() + 8.0f * s, pillRect.y + pad, std::max(0.0f, pillRect.Right() - catRect.Right() - 12.0f * s), pillRect.h - pad * 2.0f);
                    canvas.DrawTextBox(detailText, txtRect, fs, theme.palette.textPrimary, 0.0f, 0.5f);
                }
            }
        }
    }

    // 3. Render Viewport Frame Border
    if (opts.borderWidth > 0.0f) {
        const float rad = opts.borderRadius.has_value() ? theme.Scale(*opts.borderRadius) : 0.0f;
        const Color bCol = (opts.borderColor.a > 0) ? opts.borderColor : theme.palette.borderSubtle;
        canvas.DrawRoundRect(vp, rad, Color::Transparent(), bCol, theme.Scale(opts.borderWidth));
    }
}

} // namespace guikit
