#include "guikit/core/Element.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"

namespace guikit {

namespace {

ControlSpec EffectiveControlSpec(const ElementSpec& spec) noexcept {
    ControlSpec c = spec.control;
    if (!c.width.has_value() && spec.width.has_value()) {
        c.width = spec.width;
    }
    if (!c.height.has_value() && spec.height.has_value()) {
        c.height = spec.height;
    }
    c.stretchable = spec.stretchableControl;
    c.stretchHeightWhenStandalone = spec.stretchHeightWhenStandalone;
    return c;
}

bool HasLabelBar(const ElementSpec& spec) noexcept {
    return spec.icon.IsValid() || spec.label.HasContentOrReserve() || spec.badge.HasContentOrReserve();
}

ElementSpec MakeSpecFromPayload(
    const Node* n,
    const ElementPayload& ep,
    const UiTheme& theme
) noexcept {
    const bool isHovered = n->hovered;
    const bool isActive = n->pressed || n->focused || n->active;
    if (!ep.hasChildControl && n->firstChild == nullptr) {
        return Element::MakeSpec(ep.opts, n->uiSize, n->disabled, theme, isHovered, isActive);
    }

    const ComponentMetrics& cm = theme.GetMetrics(n->uiSize);
    const InputFieldMetrics& im = theme.GetInputField(n->uiSize);
    ControlSpec ctrl = ep.hasChildControl
        ? ep.childControlSpec
        : ControlSpec{
            .stretchable = true,
            .stretchHeightWhenStandalone = (n->firstChild->heightMode == SizeMode::Stretch),
            .hasIntrinsicBox = true
        };
    // The child Control node already measures and renders its own well frame via Control::Measure / Control::RenderFrame.
    ctrl.hasIntrinsicBox = true;

    const std::optional<float> effWidth = ctrl.width.has_value() ? ctrl.width : ep.opts.width;
    const std::optional<float> effHeight = ctrl.height.has_value() ? ctrl.height : ep.opts.height;
    ctrl.width = effWidth;
    ctrl.height = effHeight;

    return ElementSpec{
        .label = ep.opts.label,
        .description = ep.opts.description,
        .icon = ep.opts.icon,
        .badge = ep.opts.badge,
        .layout = ep.opts.layout.value_or(ctrl.defaultLayout),
        .inlineOrder = ep.opts.inlineOrder.value_or(ctrl.defaultInlineOrder),
        .align = ep.opts.align,
        .stretch = ep.opts.stretch.value_or(ep.hasChildControl ? StretchMode::Compact : StretchMode::Fill),
        .width = effWidth,
        .height = effHeight,
        .control = ctrl,
        .background = ep.opts.background,
        .border = ep.opts.border,
        .widgetSize = n->uiSize,
        .labelGap = (ctrl.labelGap > 0.0f) ? ctrl.labelGap : im.labelGap,
        .descGap = (ctrl.descGap > 0.0f) ? ctrl.descGap : im.descGap,
        .inlineGap = (ctrl.inlineGap > 0.0f) ? ctrl.inlineGap : cm.paddingX,
        .hasControl = true,
        .headingTypography = false,
        .stretchableControl = ctrl.stretchable,
        .stretchHeightWhenStandalone = (ctrl.stretchHeightWhenStandalone || n->heightMode == SizeMode::Stretch) && !ep.opts.background.IsBackgroundEnabled(false) && ep.opts.border.width <= 0.0f,
        .disabled = n->disabled,
        .hovered = isHovered,
        .active = isActive
    };
}

} // namespace

float Element::ResolveLabelFontSize(const ElementSpec& spec, const UiTheme& theme) noexcept {
    if (spec.label.size.px > 0.0f) {
        return spec.label.size.px;
    }
    if (spec.label.size.role.has_value()) {
        return theme.GetTypography(*spec.label.size.role).fontSize;
    }
    if (spec.headingTypography) {
        const UiSize effSize = (spec.label.size.preset != UiSize::None) ? spec.label.size.preset : spec.widgetSize;
        return theme.GetHeading(effSize).titleFontSize;
    }
    return theme.ResolveFontSize(spec.label, TextRole::LabelLarge, spec.widgetSize);
}

float Element::ResolveDescFontSize(const ElementSpec& spec, const UiTheme& theme) noexcept {
    if (spec.description.size.px > 0.0f) {
        return spec.description.size.px;
    }
    if (spec.description.size.role.has_value()) {
        return theme.GetTypography(*spec.description.size.role).fontSize;
    }
    if (spec.headingTypography) {
        const UiSize effSize = (spec.description.size.preset != UiSize::None) ? spec.description.size.preset : spec.widgetSize;
        return theme.GetHeading(effSize).subtitleFontSize;
    }
    return theme.ResolveFontSize(spec.description, TextRole::LabelSmall, spec.widgetSize);
}

Vec2 Element::MeasureLabelBar(
    const ElementSpec& spec,
    float labelFontSize,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    const HeadingMetrics& m = theme.GetHeading(spec.widgetSize);
    const UiSize badgeSize = (spec.badge.size.preset != UiSize::None) ? spec.badge.size.preset : spec.widgetSize;
    const float badgeFont = (spec.badge.size.px > 0.0f)
        ? spec.badge.size.px
        : (spec.badge.size.role.has_value()
            ? theme.GetTypography(*spec.badge.size.role).fontSize
            : theme.GetHeading(badgeSize).badgeFontSize);

    float barW = 0.0f;
    float barH = 0.0f;

    if (spec.icon.IsValid()) {
        barW += m.iconSize;
        barH = std::max(barH, m.iconSize);
    }

    if (spec.label.HasContentOrReserve()) {
        if (barW > 0.0f) barW += m.gap;
        const float textW = TextUtil::MeasureWidth(spec.label, labelFontSize, canvas);
        barW += textW;
        const float lineH = canvas ? canvas->GetLineHeight(labelFontSize) : theme.GetLineHeight(labelFontSize);
        barH = std::max(barH, lineH);
    }

    if (spec.badge.HasContentOrReserve()) {
        if (barW > 0.0f) barW += m.badgeGap;
        const float badgeTextW = TextUtil::MeasureWidth(spec.badge, badgeFont, canvas);
        const float badgeW = badgeTextW + 12.0f;
        barW += badgeW;
        barH = std::max(barH, badgeFont * 1.4f);
    }

    return {barW, barH};
}

namespace {

float ResolveChildAvailableWidth(
    const ElementSpec& spec,
    float availableWidth,
    ICanvas* canvas,
    const UiTheme& theme,
    float labelFontSize,
    float descFontSize,
    Vec2 measuredLabelBar
) noexcept {
    if (availableWidth <= 0.0f) {
        return 0.0f;
    }
    const float outerPad = Element::ComputeOuterPadding(spec, theme);
    float availW = std::max(0.0f, availableWidth - outerPad * 2.0f);
    if (spec.layout != ElementLayout::Inline) {
        return availW;
    }
    const bool hasLabel = HasLabelBar(spec);
    const bool hasDesc = spec.description.HasContentOrReserve();
    if (!hasLabel && !hasDesc) {
        return availW;
    }
    const ControlSpec ctrl = EffectiveControlSpec(spec);
    if (spec.stretch == StretchMode::Fill && ctrl.width.has_value()) {
        return availW;
    }
    if (spec.stretch == StretchMode::Fill && !ctrl.width.has_value()) {
        const float descW = hasDesc ? TextUtil::MeasureWidth(spec.description, descFontSize, canvas) : 0.0f;
        const float textW = std::max(measuredLabelBar.x, descW);
        return std::max(theme.GetMetrics(spec.widgetSize).defaultHeight, availW - textW - spec.inlineGap);
    }
    return availW;
}

} // namespace

float Element::ComputeOuterPadding(
    const ElementSpec& spec,
    const UiTheme& theme
) noexcept {
    if (!HasOuterFrame(spec)) {
        return 0.0f;
    }
    const ComponentMetrics& cm = theme.GetMetrics(spec.widgetSize);
    const float basePad = std::max(theme.Scale(8.0f), cm.paddingX * 0.75f);
    const float strokePad = (spec.border.width > 0.0f) ? theme.Scale(spec.border.width) : 0.0f;
    return basePad + strokePad;
}

ElementSpec Element::MakeSpec(
    const ElementOptions& opts,
    UiSize activeSize,
    bool isDisabled,
    const UiTheme& theme,
    bool isHovered,
    bool isActive
) noexcept {
    const HeadingMetrics& m = theme.GetHeading(activeSize);
    return ElementSpec{
        .label = opts.label,
        .description = opts.description,
        .icon = opts.icon,
        .badge = opts.badge,
        .layout = opts.layout.value_or(ElementLayout::Stacked),
        .inlineOrder = opts.inlineOrder.value_or(InlineOrder::ControlFirst),
        .align = opts.align,
        .stretch = opts.stretch.value_or(StretchMode::Fill),
        .width = opts.width,
        .height = opts.height,
        .background = opts.background,
        .border = opts.border,
        .widgetSize = activeSize,
        .labelGap = m.lineGap,
        .descGap = m.lineGap,
        .inlineGap = m.gap * 1.5f,
        .hasControl = false,
        .headingTypography = false,
        .stretchableControl = false,
        .stretchHeightWhenStandalone = false,
        .disabled = isDisabled,
        .hovered = isHovered,
        .active = isActive
    };
}

ElementScope Element::Open(
    Context& ctx,
    const ElementOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();

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

    Node* node = ctx.CreateNode(NodeType::Element, id);
    node->theme = ctx.PushScopedTheme(opts.theme, opts.scale);
    ctx.PushScale(opts.scale.value_or(ctx.GetCurrentScale()));
    ctx.PushSize(activeSize);
    ctx.PushDisabled(effectiveDisabled);

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
    node->debugBoundsOverride = opts.debugBounds;
    node->inspectModeOverride = opts.inspectMode;

    const StretchMode initStretch = opts.stretch.value_or(StretchMode::Fill);
    node->visualHash = detail::HashArgs(
        static_cast<uint8_t>(opts.layout.value_or(ElementLayout::Stacked)),
        opts.layout.has_value(),
        static_cast<uint8_t>(opts.inlineOrder.value_or(InlineOrder::ControlFirst)),
        opts.inlineOrder.has_value(),
        static_cast<uint8_t>(opts.align),
        static_cast<uint8_t>(initStretch),
        opts.stretch.has_value(),
        opts.width.value_or(0.0f),
        opts.height.value_or(0.0f),
        opts.background,
        opts.border,
        payload->opts.label,
        payload->opts.description,
        payload->opts.badge,
        effectiveDisabled,
        static_cast<uint8_t>(opts.icon.GetType()),
        static_cast<uint8_t>(opts.icon.GetId()),
        payload->opts.icon.GetPath()
    );

    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* ep = n->As<ElementPayload>();
        if (!ep) return {0.0f, 0.0f};
        ep->lastCanvas = canvas;
        const ElementSpec spec = MakeSpecFromPayload(n, *ep, theme);
        Vec2 childIntrinsic{0.0f, 0.0f};
        if ((ep->hasChildControl || n->firstChild != nullptr) && n->firstChild != nullptr) {
            const Node* child = n->firstChild;
            const UiTheme& childTheme = (child->theme != nullptr) ? *child->theme : theme;
            const float labelFs = ResolveLabelFontSize(spec, theme);
            const float descFs = ResolveDescFontSize(spec, theme);
            const Vec2 rawBar = HasLabelBar(spec) ? MeasureLabelBar(spec, labelFs, canvas, theme) : Vec2{0.0f, 0.0f};
            const float childAvailW = ResolveChildAvailableWidth(spec, availableWidth, canvas, theme, labelFs, descFs, rawBar);
            childIntrinsic = child->measureFn
                ? child->measureFn(child, childAvailW, canvas, childTheme)
                : child->intrinsicSize;
        }
        return Element::Measure(childIntrinsic, spec, canvas, theme, availableWidth);
    };

