#include "guikit/widgets/Text.hpp"
#include <algorithm>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"

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
    node->text = ctx.AllocString(label);
    node->payload = ctx.AllocPayload<TextOptions>(opts);
    node->visualHash = detail::HashArgs(opts.role, opts.variant, opts.align, node->text);
    node->measureFn = [](const Node* n, float /*availableWidth*/, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* opts = n->As<TextOptions>();
        return Text::Measure(n->text, opts ? *opts : TextOptions{}, canvas, theme);
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* opts = n->As<TextOptions>();
        Text::Render(
            canvas,
            n->computedRect,
            n->text,
            opts ? *opts : TextOptions{},
            n->hovered,
            n->disabled,
            theme
        );
    };
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed;
    node->widthMode = SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

    ctx.AdaptChildToContainer(node);

    return res;
}

Vec2 Text::Measure(std::string_view label, const TextOptions& opts, ICanvas* canvas, const UiTheme& theme) {
    if (label.empty()) {
        return {0.0f, 0.0f};
    }
    const TypographyMetrics& m = theme.GetTypography(opts.role);
    if (canvas) {
        Vec2 dims = canvas->MeasureText(label, m.fontSize);
        dims.y = std::max(dims.y, m.lineHeight);
        return dims;
    }

    const float estW = static_cast<float>(label.length()) * (m.fontSize * (m.isMonospace ? kMonospaceCharAdvanceRatio : kDefaultCharAdvanceRatio));
    return {estW, m.lineHeight};
}

Vec2 Text::Measure(std::string_view label, const TextOptions& opts, ICanvas* canvas) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(label, opts, canvas, defaultTheme);
}

void Text::Render(
    ICanvas& canvas,
    Rect bounds,
    std::string_view label,
    const TextOptions& opts,
    bool hovered,
    bool disabled,
    const UiTheme& theme
) {
    if (label.empty() || bounds.w <= 0.0f || bounds.h <= 0.0f) {
        return;
    }
    const TypographyMetrics& m = theme.GetTypography(opts.role);
    Color color = theme.GetVariant(opts.variant).text;

    if (disabled) {
        color = color.WithAlphaF(0.40f);
    } else if (hovered && opts.variant == UiVariant::Primary) {
        color = theme.GetVariant(opts.variant).bgHover;
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
