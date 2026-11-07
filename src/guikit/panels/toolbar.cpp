#include "guikit/panels/toolbar.hpp"
#include <algorithm>
#include <cmath>
#include "guikit/context.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/solver.hpp"
#include "guikit/panels/carousel.hpp"
#include "guikit/panels/scroll_area.hpp"

namespace guikit {

namespace {
struct ToolButtonPayload {
    ButtonOptions btn{};
    UiText origText{};
    UiText badge{};
    StateColor badgeColor{};
    float expandedWidth{0.0f};
    bool origSquare{false};
};
} // namespace

void ToolbarScope::InvalidateContext() noexcept {
    if (m_ctx) {
        m_ctx->InvalidateAll();
    }
}

void ToolbarScope::Separator(float heightRatio, std::source_location loc) {
    if (!m_ctx || !m_node) return;
    const Id id = m_ctx->MakeChildId(loc);
    const float scale = m_ctx->GetCurrentScale();
    const bool isVerticalBar = (m_opts.orientation == Orientation::Vertical);

    Node* sep = m_ctx->CreateNode(NodeType::Separator, id);
    if (isVerticalBar) {
        sep->widthMode = SizeMode::Stretch;
        sep->heightMode = SizeMode::Fixed;
        sep->fixedHeight = 8.0f * scale;
        sep->borderColor = m_node->borderColor;
        sep->measureFn = [](const Node* n, float /*availableWidth*/, ICanvas* /*canvas*/, const UiTheme& /*theme*/) noexcept -> Vec2 {
            return {20.0f, n->fixedHeight > 0.0f ? n->fixedHeight : 8.0f};
        };
        sep->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
            Toolbar::RenderSeparator(canvas, n->computedRect, n->borderColor, theme, Orientation::Vertical);
        };
    } else {
        sep->widthMode = SizeMode::Fixed;
        sep->fixedWidth = 10.0f * scale;
        sep->heightMode = SizeMode::Fixed;
        const float parentH = (m_node->fixedHeight > 0.0f) ? m_node->fixedHeight : (32.0f * scale);
        sep->fixedHeight = parentH * std::clamp(heightRatio, 0.1f, 1.0f);
        sep->borderColor = m_node->borderColor;
        sep->measureFn = [](const Node* n, float /*availableWidth*/, ICanvas* /*canvas*/, const UiTheme& /*theme*/) noexcept -> Vec2 {
            return {
                n->fixedWidth > 0.0f ? n->fixedWidth : 10.0f,
                n->fixedHeight > 0.0f ? n->fixedHeight : 20.0f
            };
        };
        sep->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
            Toolbar::RenderSeparator(canvas, n->computedRect, n->borderColor, theme, Orientation::Horizontal);
        };
    }
}

void ToolbarScope::Spacer(std::source_location loc) {
    if (!m_ctx) return;
    const Id id = m_ctx->MakeChildId(loc);
    Node* spacer = m_ctx->CreateNode(NodeType::Spacer, id);
    if (m_opts.orientation == Orientation::Vertical) {
        spacer->widthMode = SizeMode::Fit;
        spacer->heightMode = SizeMode::Stretch;
    } else {
        spacer->widthMode = SizeMode::Stretch;
    }
    spacer->isSpacer = true;
    spacer->flexGrow = 1.0f;
    spacer->measureFn = [](const Node*, float /*availableWidth*/, ICanvas*, const UiTheme&) noexcept -> Vec2 {
        return {0.0f, 0.0f};
    };
}

Response ToolbarScope::ToolButton(
    ToolButtonOptions opts,
    std::source_location loc
) {
    if (!m_ctx) return Response{};
    return Toolbar::ToolButton(*m_ctx, opts, loc);
}

Response ToolbarScope::Toggle(
    bool& active,
    ToolButtonOptions opts,
    std::source_location loc
) {
    if (!m_ctx) return Response{};
    opts.active = active;
    if (active && (!opts.color.HasAny() || opts.color == StateColor::Ghost())) {
        opts.color = StateColor::Primary();
    } else if (!active) {
        opts.color = StateColor::Ghost();
    }
    Response res = Toolbar::ToolButton(*m_ctx, opts, loc);
    if (res.clicked) {
        active = !active;
        res.changed = true;
        m_ctx->InvalidateAll();
    }
    return res;
}