    node->arrangeFn = &Element::Arrange;

    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* ep = n->As<ElementPayload>();
        if (!ep) return;
        const ElementSpec spec = MakeSpecFromPayload(n, *ep, theme);
        if ((!ep->hasChildControl && n->firstChild == nullptr) || !n->firstChild) {
            Element::Render(canvas, n->computedRect, spec, theme);
            return;
        }
        const Node* child = n->firstChild;
        const UiTheme& childTheme = (child->theme != nullptr) ? *child->theme : theme;
        const float labelFs = ResolveLabelFontSize(spec, theme);
        const float descFs = ResolveDescFontSize(spec, theme);
        const Vec2 rawBar = HasLabelBar(spec) ? MeasureLabelBar(spec, labelFs, &canvas, theme) : Vec2{0.0f, 0.0f};
        const float childAvailW = ResolveChildAvailableWidth(spec, n->computedRect.w, &canvas, theme, labelFs, descFs, rawBar);
        const Vec2 childIntrinsic = child->measureFn
            ? child->measureFn(child, childAvailW, &canvas, childTheme)
            : child->intrinsicSize;
        const ElementGeometry geom = Element::Resolve(n->computedRect, childIntrinsic, spec, &canvas, theme);
        Element::RenderText(canvas, geom, spec, theme);
    };

    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed;
    node->widthMode = (initStretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

    ctx.AdaptChildToContainer(node);
    ctx.PushContainer(node);

    return ElementScope(&ctx, node, res);
}

