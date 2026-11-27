#include "guikit/core/element.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/solver.hpp"
#include "guikit/core/text_util.hpp"
#include "guikit/style/style_painter.hpp"

namespace guikit {

namespace {

bool HasLabelBar(const ElementSpec& spec) noexcept {
    return spec.icon.IsValid() || spec.label.HasContentOrReserve() || spec.badge.HasContentOrReserve();
}

Node* FirstFlowChild(Node* elNode) noexcept {
    for (Node* c = elNode ? elNode->firstChild : nullptr; c != nullptr; c = c->nextSibling) {
        if (c->IsOutOfFlow()) continue;
        return c;
    }
    return nullptr;
}

const Node* FirstFlowChild(const Node* elNode) noexcept {
    for (const Node* c = elNode ? elNode->firstChild : nullptr; c != nullptr; c = c->nextSibling) {
        if (c->IsOutOfFlow()) continue;
        return c;
    }
    return nullptr;
}

void AccumulateSlotSubtree(
    const Node* sub,
    bool& ioHovered,
    bool& ioPressed,
    bool& ioActive,
    keel::u64& ioHash,
    keel::u32 depth = 0
) noexcept {
    if (!sub || depth > 8) return;
    if (sub->hovered) ioHovered = true;
    if (sub->pressed) ioPressed = true;
    if (sub->focused || sub->active) ioActive = true;
    ioHash = detail::HashArgs(
        ioHash,
        keel::as<keel::u8>(sub->type),
        sub->visualHash,
        keel::as<keel::u8>(sub->widthMode),
        keel::as<keel::u8>(sub->heightMode),
        sub->fixedWidth,
        sub->fixedHeight,
        sub->flexGrow,
        sub->gap
    );
    for (const Node* c = sub->firstChild; c != nullptr; c = c->nextSibling) {
        if (c->IsOutOfFlow()) continue;
        AccumulateSlotSubtree(c, ioHovered, ioPressed, ioActive, ioHash, depth + 1);
    }
}

ElementSpec MakeSpecFromPayload(
    const Node* n,
    const ElementPayload& ep,
    const UiTheme& theme
) noexcept {
    const bool isHovered = n->hovered;
    const bool isActive = n->pressed || n->focused || n->active;
    const Node* slotChild = FirstFlowChild(n);
    if (slotChild == nullptr) {
        ElementSpec s = detail::Element::MakeSpec(ep.opts, n->uiSize, n->disabled, theme, isHovered, isActive);
        if (!s.width.IsFixed()) {
            s.width = n->widthMode;
        }
        const keel::f32 d = n->GetDensity();
        s.labelGap *= d;
        s.descGap *= d;
        s.inlineGap *= d;
        return s;
    }

    const ComponentMetrics& cm = theme.GetMetrics(n->uiSize);
    const InputFieldMetrics& im = theme.GetInputField(n->uiSize);

    ControlSpec ctrl{};
    if (ep.controlChildCount == 1) {
        ctrl = ep.firstChildSpec;
    } else {
        ctrl.defaultLayout = ElementLayout::Stacked;
    }
    const bool slotStretchW = slotChild->isSpacer || slotChild->widthMode == SizeMode::Stretch;
    const bool slotStretchH = (!slotChild->isSpacer && slotChild->heightMode == SizeMode::Stretch) ||
                              (n->heightMode == SizeMode::Stretch);
    if (slotChild->widthMode == SizeMode::Fixed && slotChild->fixedWidth > 0.0f) {
        ctrl.width = AxisSize::Fixed(slotChild->fixedWidth);
    } else {
        ctrl.width = slotStretchW ? AxisSize::Stretch() : AxisSize::Fit();
    }
    if (slotChild->heightMode == SizeMode::Fixed && slotChild->fixedHeight > 0.0f) {
        ctrl.height = AxisSize::Fixed(slotChild->fixedHeight);
    } else {
        ctrl.height = slotStretchH ? AxisSize::Stretch() : AxisSize::Fit();
    }
    ctrl.minWidth = keel::kNullOpt;
    ctrl.minHeight = keel::kNullOpt;

    const keel::f32 d = n->GetDensity();
    const keel::f32 baseLabelGap = ((ctrl.labelGap > 0.0f) ? ctrl.labelGap : im.labelGap) * d;
    const keel::f32 baseDescGap = ((ctrl.descGap > 0.0f) ? ctrl.descGap : im.descGap) * d;
    const keel::f32 baseInlineGap = ((ctrl.inlineGap > 0.0f) ? ctrl.inlineGap : cm.paddingX) * d;

    return ElementSpec{
        .label = ep.opts.label,
        .description = ep.opts.description,
        .icon = ep.opts.icon,
        .badge = ep.opts.badge,
        .layout = ep.opts.layout.value_or(ctrl.defaultLayout),
        .align = ep.opts.align,
        .width = ep.opts.width.IsFixed() ? ep.opts.width : AxisSize(n->widthMode),
        .control = ctrl,
        .widgetSize = n->uiSize,
        .labelGap = baseLabelGap,
        .descGap = baseDescGap,
        .inlineGap = baseInlineGap,
        .hasControl = true,
        .disabled = n->disabled,
        .hovered = isHovered,
        .active = isActive
    };
}

keel::f32 ResolveSlotAvailableWidth(
    const ElementSpec& spec,
    keel::f32 availableWidth,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::f32 descFontSize,
    Vec2 measuredLabelBar
) noexcept {
    if (spec.width.IsFixed()) {
        const keel::f32 fixedOuterW = spec.width.Dp();
        availableWidth = (availableWidth > 0.0f) ? keel::min(availableWidth, fixedOuterW) : fixedOuterW;
    }
    if (availableWidth <= 0.0f) {
        return 0.0f;
    }
    if (!IsInlineElementLayout(spec.layout)) {
        return availableWidth;
    }
    const bool hasLabel = HasLabelBar(spec);
    const bool hasDesc = spec.description.HasContentOrReserve();
    if (!hasLabel && !hasDesc) {
        return availableWidth;
    }
    if (spec.control.width.IsFixed()) {
        return keel::min(availableWidth, spec.control.width.Dp());
    }
    if (spec.control.width.ResolveMode(spec.control.defaultWidthMode) == SizeMode::Stretch) {
        const keel::f32 descW = hasDesc ? TextUtil::MeasureUiTextWidth(spec.description, descFontSize, canvas) : 0.0f;
        const keel::f32 textW = keel::max(measuredLabelBar.x, descW);
        return keel::max(theme.GetMetrics(spec.widgetSize).defaultHeight, availableWidth - textW - spec.inlineGap);
    }
    return availableWidth;
}

Vec2 MeasureSingleChildNode(
    const Node* child,
    keel::f32 availW,
    ICanvas* canvas,
    const UiTheme& parentTheme
) noexcept {
    const UiTheme& childTheme = child->GetTheme(parentTheme);
    const keel::f32 childQueryW = (child->widthMode == SizeMode::Fixed && child->fixedWidth > 0.0f)
        ? ((availW > 0.0f) ? keel::min(availW, child->fixedWidth) : child->fixedWidth)
        : availW;
    Vec2 intr = child->measureFn
        ? child->measureFn(child, childQueryW, canvas, childTheme)
        : child->intrinsicSize;
    if (!child->measureFn && child->childCount > 0 && childQueryW > 0.0f) {
        intr.y = Solver::ResolveHeight(const_cast<Node*>(child), childQueryW, canvas, childTheme);
    }
    if (child->widthMode == SizeMode::Fixed && child->fixedWidth > 0.0f) {
        intr.x = child->fixedWidth;
    } else if (child->minWidth > 0.0f) {
        intr.x = keel::max(intr.x, child->minWidth);
    }
    if (child->maxWidth > 0.0f) {
        intr.x = keel::min(intr.x, child->maxWidth);
    }
    if (child->heightMode == SizeMode::Fixed && child->fixedHeight > 0.0f) {
        intr.y = child->fixedHeight;
    } else if (child->minHeight > 0.0f) {
        intr.y = keel::max(intr.y, child->minHeight);
    }
    if (child->maxHeight > 0.0f) {
        intr.y = keel::min(intr.y, child->maxHeight);
    }
    return intr;
}

Vec2 MeasureControlSlot(
    const Node* elNode,
    keel::f32 slotAvailW,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    const Node* slotChild = FirstFlowChild(elNode);
    if (slotChild == nullptr) {
        return {0.0f, 0.0f};
    }
    return MeasureSingleChildNode(slotChild, slotAvailW, canvas, theme);
}

void PlaceControlChild(Node* child, const Rect& rect, ICanvas* canvas, const UiTheme& parentTheme) noexcept {
    const UiTheme& childTheme = child->GetTheme(parentTheme);
    if (child->childCount > 0 || child->arrangeFn != nullptr) {
        Solver::Arrange(child, rect, childTheme, canvas);
    } else {
        child->computedRect = rect;
    }
}

void ArrangeControlSlot(
    Node* elNode,
    const Rect& controlRect,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    for (Node* c = elNode->firstChild; c != nullptr; c = c->nextSibling) {
        if (c->IsOutOfFlow()) {
            PlaceControlChild(c, elNode->computedRect, canvas, theme);
        }
    }
    if (Node* slotChild = FirstFlowChild(elNode)) {
        PlaceControlChild(slotChild, controlRect, canvas, theme);
    }
}

} // namespace

keel::f32 detail::Element::ResolveLabelFontSize(const ElementSpec& spec, const UiTheme& theme) noexcept {
    return theme.ResolveFontSize(spec.label, TextRole::LabelLarge, spec.widgetSize);
}

keel::f32 detail::Element::ResolveDescFontSize(const ElementSpec& spec, const UiTheme& theme) noexcept {
    return theme.ResolveFontSize(spec.description, TextRole::LabelSmall, spec.widgetSize);
}

Vec2 detail::Element::MeasureLabelBar(
    const ElementSpec& spec,
    keel::f32 labelFontSize,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    const UiSize effSize = (spec.widgetSize == UiSize::None) ? UiSize::Md : spec.widgetSize;
    const ComponentMetrics& m = theme.GetMetrics(effSize);
    const keel::f32 badgeFont = (spec.badge.size.px > 0.0f)
        ? spec.badge.size.px
        : (spec.badge.size.role.has_value()
            ? theme.GetTypography(*spec.badge.size.role).fontSize
            : (spec.badge.size.preset != UiSize::None
                ? theme.GetMetrics(spec.badge.size.preset).badgeFontSize
                : m.badgeFontSize));

    keel::f32 barW = 0.0f;
    keel::f32 barH = 0.0f;

    if (spec.icon.IsValid()) {
        barW += m.iconSize;
        barH = keel::max(barH, m.iconSize);
    }

    if (spec.label.HasContentOrReserve()) {
        if (barW > 0.0f) barW += m.itemGap;
        const keel::f32 textW = TextUtil::MeasureUiTextWidth(spec.label, labelFontSize, canvas);
        barW += textW;
        const keel::f32 lineH = canvas ? canvas->GetLineHeight(labelFontSize) : theme.GetLineHeight(labelFontSize);
        barH = keel::max(barH, lineH);
    }

    if (spec.badge.HasContentOrReserve()) {
        if (barW > 0.0f) barW += m.badgeGap;
        const keel::f32 badgeTextW = TextUtil::MeasureWidth(spec.badge, badgeFont, canvas);
        const keel::f32 badgeW = badgeTextW + 12.0f;
        barW += badgeW;
        barH = keel::max(barH, badgeFont * 1.4f);
    }

    return {barW, barH};
}

ElementSpec detail::Element::MakeSpec(
    const ElementOptions& opts,
    UiSize activeSize,
    bool isDisabled,
    const UiTheme& theme,
    bool isHovered,
    bool isActive
) noexcept {
    const ComponentMetrics& m = theme.GetMetrics(activeSize);
    const AxisSize resolvedWidth = opts.width.IsAuto()
        ? AxisSize::Stretch()
        : opts.width;
    return ElementSpec{
        .label = opts.label,
        .description = opts.description,
        .icon = opts.icon,
        .badge = opts.badge,
        .layout = opts.layout.value_or(ElementLayout::Stacked),
        .align = opts.align,
        .width = resolvedWidth,
        .control = ControlSpec{.width = SizeMode::Fit, .height = SizeMode::Fit},
        .widgetSize = activeSize,
        .labelGap = m.lineGap,
        .descGap = m.lineGap,
        .inlineGap = m.itemGap * 1.5f,
        .hasControl = false,
        .disabled = isDisabled,
        .hovered = isHovered,
        .active = isActive
    };
}

void detail::Element::SyncWithChildren(Context& /*ctx*/, Node* elNode) noexcept {
    if (!elNode) return;
    auto* ep = elNode->As<ElementPayload>();
    if (!ep) return;

    const Node* outerParent = elNode->parent;
    const bool inToolbarRow = (outerParent != nullptr) &&
        (outerParent->type == NodeType::Toolbar && outerParent->direction == FlexDirection::Row);
    const bool inHorizontalFlow = (outerParent != nullptr) &&
        (outerParent->direction == FlexDirection::Row || outerParent->direction == FlexDirection::Wrap) &&
        !outerParent->equalWidths;

    const Node* slotChild = FirstFlowChild(elNode);
    bool anyStretchW = false;
    bool anyStretchH = false;
    keel::u64 childrenHash = 0;

    elNode->minWidth = 0.0f;
    elNode->minHeight = 0.0f;

    if (slotChild != nullptr) {
        anyStretchW = slotChild->isSpacer || slotChild->widthMode == SizeMode::Stretch;
        anyStretchH = !slotChild->isSpacer && slotChild->heightMode == SizeMode::Stretch;
        bool subHovered = false;
        bool subPressed = false;
        bool subActive = false;
        AccumulateSlotSubtree(slotChild, subHovered, subPressed, subActive, childrenHash);
        if (subHovered) elNode->hovered = true;
        if (subPressed) elNode->pressed = true;
        if (subActive) elNode->active = true;

        if (!ep->opts.width.IsFixed()) {
            const keel::f32 childMinW = (slotChild->widthMode == SizeMode::Fixed && slotChild->fixedWidth > 0.0f)
                ? slotChild->fixedWidth
                : slotChild->minWidth;
            if (childMinW > 0.0f) {
                elNode->minWidth = childMinW;
            }
        }
        if (slotChild->minHeight > 0.0f) {
            elNode->minHeight = slotChild->minHeight;
        }
    }

    ep->anyChildStretchesW = anyStretchW;
    ep->anyChildStretchesH = anyStretchH;

    if (ep->opts.width.IsFixed()) {
        elNode->widthMode = SizeMode::Fixed;
        elNode->fixedWidth = ep->opts.width.Dp();
    } else if (!ep->opts.width.IsAuto()) {
        elNode->widthMode = ep->opts.width.ResolveMode(SizeMode::Stretch);
        elNode->fixedWidth = 0.0f;
    } else if (inToolbarRow) {
        elNode->widthMode = SizeMode::Fit;
        elNode->fixedWidth = 0.0f;
    } else if (slotChild == nullptr) {
        elNode->widthMode = SizeMode::Stretch;
        elNode->fixedWidth = 0.0f;
    } else {
        const ElementLayout effLayout = ep->opts.layout.value_or(
            (ep->controlChildCount == 1) ? ep->firstChildSpec.defaultLayout : ElementLayout::Stacked
        );
        if (anyStretchW) {
            elNode->widthMode = SizeMode::Stretch;
        } else if (inHorizontalFlow) {
            elNode->widthMode = SizeMode::Fit;
        } else if (effLayout == ElementLayout::Trailing && ep->opts.align == Align::Start) {
            elNode->widthMode = SizeMode::Fit;
        } else {
            elNode->widthMode = SizeMode::Stretch;
        }
        elNode->fixedWidth = 0.0f;
    }

    elNode->heightMode = anyStretchH ? SizeMode::Stretch : SizeMode::Fit;

    if (outerParent != nullptr && outerParent->direction == FlexDirection::Row) {
        if (ep->opts.width.HasCustomStretchWeight()) {
            elNode->flexGrow = ep->opts.width.StretchWeight();
        } else if (slotChild != nullptr && elNode->widthMode == SizeMode::Stretch) {
            elNode->flexGrow = slotChild->flexGrow;
        } else {
            elNode->flexGrow = 1.0f;
        }
    } else if (slotChild != nullptr && elNode->heightMode == SizeMode::Stretch) {
        elNode->flexGrow = slotChild->flexGrow;
    } else {
        elNode->flexGrow = 1.0f;
    }

    elNode->visualHash = detail::HashArgs(
        keel::as<keel::u8>(ep->opts.layout.value_or(ElementLayout::Stacked)),
        ep->opts.layout.has_value(),
        keel::as<keel::u8>(ep->opts.align),
        keel::as<keel::u8>(elNode->widthMode),
        keel::as<keel::u8>(elNode->heightMode),
        ep->opts.width,
        ep->opts.label,
        ep->opts.description,
        ep->opts.badge,
        elNode->disabled,
        keel::as<keel::u8>(ep->opts.icon.GetType()),
        keel::as<keel::u8>(ep->opts.icon.GetId()),
        ep->opts.icon.GetPath(),
        slotChild != nullptr,
        childrenHash
    );
}

ElementScope detail::Element::Open(
    Context& ctx,
    const ElementOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    Node* node = ctx.CreateNode(NodeType::Element, id, opts.disabled);
    const bool effectiveDisabled = node->disabled;

    Response res = ctx.Interact(id);
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        res.doubleClicked = false;
        res.rightClicked = false;
    }

