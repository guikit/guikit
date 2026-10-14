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
    const UiTheme& theme
) noexcept {
    const bool hasLabel = !spec.label.empty();
    const bool hasDesc = !spec.description.empty();
    const bool hasExternal = hasLabel || hasDesc;

    if (!hasExternal) {
        return intrinsicControlSize;
    }

    const float labelFontSize = theme.ResolveFontSize(spec.label, TextRole::LabelLarge, spec.widgetSize);
    const float descFontSize = theme.ResolveFontSize(spec.description, TextRole::LabelSmall, spec.widgetSize);

    Vec2 labelDims = {0.0f, 0.0f};
    if (hasLabel) {
        const float lineH = canvas ? canvas->GetLineHeight(labelFontSize) : theme.GetLineHeight(labelFontSize);
        const float w = canvas
            ? canvas->MeasureText(spec.label.text, labelFontSize).x
            : static_cast<float>(spec.label.length()) * (labelFontSize * kDefaultCharAdvanceRatio);
        labelDims = {w, lineH};
    }

    Vec2 descDims = {0.0f, 0.0f};
    if (hasDesc) {
        if (canvas) {
            descDims = canvas->MeasureText(spec.description.text, descFontSize);
        } else {
            descDims = {
                static_cast<float>(spec.description.length()) * (descFontSize * kDefaultCharAdvanceRatio),
                theme.GetLineHeight(descFontSize)
            };
        }
    }

    if (spec.layout == ElementLayout::Stacked) {
        const float totalW = std::max({intrinsicControlSize.x, labelDims.x, descDims.x});
        float totalH = intrinsicControlSize.y;
        if (hasLabel) totalH += labelDims.y + spec.labelGap;
        if (hasDesc)  totalH += spec.descGap + descDims.y;
        return {totalW, totalH};
    }

    if (spec.layout == ElementLayout::Top) {
        const float totalW = std::max({intrinsicControlSize.x, labelDims.x, descDims.x});
        float textH = 0.0f;
        if (hasLabel) textH += labelDims.y;
        if (hasDesc)  textH += (hasLabel ? spec.descGap : 0.0f) + descDims.y;
        return {totalW, textH + spec.labelGap + intrinsicControlSize.y};
    }

    if (spec.layout == ElementLayout::Bottom) {
        const float totalW = std::max({intrinsicControlSize.x, labelDims.x, descDims.x});
        float textH = 0.0f;
        if (hasLabel) textH += labelDims.y;
        if (hasDesc)  textH += (hasLabel ? spec.descGap : 0.0f) + descDims.y;
        return {totalW, intrinsicControlSize.y + spec.descGap + textH};
    }

    // ElementLayout::Inline
    const float textW = std::max(labelDims.x, descDims.x);
    float textH = 0.0f;
    if (hasLabel && hasDesc) {
        textH = labelDims.y + spec.descGap + descDims.y;
    } else if (hasLabel) {
        textH = labelDims.y;
    } else if (hasDesc) {
        textH = descDims.y;
    }
    const float totalW = intrinsicControlSize.x + (textW > 0.0f ? spec.inlineGap + textW : 0.0f);
    const float totalH = std::max(intrinsicControlSize.y, textH);
    return {totalW, totalH};
}