ContextMenuResponse ToolbarScope::Dropdown(
    ToolButtonOptions opts,
    std::span<const ContextMenuItem> menuItems,
    ContextMenuOptions menuOpts,
    std::source_location loc
) {
    if (!m_ctx) return ContextMenuResponse{};
    if (!opts.icon.IsValid() && !opts.square) {
        opts.icon = Icon::ChevronDown();
        opts.iconPlacement = Placement::Trailing;
    }
    const Response btnRes = Toolbar::ToolButton(*m_ctx, opts, loc);
    menuOpts.trigger = ContextMenuTrigger::LeftClick;
    return ContextMenu::Run(*m_ctx, nullptr, std::nullopt, btnRes.rect, menuItems, menuOpts, loc);
}

ToolbarScope Toolbar::Open(
    Context& ctx,
    ToolbarOptions opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const float activeScale = opts.scale.value_or(ctx.GetCurrentScale());
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);

    const UiTheme& theme = ctx.GetTheme();
    const ComponentMetrics& m = theme.GetMetrics(activeSize);
    const float barExtent = m.defaultHeight + 8.0f * activeScale;
    const bool isVertical = (opts.orientation == Orientation::Vertical);

    Response res{};
    res.id = id;
    res.rect = ctx.GetPrevUnclippedBounds(id);
    res.ctx = &ctx;

    Node* node = ctx.CreateNode(NodeType::Toolbar, id);
    node->theme = pushedTheme;
    node->debugBoundsOverride = opts.debugBounds;
    node->inspectModeOverride = opts.inspectMode;
    node->direction = isVertical
        ? FlexDirection::Column
        : (opts.overflow == ToolbarOverflow::Wrap ? FlexDirection::Wrap : FlexDirection::Row);
    node->align = Alignment::Center;
    const float defaultGap = std::max(4.0f * activeScale, std::round(theme.GetGap(activeSize) * 0.5f));
    const float crossPad = 4.0f * activeScale;
    node->gap = opts.gap.has_value() ? theme.ResolveGap(opts.gap, activeSize) : defaultGap;
    node->runGap = node->gap;
    node->clipChildren = true;

    Padding toolbarPadding;
    if (opts.padding.has_value()) {
        if (opts.padding->IsToken()) {
            const UiSize t = opts.padding->GetToken();
            if (t == UiSize::None) {
                toolbarPadding = Padding{};
            } else {
                const float g = std::max(4.0f * activeScale, std::round(theme.GetGap(t) * 0.5f));
                toolbarPadding = isVertical
                    ? Padding::Symmetric(crossPad, g)
                    : Padding::Symmetric(g, crossPad);
            }
        } else {
            toolbarPadding = theme.ResolvePadding(opts.padding);
        }
    } else {
        toolbarPadding = isVertical
            ? Padding::Symmetric(crossPad, defaultGap)
            : Padding::Symmetric(defaultGap, crossPad);
    }
    node->padding = toolbarPadding;

    if (isVertical) {
        node->widthMode = SizeMode::Fixed;
        node->fixedWidth = barExtent;
        node->heightMode = (opts.style == ToolbarStyle::Floating) ? SizeMode::Fit : SizeMode::Stretch;
    } else if (opts.overflow == ToolbarOverflow::Wrap) {
        node->widthMode = (opts.style == ToolbarStyle::Floating) ? SizeMode::Fit : SizeMode::Stretch;
        node->heightMode = SizeMode::Fit;
        node->minHeight = barExtent;
    } else {
        node->widthMode = (opts.style == ToolbarStyle::Floating) ? SizeMode::Fit : SizeMode::Stretch;
        node->heightMode = SizeMode::Fixed;
        node->fixedHeight = barExtent;
    }

    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;

    if (!isVertical && (opts.overflow == ToolbarOverflow::AutoCompact || opts.overflow == ToolbarOverflow::Scroll)) {
        auto* cData = ctx.AllocPayload<CarouselData>();
        cData->autoCompact = (opts.overflow == ToolbarOverflow::AutoCompact);
        node->SetPayload(cData);
        node->arrangeFn = &Toolbar::Arrange;
        node->postRenderFn = &Carousel::RenderArrows;
        Carousel::BeginScrollInteraction(ctx, node, cData, true, effectiveDisabled);
    }

    const auto& pal = theme.palette;
    const bool insideRoundedSurface =
        (opts.style == ToolbarStyle::Header) &&
        (node->parent != nullptr) &&
        (node->parent->cornerRadius > 0.0f) &&
        (node->parent->backgroundColor.a > 0);

    const Color styleBg = (opts.style == ToolbarStyle::Floating)
        ? pal.surfaceElevated
        : (insideRoundedSurface ? Color::Transparent() : pal.surface);
    if (!opts.background.IsBackgroundEnabled(true)) {
        node->backgroundColor = Color::Transparent();
    } else if (opts.background.HasAny() && !opts.background.IsSurface()) {
        node->backgroundColor = opts.background.Resolve(false, false, effectiveDisabled, pal.surface);
    } else {
        node->backgroundColor = styleBg;
    }

    node->explicitRadius = (opts.border.radius.has_value() || opts.style != ToolbarStyle::Surface) ? 1 : 0;
    node->borderColor = opts.border.color.Resolve(false, false, effectiveDisabled, pal.border);
    if (opts.border.width > 0.0f) {
        node->borderWidth = theme.Scale(opts.border.width);
    } else if (opts.border.color.HasAny() || opts.style == ToolbarStyle::Header || opts.style == ToolbarStyle::Floating) {
        node->borderWidth = theme.shell.panelBorderWidth;
    } else {
        node->borderWidth = 0.0f;
    }

    if (opts.border.radius.has_value()) {
        node->cornerRadius = theme.Scale(*opts.border.radius);
    } else if (opts.style == ToolbarStyle::Header) {
        node->cornerRadius = 0.0f;
    } else if (opts.style == ToolbarStyle::Floating) {
        node->cornerRadius = theme.ResolveRadius(std::nullopt, activeSize) + 2.0f * activeScale;
    } else {
        node->cornerRadius = theme.ResolveRadius(std::nullopt, activeSize);
    }

    const auto* tbCar = node->As<CarouselData>();
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->visualHash = detail::HashArgs(
        static_cast<uint8_t>(opts.style),
        static_cast<uint8_t>(opts.orientation),
        static_cast<uint8_t>(opts.overflow),
        node->cornerRadius,
        node->borderWidth,
        opts.background,
        opts.border,
        tbCar ? tbCar->leftArrowHovered : false,
        tbCar ? tbCar->leftArrowPressed : false,
        tbCar ? tbCar->rightArrowHovered : false,
        tbCar ? tbCar->rightArrowPressed : false
    );

    ctx.PushScale(activeScale);
    ctx.PushSize(activeSize);
    ctx.PushDisabled(effectiveDisabled);
    ctx.PushContainer(node);

    return ToolbarScope(&ctx, node, res, opts);
}