    if (res.hovered && !opts.tooltip.empty()) {
        ctx.SetTooltip(opts.tooltip, res.rect);
    }

    auto* payload = ctx.AllocPayload<ElementPayload>();
    payload->opts = opts;
    payload->opts.label = ctx.AllocText(opts.label);
    payload->opts.description = ctx.AllocText(opts.description);
    payload->opts.badge = ctx.AllocText(opts.badge);
    payload->opts.tooltip = ctx.AllocText(opts.tooltip);
    if (!opts.icon.GetPath().empty()) {
        payload->opts.icon = Icon::File(ctx.AllocString(opts.icon.GetPath()));
    }
    payload->interactionRes = res;

    node->text = payload->opts.label.text;
    node->SetPayload(payload);
    node->uiSize = opts.size;
    node->hovered = res.hovered;
    node->pressed = res.pressed;

    SyncWithChildren(ctx, node);

    node->measureFn = [](const Node* n, keel::f32 availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* ep = n->As<ElementPayload>();
        if (!ep) return {0.0f, 0.0f};
        ep->lastCanvas = canvas;
        const ElementSpec spec = MakeSpecFromPayload(n, *ep, theme);
        Vec2 slotIntrinsic{0.0f, 0.0f};
        if (spec.hasControl) {
            const keel::f32 labelFs = ResolveLabelFontSize(spec, theme);
            const keel::f32 descFs = ResolveDescFontSize(spec, theme);
            const Vec2 rawBar = HasLabelBar(spec) ? MeasureLabelBar(spec, labelFs, canvas, theme) : Vec2{0.0f, 0.0f};
            const keel::f32 slotAvailW = ResolveSlotAvailableWidth(spec, availableWidth, canvas, theme, descFs, rawBar);
            slotIntrinsic = MeasureControlSlot(n, slotAvailW, canvas, theme);
        }
        return detail::Element::Measure(slotIntrinsic, spec, canvas, theme, availableWidth);
    };