ElementGeometry ElementComposer::Resolve(
    Rect bounds,
    Vec2 intrinsicControlSize,
    const ElementSpec& spec,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    ElementGeometry geom{};
    geom.hasLabel = !spec.label.empty();
    geom.hasDesc = !spec.description.empty();
    geom.hasExternal = geom.hasLabel || geom.hasDesc;
    geom.labelFontSize = theme.ResolveFontSize(spec.label, TextRole::LabelLarge, spec.widgetSize);
    geom.descFontSize = theme.ResolveFontSize(spec.description, TextRole::LabelSmall, spec.widgetSize);

    const float alignFactor = (spec.elementAlign == Align::End) ? 1.0f : ((spec.elementAlign == Align::Center) ? 0.5f : 0.0f);

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

    const float labelH = geom.hasLabel
        ? (canvas ? canvas->GetLineHeight(geom.labelFontSize) : theme.GetLineHeight(geom.labelFontSize))
        : 0.0f;
    const float descH = geom.hasDesc
        ? (canvas ? canvas->GetLineHeight(geom.descFontSize) : theme.GetLineHeight(geom.descFontSize))
        : 0.0f;

    if (geom.hasLabel) {
        geom.measuredLabelW = canvas
            ? canvas->MeasureText(spec.label.text, geom.labelFontSize).x
            : static_cast<float>(spec.label.length()) * (geom.labelFontSize * kDefaultCharAdvanceRatio);
    }

    if (geom.hasDesc) {
        geom.measuredDescW = canvas
            ? canvas->MeasureText(spec.description.text, geom.descFontSize).x
            : static_cast<float>(spec.description.length()) * (geom.descFontSize * kDefaultCharAdvanceRatio);
    }

    const float measuredTextW = std::max(geom.measuredLabelW, geom.measuredDescW);

    if (spec.layout == ElementLayout::Stacked) {
        const float blockW = (spec.stretch == StretchMode::Compact) ? std::min(bounds.w, std::max(ctrlW, measuredTextW)) : bounds.w;
        const float blockX = bounds.x;
        const float ctrlX = blockX + (blockW - ctrlW) * alignFactor;

        float curY = bounds.y;
        if (geom.hasLabel) {
            geom.labelRect = Rect(blockX, curY, blockW, labelH);
            curY += labelH + spec.labelGap;
        }
        geom.controlRect = Rect(ctrlX, curY, ctrlW, intrinsicControlSize.y);
        curY += geom.controlRect.h + spec.descGap;
        if (geom.hasDesc) {
            geom.descRect = Rect(blockX, curY, blockW, descH);
        }
    } else if (spec.layout == ElementLayout::Top) {
        const float blockW = (spec.stretch == StretchMode::Compact) ? std::min(bounds.w, std::max(ctrlW, measuredTextW)) : bounds.w;
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
        geom.controlRect = Rect(ctrlX, curY, ctrlW, intrinsicControlSize.y);
    } else if (spec.layout == ElementLayout::Bottom) {
        const float blockW = (spec.stretch == StretchMode::Compact) ? std::min(bounds.w, std::max(ctrlW, measuredTextW)) : bounds.w;
        const float blockX = bounds.x;
        const float ctrlX = blockX + (blockW - ctrlW) * alignFactor;

        float curY = bounds.y;
        geom.controlRect = Rect(ctrlX, curY, ctrlW, intrinsicControlSize.y);
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
        const float textBlockW = std::min(measuredTextW, availTextW);

        const float totalTextH = (geom.hasLabel && geom.hasDesc) ? (labelH + spec.descGap + descH) : (geom.hasLabel ? labelH : descH);
        const float startY = bounds.y + (bounds.h - totalTextH) * 0.5f;

        if (spec.inlineOrder == InlineOrder::LabelFirst) {
            float ctrlX = bounds.x + bounds.w - ctrlW;
            if (spec.stretch == StretchMode::Compact) {
                ctrlX = bounds.x + (textBlockW > 0.0f ? textBlockW + gap : 0.0f);
            }
            geom.controlRect = Rect(ctrlX, bounds.y + (bounds.h - intrinsicControlSize.y) * 0.5f, ctrlW, intrinsicControlSize.y);

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
            geom.controlRect = Rect(bounds.x, bounds.y + (bounds.h - intrinsicControlSize.y) * 0.5f, ctrlW, intrinsicControlSize.y);

            float textX = bounds.x + ctrlW + gap;
            if (spec.stretch != StretchMode::Compact) {
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

    if (geom.hasLabel && geom.labelRect.w > 0.0f && geom.labelRect.h > 0.0f) {
        std::string truncLabel;
        std::string_view drawLabel = spec.label.text;
        if (geom.measuredLabelW > geom.labelRect.w + kTextSubpixelTolerance) {
            truncLabel = TextUtil::TruncateWithEllipsis(spec.label.text, geom.labelRect.w, geom.labelFontSize, &canvas);
            drawLabel = truncLabel;
        }
        canvas.DrawTextBox(drawLabel, geom.labelRect, geom.labelFontSize, labelCol, alignFactor, 0.5f);
    }

    if (geom.hasDesc && geom.descRect.w > 0.0f && geom.descRect.h > 0.0f) {
        std::string truncDesc;
        std::string_view drawDesc = spec.description.text;
        if (geom.measuredDescW > geom.descRect.w + kTextSubpixelTolerance) {
            truncDesc = TextUtil::TruncateWithEllipsis(spec.description.text, geom.descRect.w, geom.descFontSize, &canvas);
            drawDesc = truncDesc;
        }
        canvas.DrawTextBox(drawDesc, geom.descRect, geom.descFontSize, descCol, alignFactor, 0.5f);
    }
}

} // namespace guikit
