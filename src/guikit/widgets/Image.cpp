#include "guikit/widgets/Image.hpp"
#include <algorithm>
#include <cmath>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
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

ControlSpec EffectiveImageControl(const ImageOptions& opts, UiSize size, const UiTheme& theme) noexcept {
    const ComponentMetrics& m = theme.GetMetrics(size);
    const InputFieldMetrics& inputMetrics = theme.GetInputField(size);
    ControlSpec ctrl{};
    ctrl.width = opts.width;
    ctrl.height = opts.height;
    ctrl.align = opts.align;
    ctrl.background = opts.background;
    ctrl.border = opts.border;
    ctrl.defaultLayout = ElementLayout::Stacked;
    ctrl.defaultInlineOrder = InlineOrder::LabelFirst;
    ctrl.labelGap = inputMetrics.labelGap;
    ctrl.descGap = inputMetrics.descGap;
    ctrl.inlineGap = m.paddingX;
    ctrl.stretchable = true;
    ctrl.stretchHeightWhenStandalone = (opts.heightMode == SizeMode::Stretch);
    ctrl.hasIntrinsicBox = true;
    return ctrl;
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
    }
    if (availableWidth > 0.0f) {
        ctrlW = std::min(ctrlW, availableWidth);
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
        res.rightClicked = false;
        res.doubleClicked = false;
    }

    if (res.hovered && !opts.tooltip.empty()) {
        ctx.SetTooltip(opts.tooltip, res.rect);
    }

    ImageOptions optsCopy = opts;
    Control::InheritElementSizing(ctx, optsCopy.stretch, optsCopy.width, optsCopy.height);

    const ControlSpec ctrl = EffectiveImageControl(optsCopy, activeSize, theme);
    Node* node = ctx.CreateNode(NodeType::Image, id);
    auto* payload = ctx.AllocPayload<ImagePayload>();
    payload->source = ctx.AllocString(source);
    payload->opts = optsCopy;
    payload->opts.alt = ctx.AllocText(optsCopy.alt);
    payload->opts.tooltip = ctx.AllocString(optsCopy.tooltip);
    payload->lastCanvas = ctx.GetCanvas();
    node->text = payload->source;
    node->SetPayload(payload);

    node->visualHash = detail::HashArgs(
        payload->source,
        payload->opts.alt,
        optsCopy.background,
        optsCopy.border,
        static_cast<uint8_t>(optsCopy.fit),
        static_cast<uint8_t>(optsCopy.align),
        static_cast<uint8_t>(optsCopy.alignX),
        static_cast<uint8_t>(optsCopy.alignY),
        static_cast<uint8_t>(optsCopy.stretch),
        static_cast<uint8_t>(optsCopy.heightMode),
        optsCopy.width.value_or(0.0f),
        optsCopy.height.value_or(0.0f),
        optsCopy.maxHeight.value_or(0.0f),
        optsCopy.checkerboard,
        optsCopy.opacity,
        activeSize,
        effectiveDisabled
    );

    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& th) noexcept -> Vec2 {
        const auto* p = n->As<ImagePayload>();
        if (!p) return {0.0f, 0.0f};
        if (canvas) {
            p->lastCanvas = canvas;
        }
        return Image::Measure(p->source, p->opts, n->uiSize, canvas ? canvas : p->lastCanvas, th, availableWidth);
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

    Control::SetupNode(ctx, node, ctrl, optsCopy.stretch, optsCopy.align);
    node->heightMode = optsCopy.heightMode;
    if (ctrl.height.has_value() && *ctrl.height > 0.0f) {
        node->heightMode = SizeMode::Fixed;
        node->fixedHeight = theme.Scale(*ctrl.height);
    }
    node->minHeight = ctrl.height.has_value() ? theme.Scale(*ctrl.height) : theme.GetMetrics(activeSize).defaultHeight;

    ctx.AdaptChildToContainer(node);
    if (optsCopy.heightMode == SizeMode::Fit && node->heightMode == SizeMode::Stretch) {
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
    const Vec2 ctrlSize = ComputeControlBoxSize(naturalSize, opts, theme, availableWidth);
    const ControlSpec ctrl = EffectiveImageControl(opts, size, theme);
    return Control::Measure(ctrlSize, ctrl, opts.stretch, size, theme);
}

Vec2 Image::Measure(
    std::string_view source,
    const ImageOptions& opts,
    UiSize size,
    ICanvas* canvas,
    float availableWidth
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(source, opts, size, canvas, defaultTheme, availableWidth);
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
    bool hovered,
    bool disabled,
    const UiTheme& theme
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    const ComponentMetrics& cm = theme.GetMetrics(size);
    const Vec2 naturalSize = ResolveNaturalImageSize(source, &canvas);
    const ControlSpec ctrl = EffectiveImageControl(opts, size, theme);

    const Rect box = bounds;
    if (box.w <= 0.0f || box.h <= 0.0f) return;

    const float rad = ctrl.border.radius.has_value() ? theme.Scale(*ctrl.border.radius) : cm.cornerRadius;
    if (opts.checkerboard) {
        DrawCheckerboard(canvas, box, rad, theme);
    } else if (ctrl.HasBackground(false)) {
        const Color defaultBg = disabled ? theme.palette.surfaceElevated.WithAlphaF(0.40f) : theme.palette.surfaceElevated;
        const Color bg = (ctrl.background.HasAny() && !ctrl.background.IsSurface())
            ? ctrl.background.Resolve(hovered, false, disabled, defaultBg)
            : defaultBg;
        canvas.DrawRoundRect(box, rad, bg);
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
        const Color defaultBg = disabled ? theme.palette.surfaceElevated.WithAlphaF(0.40f) : theme.palette.surfaceElevated;
        const Color bg = (ctrl.background.HasAny() && !ctrl.background.IsSurface())
            ? ctrl.background.Resolve(hovered, false, disabled, defaultBg)
            : defaultBg;
        const Color border = theme.palette.borderSubtle;
        canvas.DrawRoundRect(box, rad, bg, disabled ? border.WithAlphaF(0.40f) : border, 1.0f);

        const float fs = cm.companionFontSize;
        std::string_view fallbackText = !opts.alt.empty() ? opts.alt.text : (!source.empty() ? source : "Image Placeholder");
        canvas.DrawTextBox(fallbackText, box.Inset(cm.paddingX), fs, disabled ? theme.palette.textDisabled : theme.palette.textMuted, 0.5f, 0.5f);
    }

    if (ctrl.border.width > 0.0f) {
        Color bCol = disabled ? theme.palette.borderSubtle.WithAlphaF(0.40f) : theme.palette.borderSubtle;
        if (ctrl.border.color.HasAny()) {
            bCol = ctrl.border.color.Resolve(hovered, false, disabled, bCol);
        }
        canvas.DrawRoundRect(box, rad, Color::Transparent(), bCol, theme.Scale(ctrl.border.width));
    }
}

} // namespace guikit