    node->arrangeFn = &detail::Element::Arrange;

    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* ep = n->As<ElementPayload>();
        if (!ep) return;
        ElementSpec spec = MakeSpecFromPayload(n, *ep, theme);
        const Node* p = n->parent;
        const bool onActiveTreeRow =
            (p && p->type == NodeType::TreeItem && p->active) ||
            (p && p->parent && p->parent->type == NodeType::TreeItem && p->parent->active);
        const bool onSolidSel = onActiveTreeRow && theme.Painter().GetChromeMetrics(theme).solidSelectionHighlight;
        if (onSolidSel) {
            const auto& pal = theme.palette;
            if (!spec.label.color.HasAny()) {
                spec.label.color = StateColor{pal.accentText};
            }
            if (!spec.description.color.HasAny()) {
                spec.description.color = StateColor{pal.accentText.WithAlphaF(0.86f)};
            }
            if (!spec.badge.empty()) {
                spec.badge.color = StateColor{pal.accentText};
            }
            spec.active = true;
        }
        if (!spec.hasControl) {
            detail::Element::Render(canvas, n->computedRect, spec, theme);
            return;
        }
        const keel::f32 labelFs = ResolveLabelFontSize(spec, theme);
        const keel::f32 descFs = ResolveDescFontSize(spec, theme);
        const Vec2 rawBar = HasLabelBar(spec) ? MeasureLabelBar(spec, labelFs, &canvas, theme) : Vec2{0.0f, 0.0f};
        const keel::f32 slotAvailW = ResolveSlotAvailableWidth(spec, n->computedRect.w, &canvas, theme, descFs, rawBar);
        const Vec2 slotIntrinsic = MeasureControlSlot(n, slotAvailW, &canvas, theme);
        const ElementGeometry geom = detail::Element::Resolve(n->computedRect, slotIntrinsic, spec, &canvas, theme);
        detail::Element::RenderText(canvas, geom, spec, theme);
    };

    ctx.FinishContainer(node);
    return ElementScope(&ctx, node, res);
}