Response Toolbar::ToolButton(
    Context& ctx,
    const ToolButtonOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiSize activeSize = ctx.GetCurrentSize();

    ctx.RegisterFocusable(id, effectiveDisabled);

    Response res = ctx.Interact(id);
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
    } else {
        if (res.clicked) {
            ctx.RequestFocus(id);
        }
        if (ctx.IsFocused(id) && (ctx.ConsumeKey(Key::Space) || ctx.ConsumeKey(Key::Enter))) {
            res.clicked = true;
        }
    }

    if (res.hovered) {
        const bool wasCompacted = opts.icon.IsValid() && !opts.text.empty() &&
                                  res.rect.w > 0.0f && res.rect.w <= res.rect.h + 2.0f;
        const UiText effTooltip = !opts.tooltip.empty()
            ? opts.tooltip
            : ((opts.square || wasCompacted) && !opts.text.empty() ? opts.text : UiText{});
        if (!effTooltip.empty()) {
            ctx.SetTooltip(effTooltip, res.rect);
        }
    }

    Node* node = ctx.CreateNode(NodeType::Button, id);

    auto* payload = ctx.AllocPayload<ToolButtonPayload>();
    payload->origText = ctx.AllocText(opts.text);
    payload->origSquare = opts.square;
    payload->btn.text = opts.square ? UiText{} : payload->origText;
    payload->btn.tooltip = ctx.AllocText(opts.tooltip);
    node->text = payload->btn.text.text;
    payload->btn.icon = ctx.AllocIcon(opts.icon);
    payload->btn.color = opts.color.HasAny() ? opts.color : StateColor::Ghost();
    payload->btn.iconPlacement = opts.iconPlacement;
    payload->btn.align = opts.align;
    payload->btn.stretch = opts.stretch;
    payload->btn.width = opts.width;
    payload->btn.minWidth = opts.minWidth;
    payload->btn.disabled = effectiveDisabled;
    payload->btn.active = opts.active;
    payload->btn.square = opts.square;
    payload->btn.iconColor = opts.iconColor;
    payload->badge = ctx.AllocText(opts.badge);
    payload->badgeColor = ( !opts.color.HasAny() || opts.color == StateColor::Ghost() )
        ? StateColor::Primary()
        : opts.color;

    node->SetPayload(payload);
    node->visualHash = detail::HashArgs(
        opts.color,
        opts.iconPlacement,
        opts.active,
        opts.square,
        opts.iconColor.value_or(Color::Transparent()),
        static_cast<uint8_t>(opts.stretch),
        activeSize,
        static_cast<uint8_t>(payload->btn.icon.GetType()),
        static_cast<uint8_t>(payload->btn.icon.GetId()),
        payload->btn.icon.GetPath(),
        payload->origText,
        payload->badge
    );
    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        auto* p = const_cast<ToolButtonPayload*>(n->As<ToolButtonPayload>());
        if (p) {
            p->btn.square = p->origSquare;
            p->btn.text = p->origSquare ? UiText{} : p->origText;
        }
        const Vec2 sz = Button::Measure(p ? p->btn : ButtonOptions{}, n->uiSize, canvas, theme, availableWidth);
        if (p) {
            p->expandedWidth = sz.x;
        }
        return sz;
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* p = n->As<ToolButtonPayload>();
        Button::Render(canvas, n->computedRect, p ? p->btn : ButtonOptions{}, n->uiSize, n->hovered, n->pressed, n->disabled, theme, n->focused);
        if (p && !p->badge.empty()) {
            const auto& pal = theme.palette;
            const VariantColors& v = theme.GetVariant(UiVariant::Primary);
            const Color resolvedBg = p->badge.color.HasAny()
                ? p->badge.color.Resolve(n->hovered, n->pressed, n->disabled, v.bg)
                : p->badgeColor.Resolve(n->hovered, n->pressed, n->disabled, v.bg);
            const float badgeFont = std::max(9.0f, theme.ResolveFontSize(p->badge, TextRole::LabelSmall, n->uiSize) - 1.5f);
            const Vec2 badgeDims = canvas.MeasureText(p->badge.text, badgeFont);
            const float badgeH = std::min(n->computedRect.h * 0.48f, std::max(13.0f, badgeFont + 3.0f));
            const float badgeW = std::min(n->computedRect.w - 4.0f, std::max(badgeH, badgeDims.x + 6.0f));
            const Rect badgeRect(
                n->computedRect.Right() - badgeW - 2.0f,
                n->computedRect.y + 2.0f,
                badgeW,
                badgeH
            );
            const Color badgeBg = n->disabled ? resolvedBg.WithAlphaF(0.45f) : resolvedBg;
            const Color badgeTextCol = n->disabled ? v.text.WithAlphaF(0.55f) : v.text;
            canvas.DrawRoundRect(badgeRect, badgeH * 0.5f, badgeBg, pal.surface, 1.5f);
            canvas.DrawTextBox(p->badge.text, badgeRect, badgeFont, badgeTextCol, 0.5f, 0.5f);
        }
    };
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed;
    node->focused = ctx.IsFocused(id);
    node->widthMode = (opts.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

    ctx.AdaptChildToContainer(node);

    return res;
}

