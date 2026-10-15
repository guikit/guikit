#include "guikit/widgets/Image.hpp"
#include <algorithm>
#include <cmath>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/ElementComposer.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"

namespace guikit {

namespace {

constexpr float kDefaultFallbackWidth = 240.0f;
constexpr float kDefaultFallbackHeight = 140.0f;

Vec2 ResolveNaturalImageSize(std::string_view source, ICanvas* canvas) noexcept {
    if (canvas && !source.empty()) {
        const Vec2 sz = canvas->GetImageSize(source);
        if (sz.x > 0.0f && sz.y > 0.0f) {
            return sz;
        }
    }
    return {kDefaultFallbackWidth, kDefaultFallbackHeight};
}

Vec2 ComputeControlBoxSize(
    Vec2 naturalSize,
    const ImageOptions& opts,
    const UiTheme& theme,
    float availableWidth
) noexcept {
    const float natW = std::max(1.0f, theme.Scale(naturalSize.x));
    const float natH = std::max(1.0f, theme.Scale(naturalSize.y));
    const float aspect = natW / natH;

    float ctrlW = natW;
    if (opts.stretch == StretchMode::Fill && availableWidth > 0.0f) {
        const float pct = std::clamp(opts.width.value_or(100.0f), 1.0f, 100.0f);
        ctrlW = availableWidth * (pct * 0.01f);
    } else if (opts.width.has_value() && *opts.width > 0.0f) {
        ctrlW = theme.Scale(*opts.width);
    } else if (availableWidth > 0.0f && (opts.fit == ObjectFit::Contain || opts.fit == ObjectFit::ScaleDown)) {
        ctrlW = std::min(natW, availableWidth);
    }

    float ctrlH = natH;
    if (opts.height.has_value() && *opts.height > 0.0f) {
        ctrlH = theme.Scale(*opts.height);
    } else if (opts.fit == ObjectFit::Original) {
        ctrlH = natH;
    } else if (aspect > 0.0f) {
        ctrlH = ctrlW / aspect;
    }

    if (opts.maxHeight.has_value() && *opts.maxHeight > 0.0f) {
        ctrlH = std::min(ctrlH, theme.Scale(*opts.maxHeight));
    }

    return { std::max(1.0f, ctrlW), std::max(1.0f, ctrlH) };
}

ElementSpec MakeImageSpec(const ImageOptions& opts, UiSize size, const UiTheme& theme, bool disabled) noexcept {
    return ElementSpec{
        .label = opts.label,
        .description = opts.description,
        .layout = opts.layout,
        .inlineOrder = opts.inlineOrder,
        .elementAlign = opts.elementAlign,
        .stretch = opts.stretch,
        .width = opts.width,
        .widgetSize = size,
        .labelGap = theme.Scale(6.0f),
        .descGap = theme.Scale(4.0f),
        .inlineGap = theme.Scale(12.0f),
        .stretchableControl = true,
        .stretchHeightWhenStandalone = (opts.heightMode == SizeMode::Stretch),
        .disabled = disabled
    };
}

} // namespace

Response Image::Run(
    Context& ctx,
    std::string_view source,
    const ImageOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiSize activeSize = opts.size.value_or(ctx.GetCurrentSize());
    const UiTheme& theme = ctx.GetTheme();

    Response res = ctx.Interact(id);
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
    }

    if (res.hovered && !opts.tooltip.empty()) {
        ctx.SetTooltip(opts.tooltip, res.rect);
    }

    Node* node = ctx.CreateNode(NodeType::Image, id);
    auto* payload = ctx.AllocPayload<ImagePayload>();
    payload->source = ctx.AllocString(source);
    payload->opts = opts;
    payload->opts.label = ctx.AllocText(opts.label);
    payload->opts.description = ctx.AllocText(opts.description);
    payload->opts.alt = ctx.AllocText(opts.alt);
    payload->opts.tooltip = ctx.AllocString(opts.tooltip);
    node->text = payload->opts.label.text.empty() ? payload->source : payload->opts.label.text;
    node->SetPayload(payload);