void detail::Element::Arrange(
    Node* node,
    const Rect& availableRect,
    const UiTheme& theme
) noexcept {
    if (!node) return;
    Rect bounds = availableRect;
    if (node->parent == nullptr) {
        if (node->widthMode == SizeMode::Fit) bounds.w = node->intrinsicSize.x;
        else if (node->widthMode == SizeMode::Fixed && node->fixedWidth > 0.0f) bounds.w = node->fixedWidth;
        if (node->heightMode == SizeMode::Fit) bounds.h = node->intrinsicSize.y;
        else if (node->heightMode == SizeMode::Fixed && node->fixedHeight > 0.0f) bounds.h = node->fixedHeight;
    }
    node->computedRect = bounds;

    const auto* ep = node->As<ElementPayload>();
    if (!ep) return;
    const ElementSpec spec = MakeSpecFromPayload(node, *ep, theme);
    if (!spec.hasControl) {
        return;
    }

    const keel::f32 labelFs = ResolveLabelFontSize(spec, theme);
    const keel::f32 descFs = ResolveDescFontSize(spec, theme);
    const Vec2 rawBar = HasLabelBar(spec) ? MeasureLabelBar(spec, labelFs, ep->lastCanvas, theme) : Vec2{0.0f, 0.0f};
    const keel::f32 slotAvailW = ResolveSlotAvailableWidth(spec, bounds.w, ep->lastCanvas, theme, descFs, rawBar);
    const Vec2 slotIntrinsic = MeasureControlSlot(node, slotAvailW, ep->lastCanvas, theme);
    const ElementGeometry geom = detail::Element::Resolve(bounds, slotIntrinsic, spec, ep->lastCanvas, theme);
    ArrangeControlSlot(node, geom.controlRect, ep->lastCanvas, theme);
}

