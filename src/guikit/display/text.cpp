#include "guikit/display/text.hpp"
#include "guikit/context.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/rich_text.hpp"
#include "guikit/core/text_util.hpp"

namespace guikit {

namespace {

struct TextPayload {
    UiText label;
    TextOptions opts;
};

} // namespace

Response detail::Text::Run(
    Context& ctx,
    UiText label,
    const TextOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const bool effectiveDisabled = ctx.IsDisabled();

    Response res = ctx.Interact(id);
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        res.rightClicked = false;
        res.doubleClicked = false;
    }

    Node* node = ctx.CreateNode(NodeType::Text, id);
    auto* data = ctx.AllocPayload<TextPayload>();
    data->label = ctx.AllocText(label);
    data->opts = opts;
    node->text = data->label.text;
    node->SetPayload(data);
    node->visualHash = detail::HashArgs(opts.role, opts.variant, opts.align, data->label);
    node->measureFn = [](const Node* n, keel::f32 availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* d = n->As<TextPayload>();
        if (!d) {
            return detail::Text::Measure(n->text, TextOptions{}, canvas, theme, availableWidth);
        }
        return detail::Text::Measure(d->label, d->opts, canvas, theme, availableWidth);
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* d = n->As<TextPayload>();
        detail::Text::Render(
            canvas,
            n->computedRect,
            d ? d->label : UiText(n->text),
            d ? d->opts : TextOptions{},
            n->hovered,
            n->disabled,
            theme
        );
    };
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed;
    node->align = opts.align;
    node->uiSize = UiSize::Md;
    node->widthMode = (opts.align != Alignment::Start || label.wrap.IsWordWrap())
        ? SizeMode::Stretch
        : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

    ctx.AdaptChildToContainer(node);

    return res;
}

Vec2 detail::Text::Measure(
    const UiText& label,
    const TextOptions& opts,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::f32 availableWidth
) {
    if (!label.HasContentOrReserve()) {
        return {0.0f, 0.0f};
    }
    const TextRole effectiveRole = (opts.role != TextRole::BodyMedium)
        ? opts.role
        : label.size.role.value_or(opts.role);
    const TypographyMetrics& m = theme.GetTypography(effectiveRole);
    const keel::f32 fontSize = (label.size.px > 0.0f)
        ? label.size.px
        : (label.size.preset != UiSize::None ? theme.ResolveFontSize(effectiveRole, label.size.preset) : m.fontSize);
    const keel::f32 lineHeight = (label.size.px > 0.0f || label.size.preset != UiSize::None)
        ? theme.GetLineHeight(fontSize)
        : m.lineHeight;

    keel::f32 w = 0.0f;
    if (label.wrap.rich) {
        w = TextUtil::MeasureUiTextWidth(label, fontSize, canvas);
    } else {
        auto measureSingleLineW = [&](keel::StringRef sv) -> keel::f32 {
            if (sv.empty()) {
                return 0.0f;
            }
            if (canvas) {
                return canvas->MeasureText(sv, fontSize).x;
            }
            return keel::as<keel::f32>(sv.length()) * (fontSize * (m.isMonospace ? kMonospaceCharAdvanceRatio : kDefaultCharAdvanceRatio));
        };

        w = measureSingleLineW(label.text);
        for (keel::StringRef var : label.variants) {
            w = keel::max(w, measureSingleLineW(var));
        }
    }

    keel::f32 h = TextUtil::MeasureUiTextHeight(label, availableWidth, fontSize, lineHeight, canvas, &theme);
    if (canvas && !label.text.empty() && !label.wrap.IsWordWrap() && label.wrap.minLines <= 1 && !label.wrap.rich) {
        h = keel::max(h, canvas->MeasureText(label.text, fontSize).y);
    }
    return {w, h};
}

Vec2 detail::Text::Measure(
    const UiText& label,
    const TextOptions& opts,
    ICanvas* canvas,
    keel::f32 availableWidth
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(label, opts, canvas, defaultTheme, availableWidth);
}