void Toolbar::RenderSeparator(
    ICanvas& canvas,
    Rect bounds,
    Color color,
    const UiTheme& theme,
    Orientation orientation
) {
    const Color stroke = color.a > 0 ? color : theme.palette.borderSubtle;
    if (orientation == Orientation::Vertical) {
        const float midY = std::floor(bounds.Center().y) + 0.5f;
        const float padX = std::max(2.0f, bounds.w * 0.18f);
        canvas.DrawLine(
            Vec2(bounds.x + padX, midY),
            Vec2(bounds.Right() - padX, midY),
            stroke,
            1.0f
        );
    } else {
        const float midX = std::floor(bounds.Center().x) + 0.5f;
        const float padY = std::max(2.0f, bounds.h * 0.20f);
        canvas.DrawLine(
            Vec2(midX, bounds.y + padY),
            Vec2(midX, bounds.Bottom() - padY),
            stroke,
            1.0f
        );
    }
}

void Toolbar::Arrange(
    Node* node,
    const Rect& availableRect,
    const UiTheme& theme
) {
    if (!node) return;

    float w = availableRect.w;
    float h = (node->heightMode == SizeMode::Fixed) ? node->fixedHeight : availableRect.h;
    if (node->parent == nullptr && !node->isSpacer && node->widthMode == SizeMode::Fit) {
        w = node->intrinsicSize.x;
    }

    node->computedRect = Rect(availableRect.x, availableRect.y, w, h);
    if (node->childCount == 0) return;

    auto arrangeFittingRow = [&](float contentTotalW) {
        float boxW = w;
        float boxX = availableRect.x;
        if (node->widthMode == SizeMode::Fit) {
            boxW = std::min(w, contentTotalW + node->padding.Horizontal());
            if (node->parent != nullptr) {
                const bool centerInCol = (node->parent->direction == FlexDirection::Column && node->parent->align == Alignment::Center);
                const bool centerInRow = (node->parent->direction == FlexDirection::Row && node->parent->justify == Justify::Center);
                const bool endInCol = (node->parent->direction == FlexDirection::Column && node->parent->align == Alignment::End);
                const bool endInRow = (node->parent->direction == FlexDirection::Row && node->parent->justify == Justify::End);
                if (centerInCol || centerInRow) {
                    boxX = availableRect.x + (availableRect.w - boxW) * 0.5f;
                } else if (endInCol || endInRow) {
                    boxX = availableRect.Right() - boxW;
                }
            }
        }
        node->computedRect.x = boxX;
        node->computedRect.w = boxW;
        const float sX = boxX + node->padding.left;
        const float sY = node->computedRect.y + node->padding.top;
        const float fW = std::max(0.0f, boxW - node->padding.Horizontal());
        const float fH = std::max(0.0f, node->computedRect.h - node->padding.Vertical());

        if (auto* cData = node->As<CarouselData>()) {
            cData->showLeftArrow = false;
            cData->showRightArrow = false;
            cData->leftArrowRect = Rect(0.0f, 0.0f, 0.0f, 0.0f);
            cData->rightArrowRect = Rect(0.0f, 0.0f, 0.0f, 0.0f);
            cData->viewportRect = Rect(sX, sY, fW, fH);
            cData->targetOffsetX = 0.0f;
            cData->animatingX = false;
            cData->stepDirection = 0;
        }
        node->scrollOffset.x = 0.0f;

        float nonFlexSum = 0.0f;
        float totalFlexGrow = 0.0f;
        uint32_t flowCount = 0;
        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            if (child->type == NodeType::Modal) continue;
            ++flowCount;
            if (child->isSpacer || child->widthMode == SizeMode::Stretch) {
                totalFlexGrow += std::max(0.01f, child->flexGrow);
            } else if (child->widthMode == SizeMode::Fixed) {
                nonFlexSum += child->fixedWidth;
            } else {
                nonFlexSum += child->intrinsicSize.x;
            }
        }

        const float totalGap = (flowCount > 1) ? static_cast<float>(flowCount - 1) * node->gap : 0.0f;
        const float freeSpace = std::max(0.0f, fW - totalGap - nonFlexSum);
        node->scrollContentSize = {nonFlexSum + totalGap, fH};

        float curX = sX;
        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            if (child->type == NodeType::Modal) continue;
            float childW = 0.0f;
            if (child->isSpacer || child->widthMode == SizeMode::Stretch) {
                childW = (totalFlexGrow > 0.0f)
                    ? (freeSpace * (std::max(0.01f, child->flexGrow) / totalFlexGrow))
                    : 0.0f;
            } else if (child->widthMode == SizeMode::Fixed) {
                childW = child->fixedWidth;
            } else {
                childW = child->intrinsicSize.x;
            }
            const float childH = (child->heightMode == SizeMode::Fixed) ? child->fixedHeight :
                                 (child->heightMode == SizeMode::Stretch ? fH : child->intrinsicSize.y);
            float childY = sY;
            if (node->align == Alignment::Center) {
                childY += (fH - childH) * 0.5f;
            } else if (node->align == Alignment::End) {
                childY += (fH - childH);
            }
            child->computedRect = Rect(curX, childY, childW, childH);
            Solver::Arrange(child, child->computedRect, theme);
            curX += childW + node->gap;
        }
    };

    const float availContentW = std::max(0.0f, w - node->padding.Horizontal());
    float expandedTotalW = 0.0f;
    uint32_t flowCount = 0;

    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
        if (child->type == NodeType::Modal) continue;
        if (child->type == NodeType::Button) {
            if (auto* p = child->As<ToolButtonPayload>()) {
                p->btn.square = p->origSquare;
                p->btn.text = p->origSquare ? UiText{} : p->origText;
                child->text = p->btn.text.text;
                if (p->expandedWidth > 0.0f) {
                    child->intrinsicSize.x = p->expandedWidth;
                }
            }
        }
        ++flowCount;
        if (!child->isSpacer) {
            const float cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
            expandedTotalW += cw;
        }
    }
    if (flowCount > 1) {
        expandedTotalW += static_cast<float>(flowCount - 1) * node->gap;
    }

    if (expandedTotalW <= availContentW + 0.5f) {
        arrangeFittingRow(expandedTotalW);
        return;
    }

    auto* cData = node->As<CarouselData>();
    if (cData && cData->autoCompact) {
        float compactTotalW = 0.0f;
        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            if (child->type == NodeType::Modal) continue;
            if (child->type == NodeType::Button) {
                if (auto* p = child->As<ToolButtonPayload>()) {
                    if (!p->origSquare && p->btn.icon.IsValid() && !p->origText.empty() &&
                        p->btn.iconPlacement == Placement::Leading) {
                        p->btn.square = true;
                        p->btn.text = UiText{};
                        child->text = {};
                        child->intrinsicSize.x = Button::Measure(p->btn, child->uiSize, nullptr, theme).x;
                    }
                }
            }
            if (!child->isSpacer) {
                const float cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
                compactTotalW += cw;
            }
        }
        if (flowCount > 1) {
            compactTotalW += static_cast<float>(flowCount - 1) * node->gap;
        }

        if (compactTotalW <= availContentW + 0.5f) {
            arrangeFittingRow(compactTotalW);
            return;
        }
    }

    Carousel::Arrange(node, availableRect, theme);
}

Rect Toolbar::GetContentClipRect(const Node* node) noexcept {
    if (!node) return Rect{};
    if (const auto* cData = node->As<CarouselData>()) {
        if (cData->viewportRect.w > 0.0f || cData->viewportRect.h > 0.0f) {
            return Rect(
                cData->viewportRect.x,
                node->computedRect.y,
                cData->viewportRect.w,
                node->computedRect.h
            );
        }
    }
    return Rect(
        node->computedRect.x + node->padding.left,
        node->computedRect.y,
        std::max(0.0f, node->computedRect.w - node->padding.Horizontal()),
        node->computedRect.h
    );
}

} // namespace guikit