Vec2 detail::Element::Measure(
    Vec2 intrinsicControlSize,
    const ElementSpec& spec,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::f32 availableWidth
) noexcept {
    const keel::f32 labelFontSize = ResolveLabelFontSize(spec, theme);
    const keel::f32 descFontSize = ResolveDescFontSize(spec, theme);

    if (!spec.hasControl) {
        intrinsicControlSize = MeasureLabelBar(spec, labelFontSize, canvas, theme);
    } else {
        intrinsicControlSize = Control::Measure(intrinsicControlSize, spec.control, theme);
    }

    const bool hasOuterDpW = spec.width.IsFixed();
    const bool outerIsFit = spec.width.IsFit();
    const bool ctrlStretchesW = spec.hasControl &&
        (spec.control.width.ResolveMode(spec.control.defaultWidthMode) == SizeMode::Stretch);

    const bool hasLabel = spec.hasControl && HasLabelBar(spec);
    const bool hasDesc = spec.description.HasContentOrReserve();
    const bool hasExternal = hasLabel || hasDesc;
    const keel::f32 rawDescW = hasDesc ? TextUtil::MeasureUiTextWidth(spec.description, descFontSize, canvas) : 0.0f;

    keel::f32 minNoCtrlW = intrinsicControlSize.x;
    if (!spec.hasControl) {
        const keel::f32 natW = IsInlineElementLayout(spec.layout)
            ? (intrinsicControlSize.x + (hasDesc && rawDescW > 0.0f ? spec.inlineGap + rawDescW : 0.0f))
            : keel::max(intrinsicControlSize.x, rawDescW);
        minNoCtrlW = (hasDesc && !spec.description.wrap.IsWordWrap()) ? natW : intrinsicControlSize.x;
    }

    if (hasOuterDpW) {
        const keel::f32 targetW = spec.hasControl
            ? spec.width.Dp()
            : keel::max(minNoCtrlW, spec.width.Dp());
        availableWidth = (availableWidth > 0.0f) ? keel::min(availableWidth, targetW) : targetW;
    }

    const auto applyOuterFixedWidth = [&](Vec2 measured) noexcept -> Vec2 {
        if (hasOuterDpW) {
            measured.x = spec.hasControl
                ? spec.width.Dp()
                : keel::max(measured.x, spec.width.Dp());
        }
        return measured;
    };

    if (!hasExternal) {
        return applyOuterFixedWidth(intrinsicControlSize);
    }

    const Vec2 rawLabelBar = hasLabel ? MeasureLabelBar(spec, labelFontSize, canvas, theme) : Vec2{0.0f, 0.0f};
    keel::f32 effLabelW = rawLabelBar.x;
    keel::f32 effDescW = rawDescW;

    if (!IsInlineElementLayout(spec.layout) && outerIsFit && ctrlStretchesW) {
        if (spec.label.wrap.IsWordWrap()) {
            effLabelW = keel::min(effLabelW, intrinsicControlSize.x);
        }
        const keel::f32 baseW = keel::max(intrinsicControlSize.x, effLabelW);
        if (spec.description.wrap.IsWordWrap()) {
            effDescW = keel::min(effDescW, baseW);
        }
    } else if (!spec.hasControl && hasOuterDpW && spec.description.wrap.IsWordWrap()) {
        const keel::f32 targetW = keel::max(minNoCtrlW, spec.width.Dp());
        const keel::f32 maxDescW = IsInlineElementLayout(spec.layout)
            ? keel::max(0.0f, targetW - intrinsicControlSize.x - spec.inlineGap)
            : targetW;
        effDescW = keel::min(rawDescW, maxDescW);
    }

    keel::f32 textWrapW = availableWidth;
    if (spec.hasControl && spec.control.width.IsFixed() && outerIsFit) {
        const keel::f32 fixedCtrlW = spec.control.width.Dp();
        textWrapW = (availableWidth > 0.0f) ? keel::min(availableWidth, fixedCtrlW) : fixedCtrlW;
    }
    if (IsInlineElementLayout(spec.layout)) {
        if (textWrapW > 0.0f) {
            const keel::f32 ctrlW = keel::min(textWrapW, intrinsicControlSize.x);
            textWrapW = keel::max(0.0f, textWrapW - ctrlW - spec.inlineGap);
        }
    } else if (outerIsFit && ctrlStretchesW) {
        const keel::f32 compactBlockW = keel::max(intrinsicControlSize.x, keel::max(effLabelW, effDescW));
        textWrapW = (availableWidth > 0.0f) ? keel::min(availableWidth, compactBlockW) : compactBlockW;
    }

    Vec2 labelDims{0.0f, 0.0f};
    if (hasLabel) {
        const keel::f32 lineH = canvas ? canvas->GetLineHeight(labelFontSize) : theme.GetLineHeight(labelFontSize);
        const keel::f32 wrappedH = spec.label.HasContentOrReserve()
            ? TextUtil::MeasureUiTextHeight(spec.label, textWrapW, labelFontSize, lineH, canvas, &theme)
            : 0.0f;
        labelDims = {effLabelW, keel::max(rawLabelBar.y, wrappedH)};
    }

    Vec2 descDims{0.0f, 0.0f};
    if (hasDesc) {
        const keel::f32 lineH = canvas ? canvas->GetLineHeight(descFontSize) : theme.GetLineHeight(descFontSize);
        const keel::f32 h = TextUtil::MeasureUiTextHeight(spec.description, textWrapW, descFontSize, lineH, canvas, &theme);
        descDims = {effDescW, h};
    }

    if (spec.layout == ElementLayout::Stacked) {
        const keel::f32 totalW = keel::max(intrinsicControlSize.x, keel::max(labelDims.x, descDims.x));
        keel::f32 totalH = intrinsicControlSize.y;
        if (hasLabel) totalH += labelDims.y + spec.labelGap;
        if (hasDesc)  totalH += spec.descGap + descDims.y;
        return applyOuterFixedWidth({totalW, totalH});
    }

    const ComponentMetrics& cm = theme.GetMetrics(spec.widgetSize);
    const keel::f32 adjacentTextGap = keel::min(spec.descGap, cm.lineGap);

    if (spec.layout == ElementLayout::Top) {
        const keel::f32 totalW = keel::max(intrinsicControlSize.x, keel::max(labelDims.x, descDims.x));
        keel::f32 textH = 0.0f;
        if (hasLabel) textH += labelDims.y;
        if (hasDesc)  textH += (hasLabel ? adjacentTextGap : 0.0f) + descDims.y;
        return applyOuterFixedWidth({totalW, textH + spec.labelGap + intrinsicControlSize.y});
    }

    if (spec.layout == ElementLayout::Bottom) {
        const keel::f32 totalW = keel::max(intrinsicControlSize.x, keel::max(labelDims.x, descDims.x));
        keel::f32 textH = 0.0f;
        if (hasLabel) textH += labelDims.y;
        if (hasDesc)  textH += (hasLabel ? adjacentTextGap : 0.0f) + descDims.y;
        return applyOuterFixedWidth({totalW, intrinsicControlSize.y + spec.descGap + textH});
    }

    // ElementLayout::Leading / ElementLayout::Trailing
    const keel::f32 textW = keel::max(labelDims.x, descDims.x);
    const bool hasAnyText = (textW > 0.0f) || hasLabel || hasDesc;
    keel::f32 textH = 0.0f;
    if (hasLabel && hasDesc) {
        textH = labelDims.y + adjacentTextGap + descDims.y;
    } else if (hasLabel) {
        textH = labelDims.y;
    } else if (hasDesc) {
        textH = descDims.y;
    }
    const keel::f32 totalW = intrinsicControlSize.x + (hasAnyText && textW > 0.0f ? spec.inlineGap + textW : 0.0f);
    const keel::f32 totalH = keel::max(intrinsicControlSize.y, textH);
    return applyOuterFixedWidth({totalW, totalH});
}