void detail::Text::Render(
    ICanvas& canvas,
    Rect bounds,
    const UiText& label,
    const TextOptions& opts,
    bool hovered,
    bool disabled,
    const UiTheme& theme
) {
    if (label.empty() || bounds.w <= 0.0f || bounds.h <= 0.0f) {
        return;
    }
    const TextRole effectiveRole = (opts.role != TextRole::BodyMedium)
        ? opts.role
        : label.size.role.value_or(opts.role);
    const TypographyMetrics& m = theme.GetTypography(effectiveRole);
    const keel::f32 fontSize = (label.size.px > 0.0f)
        ? label.size.px
        : (label.size.preset != UiSize::None ? theme.ResolveFontSize(effectiveRole, label.size.preset) : m.fontSize);
    const keel::f32 lineHeight = (label.size.px > 0.0f || label.size.preset != UiSize::None)
        ? theme.GetLineHeight(fontSize)
        : m.lineHeight;
    const auto& pal = theme.palette;

    Color color;
    if (label.color.HasAny()) {
        const Color fallback = theme.GetTextColor(opts.variant);
        color = theme.ResolveTextStateColor(label.color).Resolve(
            hovered,
            false,
            disabled,
            fallback,
            pal.textPrimary,
            pal.textPrimary,
            fallback.WithAlphaF(0.40f)
        );
    } else {
        color = theme.GetTextColor(opts.variant);
        if (disabled) {
            color = color.WithAlphaF(0.40f);
        } else if (hovered) {
            if (opts.variant == UiVariant::Default || opts.variant == UiVariant::Secondary || opts.variant == UiVariant::Muted || opts.variant == UiVariant::Ghost) {
                color = pal.textPrimary;
            } else {
                color = theme.GetVariant(opts.variant).bgHover;
            }
        }
    }

    keel::f32 alignFactor = 0.0f;
    if (opts.align == Align::Center) {
        alignFactor = 0.5f;
    } else if (opts.align == Align::End) {
        alignFactor = 1.0f;
    }

    if (label.wrap.rich) {
        const keel::f32 baseLh = canvas.GetLineHeight(fontSize);
        const keel::f32 lhScale = (baseLh > 0.0f) ? (lineHeight / baseLh) : 1.0f;
        const keel::f32 effAvailW = label.wrap.IsWordWrap() ? bounds.w : 100000.0f;
        auto flow = GetOrBuildRichTextFlow(
            &canvas,
            label.text,
            effAvailW,
            fontSize,
            fontSize * 0.92f,
            lhScale,
            theme,
            1.0f,
            m.isMonospace ? FontStyle::Monospace : FontStyle::Regular,
            label.wrap.IsWordWrap() ? opts.align : Align::Start
        );
        keel::f32 drawY = bounds.y;
        if (!label.wrap.IsWordWrap() && bounds.h > flow->height + kTextSubpixelTolerance) {
            drawY += (bounds.h - flow->height) * 0.5f;
        }
        keel::f32 drawX = bounds.x;
        if (!label.wrap.IsWordWrap() && opts.align != Align::Start) {
            drawX += (bounds.w - flow->width) * alignFactor;
        }
        const bool needsClip = (!label.wrap.IsWordWrap() && flow->width > bounds.w + kTextSubpixelTolerance) ||
                               (label.wrap.maxLines > 0 && flow->height > bounds.h + kTextSubpixelTolerance);
        if (needsClip) {
            canvas.PushClip(bounds);
        }
        RenderInlineFlowLayout(canvas, *flow, {drawX, drawY}, {drawX, drawY}, color, nullptr, nullptr, theme);
        if (needsClip) {
            canvas.PopClip();
        }
        return;
    }

    TextUtil::DrawUiText(canvas, label, bounds, fontSize, lineHeight, color, alignFactor, 0.5f, &theme);
}

} // namespace guikit
