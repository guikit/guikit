#include "guikit/core/ElementComposer.hpp"
#include <algorithm>
#include <string>
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/TextUtil.hpp"

namespace guikit {

Vec2 ElementComposer::Measure(
    Vec2 intrinsicControlSize,
    const ElementSpec& spec,
    ICanvas* canvas,
    const UiTheme& theme,
    float availableWidth
) noexcept {
    if (!spec.stretchableControl && spec.width.has_value() && spec.stretch == StretchMode::Fill && availableWidth > 0.0f) {
        const float pct = std::clamp(*spec.width, 0.0f, 100.0f);
        availableWidth *= (pct * 0.01f);
    }

    const auto applyFixedWidthIfNeeded = [&](Vec2 measured) noexcept -> Vec2 {
        if (!spec.stretchableControl && spec.width.has_value() && spec.stretch != StretchMode::Fill) {
            measured.x = std::max(measured.x, theme.Scale(*spec.width));
        }
        return measured;
    };

    const bool hasLabel = spec.label.HasContentOrReserve();
    const bool hasDesc = spec.description.HasContentOrReserve();
    const bool hasExternal = hasLabel || hasDesc;

    if (!hasExternal) {
        return applyFixedWidthIfNeeded(intrinsicControlSize);
    }

    const float labelFontSize = theme.ResolveFontSize(spec.label, TextRole::LabelLarge, spec.widgetSize);
    const float descFontSize = theme.ResolveFontSize(spec.description, TextRole::LabelSmall, spec.widgetSize);

    const float rawLabelW = hasLabel ? TextUtil::MeasureWidth(spec.label, labelFontSize, canvas) : 0.0f;
    const float rawDescW = hasDesc ? TextUtil::MeasureWidth(spec.description, descFontSize, canvas) : 0.0f;

    float effLabelW = rawLabelW;
    float effDescW = rawDescW;
    if (spec.layout != ElementLayout::Inline && spec.stretch == StretchMode::Compact && spec.stretchableControl) {
        if (spec.label.wrap == TextWrap::Word) {
            effLabelW = std::min(rawLabelW, intrinsicControlSize.x);
        }
        const float baseW = std::max(intrinsicControlSize.x, effLabelW);
        if (spec.description.wrap == TextWrap::Word) {
            effDescW = std::min(rawDescW, baseW);
        }
    }

    float textWrapW = availableWidth;
    if (!spec.stretchableControl && spec.width.has_value() && spec.stretch != StretchMode::Fill) {
        const float fixedW = theme.Scale(*spec.width);
        textWrapW = (availableWidth > 0.0f) ? std::min(availableWidth, fixedW) : fixedW;
    }
    if (spec.layout == ElementLayout::Inline) {
        if (textWrapW > 0.0f) {
            float ctrlW = std::min(textWrapW, intrinsicControlSize.x);
            if (spec.stretchableControl && spec.stretch == StretchMode::Fill && spec.width.has_value()) {
                const float pct = std::clamp(*spec.width, 0.0f, 100.0f);
                ctrlW = textWrapW * (pct * 0.01f);
            }
            textWrapW = std::max(0.0f, textWrapW - ctrlW - spec.inlineGap);
        }
    } else if (spec.stretch == StretchMode::Compact && spec.stretchableControl) {
        const float compactBlockW = std::max({intrinsicControlSize.x, effLabelW, effDescW});
        textWrapW = (availableWidth > 0.0f) ? std::min(availableWidth, compactBlockW) : compactBlockW;
    }

    Vec2 labelDims = {0.0f, 0.0f};
    if (hasLabel) {
        const float lineH = canvas ? canvas->GetLineHeight(labelFontSize) : theme.GetLineHeight(labelFontSize);
        const float h = TextUtil::MeasureUiTextHeight(spec.label, textWrapW, labelFontSize, lineH, canvas);
        labelDims = {effLabelW, h};
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
        return applyFixedWidthIfNeeded({totalW, totalH});
    }

    if (spec.layout == ElementLayout::Top) {
        const float totalW = std::max({intrinsicControlSize.x, labelDims.x, descDims.x});
        float textH = 0.0f;
        if (hasLabel) textH += labelDims.y;
        if (hasDesc)  textH += (hasLabel ? spec.descGap : 0.0f) + descDims.y;
        return applyFixedWidthIfNeeded({totalW, textH + spec.labelGap + intrinsicControlSize.y});
    }

    if (spec.layout == ElementLayout::Bottom) {
        const float totalW = std::max({intrinsicControlSize.x, labelDims.x, descDims.x});
        float textH = 0.0f;
        if (hasLabel) textH += labelDims.y;
        if (hasDesc)  textH += (hasLabel ? spec.descGap : 0.0f) + descDims.y;
        return applyFixedWidthIfNeeded({totalW, intrinsicControlSize.y + spec.descGap + textH});
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
    return applyFixedWidthIfNeeded({totalW, totalH});
}

ElementGeometry ElementComposer::Resolve(
    Rect bounds,
    Vec2 intrinsicControlSize,
    const ElementSpec& spec,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    ElementGeometry geom{};
    geom.hasLabel = spec.label.HasContentOrReserve();
    geom.hasDesc = spec.description.HasContentOrReserve();
    geom.hasExternal = geom.hasLabel || geom.hasDesc;
    geom.labelFontSize = theme.ResolveFontSize(spec.label, TextRole::LabelLarge, spec.widgetSize);
    geom.descFontSize = theme.ResolveFontSize(spec.description, TextRole::LabelSmall, spec.widgetSize);

    const float alignFactor = (spec.elementAlign == Align::End) ? 1.0f : ((spec.elementAlign == Align::Center) ? 0.5f : 0.0f);

    if (!spec.stretchableControl && spec.stretch == StretchMode::Fill && spec.width.has_value()) {
        const float pct = std::clamp(*spec.width, 0.0f, 100.0f);
        const float effW = bounds.w * (pct * 0.01f);
        bounds.x += (bounds.w - effW) * alignFactor;
        bounds.w = effW;
    }

    float ctrlW = std::min(bounds.w, intrinsicControlSize.x);
    if (spec.stretchableControl && spec.stretch == StretchMode::Fill) {
        if (!geom.hasExternal || spec.layout != ElementLayout::Inline || spec.width.has_value()) {
            const float pct = std::clamp(spec.width.value_or(100.0f), 0.0f, 100.0f);
            ctrlW = bounds.w * (pct * 0.01f);
        }
    }

    if (!geom.hasExternal) {
        const float ctrlX = bounds.x + (bounds.w - ctrlW) * alignFactor;
        const float ctrlH = spec.stretchHeightWhenStandalone ? bounds.h : intrinsicControlSize.y;
        const float ctrlY = spec.stretchHeightWhenStandalone ? bounds.y : (bounds.y + (bounds.h - ctrlH) * 0.5f);
        geom.controlRect = Rect(ctrlX, ctrlY, ctrlW, ctrlH);
        return geom;
    }

    const float labelLineH = geom.hasLabel
        ? (canvas ? canvas->GetLineHeight(geom.labelFontSize) : theme.GetLineHeight(geom.labelFontSize))
        : 0.0f;
    const float descLineH = geom.hasDesc
        ? (canvas ? canvas->GetLineHeight(geom.descFontSize) : theme.GetLineHeight(geom.descFontSize))
        : 0.0f;

    if (geom.hasLabel) {
        geom.measuredLabelW = TextUtil::MeasureWidth(spec.label, geom.labelFontSize, canvas);
    }

    if (geom.hasDesc) {
        geom.measuredDescW = TextUtil::MeasureWidth(spec.description, geom.descFontSize, canvas);
    }

    float effLabelW = geom.measuredLabelW;
    float effDescW = geom.measuredDescW;
    if (spec.layout != ElementLayout::Inline && spec.stretch == StretchMode::Compact && spec.stretchableControl) {
        if (spec.label.wrap == TextWrap::Word) {
            effLabelW = std::min(effLabelW, ctrlW);
        }
        const float baseW = std::max(ctrlW, effLabelW);
        if (spec.description.wrap == TextWrap::Word) {
            effDescW = std::min(effDescW, baseW);
        }
    }

    const float measuredTextW = std::max(effLabelW, effDescW);
    const bool compactNaturalBlock = (spec.stretch == StretchMode::Compact) && (spec.stretchableControl || !spec.width.has_value());

    if (spec.layout == ElementLayout::Stacked) {
        const float blockW = compactNaturalBlock ? std::min(bounds.w, std::max(ctrlW, measuredTextW)) : bounds.w;
        const float labelH = geom.hasLabel
            ? TextUtil::MeasureUiTextHeight(spec.label, blockW, geom.labelFontSize, labelLineH, canvas)
            : 0.0f;
        const float descH = geom.hasDesc
            ? TextUtil::MeasureUiTextHeight(spec.description, blockW, geom.descFontSize, descLineH, canvas)
            : 0.0f;
        const float availCtrlH = std::max(0.0f, bounds.h - (geom.hasLabel ? labelH + spec.labelGap : 0.0f) - (geom.hasDesc ? descH + spec.descGap : 0.0f));
        const float ctrlH = spec.stretchHeightWhenStandalone ? availCtrlH : intrinsicControlSize.y;
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
        const float labelH = geom.hasLabel
            ? TextUtil::MeasureUiTextHeight(spec.label, blockW, geom.labelFontSize, labelLineH, canvas)
            : 0.0f;
        const float descH = geom.hasDesc
            ? TextUtil::MeasureUiTextHeight(spec.description, blockW, geom.descFontSize, descLineH, canvas)
            : 0.0f;
        const float textTotalH = (geom.hasLabel ? labelH : 0.0f) + (geom.hasDesc ? (geom.hasLabel ? spec.descGap : 0.0f) + descH : 0.0f) + spec.labelGap;
        const float ctrlH = spec.stretchHeightWhenStandalone ? std::max(0.0f, bounds.h - textTotalH) : intrinsicControlSize.y;
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
        const float labelH = geom.hasLabel
            ? TextUtil::MeasureUiTextHeight(spec.label, blockW, geom.labelFontSize, labelLineH, canvas)
            : 0.0f;
        const float descH = geom.hasDesc
            ? TextUtil::MeasureUiTextHeight(spec.description, blockW, geom.descFontSize, descLineH, canvas)
            : 0.0f;
        const float textTotalH = spec.descGap + (geom.hasLabel ? labelH : 0.0f) + (geom.hasDesc ? (geom.hasLabel ? spec.descGap : 0.0f) + descH : 0.0f);
        const float ctrlH = spec.stretchHeightWhenStandalone ? std::max(0.0f, bounds.h - textTotalH) : intrinsicControlSize.y;
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
        if (spec.stretchableControl && spec.stretch == StretchMode::Fill && !spec.width.has_value()) {
            ctrlW = std::max(0.0f, bounds.w - measuredTextW - gap);
        }
        const float availTextW = std::max(0.0f, bounds.w - ctrlW - gap);
        const float textBlockW = (spec.label.wrap == TextWrap::Word || spec.description.wrap == TextWrap::Word)
            ? (compactNaturalBlock ? std::min(measuredTextW, availTextW) : availTextW)
            : std::min(measuredTextW, availTextW);

        const float labelH = geom.hasLabel
            ? TextUtil::MeasureUiTextHeight(spec.label, textBlockW, geom.labelFontSize, labelLineH, canvas)
            : 0.0f;
        const float descH = geom.hasDesc
            ? TextUtil::MeasureUiTextHeight(spec.description, textBlockW, geom.descFontSize, descLineH, canvas)
            : 0.0f;

        const float totalTextH = (geom.hasLabel && geom.hasDesc) ? (labelH + spec.descGap + descH) : (geom.hasLabel ? labelH : descH);
        const float startY = bounds.y + (bounds.h - totalTextH) * 0.5f;
        const float ctrlH = spec.stretchHeightWhenStandalone ? bounds.h : intrinsicControlSize.y;

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

    return geom;
}

void ElementComposer::RenderText(
    ICanvas& canvas,
    const ElementGeometry& geom,
    const ElementSpec& spec,
    const UiTheme& theme
) noexcept {
    if (!geom.hasExternal) {
        return;
    }

    const float alignFactor = (spec.elementAlign == Align::End) ? 1.0f : ((spec.elementAlign == Align::Center) ? 0.5f : 0.0f);
    const Color labelCol = theme.ResolveTextColor(spec.label, UiVariant::Default, spec.disabled);
    const Color descCol = theme.ResolveTextColor(spec.description, UiVariant::Muted, spec.disabled);

    if (geom.hasLabel && !spec.label.empty() && geom.labelRect.w > 0.0f && geom.labelRect.h > 0.0f) {
        const float lineH = canvas.GetLineHeight(geom.labelFontSize);
        TextUtil::DrawUiText(canvas, spec.label, geom.labelRect, geom.labelFontSize, lineH, labelCol, alignFactor, 0.5f);
    }

    if (geom.hasDesc && !spec.description.empty() && geom.descRect.w > 0.0f && geom.descRect.h > 0.0f) {
        const float lineH = canvas.GetLineHeight(geom.descFontSize);
        TextUtil::DrawUiText(canvas, spec.description, geom.descRect, geom.descFontSize, lineH, descCol, alignFactor, 0.5f);
    }
}

} // namespace guikit