    node->visualHash = detail::HashArgs(
        payload->source,
        payload->opts.label,
        payload->opts.description,
        payload->opts.alt,
        static_cast<uint8_t>(opts.fit),
        static_cast<uint8_t>(opts.layout),
        static_cast<uint8_t>(opts.inlineOrder),
        static_cast<uint8_t>(opts.elementAlign),
        static_cast<uint8_t>(opts.alignX),
        static_cast<uint8_t>(opts.alignY),
        static_cast<uint8_t>(opts.stretch),
        static_cast<uint8_t>(opts.heightMode),
        opts.width.value_or(0.0f),
        opts.height.value_or(0.0f),
        opts.maxHeight.value_or(0.0f),
        opts.radius.value_or(-1.0f),
        opts.borderWidth,
        opts.borderColor.ToUint32(),
        opts.backgroundColor.ToUint32(),
        opts.checkerboard,
        opts.opacity,
        activeSize,
        effectiveDisabled
    );

    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& th) noexcept -> Vec2 {
        const auto* p = n->As<ImagePayload>();
        if (!p) return {0.0f, 0.0f};
        return Image::Measure(p->source, p->opts, n->uiSize, canvas, th, availableWidth);
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& th) noexcept {
        const auto* p = n->As<ImagePayload>();
        if (!p) return;
        Image::Render(canvas, n->computedRect, p->source, p->opts, n->uiSize, n->hovered, n->disabled, th);
    };

    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed;
    node->widthMode = (opts.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = opts.heightMode;
    if (opts.height.has_value() && *opts.height > 0.0f && !opts.label.HasContentOrReserve() && !opts.description.HasContentOrReserve()) {
        node->heightMode = SizeMode::Fixed;
        node->fixedHeight = theme.Scale(*opts.height);
    }
    node->minHeight = opts.height.has_value() ? theme.Scale(*opts.height) : theme.Scale(48.0f);

    ctx.AdaptChildToContainer(node);
    if (opts.heightMode == SizeMode::Fit && node->heightMode == SizeMode::Stretch) {
        node->heightMode = SizeMode::Fit;
    }

    return res;
}

Vec2 Image::Measure(
    std::string_view source,
    const ImageOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float availableWidth
) {
    const Vec2 naturalSize = ResolveNaturalImageSize(source, canvas);
    const ElementSpec spec = MakeImageSpec(opts, size, theme, opts.disabled);

    // If Inline layout with external text, reduce available width for the image control
    float ctrlAvailW = availableWidth;
    if (availableWidth > 0.0f && opts.layout == ElementLayout::Inline &&
        (opts.label.HasContentOrReserve() || opts.description.HasContentOrReserve())) {
        const float labelFs = theme.ResolveFontSize(opts.label, TextRole::LabelLarge, size);
        const float descFs = theme.ResolveFontSize(opts.description, TextRole::LabelSmall, size);
        const float lw = opts.label.HasContentOrReserve() ? TextUtil::MeasureWidth(opts.label, labelFs, canvas) : 0.0f;
        const float dw = opts.description.HasContentOrReserve() ? TextUtil::MeasureWidth(opts.description, descFs, canvas) : 0.0f;
        const float textW = std::max(lw, dw);
        if (opts.stretch == StretchMode::Fill && opts.width.has_value()) {
            const float pct = std::clamp(*opts.width, 1.0f, 100.0f);
            ctrlAvailW = availableWidth * (pct * 0.01f);
        } else {
            ctrlAvailW = std::max(40.0f, availableWidth - textW - spec.inlineGap);
        }
    }

    const Vec2 ctrlSize = ComputeControlBoxSize(naturalSize, opts, theme, ctrlAvailW);
    return ElementComposer::Measure(ctrlSize, spec, canvas, theme, availableWidth);
}

void Image::DrawCheckerboard(
    ICanvas& canvas,
    Rect rect,
    float radius,
    const UiTheme& theme,
    float cellSize
) {
    if (rect.w <= 0.0f || rect.h <= 0.0f) return;

    const bool isDark = (theme.mode == ThemeMode::Dark);
    const Color c1 = isDark ? Color(24, 30, 42, 255) : Color(238, 242, 246, 255);
    const Color c2 = isDark ? Color(34, 42, 58, 255) : Color(220, 226, 234, 255);

    canvas.DrawRoundRect(rect, radius, c1);
    canvas.PushClip(rect.Inset(std::max(1.0f, radius * 0.25f)));

    const float step = std::max(4.0f, theme.Scale(cellSize));
    const int cols = static_cast<int>(std::ceil(rect.w / step));
    const int rows = static_cast<int>(std::ceil(rect.h / step));

    for (int r = 0; r < rows; ++r) {
        for (int c = (r & 1); c < cols; c += 2) {
            const float x = rect.x + static_cast<float>(c) * step;
            const float y = rect.y + static_cast<float>(r) * step;
            const float w = std::min(step, rect.Right() - x);
            const float h = std::min(step, rect.Bottom() - y);
            if (w > 0.0f && h > 0.0f) {
                canvas.DrawRect(Rect(x, y, w, h), c2);
            }
        }
    }

    canvas.PopClip();
}