void Element::Arrange(
    Node* node,
    const Rect& availableRect,
    const UiTheme& theme
) noexcept {
    if (!node) return;
    Rect bounds = availableRect;
    if (node->parent == nullptr) {
        if (node->widthMode == SizeMode::Fit) bounds.w = node->intrinsicSize.x;
        if (node->heightMode == SizeMode::Fit) bounds.h = node->intrinsicSize.y;
    }
    node->computedRect = bounds;

    const auto* ep = node->As<ElementPayload>();
    if (!ep || (!ep->hasChildControl && node->firstChild == nullptr) || !node->firstChild) {
        return;
    }

    Node* child = node->firstChild;
    const UiTheme& childTheme = (child->theme != nullptr) ? *child->theme : theme;
    const ElementSpec spec = MakeSpecFromPayload(node, *ep, theme);
    const float labelFs = ResolveLabelFontSize(spec, theme);
    const float descFs = ResolveDescFontSize(spec, theme);
    const Vec2 rawBar = HasLabelBar(spec) ? MeasureLabelBar(spec, labelFs, ep->lastCanvas, theme) : Vec2{0.0f, 0.0f};
    const float childAvailW = ResolveChildAvailableWidth(spec, bounds.w, ep->lastCanvas, theme, labelFs, descFs, rawBar);
    const Vec2 childIntrinsic = child->measureFn
        ? child->measureFn(child, childAvailW, ep->lastCanvas, childTheme)
        : child->intrinsicSize;
    const ElementGeometry geom = Element::Resolve(bounds, childIntrinsic, spec, ep->lastCanvas, theme);
    child->computedRect = geom.controlRect;
    if (child->arrangeFn) {
        child->arrangeFn(child, geom.controlRect, childTheme);
    }
}

