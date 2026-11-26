#include "guikit/surfaces/card.hpp"
#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/node.hpp"

namespace guikit {

ContainerScope detail::Card::Open(
    Context& ctx,
    const CardOptions& opts,
    keel::SourceLocation loc
) {
    const Id idOuter = ctx.MakeChildId(loc);

    Node* outerNode = ctx.CreateNode(NodeType::Card, idOuter, opts.disabled);
    const UiTheme& theme = outerNode->GetTheme(ctx.GetTheme());
    const keel::f32 density = outerNode->GetDensity();
    const bool activeDisabled = outerNode->disabled;
    outerNode->direction = FlexDirection::Column;
    outerNode->gap = theme.ResolveGap(opts.gap, UiSize::Md, density);
    if (opts.minWidth.has_value() && *opts.minWidth > 0.0f) {
        outerNode->minWidth = *opts.minWidth;
    }
    outerNode->widthMode = opts.width.ResolveMode(SizeMode::Stretch);
    outerNode->fixedWidth = opts.width.IsFixed() ? opts.width.Dp() : 0.0f;
    outerNode->heightMode = opts.height.ResolveMode(SizeMode::Fit);
    outerNode->fixedHeight = opts.height.IsFixed() ? opts.height.Dp() : 0.0f;
    if (opts.maxWidth.has_value() && *opts.maxWidth > 0.0f) {
        outerNode->maxWidth = *opts.maxWidth;
    }
    if (opts.minHeight.has_value() && *opts.minHeight > 0.0f) {
        outerNode->minHeight = *opts.minHeight;
    }
    if (opts.maxHeight.has_value() && *opts.maxHeight > 0.0f) {
        outerNode->maxHeight = *opts.maxHeight;
    }
    outerNode->flexGrow = Control::ResolveFlexGrow(outerNode->parent, opts.width, opts.height, opts.flexGrow);
    outerNode->padding = theme.ResolvePadding(opts.padding, UiSize::Md, density);
    outerNode->explicitRadius = opts.border.radius.has_value() ? 1 : 0;
    outerNode->explicitBorder = opts.border.HasExplicitStroke() ? 1 : 0;
    outerNode->cornerRadius = theme.ResolveRadius(opts.border.radius, UiSize::Md);
    outerNode->clipChildren = opts.clipChildren;
    outerNode->uiSize = UiSize::Md;
    outerNode->SetSurfaceRole(opts.surface);

    const auto& pal = theme.palette;
    const bool isRaised = (opts.surface == SurfaceRole::Raised || opts.surface == SurfaceRole::Popup);
    const bool isSunken = (opts.surface == SurfaceRole::Sunken);
    const Color defaultBg = isRaised ? pal.surfaceElevated : (isSunken ? pal.inputBackground : pal.surface);
    const Color defaultBorder = isSunken ? pal.inputBorder : pal.border;
    const bool wantsBgHover = opts.background.hover.has_value();
    const bool wantsBorderHover = opts.border.width > 0.0f && opts.border.color.hover.has_value();
    bool isHovered = false;
    if (!activeDisabled && (wantsBgHover || wantsBorderHover)) {
        const Rect prevRect = ctx.GetPrevFrameBounds(idOuter);
        if (prevRect.w > 0.0f && prevRect.h > 0.0f) {
            isHovered = ctx.IsRectHovered(idOuter, prevRect);
        }
    }
    const bool defaultBgOn = (opts.surface != SurfaceRole::None);
    outerNode->backgroundColor = opts.background.IsBackgroundEnabled(defaultBgOn)
        ? theme.ResolveColor(opts.background, wantsBgHover && isHovered, false, activeDisabled, defaultBg)
        : Color::Transparent();

    outerNode->borderWidth = (opts.surface == SurfaceRole::None && !opts.border.color.HasAny() && opts.border.width == 1.0f)
        ? 0.0f
        : opts.border.width;
    outerNode->borderColor = (outerNode->borderWidth > 0.0f)
        ? theme.ResolveColor(opts.border.color, wantsBorderHover && isHovered, false, activeDisabled, defaultBorder)
        : Color::Transparent();
    outerNode->visualHash = detail::HashArgs(keel::as<keel::u8>(theme.shellStyle), opts.background, opts.border, opts.surface, isHovered);

    ctx.FinishContainer(outerNode);
    return ContainerScope(&ctx, outerNode);
}

} // namespace guikit
