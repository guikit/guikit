#include "guikit/widgets/Text.hpp"
#include <algorithm>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"

namespace guikit {

namespace {

struct TextPayload {
    UiText label;
    TextOptions opts;
};

} // namespace

Response Text::Run(
    Context& ctx,
    UiText label,
    const TextOptions& opts,
    std::source_location loc
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
    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* d = n->As<TextPayload>();
        if (!d) {
            return Text::Measure(n->text, TextOptions{}, canvas, theme, availableWidth);
        }
        return Text::Measure(d->label, d->opts, canvas, theme, availableWidth);
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* d = n->As<TextPayload>();
        Text::Render(
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
    node->widthMode = (opts.align != Alignment::Start || label.wrap.IsWordWrap())
        ? SizeMode::Stretch
        : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

    ctx.AdaptChildToContainer(node);

    return res;
}

Vec2 Text::Measure(
    const UiText& label,
    const TextOptions& opts,
    ICanvas* canvas,
    const UiTheme& theme,
    float availableWidth
) {
    if (!label.HasContentOrReserve()) {
        return {0.0f, 0.0f};
    }
    const TextRole effectiveRole = (opts.role != TextRole::BodyMedium)
        ? opts.role
        : label.size.role.value_or(opts.role);
    const TypographyMetrics& m = theme.GetTypography(effectiveRole);
    const float fontSize = (label.size.px > 0.0f)
        ? label.size.px
        : (label.size.preset != UiSize::None ? theme.ResolveFontSize(effectiveRole, label.size.preset) : m.fontSize);
    const float lineHeight = (label.size.px > 0.0f || label.size.preset != UiSize::None)
        ? theme.GetLineHeight(fontSize)
        : m.lineHeight;

    auto measureSingleLineW = [&](std::string_view sv) -> float {
        if (sv.empty()) {
            return 0.0f;
        }
        if (canvas) {
            return canvas->MeasureText(sv, fontSize).x;
        }
        return static_cast<float>(sv.length()) * (fontSize * (m.isMonospace ? kMonospaceCharAdvanceRatio : kDefaultCharAdvanceRatio));
    };

    float w = measureSingleLineW(label.text);
    for (std::string_view var : label.variants) {
        w = std::max(w, measureSingleLineW(var));
    }

    float h = TextUtil::MeasureUiTextHeight(label, availableWidth, fontSize, lineHeight, canvas);
    if (canvas && !label.text.empty() && !label.wrap.IsWordWrap() && label.wrap.minLines <= 1) {
        h = std::max(h, canvas->MeasureText(label.text, fontSize).y);
    }
    return {w, h};
}

Vec2 Text::Measure(
    const UiText& label,
    const TextOptions& opts,
    ICanvas* canvas,
    float availableWidth
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(label, opts, canvas, defaultTheme, availableWidth);
}

void Text::Render(
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
    const float fontSize = (label.size.px > 0.0f)
        ? label.size.px
        : (label.size.preset != UiSize::None ? theme.ResolveFontSize(effectiveRole, label.size.preset) : m.fontSize);
    const float lineHeight = (label.size.px > 0.0f || label.size.preset != UiSize::None)
        ? theme.GetLineHeight(fontSize)
        : m.lineHeight;
    const auto& pal = theme.palette;

    Color color;
    if (label.color.HasAny()) {
        const Color fallback = theme.GetTextColor(opts.variant);
        color = label.color.Resolve(hovered, false, disabled, fallback, pal.textPrimary, pal.textPrimary, fallback.WithAlphaF(0.40f));
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

    float alignFactor = 0.0f;
    if (opts.align == Align::Center) {
        alignFactor = 0.5f;
    } else if (opts.align == Align::End) {
        alignFactor = 1.0f;
    }

    TextUtil::DrawUiText(canvas, label, bounds, fontSize, lineHeight, color, alignFactor, 0.5f);
}

} // namespace guikit
