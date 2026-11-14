#include "guikit/layout/separator.hpp"
#include <algorithm>
#include <cmath>
#include "guikit/context.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/core/node.hpp"

namespace guikit {

void Separator::RenderLine(
    ICanvas& canvas,
    Vec2 p0,
    Vec2 p1,
    Color color,
    float thickness
) noexcept {
    if (color.a == 0 || thickness <= 0.0f) {
        return;
    }
    canvas.DrawLine(p0, p1, color, thickness);
}

void Separator::Render(
    ICanvas& canvas,
    Rect bounds,
    Color color,
    const UiTheme& theme,
    Orientation orientation,
    float insetRatio
) noexcept {
    const Color stroke = color.a > 0 ? color : theme.palette.borderSubtle;
    if (orientation == Orientation::Vertical) {
        const float midY = std::floor(bounds.Center().y) + 0.5f;
        const float padX = std::max(2.0f, bounds.w * insetRatio);
        RenderLine(
            canvas,
            Vec2(bounds.x + padX, midY),
            Vec2(bounds.Right() - padX, midY),
            stroke,
            1.0f
        );
    } else {
        const float midX = std::floor(bounds.Center().x) + 0.5f;
        const float padY = std::max(2.0f, bounds.h * insetRatio);
        RenderLine(
            canvas,
            Vec2(midX, bounds.y + padY),
            Vec2(midX, bounds.Bottom() - padY),
            stroke,
            1.0f
        );
    }
}

void Separator::Run(
    Context& ctx,
    float extentRatio,
    Color colorOverride,
    std::source_location loc,
    std::optional<Orientation> lineOrientationOverride
) {
    const Id id = ctx.MakeChildId(loc);
    const float scale = ctx.GetCurrentScale();
    Node* parent = ctx.GetCurrentContainer();
    const bool drawVerticalLine = lineOrientationOverride.has_value()
        ? (*lineOrientationOverride == Orientation::Vertical)
        : (parent != nullptr && parent->direction == FlexDirection::Row);

    Node* sep = ctx.CreateNode(NodeType::Separator, id);
    if (colorOverride.a > 0) {
        sep->borderColor = colorOverride;
    } else if (parent != nullptr && parent->borderColor.a > 0) {
        sep->borderColor = parent->borderColor;
    } else {
        sep->borderColor = ctx.GetTheme().palette.borderSubtle;
    }
    const float clampedExtent = (extentRatio > 0.0f) ? std::clamp(extentRatio, 0.05f, 1.0f) : 0.0f;
    sep->visualHash = detail::HashArgs(sep->borderColor, drawVerticalLine, clampedExtent);

    if (drawVerticalLine) {
        const bool isStatusBar = (parent != nullptr && parent->type == NodeType::StatusBar);
        sep->widthMode = SizeMode::Fixed;
        sep->fixedWidth = (isStatusBar ? 8.0f : 10.0f) * scale;
        if ((parent != nullptr && (parent->fixedHeight > 0.0f || parent->type == NodeType::Toolbar)) || clampedExtent > 0.0f || isStatusBar) {
            const float minR = isStatusBar ? 0.2f : 0.1f;
            const float maxR = isStatusBar ? 0.9f : 1.0f;
            const float ratio = (clampedExtent > 0.0f) ? std::clamp(clampedExtent, minR, maxR) : 0.55f;
            const float parentH = (parent != nullptr && parent->fixedHeight > 0.0f)
                ? parent->fixedHeight
                : ((isStatusBar ? 26.0f : 32.0f) * scale);
            sep->heightMode = SizeMode::Fixed;
            sep->fixedHeight = parentH * ratio;
        } else {
            sep->heightMode = SizeMode::Stretch;
        }
        sep->measureFn = [](const Node* n, float /*availableWidth*/, ICanvas* /*canvas*/, const UiTheme& /*theme*/) noexcept -> Vec2 {
            const bool sb = (n->parent != nullptr && n->parent->type == NodeType::StatusBar);
            return {
                n->fixedWidth > 0.0f ? n->fixedWidth : (sb ? 8.0f : 10.0f),
                n->fixedHeight > 0.0f ? n->fixedHeight : (sb ? 14.0f : 20.0f)
            };
        };
        sep->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
            if (n->parent != nullptr && n->parent->type == NodeType::StatusBar) {
                const Color stroke = n->borderColor.a > 0 ? n->borderColor : theme.palette.borderSubtle;
                const float midX = std::floor(n->computedRect.Center().x) + 0.5f;
                const float centerY = n->computedRect.Center().y;
                const float halfH = std::max(4.0f, std::round(n->computedRect.h * 0.42f));
                Separator::RenderLine(
                    canvas,
                    Vec2(midX, std::floor(centerY - halfH) + 0.5f),
                    Vec2(midX, std::floor(centerY + halfH) + 0.5f),
                    stroke,
                    1.0f
                );
                return;
            }
            Separator::Render(canvas, n->computedRect, n->borderColor, theme, Orientation::Horizontal, 0.20f);
        };
    } else {
        sep->widthMode = SizeMode::Stretch;
        if (clampedExtent > 0.0f && clampedExtent < 1.0f) {
            sep->fixedWidth = clampedExtent;
        }
        sep->heightMode = SizeMode::Fixed;
        sep->fixedHeight = 8.0f * scale;
        sep->measureFn = [](const Node* n, float /*availableWidth*/, ICanvas* /*canvas*/, const UiTheme& /*theme*/) noexcept -> Vec2 {
            const bool inToolbar = (n->parent != nullptr && n->parent->type == NodeType::Toolbar);
            return {
                inToolbar ? 20.0f : 0.0f,
                n->fixedHeight > 0.0f ? n->fixedHeight : 8.0f
            };
        };
        sep->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
            const float y = std::floor(n->computedRect.Center().y) + 0.5f;
            const Color stroke = n->borderColor.a > 0 ? n->borderColor : theme.palette.borderSubtle;
            float padX = 2.0f;
            if (n->fixedWidth > 0.0f && n->fixedWidth < 1.0f) {
                padX = std::max(0.0f, n->computedRect.w * (1.0f - n->fixedWidth) * 0.5f);
            } else if (n->parent && n->parent->type == NodeType::Toolbar) {
                padX = std::max(2.0f, n->computedRect.w * 0.18f);
            } else if (n->parent && n->parent->type == NodeType::Sidebar && n->parent->sidebarCompact) {
                padX = std::min(6.0f, n->computedRect.w * 0.18f);
            }
            Separator::RenderLine(
                canvas,
                Vec2(n->computedRect.x + padX, y),
                Vec2(n->computedRect.Right() - padX, y),
                stroke,
                1.0f
            );
        };
    }
}

} // namespace guikit
