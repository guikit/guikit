#include "guikit/widgets/Text.hpp"
#include <algorithm>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/layout/Node.hpp"

namespace guikit {

Response Text::Run(
    Context& ctx,
    std::string_view label,
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
    node->text = ctx.InternString(label);
    node->textOpts = opts;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed;
    node->widthMode = SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

    ctx.AdaptChildToContainer(node);

    return res;
}

Vec2 Text::Measure(std::string_view label, const TextOptions& opts, ICanvas* canvas) {
    if (label.empty()) {
        return {0.0f, 0.0f};
    }
    const TypographyMetrics m = GetTypographyMetrics(opts.role);
    if (canvas) {
        Vec2 dims = canvas->MeasureText(label, m.fontSize);
        dims.y = std::max(dims.y, m.lineHeight);
        return dims;
    }

    const float estW = static_cast<float>(label.length()) * (m.fontSize * (m.isMonospace ? 0.62f : 0.58f));
    return {estW, m.lineHeight};
}

void Text::Render(
    ICanvas& canvas,
    Rect bounds,
    std::string_view label,
    const TextOptions& opts,
    bool hovered,
    bool disabled,
    ThemeMode theme
) {
    if (label.empty() || bounds.w <= 0.0f || bounds.h <= 0.0f) {
        return;
    }
    const TypographyMetrics m = GetTypographyMetrics(opts.role);
    Color color = GetVariantColors(opts.variant, theme).text;

    if (disabled) {
        color = color.WithAlphaF(0.40f);
    } else if (hovered && opts.variant == UiVariant::Primary) {
        color = GetVariantColors(opts.variant).bgHover;
    }

    float alignFactor = 0.0f;
    if (opts.align == Align::Center) {
        alignFactor = 0.5f;
    } else if (opts.align == Align::End) {
        alignFactor = 1.0f;
    }

    canvas.DrawTextBox(label, bounds, m.fontSize, color, alignFactor, 0.5f);
}

} // namespace guikit
