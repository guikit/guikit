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

float ResolveLabelFontSize(const ElementSpec& spec, const UiTheme& theme) noexcept {
    if (spec.label.role.has_value()) {
        return theme.GetTypography(*spec.label.role).fontSize;
    }
    if (spec.headingTypography) {
        return theme.GetHeading(spec.widgetSize).titleFontSize;
    }
    return theme.ResolveFontSize(spec.label, TextRole::LabelLarge, spec.widgetSize);
}

float ResolveDescFontSize(const ElementSpec& spec, const UiTheme& theme) noexcept {
    if (spec.description.role.has_value()) {
        return theme.GetTypography(*spec.description.role).fontSize;
    }
    if (spec.headingTypography) {
        return theme.GetHeading(spec.widgetSize).subtitleFontSize;
    }
    return theme.ResolveFontSize(spec.description, TextRole::LabelSmall, spec.widgetSize);
}

bool HasLabelBar(const ElementSpec& spec) noexcept {
    return spec.icon.IsValid() || spec.label.HasContentOrReserve() || spec.badge.HasContentOrReserve();
}

Vec2 MeasureLabelBar(
    const ElementSpec& spec,
    float labelFontSize,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    const HeadingMetrics& m = theme.GetHeading(spec.widgetSize);
    const float badgeFont = spec.badge.role.has_value()
        ? theme.GetTypography(*spec.badge.role).fontSize
        : m.badgeFontSize;

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

float ComputeBottomDividerSpace(const ElementSpec& spec, const UiTheme& theme) noexcept {
    if (spec.divider && spec.dividerStyle == ElementDivider::Bottom) {
        const HeadingMetrics& m = theme.GetHeading(spec.widgetSize);
        const float divPad = std::max(4.0f, m.gap * 0.75f);
        return divPad + 1.0f + divPad * 0.5f;
    }
    return 0.0f;
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
    const float strokePad = (spec.borderWidth > 0.0f) ? theme.Scale(spec.borderWidth) : 0.0f;
    return basePad + strokePad;
}

ElementSpec Element::MakeSpec(
    const ElementOptions& opts,
    UiSize activeSize,
    bool isDisabled,
    const UiTheme& theme
) noexcept {
    const HeadingMetrics& m = theme.GetHeading(activeSize);
    UiText subSpec = opts.EffectiveSubtitle();
    if (opts.wordWrap && subSpec.HasContentOrReserve()) {
        subSpec.wrap = TextWrap::Word;
    }
    return ElementSpec{
        .label = opts.EffectiveTitle(),
        .description = subSpec,
        .icon = opts.icon,
        .badge = opts.badge,
        .variant = opts.variant,
        .layout = opts.layout,
        .inlineOrder = opts.inlineOrder,
        .elementAlign = opts.elementAlign,
        .stretch = opts.stretch,
        .width = opts.width,
        .height = opts.height,
        .background = opts.background,
        .borderWidth = opts.borderWidth,
        .borderRadius = opts.borderRadius,
        .widgetSize = activeSize,
        .labelGap = m.lineGap,
        .descGap = m.lineGap,
        .inlineGap = m.gap * 1.5f,
        .hasControl = false,
        .headingTypography = opts.headingTypography,
        .divider = opts.divider,
        .dividerStyle = opts.dividerStyle,
        .stretchableControl = false,
        .stretchHeightWhenStandalone = false,
        .disabled = isDisabled
    };
}

Response Element::Run(
    Context& ctx,
    const ElementOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size.value_or(ctx.GetCurrentSize());
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();

    Response res = ctx.Interact(id);
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
    }

    Node* node = ctx.CreateNode(NodeType::Heading, id);
    auto* eOpts = ctx.AllocPayload<ElementOptions>(opts);
    eOpts->title = ctx.AllocText(opts.title);
    eOpts->subtitle = ctx.AllocText(opts.subtitle);
    eOpts->label = ctx.AllocText(opts.label);
    eOpts->description = ctx.AllocText(opts.description);
    eOpts->badge = ctx.AllocText(opts.badge);
    node->text = eOpts->EffectiveTitle().text;
    node->SetPayload(eOpts);
    node->visualHash = detail::HashArgs(
        opts.variant,
        static_cast<uint8_t>(opts.layout),
        static_cast<uint8_t>(opts.inlineOrder),
        static_cast<uint8_t>(opts.elementAlign),
        static_cast<uint8_t>(opts.stretch),
        opts.width.value_or(0.0f),
        opts.height.value_or(0.0f),
        opts.background,
        opts.borderWidth,
        opts.borderRadius.value_or(-1.0f),
        eOpts->EffectiveTitle(),
        eOpts->EffectiveSubtitle(),
        eOpts->badge,
        opts.divider,
        static_cast<uint8_t>(opts.dividerStyle),
        opts.wordWrap,
        opts.headingTypography,
        effectiveDisabled,
        static_cast<uint8_t>(opts.icon.GetType()),
        static_cast<uint8_t>(opts.icon.GetId()),
        opts.icon.GetPath()
    );
    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* opts = n->As<ElementOptions>();
        return Element::Measure(opts ? *opts : ElementOptions{}, n->uiSize, canvas, theme, availableWidth);
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* opts = n->As<ElementOptions>();
        Element::Render(
            canvas,
            n->computedRect,
            opts ? *opts : ElementOptions{},
            n->uiSize,
            theme,
            n->disabled
        );
    };

    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed;
    node->widthMode = (opts.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

    ctx.AdaptChildToContainer(node);

    return res;
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
        const float pct = std::clamp(*ctrlSpec.width, 0.0f, 100.0f);
        availableWidth *= (pct * 0.01f);
    }

    const float bottomDivSpace = ComputeBottomDividerSpace(spec, theme);

    const auto applyFixedWidthAndDiv = [&](Vec2 measured) noexcept -> Vec2 {
        if (!stretchableCtrl && ctrlSpec.width.has_value() && spec.stretch != StretchMode::Fill) {
            measured.x = std::max(measured.x, theme.Scale(*ctrlSpec.width));
        }
        measured.y += bottomDivSpace;
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
        return applyFixedWidthAndDiv(intrinsicControlSize);
    }

    const Vec2 rawLabelBar = hasLabel ? MeasureLabelBar(spec, labelFontSize, canvas, theme) : Vec2{0.0f, 0.0f};
    const float rawLabelW = rawLabelBar.x;
    const float rawDescW = hasDesc ? TextUtil::MeasureWidth(spec.description, descFontSize, canvas) : 0.0f;

    float effLabelW = rawLabelW;
    float effDescW = rawDescW;
    if (spec.layout != ElementLayout::Inline && spec.stretch == StretchMode::Compact && stretchableCtrl) {
        if (spec.label.wrap == TextWrap::Word) {
            effLabelW = std::min(rawLabelW, intrinsicControlSize.x);
        }
        const float baseW = std::max(intrinsicControlSize.x, effLabelW);
        if (spec.description.wrap == TextWrap::Word) {
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
                const float pct = std::clamp(*ctrlSpec.width, 0.0f, 100.0f);
                ctrlW = textWrapW * (pct * 0.01f);
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
        return applyFixedWidthAndDiv({totalW, totalH});
    }

    if (spec.layout == ElementLayout::Top) {
        const float totalW = std::max({intrinsicControlSize.x, labelDims.x, descDims.x});
        float textH = 0.0f;
        if (hasLabel) textH += labelDims.y;
        if (hasDesc)  textH += (hasLabel ? spec.descGap : 0.0f) + descDims.y;
        return applyFixedWidthAndDiv({totalW, textH + spec.labelGap + intrinsicControlSize.y});
    }

    if (spec.layout == ElementLayout::Bottom) {
        const float totalW = std::max({intrinsicControlSize.x, labelDims.x, descDims.x});
        float textH = 0.0f;
        if (hasLabel) textH += labelDims.y;
        if (hasDesc)  textH += (hasLabel ? spec.descGap : 0.0f) + descDims.y;
        return applyFixedWidthAndDiv({totalW, intrinsicControlSize.y + spec.descGap + textH});
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
    return applyFixedWidthAndDiv({totalW, totalH});
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

    const float bottomDivSpace = ComputeBottomDividerSpace(spec, theme);
    if (bottomDivSpace > 0.0f) {
        bounds.h = std::max(0.0f, bounds.h - bottomDivSpace);
    }

    const bool stretchableCtrl = spec.hasControl ? ctrlSpec.stretchable : false;
    const bool stretchStandaloneH = spec.hasControl ? ctrlSpec.stretchHeightWhenStandalone : false;

    geom.hasLabel = spec.hasControl && HasLabelBar(spec);
    geom.hasDesc = spec.description.HasContentOrReserve();
    geom.hasExternal = geom.hasLabel || geom.hasDesc;

    const float alignFactor = (spec.elementAlign == Align::End) ? 1.0f : ((spec.elementAlign == Align::Center) ? 0.5f : 0.0f);

    if (!stretchableCtrl && spec.stretch == StretchMode::Fill && ctrlSpec.width.has_value()) {
        const float pct = std::clamp(*ctrlSpec.width, 0.0f, 100.0f);
        const float effW = bounds.w * (pct * 0.01f);
        bounds.x += (bounds.w - effW) * alignFactor;
        bounds.w = effW;
    }

    float ctrlW = std::min(bounds.w, intrinsicControlSize.x);
    if (stretchableCtrl && spec.stretch == StretchMode::Fill) {
        if (!geom.hasExternal || spec.layout != ElementLayout::Inline || ctrlSpec.width.has_value()) {
            const float pct = std::clamp(ctrlSpec.width.value_or(100.0f), 0.0f, 100.0f);
            ctrlW = bounds.w * (pct * 0.01f);
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
        if (spec.label.wrap == TextWrap::Word) {
            effLabelW = std::min(effLabelW, ctrlW);
        }
        const float baseW = std::max(ctrlW, effLabelW);
        if (spec.description.wrap == TextWrap::Word) {
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
        const float textBlockW = (spec.label.wrap == TextWrap::Word || spec.description.wrap == TextWrap::Word)
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
    const float radius = spec.borderRadius.has_value()
        ? theme.Scale(*spec.borderRadius)
        : cm.cornerRadius;

    Color bg = Color::Transparent();
    if (spec.background) {
        bg = theme.palette.surface;
        if (spec.variant != UiVariant::Default && spec.variant != UiVariant::Ghost && spec.variant != UiVariant::Muted) {
            const Color accent = theme.GetVariant(spec.variant).bg;
            bg = Color(
                static_cast<uint8_t>((static_cast<int>(bg.r) * 90 + static_cast<int>(accent.r) * 10) / 100),
                static_cast<uint8_t>((static_cast<int>(bg.g) * 90 + static_cast<int>(accent.g) * 10) / 100),
                static_cast<uint8_t>((static_cast<int>(bg.b) * 90 + static_cast<int>(accent.b) * 10) / 100),
                255
            );
        }
        if (spec.disabled) {
            bg = bg.WithAlphaF(0.40f);
        }
    }

    const float strokeW = (spec.borderWidth > 0.0f) ? theme.Scale(spec.borderWidth) : 0.0f;
    Color borderCol = Color::Transparent();
    if (strokeW > 0.0f) {
        borderCol = (spec.variant != UiVariant::Default && spec.variant != UiVariant::Ghost && spec.variant != UiVariant::Muted)
            ? theme.GetVariant(spec.variant).bg.WithAlphaF(0.55f)
            : theme.palette.borderSubtle;
        if (spec.disabled) {
            borderCol = borderCol.WithAlphaF(0.40f);
        }
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

    const ControlSpec ctrlSpec = EffectiveControlSpec(spec);
    if (spec.hasControl) {
        Control::RenderFrame(canvas, geom.controlRect, ctrlSpec, spec.variant, spec.widgetSize, spec.disabled, theme);
    }

    const float alignFactor = (spec.elementAlign == Align::End) ? 1.0f : ((spec.elementAlign == Align::Center) ? 0.5f : 0.0f);
    const UiVariant defaultLabelVar = (!spec.hasControl && spec.variant != UiVariant::Default) ? spec.variant : UiVariant::Default;
    const UiVariant defaultDescVar = spec.headingTypography ? UiVariant::Secondary : UiVariant::Muted;

    const Color labelCol = theme.ResolveTextColor(spec.label, defaultLabelVar, spec.disabled);
    const Color descCol = theme.ResolveTextColor(spec.description, defaultDescVar, spec.disabled);
    const HeadingMetrics& m = theme.GetHeading(spec.widgetSize);
    const auto& pal = theme.palette;
    const float badgeFont = spec.badge.role.has_value()
        ? theme.GetTypography(*spec.badge.role).fontSize
        : m.badgeFontSize;

    const bool hasDecoratedLabelBar = !spec.hasControl || spec.icon.IsValid() || !spec.badge.empty();
    const Rect labelBarRect = spec.hasControl ? geom.labelRect : geom.controlRect;
    float barLeftX = labelBarRect.x;
    float barRightX = labelBarRect.x;
    const float topY = labelBarRect.y;
    const float titleLineH = labelBarRect.h;

    if (spec.hasControl && geom.hasLabel && !hasDecoratedLabelBar) {
        if (!spec.label.empty() && geom.labelRect.w > 0.0f && geom.labelRect.h > 0.0f) {
            const float lineH = canvas.GetLineHeight(geom.labelFontSize);
            TextUtil::DrawUiText(canvas, spec.label, geom.labelRect, geom.labelFontSize, lineH, labelCol, alignFactor, 0.5f);
            const float textW = std::min(geom.labelRect.w, canvas.MeasureText(spec.label.text, geom.labelFontSize).x);
            barLeftX = geom.labelRect.x + (geom.labelRect.w - textW) * alignFactor;
            barRightX = barLeftX + textW;
        }
    } else if ((!spec.hasControl || geom.hasLabel) && labelBarRect.w > 0.0f && labelBarRect.h > 0.0f) {
        const Vec2 rawBar = MeasureLabelBar(spec, geom.labelFontSize, &canvas, theme);
        const float barW = std::min(labelBarRect.w, rawBar.x);
        float curX = labelBarRect.x + std::max(0.0f, labelBarRect.w - barW) * alignFactor;
        barLeftX = curX;

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

            const UiVariant badgeVar = spec.badge.variant.value_or(spec.variant);
            Color badgeBg = (badgeVar != UiVariant::Default)
                ? theme.GetVariant(badgeVar).bgActive
                : pal.surfaceHover;
            Color badgeTextCol = (badgeVar != UiVariant::Default)
                ? theme.GetVariant(badgeVar).text
                : pal.textPrimary;
            if (spec.disabled) {
                badgeBg = badgeBg.WithAlphaF(0.40f);
                badgeTextCol = badgeTextCol.WithAlphaF(0.40f);
            }

            canvas.DrawRoundRect(Rect(curX, badgeY, badgeW, badgeH), badgeH * 0.5f, badgeBg);
            canvas.DrawTextBox(spec.badge.text, Rect(curX, badgeY, badgeW, badgeH), badgeFont, badgeTextCol, 0.5f, 0.5f);
            curX += badgeW;
        }
        barRightX = curX;
    }

    if (geom.hasDesc && !spec.description.empty() && geom.descRect.w > 0.0f && geom.descRect.h > 0.0f) {
        const float lineH = canvas.GetLineHeight(geom.descFontSize);
        TextUtil::DrawUiText(canvas, spec.description, geom.descRect, geom.descFontSize, lineH, descCol, alignFactor, 0.5f);
    }

    if (spec.divider) {
        Color divColor = (spec.variant != UiVariant::Default)
            ? theme.GetVariant(spec.variant).border
            : pal.borderSubtle;
        if (spec.disabled) {
            divColor = divColor.WithAlphaF(0.40f);
        }

        const float outerPad = ComputeOuterPadding(spec, theme);
        const Rect outer = geom.outerBounds.Inset(outerPad);
        float spanX = outer.x;
        float spanW = outer.w;
        if (spec.stretch == StretchMode::Fill && ctrlSpec.width.has_value()) {
            const float pct = std::clamp(*ctrlSpec.width, 0.0f, 100.0f);
            spanW = outer.w * (pct * 0.01f);
            spanX = outer.x + (outer.w - spanW) * alignFactor;
        } else if (spec.stretch == StretchMode::Compact) {
            const float maxElemW = std::max({
                geom.controlRect.w,
                geom.hasLabel ? geom.labelRect.w : 0.0f,
                geom.hasDesc ? geom.descRect.w : 0.0f
            });
            const float natSpanW = ctrlSpec.width.has_value()
                ? std::min(outer.w, theme.Scale(*ctrlSpec.width))
                : std::min(outer.w, maxElemW);
            spanW = natSpanW;
            spanX = outer.x + (outer.w - spanW) * alignFactor;
        }

        if (spec.dividerStyle == ElementDivider::Bottom) {
            const float divPad = std::max(4.0f, m.gap * 0.75f);
            float contentBottom = geom.controlRect.Bottom();
            if (geom.hasLabel) contentBottom = std::max(contentBottom, geom.labelRect.Bottom());
            if (geom.hasDesc)  contentBottom = std::max(contentBottom, geom.descRect.Bottom());
            const float divY = std::floor(contentBottom + divPad) + 0.5f;
            canvas.DrawLine(
                Vec2(spanX, divY),
                Vec2(spanX + spanW, divY),
                divColor,
                1.0f
            );
        } else if (spec.dividerStyle == ElementDivider::Trailing && labelBarRect.h > 0.0f) {
            const float lineY = std::floor(topY + titleLineH * 0.5f) + 0.5f;
            const float leftEnd = barLeftX - m.gap;
            const float rightStart = barRightX + m.gap;
            const float spanRight = spanX + spanW;

            if (leftEnd > spanX + 4.0f) {
                canvas.DrawLine(Vec2(spanX, lineY), Vec2(leftEnd, lineY), divColor, 1.0f);
            }
            if (spanRight > rightStart + 4.0f) {
                canvas.DrawLine(Vec2(rightStart, lineY), Vec2(spanRight, lineY), divColor, 1.0f);
            }
        }
    }
}

} // namespace guikit