void Image::Render(
    ICanvas& canvas,
    Rect bounds,
    std::string_view source,
    const ImageOptions& opts,
    UiSize size,
    bool /*hovered*/,
    bool disabled,
    const UiTheme& theme
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    const Vec2 naturalSize = ResolveNaturalImageSize(source, &canvas);
    const ElementSpec spec = MakeImageSpec(opts, size, theme, disabled);

    float ctrlAvailW = bounds.w;
    if (opts.layout == ElementLayout::Inline &&
        (opts.label.HasContentOrReserve() || opts.description.HasContentOrReserve())) {
        const float labelFs = theme.ResolveFontSize(opts.label, TextRole::LabelLarge, size);
        const float descFs = theme.ResolveFontSize(opts.description, TextRole::LabelSmall, size);
        const float lw = opts.label.HasContentOrReserve() ? TextUtil::MeasureWidth(opts.label, labelFs, &canvas) : 0.0f;
        const float dw = opts.description.HasContentOrReserve() ? TextUtil::MeasureWidth(opts.description, descFs, &canvas) : 0.0f;
        const float textW = std::max(lw, dw);
        if (opts.stretch == StretchMode::Fill && opts.width.has_value()) {
            const float pct = std::clamp(*opts.width, 1.0f, 100.0f);
            ctrlAvailW = bounds.w * (pct * 0.01f);
        } else {
            ctrlAvailW = std::max(40.0f, bounds.w - textW - spec.inlineGap);
        }
    }

    const Vec2 ctrlSize = ComputeControlBoxSize(naturalSize, opts, theme, ctrlAvailW);
    const ElementGeometry geom = ElementComposer::Resolve(bounds, ctrlSize, spec, &canvas, theme);
    ElementComposer::RenderText(canvas, geom, spec, theme);

    const Rect box = geom.controlRect;
    if (box.w <= 0.0f || box.h <= 0.0f) return;

    const float rad = opts.radius.has_value() ? theme.Scale(*opts.radius) : 0.0f;
    if (opts.checkerboard) {
        DrawCheckerboard(canvas, box, rad, theme);
    } else if (opts.backgroundColor.a > 0) {
        canvas.DrawRoundRect(box, rad, opts.backgroundColor);
    }

    const float ax = (opts.alignX == Align::End) ? 1.0f : ((opts.alignX == Align::Center) ? 0.5f : 0.0f);
    const float ay = (opts.alignY == Align::End) ? 1.0f : ((opts.alignY == Align::Center) ? 0.5f : 0.0f);
    const Vec2 scaledNat = { theme.Scale(naturalSize.x), theme.Scale(naturalSize.y) };
    const Rect fitted = ComputeBoxFit(opts.fit, scaledNat, box, ax, ay);

    const bool needsClip = (opts.fit == ObjectFit::Cover || opts.fit == ObjectFit::Original ||
                            fitted.w > box.w + 0.5f || fitted.h > box.h + 0.5f);
    if (needsClip) {
        canvas.PushClip(box);
    }

    const uint8_t effAlpha = disabled ? static_cast<uint8_t>(opts.opacity / 2) : opts.opacity;
    const float imgRad = (std::abs(fitted.w - box.w) < 1.5f && std::abs(fitted.h - box.h) < 1.5f) ? rad : 0.0f;
    const bool drawn = canvas.DrawImage(source, fitted, imgRad, effAlpha);

    if (needsClip) {
        canvas.PopClip();
    }

    if (!drawn) {
        // Clean vector placeholder card when image is missing or empty
        const Color bg = (opts.backgroundColor.a > 0) ? opts.backgroundColor : theme.palette.surfaceElevated;
        const Color border = theme.palette.borderSubtle;
        canvas.DrawRoundRect(box, rad, bg, border, 1.0f);

        const float fs = theme.GetFontSize(TextRole::LabelMedium);
        std::string_view fallbackText = !opts.alt.empty() ? opts.alt.text : (!source.empty() ? source : "Image Placeholder");
        canvas.DrawTextBox(fallbackText, box.Inset(8.0f), fs, theme.palette.textMuted, 0.5f, 0.5f);
    }

    if (opts.borderWidth > 0.0f) {
        const Color bCol = (opts.borderColor.a > 0) ? opts.borderColor : theme.palette.borderSubtle;
        canvas.DrawRoundRect(box, rad, Color::Transparent(), bCol, theme.Scale(opts.borderWidth));
    }
}

} // namespace guikit