Vec2 Element::Measure(
    Vec2 intrinsicControlSize,
    const ElementSpec& spec,
    ICanvas* canvas,
    const UiTheme& theme,
    float availableWidth
) noexcept {
    const float labelFontSize = ResolveLabelFontSize(spec, theme);
    const float descFontSize = ResolveDescFontSize(spec, theme);
    const float outerPad = ComputeOuterPadding(spec, theme);

    if (availableWidth > 0.0f && outerPad > 0.0f) {
        availableWidth = std::max(0.0f, availableWidth - outerPad * 2.0f);
    }

    const ControlSpec ctrlSpec = EffectiveControlSpec(spec);
    if (!spec.hasControl) {
        intrinsicControlSize = MeasureLabelBar(spec, labelFontSize, canvas, theme);
        if (ctrlSpec.height.has_value() && *ctrlSpec.height > 0.0f) {
            intrinsicControlSize.y = std::max(intrinsicControlSize.y, theme.Scale(*ctrlSpec.height));
        }
    } else {
        intrinsicControlSize = Control::Measure(intrinsicControlSize, ctrlSpec, spec.stretch, spec.widgetSize, theme);
    }

    const bool stretchableCtrl = spec.hasControl ? ctrlSpec.stretchable : false;

    if (!stretchableCtrl && ctrlSpec.width.has_value() && spec.stretch == StretchMode::Fill && availableWidth > 0.0f) {
        const float ratio = std::clamp(*ctrlSpec.width, 0.0f, 1.0f);
        availableWidth *= ratio;
    }

    const auto applyFixedWidthAndOuterPad = [&](Vec2 measured) noexcept -> Vec2 {
        if (!stretchableCtrl && ctrlSpec.width.has_value() && spec.stretch != StretchMode::Fill) {
            measured.x = std::max(measured.x, theme.Scale(*ctrlSpec.width));
        }
        if (outerPad > 0.0f) {
            measured.x += outerPad * 2.0f;
            measured.y += outerPad * 2.0f;
        }
        return measured;
    };

    const bool hasLabel = spec.hasControl && HasLabelBar(spec);
    const bool hasDesc = spec.description.HasContentOrReserve();
    const bool hasExternal = hasLabel || hasDesc;

    if (!hasExternal) {
        return applyFixedWidthAndOuterPad(intrinsicControlSize);
    }

    const Vec2 rawLabelBar = hasLabel ? MeasureLabelBar(spec, labelFontSize, canvas, theme) : Vec2{0.0f, 0.0f};
    const float rawLabelW = rawLabelBar.x;
    const float rawDescW = hasDesc ? TextUtil::MeasureWidth(spec.description, descFontSize, canvas) : 0.0f;

    float effLabelW = rawLabelW;
    float effDescW = rawDescW;
    if (spec.layout != ElementLayout::Inline && spec.stretch == StretchMode::Compact && stretchableCtrl) {
        if (spec.label.wrap.IsWordWrap()) {
            effLabelW = std::min(rawLabelW, intrinsicControlSize.x);
        }
        const float baseW = std::max(intrinsicControlSize.x, effLabelW);
        if (spec.description.wrap.IsWordWrap()) {
            effDescW = std::min(rawDescW, baseW);
        }
    }

    float textWrapW = availableWidth;
    if (!stretchableCtrl && ctrlSpec.width.has_value() && spec.stretch != StretchMode::Fill) {
        const float fixedW = theme.Scale(*ctrlSpec.width);
        textWrapW = (availableWidth > 0.0f) ? std::min(availableWidth, fixedW) : fixedW;
    }
    if (spec.layout == ElementLayout::Inline) {
        if (textWrapW > 0.0f) {
            float ctrlW = std::min(textWrapW, intrinsicControlSize.x);
            if (stretchableCtrl && spec.stretch == StretchMode::Fill && ctrlSpec.width.has_value()) {
                const float ratio = std::clamp(*ctrlSpec.width, 0.0f, 1.0f);
                ctrlW = textWrapW * ratio;
            }
            textWrapW = std::max(0.0f, textWrapW - ctrlW - spec.inlineGap);
        }
    } else if (spec.stretch == StretchMode::Compact && stretchableCtrl) {
        const float compactBlockW = std::max({intrinsicControlSize.x, effLabelW, effDescW});
        textWrapW = (availableWidth > 0.0f) ? std::min(availableWidth, compactBlockW) : compactBlockW;
    }

    Vec2 labelDims = {0.0f, 0.0f};
    if (hasLabel) {
        const float lineH = canvas ? canvas->GetLineHeight(labelFontSize) : theme.GetLineHeight(labelFontSize);
        const float wrappedH = spec.label.HasContentOrReserve()
            ? TextUtil::MeasureUiTextHeight(spec.label, textWrapW, labelFontSize, lineH, canvas)
            : 0.0f;
        labelDims = {effLabelW, std::max(rawLabelBar.y, wrappedH)};
    }

    Vec2 descDims = {0.0f, 0.0f};
    if (hasDesc) {
        const float lineH = canvas ? canvas->GetLineHeight(descFontSize) : theme.GetLineHeight(descFontSize);
        const float h = TextUtil::MeasureUiTextHeight(spec.description, textWrapW, descFontSize, lineH, canvas);
        descDims = {effDescW, h};
    }

    if (spec.layout == ElementLayout::Stacked) {
        const float totalW = std::max({intrinsicControlSize.x, labelDims.x, descDims.x});
        float totalH = intrinsicControlSize.y;
        if (hasLabel) totalH += labelDims.y + spec.labelGap;
        if (hasDesc)  totalH += spec.descGap + descDims.y;
        return applyFixedWidthAndOuterPad({totalW, totalH});
    }

    if (spec.layout == ElementLayout::Top) {
        const float totalW = std::max({intrinsicControlSize.x, labelDims.x, descDims.x});
        float textH = 0.0f;
        if (hasLabel) textH += labelDims.y;
        if (hasDesc)  textH += (hasLabel ? spec.descGap : 0.0f) + descDims.y;
        return applyFixedWidthAndOuterPad({totalW, textH + spec.labelGap + intrinsicControlSize.y});
    }

    if (spec.layout == ElementLayout::Bottom) {
        const float totalW = std::max({intrinsicControlSize.x, labelDims.x, descDims.x});
        float textH = 0.0f;
        if (hasLabel) textH += labelDims.y;
        if (hasDesc)  textH += (hasLabel ? spec.descGap : 0.0f) + descDims.y;
        return applyFixedWidthAndOuterPad({totalW, intrinsicControlSize.y + spec.descGap + textH});
    }

    // ElementLayout::Inline
    const float textW = std::max(labelDims.x, descDims.x);
    const bool hasAnyText = (textW > 0.0f) || hasLabel || hasDesc;
    float textH = 0.0f;
    if (hasLabel && hasDesc) {
        textH = labelDims.y + spec.descGap + descDims.y;
    } else if (hasLabel) {
        textH = labelDims.y;
    } else if (hasDesc) {
        textH = descDims.y;
    }
    const float totalW = intrinsicControlSize.x + (hasAnyText && textW > 0.0f ? spec.inlineGap + textW : 0.0f);
    const float totalH = std::max(intrinsicControlSize.y, textH);
    return applyFixedWidthAndOuterPad({totalW, totalH});
}