ElementGeometry detail::Element::Resolve(
    Rect bounds,
    Vec2 intrinsicControlSize,
    const ElementSpec& spec,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    ElementGeometry geom{};
    geom.bounds = bounds;
    geom.labelFontSize = ResolveLabelFontSize(spec, theme);
    geom.descFontSize = ResolveDescFontSize(spec, theme);

    if (!spec.hasControl) {
        intrinsicControlSize = MeasureLabelBar(spec, geom.labelFontSize, canvas, theme);
    } else {
        intrinsicControlSize = Control::Measure(intrinsicControlSize, spec.control, theme);
    }

    const bool hasOuterDpW = spec.width.IsFixed();
    const bool outerIsFit = spec.width.IsFit();
    const bool ctrlStretchesW = spec.hasControl &&
        (spec.control.width.ResolveMode(spec.control.defaultWidthMode) == SizeMode::Stretch);
    const bool ctrlStretchesH = spec.hasControl &&
        (spec.control.height.ResolveMode(spec.control.defaultHeightMode) == SizeMode::Stretch);

    geom.hasLabel = spec.hasControl && HasLabelBar(spec);
    geom.hasDesc = spec.description.HasContentOrReserve();
    geom.hasExternal = geom.hasLabel || geom.hasDesc;

    const keel::f32 alignFactor = (spec.align == Align::End) ? 1.0f : ((spec.align == Align::Center) ? 0.5f : 0.0f);

    if (!spec.hasControl) {
        const keel::f32 rawDescW = geom.hasDesc
            ? TextUtil::MeasureUiTextWidth(spec.description, geom.descFontSize, canvas)
            : 0.0f;
        const keel::f32 natW = IsInlineElementLayout(spec.layout)
            ? (intrinsicControlSize.x + (geom.hasDesc && rawDescW > 0.0f ? spec.inlineGap + rawDescW : 0.0f))
            : keel::max(intrinsicControlSize.x, rawDescW);
        const keel::f32 minW = (geom.hasDesc && !spec.description.wrap.IsWordWrap()) ? natW : intrinsicControlSize.x;

        if (hasOuterDpW) {
            geom.bounds.w = keel::min(geom.bounds.w, keel::max(minW, spec.width.Dp()));
        } else if (outerIsFit) {
            geom.bounds.w = keel::min(geom.bounds.w, natW);
        }
    } else if (hasOuterDpW) {
        geom.bounds.w = keel::min(geom.bounds.w, spec.width.Dp());
    }

    bounds = geom.bounds;
    const bool compactOuter = outerIsFit;

    keel::f32 ctrlW = keel::min(bounds.w, intrinsicControlSize.x);
    if (ctrlStretchesW && (!geom.hasExternal || !IsInlineElementLayout(spec.layout))) {
        ctrlW = compactOuter ? keel::min(bounds.w, intrinsicControlSize.x) : bounds.w;
    }

    if (!geom.hasExternal) {
        const keel::f32 ctrlX = bounds.x + (bounds.w - ctrlW) * alignFactor;
        const keel::f32 ctrlH = ctrlStretchesH ? bounds.h : keel::min(intrinsicControlSize.y, bounds.h);
        const keel::f32 ctrlY = ctrlStretchesH ? bounds.y : (bounds.y + (bounds.h - ctrlH) * 0.5f);
        geom.controlRect = Rect(ctrlX, ctrlY, ctrlW, ctrlH);
        return geom;
    }

    const keel::f32 labelLineH = geom.hasLabel
        ? (canvas ? canvas->GetLineHeight(geom.labelFontSize) : theme.GetLineHeight(geom.labelFontSize))
        : 0.0f;
    const keel::f32 descLineH = geom.hasDesc
        ? (canvas ? canvas->GetLineHeight(geom.descFontSize) : theme.GetLineHeight(geom.descFontSize))
        : 0.0f;

    const Vec2 rawLabelBar = geom.hasLabel ? MeasureLabelBar(spec, geom.labelFontSize, canvas, theme) : Vec2{0.0f, 0.0f};
    if (geom.hasLabel) {
        geom.measuredLabelW = rawLabelBar.x;
    }
    if (geom.hasDesc) {
        geom.measuredDescW = TextUtil::MeasureUiTextWidth(spec.description, geom.descFontSize, canvas);
    }

    keel::f32 effLabelW = geom.measuredLabelW;
    keel::f32 effDescW = geom.measuredDescW;
    if (!IsInlineElementLayout(spec.layout) && compactOuter && ctrlStretchesW) {
        if (spec.label.wrap.IsWordWrap()) {
            effLabelW = keel::min(effLabelW, ctrlW);
        }
        const keel::f32 baseW = keel::max(ctrlW, effLabelW);
        if (spec.description.wrap.IsWordWrap()) {
            effDescW = keel::min(effDescW, baseW);
        }
    }

    const keel::f32 measuredTextW = keel::max(effLabelW, effDescW);
    const ComponentMetrics& cm = theme.GetMetrics(spec.widgetSize);
    const keel::f32 adjacentTextGap = keel::min(spec.descGap, cm.lineGap);

    const auto resolveLabelH = [&](keel::f32 availW) noexcept -> keel::f32 {
        if (!geom.hasLabel) return 0.0f;
        const keel::f32 wrappedH = spec.label.HasContentOrReserve()
            ? TextUtil::MeasureUiTextHeight(spec.label, availW, geom.labelFontSize, labelLineH, canvas, &theme)
            : 0.0f;
        return keel::max(rawLabelBar.y, wrappedH);
    };

    if (spec.layout == ElementLayout::Stacked) {
        const keel::f32 blockW = compactOuter ? keel::min(bounds.w, keel::max(ctrlW, measuredTextW)) : bounds.w;
        if (compactOuter && ctrlStretchesW) {
            ctrlW = blockW;
        }
        const keel::f32 labelH = resolveLabelH(blockW);
        const keel::f32 descH = geom.hasDesc
            ? TextUtil::MeasureUiTextHeight(spec.description, blockW, geom.descFontSize, descLineH, canvas, &theme)
            : 0.0f;
        const keel::f32 nonCtrlH = (geom.hasLabel ? labelH + spec.labelGap : 0.0f) + (geom.hasDesc ? descH + spec.descGap : 0.0f);
        const keel::f32 availCtrlH = keel::max(0.0f, bounds.h - nonCtrlH);
        const keel::f32 ctrlH = ctrlStretchesH ? availCtrlH : keel::min(intrinsicControlSize.y, availCtrlH);
        const keel::f32 blockX = bounds.x;
        const keel::f32 ctrlX = blockX + (blockW - ctrlW) * alignFactor;

        keel::f32 curY = bounds.y;
        if (geom.hasLabel) {
            geom.labelRect = Rect(blockX, curY, blockW, labelH);
            curY += labelH + spec.labelGap;
        }
        geom.controlRect = Rect(ctrlX, curY, ctrlW, ctrlH);
        curY += geom.controlRect.h + spec.descGap;
        if (geom.hasDesc) {
            geom.descRect = Rect(blockX, curY, blockW, descH);
        }
    } else if (spec.layout == ElementLayout::Top) {
        const keel::f32 blockW = compactOuter ? keel::min(bounds.w, keel::max(ctrlW, measuredTextW)) : bounds.w;
        if (compactOuter && ctrlStretchesW) {
            ctrlW = blockW;
        }
        const keel::f32 labelH = resolveLabelH(blockW);
        const keel::f32 descH = geom.hasDesc
            ? TextUtil::MeasureUiTextHeight(spec.description, blockW, geom.descFontSize, descLineH, canvas, &theme)
            : 0.0f;
        const keel::f32 textTotalH = (geom.hasLabel ? labelH : 0.0f) + (geom.hasDesc ? (geom.hasLabel ? adjacentTextGap : 0.0f) + descH : 0.0f) + spec.labelGap;
        const keel::f32 availCtrlH = keel::max(0.0f, bounds.h - textTotalH);
        const keel::f32 ctrlH = ctrlStretchesH ? availCtrlH : keel::min(intrinsicControlSize.y, availCtrlH);
        const keel::f32 blockX = bounds.x;
        const keel::f32 ctrlX = blockX + (blockW - ctrlW) * alignFactor;

        keel::f32 curY = bounds.y;
        if (geom.hasLabel) {
            geom.labelRect = Rect(blockX, curY, blockW, labelH);
            curY += labelH + (geom.hasDesc ? adjacentTextGap : 0.0f);
        }
        if (geom.hasDesc) {
            geom.descRect = Rect(blockX, curY, blockW, descH);
            curY += descH;
        }
        curY += spec.labelGap;
        geom.controlRect = Rect(ctrlX, curY, ctrlW, ctrlH);
    } else if (spec.layout == ElementLayout::Bottom) {
        const keel::f32 blockW = compactOuter ? keel::min(bounds.w, keel::max(ctrlW, measuredTextW)) : bounds.w;
        if (compactOuter && ctrlStretchesW) {
            ctrlW = blockW;
        }
        const keel::f32 labelH = resolveLabelH(blockW);
        const keel::f32 descH = geom.hasDesc
            ? TextUtil::MeasureUiTextHeight(spec.description, blockW, geom.descFontSize, descLineH, canvas, &theme)
            : 0.0f;
        const keel::f32 textTotalH = spec.descGap + (geom.hasLabel ? labelH : 0.0f) + (geom.hasDesc ? (geom.hasLabel ? adjacentTextGap : 0.0f) + descH : 0.0f);
        const keel::f32 availCtrlH = keel::max(0.0f, bounds.h - textTotalH);
        const keel::f32 ctrlH = ctrlStretchesH ? availCtrlH : keel::min(intrinsicControlSize.y, availCtrlH);
        const keel::f32 blockX = bounds.x;
        const keel::f32 ctrlX = blockX + (blockW - ctrlW) * alignFactor;

        keel::f32 curY = bounds.y;
        geom.controlRect = Rect(ctrlX, curY, ctrlW, ctrlH);
        curY += geom.controlRect.h + spec.descGap;
        if (geom.hasLabel) {
            geom.labelRect = Rect(blockX, curY, blockW, labelH);
            curY += labelH + (geom.hasDesc ? adjacentTextGap : 0.0f);
        }
        if (geom.hasDesc) {
            geom.descRect = Rect(blockX, curY, blockW, descH);
        }
    } else { // ElementLayout::Leading / ElementLayout::Trailing
        const keel::f32 gap = spec.inlineGap;
        if (ctrlStretchesW && !compactOuter) {
            ctrlW = keel::max(0.0f, bounds.w - measuredTextW - gap);
        }
        const keel::f32 availTextW = keel::max(0.0f, bounds.w - ctrlW - gap);
        const keel::f32 textBlockW = (spec.label.wrap.IsWordWrap() || spec.description.wrap.IsWordWrap())
            ? (compactOuter ? keel::min(measuredTextW, availTextW) : availTextW)
            : keel::min(measuredTextW, availTextW);

        const keel::f32 labelH = resolveLabelH(textBlockW);
        const keel::f32 descH = geom.hasDesc
            ? TextUtil::MeasureUiTextHeight(spec.description, textBlockW, geom.descFontSize, descLineH, canvas, &theme)
            : 0.0f;

        const keel::f32 totalTextH = (geom.hasLabel && geom.hasDesc) ? (labelH + adjacentTextGap + descH) : (geom.hasLabel ? labelH : descH);
        const keel::f32 startY = bounds.y + (bounds.h - totalTextH) * 0.5f;
        const keel::f32 ctrlH = ctrlStretchesH ? bounds.h : keel::min(intrinsicControlSize.y, bounds.h);

        const bool controlSlotOnRight = spec.hasControl
            ? (spec.layout == ElementLayout::Leading)
            : (spec.layout == ElementLayout::Trailing);
        if (controlSlotOnRight) {
            const keel::f32 ctrlX = compactOuter
                ? (bounds.x + (textBlockW > 0.0f ? textBlockW + gap : 0.0f))
                : (bounds.Right() - ctrlW);
            geom.controlRect = Rect(ctrlX, bounds.y + (bounds.h - ctrlH) * 0.5f, ctrlW, ctrlH);

            keel::f32 textY = startY;
            if (geom.hasLabel) {
                geom.labelRect = Rect(bounds.x, textY, textBlockW, labelH);
                textY += labelH + adjacentTextGap;
            }
            if (geom.hasDesc) {
                geom.descRect = Rect(bounds.x, textY, textBlockW, descH);
            }
        } else {
            // ElementLayout::Trailing ([Control] Label)
            const keel::f32 contentW = ctrlW + (textBlockW > 0.0f ? gap + textBlockW : 0.0f);
            const keel::f32 groupStartX = compactOuter
                ? bounds.x
                : (bounds.x + keel::max(0.0f, bounds.w - contentW) * alignFactor);
            geom.controlRect = Rect(groupStartX, bounds.y + (bounds.h - ctrlH) * 0.5f, ctrlW, ctrlH);

            const keel::f32 textX = groupStartX + ctrlW + gap;
            keel::f32 textY = startY;
            if (geom.hasLabel) {
                geom.labelRect = Rect(textX, textY, textBlockW, labelH);
                textY += labelH + adjacentTextGap;
            }
            if (geom.hasDesc) {
                geom.descRect = Rect(textX, textY, textBlockW, descH);
            }
        }
    }

    return geom;
}

