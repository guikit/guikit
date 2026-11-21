#include "guikit/widgets/image.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"
#include "keel/math.h"

namespace guikit {

namespace {

constexpr keel::f32 kDefaultFallbackWidth = 240.0f;
constexpr keel::f32 kDefaultFallbackHeight = 140.0f;

Vec2 ResolveNaturalImageSize(keel::StringRef source, ICanvas* canvas) noexcept {
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
    ControlSpec ctrl = Control::FromOptions(
        opts,
        SizeMode::Fit,
        SizeMode::Fit,
        ElementLayout::Stacked,
        inputMetrics.labelGap,
        inputMetrics.descGap,
        m.paddingX
    );
    if (!ctrl.minHeight.has_value() && ctrl.height.IsFixed()) {
        ctrl.minHeight = ctrl.height.Dp();
    }
    return ctrl;
}

Vec2 ComputeControlBoxSize(
    Vec2 naturalSize,
    const ImageOptions& opts,
    keel::f32 availableWidth
) noexcept {
    const keel::f32 natW = keel::max(1.0f, naturalSize.x);
    const keel::f32 natH = keel::max(1.0f, naturalSize.y);
    const keel::f32 aspect = natW / natH;

    keel::f32 ctrlW = natW;
    if (opts.width.IsFixed()) {
        ctrlW = opts.width.Dp();
    } else if (opts.width.ResolveMode(SizeMode::Fit) == SizeMode::Stretch && availableWidth > 0.0f) {
        ctrlW = availableWidth;
    }
    if (availableWidth > 0.0f) {
        ctrlW = keel::min(ctrlW, availableWidth);
    }

    keel::f32 ctrlH = natH;
    if (opts.height.IsFixed()) {
        ctrlH = opts.height.Dp();
    } else if (opts.fit == ObjectFit::Original) {
        ctrlH = natH;
    } else if (aspect > 0.0f) {
        ctrlH = ctrlW / aspect;
    }

    if (opts.maxHeight.has_value() && *opts.maxHeight > 0.0f) {
        ctrlH = keel::min(ctrlH, *opts.maxHeight);
    }

    return { keel::max(1.0f, ctrlW), keel::max(1.0f, ctrlH) };
}

} // namespace

Response Image::Run(
    Context& ctx,
    keel::StringRef source,
    const ImageOptions& opts,
    keel::SourceLocation loc
) {

    const Id id = ctx.MakeChildId(loc);
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiSize activeSize = opts.size;
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
    if (optsCopy.width.IsAuto()) {
        optsCopy.width = Control::ResolveWidthMode(ctx, opts.width, SizeMode::Fit);
    }

    const ControlSpec ctrl = EffectiveImageControl(optsCopy, activeSize, theme);
    Node* node = ctx.CreateNode(NodeType::Image, id);
    auto* payload = ctx.AllocPayload<ImagePayload>();
    payload->source = ctx.AllocString(source);
    payload->opts = optsCopy;
    payload->opts.alt = ctx.AllocText(optsCopy.alt);
    payload->opts.tooltip = ctx.AllocText(optsCopy.tooltip);
    payload->lastCanvas = ctx.GetCanvas();
    node->text = payload->source;
    node->SetPayload(payload);

    node->visualHash = detail::HashArgs(
        payload->source,
        payload->opts.alt,
        optsCopy.color,
        optsCopy.background,
        optsCopy.border,
        keel::as<keel::u8>(optsCopy.fit),
        keel::as<keel::u8>(optsCopy.alignX),
        keel::as<keel::u8>(optsCopy.alignY),
        optsCopy.width,
        optsCopy.height,
        optsCopy.minWidth.value_or(0.0f),
        optsCopy.maxWidth.value_or(0.0f),
        optsCopy.minHeight.value_or(0.0f),
        optsCopy.maxHeight.value_or(0.0f),
        optsCopy.checkerboard,
        optsCopy.opacity,
        activeSize,
        effectiveDisabled
    );

    node->measureFn = [](const Node* n, keel::f32 availableWidth, ICanvas* canvas, const UiTheme& th) noexcept -> Vec2 {
        const auto* p = n->As<ImagePayload>();
        if (!p) return {0.0f, 0.0f};
        if (canvas) {
            p->lastCanvas = canvas;
        }
        ImageOptions effOpts = p->opts;
        if (n->widthMode == SizeMode::Stretch && !effOpts.width.IsFixed()) {
            effOpts.width = SizeMode::Stretch;
        }
        return Image::Measure(p->source, effOpts, n->uiSize, canvas ? canvas : p->lastCanvas, th, availableWidth);
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

    Control::SetupNode(ctx, node, ctrl);
    node->minHeight = keel::max(
        node->minHeight,
        ctrl.height.IsFixed() ? ctrl.height.Dp() : theme.GetMetrics(activeSize).defaultHeight
    );
    if ((!node->parent || node->parent->type != NodeType::Element) &&
        optsCopy.height.ResolveMode(SizeMode::Fit) == SizeMode::Fit &&
        node->heightMode == SizeMode::Stretch) {
        node->heightMode = SizeMode::Fit;
    }

    return res;
}

Vec2 Image::Measure(
    keel::StringRef source,
    const ImageOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::f32 availableWidth
) {
    const Vec2 naturalSize = ResolveNaturalImageSize(source, canvas);
    const Vec2 ctrlSize = ComputeControlBoxSize(naturalSize, opts, availableWidth);
    const ControlSpec ctrl = EffectiveImageControl(opts, size, theme);
    return Control::Measure(ctrlSize, ctrl, theme);
}

Vec2 Image::Measure(
    keel::StringRef source,
    const ImageOptions& opts,
    UiSize size,
    ICanvas* canvas,
    keel::f32 availableWidth
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(source, opts, size, canvas, defaultTheme, availableWidth);
}

void Image::DrawCheckerboard(
    ICanvas& canvas,
    Rect rect,
    keel::f32 radius,
    const UiTheme& theme,
    keel::f32 cellSize
) {
    if (rect.w <= 0.0f || rect.h <= 0.0f) return;

    const bool isDark = (theme.mode == ThemeMode::Dark);
    const Color c1 = isDark ? Color(24, 30, 42, 255) : Color(238, 242, 246, 255);
    const Color c2 = isDark ? Color(34, 42, 58, 255) : Color(220, 226, 234, 255);

    canvas.DrawRoundRect(rect, radius, c1);
    canvas.PushClip(rect.Inset(keel::max(1.0f, radius * 0.25f)));

    const keel::f32 step = keel::max(4.0f, cellSize);
    const keel::i32 cols = keel::ceil_i32(rect.w / step);
    const keel::i32 rows = keel::ceil_i32(rect.h / step);

    for (keel::i32 r = 0; r < rows; ++r) {
        for (keel::i32 c = (r & 1); c < cols; c += 2) {
            const keel::f32 x = rect.x + keel::as<keel::f32>(c) * step;
            const keel::f32 y = rect.y + keel::as<keel::f32>(r) * step;
            const keel::f32 w = keel::min(step, rect.Right() - x);
            const keel::f32 h = keel::min(step, rect.Bottom() - y);
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
    keel::StringRef source,
    const ImageOptions& opts,
    UiSize size,
    bool hovered,
    bool disabled,
    const UiTheme& theme
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    const ComponentMetrics& cm = theme.GetMetrics(size);
    const Vec2 naturalSize = ResolveNaturalImageSize(source, &canvas);

    const Rect box = bounds;
    if (box.w <= 0.0f || box.h <= 0.0f) return;

    const keel::f32 rad = opts.border.radius.has_value() ? *opts.border.radius : cm.cornerRadius;
    if (opts.checkerboard) {
        DrawCheckerboard(canvas, box, rad, theme);
    } else if (opts.background.IsBackgroundEnabled(false)) {
        const Color defaultBg = disabled ? theme.palette.surfaceElevated.WithAlphaF(0.40f) : theme.palette.surfaceElevated;
        const Color bg = (!opts.background.HasAny())
            ? defaultBg
            : (opts.background.IsSurface()
                ? defaultBg.ScaleAlpha(opts.background.alphaScale)
                : theme.ResolveColor(opts.background, hovered, false, disabled, defaultBg));
        canvas.DrawRoundRect(box, rad, bg);
    }

    const keel::f32 ax = (opts.alignX == Align::End) ? 1.0f : ((opts.alignX == Align::Center) ? 0.5f : 0.0f);
    const keel::f32 ay = (opts.alignY == Align::End) ? 1.0f : ((opts.alignY == Align::Center) ? 0.5f : 0.0f);
    const Vec2 scaledNat = naturalSize;
    const Rect fitted = ComputeBoxFit(opts.fit, scaledNat, box, ax, ay);

    const bool needsClip = (opts.fit == ObjectFit::Cover || opts.fit == ObjectFit::Original ||
                            fitted.w > box.w + 0.5f || fitted.h > box.h + 0.5f);
    if (needsClip) {
        canvas.PushClip(box);
    }

    const keel::u8 effAlpha = disabled ? keel::as<keel::u8>(opts.opacity / 2) : opts.opacity;
    const keel::f32 imgRad = (keel::abs(fitted.w - box.w) < 1.5f && keel::abs(fitted.h - box.h) < 1.5f) ? rad : 0.0f;
    const bool drawn = canvas.DrawImage(source, fitted, imgRad, effAlpha);

    if (needsClip) {
        canvas.PopClip();
    }

    if (!drawn) {
        // Clean vector placeholder card when image is missing or empty
        const Color defaultBg = disabled ? theme.palette.surfaceElevated.WithAlphaF(0.40f) : theme.palette.surfaceElevated;
        const Color bg = (!opts.background.HasAny())
            ? defaultBg
            : (opts.background.IsSurface()
                ? defaultBg.ScaleAlpha(opts.background.alphaScale)
                : theme.ResolveColor(opts.background, hovered, false, disabled, defaultBg));
        const Color border = theme.palette.borderSubtle;
        canvas.DrawRoundRect(box, rad, bg, disabled ? border.WithAlphaF(0.40f) : border, 1.0f);

        const keel::f32 fs = cm.companionFontSize;
        keel::StringRef fallbackText = !opts.alt.empty() ? opts.alt.text : (!source.empty() ? source : keel::StringRef("Image Placeholder"));
        canvas.DrawTextBox(fallbackText, box.Inset(cm.paddingX), fs, disabled ? theme.palette.textDisabled : theme.palette.textMuted, 0.5f, 0.5f);
    }

    if (opts.border.width > 0.0f) {
        Color bCol = disabled ? theme.palette.borderSubtle.WithAlphaF(0.40f) : theme.palette.borderSubtle;
        if (opts.border.color.HasAny()) {
            bCol = theme.ResolveColor(opts.border.color, hovered, false, disabled, bCol);
        }
        canvas.DrawRoundRect(box, rad, Color::Transparent(), bCol, opts.border.width);
    }
}

} // namespace guikit