ElementGeometry Element::Resolve(
    Rect bounds,
    Vec2 intrinsicControlSize,
    const ElementSpec& spec,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    ElementGeometry geom{};
    geom.outerBounds = bounds;
    geom.labelFontSize = ResolveLabelFontSize(spec, theme);
    geom.descFontSize = ResolveDescFontSize(spec, theme);

    const float outerPad = ComputeOuterPadding(spec, theme);
    if (outerPad > 0.0f) {
        bounds = bounds.Inset(outerPad);
    }

    const ControlSpec ctrlSpec = EffectiveControlSpec(spec);
    if (!spec.hasControl) {
        intrinsicControlSize = MeasureLabelBar(spec, geom.labelFontSize, canvas, theme);
        if (ctrlSpec.height.has_value() && *ctrlSpec.height > 0.0f) {
            intrinsicControlSize.y = std::max(intrinsicControlSize.y, theme.Scale(*ctrlSpec.height));
        }
    } else {
        intrinsicControlSize = Control::Measure(intrinsicControlSize, ctrlSpec, spec.stretch, spec.widgetSize, theme);
    }

    const bool stretchableCtrl = spec.hasControl ? ctrlSpec.stretchable : false;
    const bool stretchStandaloneH = spec.hasControl ? ctrlSpec.stretchHeightWhenStandalone : false;

    geom.hasLabel = spec.hasControl && HasLabelBar(spec);
    geom.hasDesc = spec.description.HasContentOrReserve();
    geom.hasExternal = geom.hasLabel || geom.hasDesc;

    const float alignFactor = (spec.align == Align::End) ? 1.0f : ((spec.align == Align::Center) ? 0.5f : 0.0f);

    if (!stretchableCtrl && spec.stretch == StretchMode::Fill && ctrlSpec.width.has_value()) {
        const float ratio = std::clamp(*ctrlSpec.width, 0.0f, 1.0f);
        const float effW = bounds.w * ratio;
        bounds.x += (bounds.w - effW) * alignFactor;
        bounds.w = effW;
    }

    float ctrlW = std::min(bounds.w, intrinsicControlSize.x);
    if (stretchableCtrl && spec.stretch == StretchMode::Fill) {
        if (!geom.hasExternal || spec.layout != ElementLayout::Inline || ctrlSpec.width.has_value()) {
            const float ratio = std::clamp(ctrlSpec.width.value_or(1.0f), 0.0f, 1.0f);
            ctrlW = bounds.w * ratio;
        }
    }

    if (!geom.hasExternal) {
        const float ctrlX = bounds.x + (bounds.w - ctrlW) * alignFactor;
        const float ctrlH = stretchStandaloneH ? bounds.h : std::min(intrinsicControlSize.y, bounds.h);
        const float ctrlY = stretchStandaloneH ? bounds.y : (bounds.y + (bounds.h - ctrlH) * 0.5f);
        geom.controlRect = Rect(ctrlX, ctrlY, ctrlW, ctrlH);
        geom.innerControlRect = spec.hasControl
            ? Control::ResolveInnerRect(geom.controlRect, ctrlSpec, spec.widgetSize, theme)
            : geom.controlRect;
        return geom;
    }

    const float labelLineH = geom.hasLabel
        ? (canvas ? canvas->GetLineHeight(geom.labelFontSize) : theme.GetLineHeight(geom.labelFontSize))
        : 0.0f;
    const float descLineH = geom.hasDesc
        ? (canvas ? canvas->GetLineHeight(geom.descFontSize) : theme.GetLineHeight(geom.descFontSize))
        : 0.0f;

    const Vec2 rawLabelBar = geom.hasLabel ? MeasureLabelBar(spec, geom.labelFontSize, canvas, theme) : Vec2{0.0f, 0.0f};
    if (geom.hasLabel) {
        geom.measuredLabelW = rawLabelBar.x;
    }

    if (geom.hasDesc) {
        geom.measuredDescW = TextUtil::MeasureWidth(spec.description, geom.descFontSize, canvas);
    }

    float effLabelW = geom.measuredLabelW;
    float effDescW = geom.measuredDescW;
    if (spec.layout != ElementLayout::Inline && spec.stretch == StretchMode::Compact && stretchableCtrl) {
        if (spec.label.wrap.IsWordWrap()) {
            effLabelW = std::min(effLabelW, ctrlW);
        }
        const float baseW = std::max(ctrlW, effLabelW);
        if (spec.description.wrap.IsWordWrap()) {
            effDescW = std::min(effDescW, baseW);
        }
    }

    const float measuredTextW = std::max(effLabelW, effDescW);
    const bool compactNaturalBlock = (spec.stretch == StretchMode::Compact) && (stretchableCtrl || !ctrlSpec.width.has_value());

    const auto resolveLabelH = [&](float availW) noexcept -> float {
        if (!geom.hasLabel) return 0.0f;
        const float wrappedH = spec.label.HasContentOrReserve()
            ? TextUtil::MeasureUiTextHeight(spec.label, availW, geom.labelFontSize, labelLineH, canvas)
            : 0.0f;
        return std::max(rawLabelBar.y, wrappedH);
    };

    if (spec.layout == ElementLayout::Stacked) {
        const float blockW = compactNaturalBlock ? std::min(bounds.w, std::max(ctrlW, measuredTextW)) : bounds.w;
        const float labelH = resolveLabelH(blockW);
        const float descH = geom.hasDesc
            ? TextUtil::MeasureUiTextHeight(spec.description, blockW, geom.descFontSize, descLineH, canvas)
            : 0.0f;
        const float availCtrlH = std::max(0.0f, bounds.h - (geom.hasLabel ? labelH + spec.labelGap : 0.0f) - (geom.hasDesc ? descH + spec.descGap : 0.0f));
        const float ctrlH = stretchStandaloneH ? availCtrlH : std::min(intrinsicControlSize.y, availCtrlH);
        const float blockX = bounds.x;
        const float ctrlX = blockX + (blockW - ctrlW) * alignFactor;

        float curY = bounds.y;
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
        const float blockW = compactNaturalBlock ? std::min(bounds.w, std::max(ctrlW, measuredTextW)) : bounds.w;
        const float labelH = resolveLabelH(blockW);
        const float descH = geom.hasDesc
            ? TextUtil::MeasureUiTextHeight(spec.description, blockW, geom.descFontSize, descLineH, canvas)
            : 0.0f;
        const float textTotalH = (geom.hasLabel ? labelH : 0.0f) + (geom.hasDesc ? (geom.hasLabel ? spec.descGap : 0.0f) + descH : 0.0f) + spec.labelGap;
        const float availCtrlH = std::max(0.0f, bounds.h - textTotalH);
        const float ctrlH = stretchStandaloneH ? availCtrlH : std::min(intrinsicControlSize.y, availCtrlH);
        const float blockX = bounds.x;
        const float ctrlX = blockX + (blockW - ctrlW) * alignFactor;

        float curY = bounds.y;
        if (geom.hasLabel) {
            geom.labelRect = Rect(blockX, curY, blockW, labelH);
            curY += labelH + (geom.hasDesc ? spec.descGap : 0.0f);
        }
        if (geom.hasDesc) {
            geom.descRect = Rect(blockX, curY, blockW, descH);
            curY += descH;
        }
        curY += spec.labelGap;
        geom.controlRect = Rect(ctrlX, curY, ctrlW, ctrlH);
    } else if (spec.layout == ElementLayout::Bottom) {
        const float blockW = compactNaturalBlock ? std::min(bounds.w, std::max(ctrlW, measuredTextW)) : bounds.w;
        const float labelH = resolveLabelH(blockW);
        const float descH = geom.hasDesc
            ? TextUtil::MeasureUiTextHeight(spec.description, blockW, geom.descFontSize, descLineH, canvas)
            : 0.0f;
        const float textTotalH = spec.descGap + (geom.hasLabel ? labelH : 0.0f) + (geom.hasDesc ? (geom.hasLabel ? spec.descGap : 0.0f) + descH : 0.0f);
        const float availCtrlH = std::max(0.0f, bounds.h - textTotalH);
        const float ctrlH = stretchStandaloneH ? availCtrlH : std::min(intrinsicControlSize.y, availCtrlH);
        const float blockX = bounds.x;
        const float ctrlX = blockX + (blockW - ctrlW) * alignFactor;

        float curY = bounds.y;
        geom.controlRect = Rect(ctrlX, curY, ctrlW, ctrlH);
        curY += geom.controlRect.h + spec.descGap;
        if (geom.hasLabel) {
            geom.labelRect = Rect(blockX, curY, blockW, labelH);
            curY += labelH + (geom.hasDesc ? spec.descGap : 0.0f);
        }
        if (geom.hasDesc) {
            geom.descRect = Rect(blockX, curY, blockW, descH);
        }
    } else { // ElementLayout::Inline
        const float gap = spec.inlineGap;
        if (stretchableCtrl && spec.stretch == StretchMode::Fill && !ctrlSpec.width.has_value()) {
            ctrlW = std::max(0.0f, bounds.w - measuredTextW - gap);
        }
        const float availTextW = std::max(0.0f, bounds.w - ctrlW - gap);
        const float textBlockW = (spec.label.wrap.IsWordWrap() || spec.description.wrap.IsWordWrap())
            ? (compactNaturalBlock ? std::min(measuredTextW, availTextW) : availTextW)
            : std::min(measuredTextW, availTextW);

        const float labelH = resolveLabelH(textBlockW);
        const float descH = geom.hasDesc
            ? TextUtil::MeasureUiTextHeight(spec.description, textBlockW, geom.descFontSize, descLineH, canvas)
            : 0.0f;

        const float totalTextH = (geom.hasLabel && geom.hasDesc) ? (labelH + spec.descGap + descH) : (geom.hasLabel ? labelH : descH);
        const float startY = bounds.y + (bounds.h - totalTextH) * 0.5f;
        const float ctrlH = stretchStandaloneH ? bounds.h : std::min(intrinsicControlSize.y, bounds.h);

        if (spec.inlineOrder == InlineOrder::LabelFirst) {
            float ctrlX = bounds.x + bounds.w - ctrlW;
            if (compactNaturalBlock) {
                ctrlX = bounds.x + (textBlockW > 0.0f ? textBlockW + gap : 0.0f);
            }
            geom.controlRect = Rect(ctrlX, bounds.y + (bounds.h - ctrlH) * 0.5f, ctrlW, ctrlH);

            float textY = startY;
            if (geom.hasLabel) {
                geom.labelRect = Rect(bounds.x, textY, textBlockW, labelH);
                textY += labelH + spec.descGap;
            }
            if (geom.hasDesc) {
                geom.descRect = Rect(bounds.x, textY, textBlockW, descH);
            }
        } else {
            // InlineOrder::ControlFirst
            geom.controlRect = Rect(bounds.x, bounds.y + (bounds.h - ctrlH) * 0.5f, ctrlW, ctrlH);

            float textX = bounds.x + ctrlW + gap;
            if (!compactNaturalBlock) {
                textX = std::max(bounds.x + ctrlW + gap, bounds.x + bounds.w - textBlockW);
            }
            float textY = startY;
            if (geom.hasLabel) {
                geom.labelRect = Rect(textX, textY, textBlockW, labelH);
                textY += labelH + spec.descGap;
            }
            if (geom.hasDesc) {
                geom.descRect = Rect(textX, textY, textBlockW, descH);
            }
        }
    }

    geom.innerControlRect = spec.hasControl
        ? Control::ResolveInnerRect(geom.controlRect, ctrlSpec, spec.widgetSize, theme)
        : geom.controlRect;
    return geom;
}

