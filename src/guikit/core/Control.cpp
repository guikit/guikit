#include "guikit/core/Control.hpp"
#include <algorithm>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Element.hpp"
#include "guikit/core/Node.hpp"

namespace guikit {

float Control::ComputeWellPadding(
    const ControlSpec& spec,
    UiSize size,
    const UiTheme& theme
) noexcept {
    if (!HasWellFrame(spec)) {
        return 0.0f;
    }
    const ComponentMetrics& cm = theme.GetMetrics(size);
    const float basePad = std::max(theme.Scale(4.0f), cm.paddingX * 0.5f);
    const float strokePad = (spec.border.width > 0.0f) ? theme.Scale(spec.border.width) : 0.0f;
    return basePad + strokePad;
}

Vec2 Control::Measure(
    Vec2 intrinsicSize,
    const ControlSpec& spec,
    StretchMode stretch,
    UiSize size,
    const UiTheme& theme
) noexcept {
    Vec2 result = intrinsicSize;
    const float wellPad = ComputeWellPadding(spec, size, theme);
    if (wellPad > 0.0f) {
        result.x += wellPad * 2.0f;
        result.y += wellPad * 2.0f;
    }
    if (!spec.stretchable && spec.width.has_value() && *spec.width > 0.0f && stretch != StretchMode::Fill) {
        result.x = std::max(result.x, theme.Scale(*spec.width));
    }
    if (spec.height.has_value() && *spec.height > 0.0f) {
        result.y = theme.Scale(*spec.height);
    }
    return result;
}

Rect Control::ResolveInnerRect(
    Rect controlRect,
    const ControlSpec& spec,
    UiSize size,
    const UiTheme& theme
) noexcept {
    const float wellPad = ComputeWellPadding(spec, size, theme);
    if (wellPad <= 0.0f) {
        return controlRect;
    }
    return controlRect.Inset(wellPad);
}

void Control::RenderFrame(
    ICanvas& canvas,
    Rect controlRect,
    const ControlSpec& spec,
    UiSize size,
    bool disabled,
    const UiTheme& theme,
    bool hovered,
    bool active
) noexcept {
    if (!HasWellFrame(spec) || controlRect.w <= 0.0f || controlRect.h <= 0.0f) {
        return;
    }

    const ComponentMetrics& cm = theme.GetMetrics(size);
    const float radius = spec.border.radius.has_value()
        ? theme.Scale(*spec.border.radius)
        : cm.cornerRadius;

    const auto& pal = theme.palette;
    const Color resolvedBorderAccent = spec.border.color.Resolve(
        hovered,
        active,
        disabled,
        pal.border,
        pal.borderHover,
        pal.focusRing,
        pal.border.WithAlphaF(0.40f)
    );

    Color bg = Color::Transparent();
    if (spec.HasBackground(false)) {
        if (spec.background.color.HasAny()) {
            bg = spec.background.color.Resolve(
                hovered,
                active,
                disabled,
                pal.surface,
                pal.surfaceHover,
                pal.surfaceActive,
                pal.surface.WithAlphaF(0.40f)
            );
        } else {
            bg = pal.surface;
            if (spec.border.color.HasAny()) {
                bg = Color(
                    static_cast<uint8_t>((static_cast<int>(bg.r) * 86 + static_cast<int>(resolvedBorderAccent.r) * 14) / 100),
                    static_cast<uint8_t>((static_cast<int>(bg.g) * 86 + static_cast<int>(resolvedBorderAccent.g) * 14) / 100),
                    static_cast<uint8_t>((static_cast<int>(bg.b) * 86 + static_cast<int>(resolvedBorderAccent.b) * 14) / 100),
                    255
                );
            }
            if (disabled) {
                bg = bg.WithAlphaF(0.40f);
            }
        }
    }

    const float strokeW = (spec.border.width > 0.0f) ? theme.Scale(spec.border.width) : 0.0f;
    Color borderCol = Color::Transparent();
    if (strokeW > 0.0f) {
        borderCol = resolvedBorderAccent;
    }

    canvas.DrawRoundRect(controlRect, radius, bg, borderCol, strokeW);
}

void Control::InheritElementSizing(
    Context& ctx,
    StretchMode& ioStretch,
    std::optional<float>& ioWidth,
    std::optional<float>& ioHeight,
    bool inheritWidth
) noexcept {
    Node* parent = ctx.GetCurrentContainer();
    if (!parent || parent->type != NodeType::Element) {
        return;
    }
    auto* ep = parent->As<ElementPayload>();
    if (!ep) {
        return;
    }
    if (ep->opts.stretch.has_value()) {
        ioStretch = *ep->opts.stretch;
    } else {
        ep->opts.stretch = ioStretch;
    }
    if (inheritWidth && !ioWidth.has_value() && ep->opts.width.has_value()) {
        ioWidth = ep->opts.width;
    }
    if (!ioHeight.has_value() && ep->opts.height.has_value()) {
        ioHeight = ep->opts.height;
    }
}

void Control::ForwardParentElementInteraction(
    Context& ctx,
    bool disabled,
    Response& ioRes
) noexcept {
    if (disabled) {
        return;
    }
    Node* parent = ctx.GetCurrentContainer();
    if (!parent || parent->type != NodeType::Element) {
        return;
    }
    const auto* ep = parent->As<ElementPayload>();
    if (!ep) {
        return;
    }
    ioRes.hovered = ioRes.hovered || ep->interactionRes.hovered;
    ioRes.pressed = ioRes.pressed || ep->interactionRes.pressed;
    ioRes.clicked = ioRes.clicked || ep->interactionRes.clicked;
    ioRes.doubleClicked = ioRes.doubleClicked || ep->interactionRes.doubleClicked;
    ioRes.rightClicked = ioRes.rightClicked || ep->interactionRes.rightClicked;
    if (!ioRes.clicked && ctx.GetActiveId() == ioRes.id && ctx.IsMouseReleased() && ep->interactionRes.hovered) {
        ioRes.clicked = true;
        ctx.SetActiveId(Id::Null());
    }
}

void Control::SetupNode(
    Context& ctx,
    Node* node,
    const ControlSpec& spec,
    StretchMode stretch,
    Align standaloneAlign
) noexcept {
    if (!node) {
        return;
    }
    Node* parent = node->parent;
    if (parent && parent->type == NodeType::Element) {
        const bool childWantsStretchH = (node->heightMode == SizeMode::Stretch);
        node->widthMode = SizeMode::Fit;
        node->heightMode = SizeMode::Fit;
        node->arrangeFn = nullptr;
        if (auto* ep = parent->As<ElementPayload>()) {
            ep->hasChildControl = true;
            ep->childControlSpec = spec;
            if (!ep->opts.stretch.has_value()) {
                ep->opts.stretch = stretch;
            }
            const StretchMode effStretch = ep->opts.stretch.value_or(stretch);
            parent->widthMode = (effStretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
            parent->heightMode = childWantsStretchH ? SizeMode::Stretch : SizeMode::Fit;
            ctx.AdaptChildToContainer(parent);
            if (!childWantsStretchH && parent->heightMode == SizeMode::Stretch &&
                (!parent->parent || parent->parent->heightMode != SizeMode::Fixed)) {
                parent->heightMode = SizeMode::Fit;
            }
            parent->visualHash = detail::HashArgs(
                parent->visualHash,
                node->visualHash,
                static_cast<uint8_t>(effStretch),
                spec.width.value_or(0.0f),
                spec.height.value_or(0.0f),
                spec.minWidth.value_or(0.0f),
                static_cast<uint8_t>(spec.defaultLayout),
                static_cast<uint8_t>(spec.defaultInlineOrder),
                spec.stretchable,
                spec.stretchHeightWhenStandalone
            );
        }
        if (node->hovered) parent->hovered = true;
        if (node->pressed) parent->pressed = true;
        if (node->focused || node->active) parent->active = true;
        if (node->minWidth > 0.0f) {
            parent->minWidth = std::max(parent->minWidth, node->minWidth);
        }
        if (node->minHeight > 0.0f) {
            parent->minHeight = std::max(parent->minHeight, node->minHeight);
        }
        return;
    }

    node->widthMode = (stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->ctrlStretchable = spec.stretchable;
    node->ctrlStretchH = spec.stretchHeightWhenStandalone;
    node->align = (standaloneAlign == Align::End)
        ? Alignment::End
        : ((standaloneAlign == Align::Center) ? Alignment::Center : Alignment::Start);
    node->fixedWidth = (stretch == StretchMode::Fill) ? spec.width.value_or(100.0f) : -1.0f;
    node->arrangeFn = &Control::ArrangeStandaloneNode;
    ctx.AdaptChildToContainer(node);
}

void Control::ArrangeStandaloneNode(
    Node* node,
    const Rect& availableRect,
    const UiTheme& theme
) noexcept {
    if (!node) {
        return;
    }
    Rect bounds = availableRect;
    Vec2 intrinsic = node->intrinsicSize;
    if (node->type == NodeType::Image && node->measureFn && bounds.w > 0.0f) {
        intrinsic = node->measureFn(node, bounds.w, nullptr, theme);
    }

    if (node->parent == nullptr) {
        if (node->widthMode == SizeMode::Fit) {
            bounds.w = intrinsic.x;
        }
        if (node->heightMode == SizeMode::Fit) {
            bounds.h = intrinsic.y;
        }
    }

    const bool isFill = (node->widthMode == SizeMode::Stretch && node->fixedWidth >= 0.0f);
    const float alignFactor = (node->align == Alignment::End)
        ? 1.0f
        : ((node->align == Alignment::Center) ? 0.5f : 0.0f);

    if (!node->ctrlStretchable && isFill && node->fixedWidth < 100.0f) {
        const float pct = std::clamp(node->fixedWidth, 0.0f, 100.0f);
        const float effW = bounds.w * (pct * 0.01f);
        bounds.x += (bounds.w - effW) * alignFactor;
        bounds.w = effW;
    }

    float ctrlW = std::min(bounds.w, intrinsic.x);
    if (node->ctrlStretchable && isFill) {
        const float pct = std::clamp(node->fixedWidth, 0.0f, 100.0f);
        ctrlW = bounds.w * (pct * 0.01f);
    }

    const float ctrlX = bounds.x + (bounds.w - ctrlW) * alignFactor;
    const float ctrlH = node->ctrlStretchH ? bounds.h : std::min(intrinsic.y, bounds.h);
    const float ctrlY = node->ctrlStretchH ? bounds.y : (bounds.y + (bounds.h - ctrlH) * 0.5f);
    node->computedRect = Rect(ctrlX, ctrlY, ctrlW, ctrlH);
}

} // namespace guikit
