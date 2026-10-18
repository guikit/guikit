#include "guikit/widgets/Image.hpp"
#include <algorithm>
#include <cmath>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Element.hpp"
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

ControlSpec EffectiveImageControl(const ImageOptions& opts) noexcept {
    ControlSpec ctrl = opts.control;
    if (!ctrl.width.has_value() && opts.width.has_value()) {
        ctrl.width = opts.width;
    }
    if (!ctrl.height.has_value() && opts.height.has_value()) {
        ctrl.height = opts.height;
    }
    if (!ctrl.background.has_value()) {
        ctrl.background = false;
    }
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
    const ControlSpec ctrl = EffectiveImageControl(opts);
    const float natW = std::max(1.0f, theme.Scale(naturalSize.x));
    const float natH = std::max(1.0f, theme.Scale(naturalSize.y));
    const float aspect = natW / natH;

    float ctrlW = natW;
    if (opts.stretch == StretchMode::Fill && availableWidth > 0.0f) {
        const float pct = std::clamp(ctrl.width.value_or(100.0f), 1.0f, 100.0f);
        ctrlW = availableWidth * (pct * 0.01f);
    } else if (ctrl.width.has_value() && *ctrl.width > 0.0f) {
        ctrlW = theme.Scale(*ctrl.width);
    }
    if (availableWidth > 0.0f) {
        ctrlW = std::min(ctrlW, availableWidth);
    }

    float ctrlH = natH;
    if (ctrl.height.has_value() && *ctrl.height > 0.0f) {
        ctrlH = theme.Scale(*ctrl.height);
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

ElementSpec MakeImageSpec(
    const ImageOptions& opts,
    UiSize size,
    const UiTheme& theme,
    bool disabled,
    bool hovered = false,
    bool active = false
) noexcept {
    const ComponentMetrics& m = theme.GetMetrics(size);
    const InputFieldMetrics& inputMetrics = theme.GetInputField(size);
    const ControlSpec ctrl = EffectiveImageControl(opts);
    return ElementSpec{
        .label = opts.label,
        .description = opts.description,
        .icon = opts.icon,
        .badge = opts.badge,
        .layout = opts.layout,
        .inlineOrder = opts.inlineOrder,
        .elementAlign = opts.elementAlign,
        .stretch = opts.stretch,
        .width = ctrl.width,
        .height = ctrl.height,
        .control = ctrl,
        .background = opts.background,
        .borderWidth = opts.borderWidth,
        .borderRadius = opts.borderRadius,
        .borderColor = opts.borderColor,
        .bgColor = opts.bgColor,
        .widgetSize = size,
        .labelGap = inputMetrics.labelGap,
        .descGap = inputMetrics.descGap,
        .inlineGap = m.paddingX,
        .stretchableControl = true,
        .stretchHeightWhenStandalone = (opts.heightMode == SizeMode::Stretch),
        .disabled = disabled,
        .hovered = hovered,
        .active = active
    };
}

float ResolveInlineControlAvailableWidth(
    const ImageOptions& opts,
    const ElementSpec& spec,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float availableWidth
) noexcept {
    if (availableWidth <= 0.0f || opts.layout != ElementLayout::Inline) {
        return availableWidth;
    }
    const bool hasLabel = opts.icon.IsValid() || opts.label.HasContentOrReserve() || opts.badge.HasContentOrReserve();
    const bool hasDesc = opts.description.HasContentOrReserve();
    if (!hasLabel && !hasDesc) {
        return availableWidth;
    }
    const ControlSpec& ctrl = spec.control;
    // When Fill mode has an explicit percentage width, Element::Resolve applies that percentage directly to bounds.w
    if (opts.stretch == StretchMode::Fill && ctrl.width.has_value()) {
        return availableWidth;
    }
    // When Fill mode has no explicit width in Inline layout, Element::Resolve subtracts measuredTextW + inlineGap
    if (opts.stretch == StretchMode::Fill && !ctrl.width.has_value()) {
        const HeadingMetrics& hm = theme.GetHeading(size);
        const float labelFs = theme.ResolveFontSize(opts.label, TextRole::LabelLarge, size);
        const float descFs = theme.ResolveFontSize(opts.description, TextRole::LabelSmall, size);
        float labelBarW = 0.0f;
        if (opts.icon.IsValid()) {
            labelBarW += hm.iconSize;
        }
        if (opts.label.HasContentOrReserve()) {
            if (labelBarW > 0.0f) labelBarW += hm.gap;
            labelBarW += TextUtil::MeasureWidth(opts.label, labelFs, canvas);
        }
        if (opts.badge.HasContentOrReserve()) {
            if (labelBarW > 0.0f) labelBarW += hm.badgeGap;
            const UiSize badgeSz = (opts.badge.size.preset != UiSize::None) ? opts.badge.size.preset : size;
            const float badgeFs = (opts.badge.size.px > 0.0f)
                ? opts.badge.size.px
                : (opts.badge.size.role.has_value()
                    ? theme.GetTypography(*opts.badge.size.role).fontSize
                    : theme.GetHeading(badgeSz).badgeFontSize);
            labelBarW += TextUtil::MeasureWidth(opts.badge, badgeFs, canvas) + 12.0f;
        }
        const float descW = hasDesc ? TextUtil::MeasureWidth(opts.description, descFs, canvas) : 0.0f;
        const float textW = std::max(labelBarW, descW);
        return std::max(theme.GetMetrics(size).defaultHeight, availableWidth - textW - spec.inlineGap);
    }
    return availableWidth;
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

    const ControlSpec ctrl = EffectiveImageControl(opts);
    Node* node = ctx.CreateNode(NodeType::Image, id);
    auto* payload = ctx.AllocPayload<ImagePayload>();
    payload->source = ctx.AllocString(source);
    payload->opts = opts;
    payload->opts.label = ctx.AllocText(opts.label);
    payload->opts.description = ctx.AllocText(opts.description);
    payload->opts.badge = ctx.AllocText(opts.badge);
    payload->opts.alt = ctx.AllocText(opts.alt);
    payload->opts.tooltip = ctx.AllocString(opts.tooltip);
    node->text = payload->opts.label.text.empty() ? payload->source : payload->opts.label.text;
    node->SetPayload(payload);

    node->visualHash = detail::HashArgs(
        payload->source,
        payload->opts.label,
        payload->opts.description,
        payload->opts.badge,
        payload->opts.alt,
        opts.background,
        opts.borderWidth,
        opts.borderRadius.value_or(-1.0f),
        opts.borderColor,
        opts.bgColor,
        ctrl.HasBackground(false),
        ctrl.borderColor,
        ctrl.bgColor,
        static_cast<uint8_t>(opts.icon.GetType()),
        static_cast<uint8_t>(opts.icon.GetId()),
        opts.icon.GetPath(),
        static_cast<uint8_t>(opts.fit),
        static_cast<uint8_t>(opts.layout),
        static_cast<uint8_t>(opts.inlineOrder),
        static_cast<uint8_t>(opts.elementAlign),
        static_cast<uint8_t>(opts.alignX),
        static_cast<uint8_t>(opts.alignY),
        static_cast<uint8_t>(opts.stretch),
        static_cast<uint8_t>(opts.heightMode),
        ctrl.width.value_or(0.0f),
        ctrl.height.value_or(0.0f),
        opts.maxHeight.value_or(0.0f),
        ctrl.borderRadius.value_or(-1.0f),
        ctrl.borderWidth,
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
    const bool hasExternal = opts.icon.IsValid() || opts.label.HasContentOrReserve() ||
                             opts.description.HasContentOrReserve() || opts.badge.HasContentOrReserve();
    if (ctrl.height.has_value() && *ctrl.height > 0.0f && !hasExternal && !opts.background && opts.borderWidth <= 0.0f) {
        node->heightMode = SizeMode::Fixed;
        node->fixedHeight = theme.Scale(*ctrl.height);
    }
    node->minHeight = ctrl.height.has_value() ? theme.Scale(*ctrl.height) : theme.GetMetrics(activeSize).defaultHeight;

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
    const float ctrlAvailW = ResolveInlineControlAvailableWidth(opts, spec, size, canvas, theme, availableWidth);
    const Vec2 ctrlSize = ComputeControlBoxSize(naturalSize, opts, theme, ctrlAvailW);
    return Element::Measure(ctrlSize, spec, canvas, theme, availableWidth);
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
    const ElementSpec spec = MakeImageSpec(opts, size, theme, disabled, hovered, false);
    const float ctrlAvailW = ResolveInlineControlAvailableWidth(opts, spec, size, &canvas, theme, bounds.w);
    const Vec2 ctrlSize = ComputeControlBoxSize(naturalSize, opts, theme, ctrlAvailW);
    const ElementGeometry geom = Element::Resolve(bounds, ctrlSize, spec, &canvas, theme);
    Element::RenderText(canvas, geom, spec, theme);

    const Rect box = geom.controlRect;
    if (box.w <= 0.0f || box.h <= 0.0f) return;

    const ControlSpec& ctrl = spec.control;
    const float rad = ctrl.borderRadius.has_value() ? theme.Scale(*ctrl.borderRadius) : cm.cornerRadius;
    if (opts.checkerboard) {
        DrawCheckerboard(canvas, box, rad, theme);
    } else if (ctrl.bgColor.HasAny() && ctrl.HasBackground(true)) {
        const Color defaultBg = (opts.backgroundColor.a > 0) ? opts.backgroundColor : theme.palette.surfaceElevated;
        const Color bg = ctrl.bgColor.Resolve(hovered, false, disabled, disabled ? defaultBg.WithAlphaF(0.40f) : defaultBg);
        canvas.DrawRoundRect(box, rad, bg);
    } else if (opts.backgroundColor.a > 0) {
        const Color bg = disabled ? opts.backgroundColor.WithAlphaF(0.40f) : opts.backgroundColor;
        canvas.DrawRoundRect(box, rad, bg);
    } else if (ctrl.HasBackground(false)) {
        const Color bg = disabled ? theme.palette.surfaceElevated.WithAlphaF(0.40f) : theme.palette.surfaceElevated;
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
        const Color bg = (opts.backgroundColor.a > 0) ? opts.backgroundColor : theme.palette.surfaceElevated;
        const Color border = theme.palette.borderSubtle;
        canvas.DrawRoundRect(box, rad, disabled ? bg.WithAlphaF(0.40f) : bg, disabled ? border.WithAlphaF(0.40f) : border, 1.0f);

        const float fs = cm.companionFontSize;
        std::string_view fallbackText = !opts.alt.empty() ? opts.alt.text : (!source.empty() ? source : "Image Placeholder");
        canvas.DrawTextBox(fallbackText, box.Inset(cm.paddingX), fs, disabled ? theme.palette.textDisabled : theme.palette.textMuted, 0.5f, 0.5f);
    }

    if (ctrl.borderWidth > 0.0f) {
        Color bCol = disabled ? theme.palette.borderSubtle.WithAlphaF(0.40f) : theme.palette.borderSubtle;
        if (ctrl.borderColor.HasAny()) {
            bCol = ctrl.borderColor.Resolve(hovered, false, disabled, bCol);
        }
        canvas.DrawRoundRect(box, rad, Color::Transparent(), bCol, theme.Scale(ctrl.borderWidth));
    }
}

} // namespace guikit