void Element::RenderFrame(
    ICanvas& canvas,
    const ElementGeometry& geom,
    const ElementSpec& spec,
    const UiTheme& theme
) noexcept {
    if (!HasOuterFrame(spec) || geom.outerBounds.w <= 0.0f || geom.outerBounds.h <= 0.0f) {
        return;
    }

    const ComponentMetrics& cm = theme.GetMetrics(spec.widgetSize);
    const float radius = spec.border.radius.has_value()
        ? theme.Scale(*spec.border.radius)
        : cm.cornerRadius;

    const auto& pal = theme.palette;
    const Color resolvedBorderAccent = spec.border.color.Resolve(
        spec.hovered,
        spec.active,
        spec.disabled,
        pal.borderSubtle,
        pal.borderHover,
        pal.focusRing,
        pal.borderSubtle.WithAlphaF(0.40f)
    );

    Color bg = Color::Transparent();
    if (spec.background.IsBackgroundEnabled(false)) {
        if (spec.background.HasAny() && !spec.background.IsSurface()) {
            bg = spec.background.Resolve(
                spec.hovered,
                spec.active,
                spec.disabled,
                pal.surface,
                pal.surfaceHover,
                pal.surfaceActive,
                pal.surface.WithAlphaF(0.40f)
            );
        } else {
            bg = pal.surface;
            if (spec.border.color.HasAny()) {
                bg = Color(
                    static_cast<uint8_t>((static_cast<int>(bg.r) * 90 + static_cast<int>(resolvedBorderAccent.r) * 10) / 100),
                    static_cast<uint8_t>((static_cast<int>(bg.g) * 90 + static_cast<int>(resolvedBorderAccent.g) * 10) / 100),
                    static_cast<uint8_t>((static_cast<int>(bg.b) * 90 + static_cast<int>(resolvedBorderAccent.b) * 10) / 100),
                    255
                );
            }
            if (spec.disabled) {
                bg = bg.WithAlphaF(0.40f);
            }
        }
    }

    const float strokeW = (spec.border.width > 0.0f) ? theme.Scale(spec.border.width) : 0.0f;
    Color borderCol = Color::Transparent();
    if (strokeW > 0.0f) {
        borderCol = resolvedBorderAccent;
    }

    canvas.DrawRoundRect(geom.outerBounds, radius, bg, borderCol, strokeW);
}

