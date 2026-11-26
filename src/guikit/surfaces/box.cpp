#include "guikit/surfaces/box.hpp"
#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/node.hpp"

namespace guikit {

ContainerScope detail::Box::Open(
    Context& ctx,
    const BoxOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    Node* node = ctx.CreateNode(NodeType::Box, id, opts.disabled);
    const UiTheme& theme = node->GetTheme(ctx.GetTheme());
    const keel::f32 density = node->GetDensity();
    const bool activeDisabled = node->disabled;
    node->direction = FlexDirection::Column;
    node->explicitGap = opts.gap.has_value() ? 1 : 0;
    node->explicitPadding = opts.padding.has_value() ? 1 : 0;
    node->isShell = opts.shell;
    node->gap = theme.ResolveGap(opts.gap, UiSize::Md, density);
    node->padding = theme.ResolvePadding(opts.padding, UiSize::None, density);
    const auto& pal = theme.palette;
    const bool hasSurfaceRole = (opts.surface != SurfaceRole::None);
    const Color defaultBg = (opts.surface == SurfaceRole::Raised || opts.surface == SurfaceRole::Popup)
        ? pal.surfaceElevated
        : (opts.surface == SurfaceRole::Sunken ? pal.inputBackground : pal.surface);
    const Color defaultBorder = (opts.surface == SurfaceRole::Sunken)
        ? pal.inputBorder
        : pal.border;
    node->backgroundColor = opts.background.IsBackgroundEnabled(hasSurfaceRole)
        ? theme.ResolveColor(opts.background, false, false, activeDisabled, defaultBg)
        : Color::Transparent();
    const keel::f32 bw = (opts.border.width > 0.0f)
        ? opts.border.width
        : (opts.border.color.HasAny() ? 1.0f : 0.0f);
    node->borderWidth = bw;
    node->borderColor = (bw > 0.0f) ? theme.ResolveColor(opts.border.color, false, false, activeDisabled, defaultBorder) : Color::Transparent();
    ctx.ApplySurfaceGradients(node, theme, opts.background, (bw > 0.0f) ? opts.border.color : StateColor{}, false, false, activeDisabled);
    node->explicitRadius = opts.border.radius.has_value() ? 1 : 0;
    node->explicitBorder = opts.border.HasExplicitStroke() ? 1 : 0;
    node->cornerRadius = opts.border.radius.has_value()
        ? *opts.border.radius
        : (hasSurfaceRole ? theme.ResolveRadius(keel::kNullOpt, UiSize::Md) : 0.0f);
    node->SetSurfaceRole(hasSurfaceRole ? opts.surface : ((bw > 0.0f) ? SurfaceRole::Flat : SurfaceRole::None));
    node->clipChildren = opts.clipChildren;
    node->widthMode = opts.width.ResolveMode(SizeMode::Stretch);
    node->fixedWidth = opts.width.IsFixed() ? opts.width.Dp() : 0.0f;
    node->heightMode = opts.height.ResolveMode(SizeMode::Fit);
    node->fixedHeight = opts.height.IsFixed() ? opts.height.Dp() : 0.0f;
    if (opts.minWidth.has_value() && *opts.minWidth > 0.0f) {
        node->minWidth = *opts.minWidth;
    }
    if (opts.maxWidth.has_value() && *opts.maxWidth > 0.0f) {
        node->maxWidth = *opts.maxWidth;
    }
    if (opts.minHeight.has_value() && *opts.minHeight > 0.0f) {
        node->minHeight = *opts.minHeight;
    }
    if (opts.maxHeight.has_value() && *opts.maxHeight > 0.0f) {
        node->maxHeight = *opts.maxHeight;
    }
    node->flexGrow = Control::ResolveFlexGrow(node->parent, opts.width, opts.height, opts.flexGrow);

    node->uiSize = UiSize::Md;
    node->visualHash = detail::HashArgs(
        keel::as<keel::u8>(theme.shellStyle),
        opts.background,
        opts.border,
        keel::as<keel::u8>(opts.surface),
        node->backgroundColor,
        node->borderWidth,
        node->borderColor,
        node->cornerRadius
    );

    ctx.FinishContainer(node);
    return ContainerScope(&ctx, node);
}

} // namespace guikit