void detail::Element::RenderText(
    ICanvas& canvas,
    const ElementGeometry& geom,
    const ElementSpec& spec,
    const UiTheme& theme
) noexcept {
    const keel::f32 alignFactor = (spec.align == Align::End) ? 1.0f : ((spec.align == Align::Center) ? 0.5f : 0.0f);
    const Color labelCol = theme.ResolveTextColor(spec.label, UiVariant::Default, spec.disabled, spec.hovered, spec.active);
    const Color descCol = theme.ResolveTextColor(spec.description, UiVariant::Muted, spec.disabled, spec.hovered, spec.active);
    const UiSize effSize = (spec.widgetSize == UiSize::None) ? UiSize::Md : spec.widgetSize;
    const ComponentMetrics& m = theme.GetMetrics(effSize);
    const auto& pal = theme.palette;
    const keel::f32 badgeFont = (spec.badge.size.px > 0.0f)
        ? spec.badge.size.px
        : (spec.badge.size.role.has_value()
            ? theme.GetTypography(*spec.badge.size.role).fontSize
            : (spec.badge.size.preset != UiSize::None
                ? theme.GetMetrics(spec.badge.size.preset).badgeFontSize
                : m.badgeFontSize));

    const bool hasDecoratedLabelBar = !spec.hasControl || spec.icon.IsValid() || !spec.badge.empty();
    const Rect labelBarRect = spec.hasControl ? geom.labelRect : geom.controlRect;
    const keel::f32 topY = labelBarRect.y;
    const keel::f32 titleLineH = labelBarRect.h;

    if (spec.hasControl && geom.hasLabel && !hasDecoratedLabelBar) {
        if (!spec.label.empty() && geom.labelRect.w > 0.0f && geom.labelRect.h > 0.0f) {
            const keel::f32 lineH = canvas.GetLineHeight(geom.labelFontSize);
            TextUtil::DrawUiText(canvas, spec.label, geom.labelRect, geom.labelFontSize, lineH, labelCol, alignFactor, 0.5f, &theme);
        }
    } else if ((!spec.hasControl || geom.hasLabel) && labelBarRect.w > 0.0f && labelBarRect.h > 0.0f) {
        const Vec2 rawBar = MeasureLabelBar(spec, geom.labelFontSize, &canvas, theme);
        const keel::f32 barW = keel::min(labelBarRect.w, rawBar.x);
        keel::f32 curX = labelBarRect.x + keel::max(0.0f, labelBarRect.w - barW) * alignFactor;
        const keel::f32 barLeftX = curX;

        if (spec.icon.IsValid()) {
            const keel::f32 iconY = topY + (titleLineH - m.iconSize) * 0.5f;
            spec.icon.Draw(canvas, Rect(curX, iconY, m.iconSize, m.iconSize), labelCol);
            curX += m.iconSize + ((spec.label.HasContentOrReserve() || !spec.badge.empty()) ? m.itemGap : 0.0f);
        }

        if (!spec.label.empty()) {
            const keel::f32 badgeReserveW = spec.badge.empty()
                ? 0.0f
                : (m.badgeGap + TextUtil::MeasureWidth(spec.badge, badgeFont, &canvas) + 12.0f);
            const keel::f32 titleAvailW = keel::max(0.0f, (barLeftX + barW) - curX - badgeReserveW);
            const Rect titleR(curX, topY, titleAvailW, titleLineH);
            if (spec.label.wrap.rich) {
                const keel::f32 rawTitleW = TextUtil::MeasureUiTextWidth(spec.label, geom.labelFontSize, &canvas);
                UiText singleLineRich = spec.label;
                singleLineRich.wrap.maxLines = 1;
                TextUtil::DrawUiText(canvas, singleLineRich, titleR, geom.labelFontSize, titleLineH, labelCol, 0.0f, 0.5f, &theme);
                curX += keel::min(titleAvailW, rawTitleW);
            } else {
                keel::f32 titleW = canvas.MeasureText(spec.label.text, geom.labelFontSize).x;
                keel::FixedString<256> truncatedTitle;
                keel::StringRef displayTitle = spec.label.text;
                if (titleW > titleAvailW + kTextSubpixelTolerance && titleAvailW > 0.0f) {
                    truncatedTitle = TextUtil::TruncateWithEllipsis(spec.label.text, titleAvailW, geom.labelFontSize, &canvas);
                    displayTitle = truncatedTitle;
                    titleW = canvas.MeasureText(displayTitle, geom.labelFontSize).x;
                }
                canvas.DrawTextBox(displayTitle, titleR, geom.labelFontSize, labelCol, 0.0f, 0.5f);
                curX += titleW;
            }
        }

        if (!spec.badge.empty()) {
            if (!spec.label.empty()) {
                curX += m.badgeGap;
            }
            const keel::f32 badgeTextW = TextUtil::MeasureWidth(spec.badge, badgeFont, &canvas);
            const keel::f32 badgePadH = 6.0f;
            const keel::f32 badgeW = badgeTextW + badgePadH * 2.0f;
            const keel::f32 badgeH = badgeFont + 4.0f;
            const keel::f32 badgeY = topY + (titleLineH - badgeH) * 0.5f;

            Color badgeBg = pal.surfaceHover;
            Color badgeTextCol = pal.textPrimary;
            Color badgeBorderCol = Color::Transparent();
            keel::f32 badgeBorderW = 0.0f;
            if (spec.badge.color.HasAny()) {
                const Color resolvedBadge = theme.ResolveColor(spec.badge.color, spec.hovered, spec.active, spec.disabled, pal.textPrimary);
                badgeBg = resolvedBadge.WithAlphaF(spec.disabled ? 0.12f : 0.22f);
                const Color refBg = (spec.active && theme.Painter().GetChromeMetrics(theme).solidSelectionHighlight)
                    ? pal.accent
                    : pal.surface;
                badgeTextCol = spec.disabled ? resolvedBadge.WithAlphaF(theme.disabledOpacity) : theme.EnsureReadableInk(resolvedBadge, refBg);
                badgeBorderCol = resolvedBadge.WithAlphaF(spec.disabled ? 0.25f : 0.50f);
                badgeBorderW = 1.0f;
            } else if (spec.disabled) {
                badgeBg = badgeBg.WithAlphaF(theme.disabledOpacity);
                badgeTextCol = badgeTextCol.WithAlphaF(theme.disabledOpacity);
            }

            const Rect badgeR(curX, badgeY, badgeW, badgeH);
            theme.Painter().DrawBadge(canvas, theme, badgeR, badgeBg, badgeBorderCol, badgeBorderW, badgeH * 0.5f);
            canvas.DrawTextBox(spec.badge.text, badgeR, badgeFont, badgeTextCol, 0.5f, 0.5f);
            curX += badgeW;
        }
    }

    if (geom.hasDesc && !spec.description.empty() && geom.descRect.w > 0.0f && geom.descRect.h > 0.0f) {
        const keel::f32 lineH = canvas.GetLineHeight(geom.descFontSize);
        TextUtil::DrawUiText(canvas, spec.description, geom.descRect, geom.descFontSize, lineH, descCol, alignFactor, 0.5f, &theme);
    }
}

} // namespace guikit