void Element::RenderText(
    ICanvas& canvas,
    const ElementGeometry& geom,
    const ElementSpec& spec,
    const UiTheme& theme
) noexcept {
    RenderFrame(canvas, geom, spec, theme);

    const float alignFactor = (spec.align == Align::End) ? 1.0f : ((spec.align == Align::Center) ? 0.5f : 0.0f);
    const UiVariant defaultLabelVar = UiVariant::Default;
    const UiVariant defaultDescVar = spec.headingTypography ? UiVariant::Secondary : UiVariant::Muted;

    const Color labelCol = theme.ResolveTextColor(spec.label, defaultLabelVar, spec.disabled, spec.hovered, spec.active);
    const Color descCol = theme.ResolveTextColor(spec.description, defaultDescVar, spec.disabled, spec.hovered, spec.active);
    const HeadingMetrics& m = theme.GetHeading(spec.widgetSize);
    const auto& pal = theme.palette;
    const UiSize badgeSize = (spec.badge.size.preset != UiSize::None) ? spec.badge.size.preset : spec.widgetSize;
    const float badgeFont = (spec.badge.size.px > 0.0f)
        ? spec.badge.size.px
        : (spec.badge.size.role.has_value()
            ? theme.GetTypography(*spec.badge.size.role).fontSize
            : theme.GetHeading(badgeSize).badgeFontSize);

    const bool hasDecoratedLabelBar = !spec.hasControl || spec.icon.IsValid() || !spec.badge.empty();
    const Rect labelBarRect = spec.hasControl ? geom.labelRect : geom.controlRect;
    const float topY = labelBarRect.y;
    const float titleLineH = labelBarRect.h;

    if (spec.hasControl && geom.hasLabel && !hasDecoratedLabelBar) {
        if (!spec.label.empty() && geom.labelRect.w > 0.0f && geom.labelRect.h > 0.0f) {
            const float lineH = canvas.GetLineHeight(geom.labelFontSize);
            TextUtil::DrawUiText(canvas, spec.label, geom.labelRect, geom.labelFontSize, lineH, labelCol, alignFactor, 0.5f);
        }
    } else if ((!spec.hasControl || geom.hasLabel) && labelBarRect.w > 0.0f && labelBarRect.h > 0.0f) {
        const Vec2 rawBar = MeasureLabelBar(spec, geom.labelFontSize, &canvas, theme);
        const float barW = std::min(labelBarRect.w, rawBar.x);
        float curX = labelBarRect.x + std::max(0.0f, labelBarRect.w - barW) * alignFactor;
        const float barLeftX = curX;

        if (spec.icon.IsValid()) {
            const float iconY = topY + (titleLineH - m.iconSize) * 0.5f;
            spec.icon.Draw(canvas, Rect(curX, iconY, m.iconSize, m.iconSize), labelCol);
            curX += m.iconSize + ((spec.label.HasContentOrReserve() || !spec.badge.empty()) ? m.gap : 0.0f);
        }

        if (!spec.label.empty()) {
            const float badgeReserveW = spec.badge.empty()
                ? 0.0f
                : (m.badgeGap + TextUtil::MeasureWidth(spec.badge, badgeFont, &canvas) + 12.0f);
            const float titleAvailW = std::max(0.0f, (barLeftX + barW) - curX - badgeReserveW);
            float titleW = canvas.MeasureText(spec.label.text, geom.labelFontSize).x;
            std::string displayTitle(spec.label.text);
            if (titleW > titleAvailW + kTextSubpixelTolerance && titleAvailW > 0.0f) {
                displayTitle = TextUtil::TruncateWithEllipsis(spec.label.text, titleAvailW, geom.labelFontSize, &canvas);
                titleW = canvas.MeasureText(displayTitle, geom.labelFontSize).x;
            }
            const Rect titleR(curX, topY, titleAvailW, titleLineH);
            canvas.DrawTextBox(displayTitle, titleR, geom.labelFontSize, labelCol, 0.0f, 0.5f);
            curX += titleW;
        }

        if (!spec.badge.empty()) {
            if (!spec.label.empty()) {
                curX += m.badgeGap;
            }
            const float badgeTextW = TextUtil::MeasureWidth(spec.badge, badgeFont, &canvas);
            const float badgePadH = 6.0f;
            const float badgeW = badgeTextW + badgePadH * 2.0f;
            const float badgeH = badgeFont + 4.0f;
            const float badgeY = topY + (titleLineH - badgeH) * 0.5f;

            Color badgeBg = pal.surfaceHover;
            Color badgeTextCol = pal.textPrimary;
            if (spec.badge.color.HasAny()) {
                const Color resolvedBadge = spec.badge.color.Resolve(spec.hovered, spec.active, spec.disabled, pal.textPrimary);
                badgeBg = resolvedBadge.WithAlphaF(spec.disabled ? 0.12f : 0.22f);
                badgeTextCol = resolvedBadge;
            } else if (spec.disabled) {
                badgeBg = badgeBg.WithAlphaF(0.40f);
                badgeTextCol = badgeTextCol.WithAlphaF(0.40f);
            }

            canvas.DrawRoundRect(Rect(curX, badgeY, badgeW, badgeH), badgeH * 0.5f, badgeBg);
            canvas.DrawTextBox(spec.badge.text, Rect(curX, badgeY, badgeW, badgeH), badgeFont, badgeTextCol, 0.5f, 0.5f);
            curX += badgeW;
        }
    }

    if (geom.hasDesc && !spec.description.empty() && geom.descRect.w > 0.0f && geom.descRect.h > 0.0f) {
        const float lineH = canvas.GetLineHeight(geom.descFontSize);
        TextUtil::DrawUiText(canvas, spec.description, geom.descRect, geom.descFontSize, lineH, descCol, alignFactor, 0.5f);
    }
}

} // namespace guikit
