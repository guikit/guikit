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
    const bool hasLabel = !spec.label.empty() || spec.hasHeaderRight || !spec.headerRightText.empty();
    const bool hasDesc = !spec.description.empty();
    const bool hasExternal = hasLabel || hasDesc;

    if (!hasExternal) {
        return intrinsicControlSize;
    }

    Vec2 labelDims = {0.0f, 0.0f};
    if (hasLabel) {
        const float lineH = canvas ? canvas->GetLineHeight(spec.labelFontSize) : theme.GetLineHeight(spec.labelFontSize);
        float w = 0.0f;
        if (!spec.label.empty()) {
            if (canvas) {
                w = canvas->MeasureText(spec.label, spec.labelFontSize).x;
            } else {
                w = static_cast<float>(spec.label.length()) * (spec.labelFontSize * kDefaultCharAdvanceRatio);
            }
        }
        if (spec.hasHeaderRight || !spec.headerRightText.empty()) {
            float rightW = spec.headerRightMinWidth;
            if (!spec.headerRightText.empty()) {
                const float mw = canvas
                    ? canvas->MeasureText(spec.headerRightText, spec.labelFontSize).x
                    : static_cast<float>(spec.headerRightText.length()) * (spec.labelFontSize * kDefaultCharAdvanceRatio);
                rightW = std::max(rightW, mw);
            }
            w += (w > 0.0f ? 8.0f : 0.0f) + rightW;
        }
        labelDims = {w, lineH};
    }

    Vec2 descDims = {0.0f, 0.0f};
    if (hasDesc) {
        if (canvas) {
            descDims = canvas->MeasureText(spec.description, spec.descFontSize);
        } else {
            descDims = {
                static_cast<float>(spec.description.length()) * (spec.descFontSize * kDefaultCharAdvanceRatio),
                theme.GetLineHeight(spec.descFontSize)
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
    geom.hasLabel = !spec.label.empty() || spec.hasHeaderRight || !spec.headerRightText.empty();
    geom.hasDesc = !spec.description.empty();
    geom.hasExternal = geom.hasLabel || geom.hasDesc;

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
        ? (canvas ? canvas->GetLineHeight(spec.labelFontSize) : theme.GetLineHeight(spec.labelFontSize))
        : 0.0f;
    const float descH = geom.hasDesc
        ? (canvas ? canvas->GetLineHeight(spec.descFontSize) : theme.GetLineHeight(spec.descFontSize))
        : 0.0f;

    if (!spec.label.empty()) {
        geom.measuredLabelW = canvas
            ? canvas->MeasureText(spec.label, spec.labelFontSize).x
            : static_cast<float>(spec.label.length()) * (spec.labelFontSize * kDefaultCharAdvanceRatio);
    }
    if (!spec.headerRightText.empty()) {
        geom.measuredRightW = canvas
            ? canvas->MeasureText(spec.headerRightText, spec.labelFontSize).x
            : static_cast<float>(spec.headerRightText.length()) * (spec.labelFontSize * kDefaultCharAdvanceRatio);
    }
    float reservedRightW = 0.0f;
    if (spec.hasHeaderRight || !spec.headerRightText.empty()) {
        reservedRightW = std::max(spec.headerRightMinWidth, geom.measuredRightW);
    }
    const float measuredHeaderW = geom.measuredLabelW
        + ((geom.measuredLabelW > 0.0f && reservedRightW > 0.0f) ? 8.0f : 0.0f)
        + reservedRightW;

    if (geom.hasDesc) {
        geom.measuredDescW = canvas
            ? canvas->MeasureText(spec.description, spec.descFontSize).x
            : static_cast<float>(spec.description.length()) * (spec.descFontSize * kDefaultCharAdvanceRatio);
    }

    const float measuredTextW = std::max(measuredHeaderW, geom.measuredDescW);

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

    const auto& pal = theme.palette;
    const float alignFactor = (spec.elementAlign == Align::End) ? 1.0f : ((spec.elementAlign == Align::Center) ? 0.5f : 0.0f);

    Color labelCol = pal.textPrimary;
    Color descCol = theme.GetTextColor(spec.descriptionVariant);
    if (spec.disabled) {
        labelCol = labelCol.WithAlphaF(0.40f);
        descCol = descCol.WithAlphaF(0.40f);
    }

    if (geom.hasLabel && geom.labelRect.w > 0.0f && geom.labelRect.h > 0.0f) {
        Rect lblRect = geom.labelRect;
        if (!spec.headerRightText.empty()) {
            const Color rightCol = (spec.headerRightColor.a > 0)
                ? spec.headerRightColor
                : (spec.disabled ? pal.textDisabled : pal.textSecondary);
            std::string truncRight;
            std::string_view drawRight = spec.headerRightText;
            float rightW = geom.measuredRightW;
            if (rightW > geom.labelRect.w + kTextSubpixelTolerance) {
                truncRight = TextUtil::TruncateWithEllipsis(spec.headerRightText, geom.labelRect.w, spec.labelFontSize, &canvas);
                drawRight = truncRight;
                rightW = canvas.MeasureText(drawRight, spec.labelFontSize).x;
            }
            const float rightX = geom.labelRect.Right() - rightW;
            canvas.DrawText(drawRight, Vec2(rightX, geom.labelRect.y + (geom.labelRect.h - spec.labelFontSize) * 0.5f), spec.labelFontSize, rightCol);
            lblRect.w = std::max(0.0f, geom.labelRect.w - rightW - (!spec.label.empty() ? 8.0f : 0.0f));
        }

        if (!spec.label.empty() && lblRect.w > 0.0f) {
            std::string truncLabel;
            std::string_view drawLabel = spec.label;
            if (geom.measuredLabelW > lblRect.w + kTextSubpixelTolerance) {
                truncLabel = TextUtil::TruncateWithEllipsis(spec.label, lblRect.w, spec.labelFontSize, &canvas);
                drawLabel = truncLabel;
            }
            canvas.DrawTextBox(drawLabel, lblRect, spec.labelFontSize, labelCol, alignFactor, 0.5f);
        }
    }

    if (geom.hasDesc && geom.descRect.w > 0.0f && geom.descRect.h > 0.0f) {
        std::string truncDesc;
        std::string_view drawDesc = spec.description;
        if (geom.measuredDescW > geom.descRect.w + kTextSubpixelTolerance) {
            truncDesc = TextUtil::TruncateWithEllipsis(spec.description, geom.descRect.w, spec.descFontSize, &canvas);
            drawDesc = truncDesc;
        }
        canvas.DrawTextBox(drawDesc, geom.descRect, spec.descFontSize, descCol, alignFactor, 0.5f);
    }
}

} // namespace guikit
