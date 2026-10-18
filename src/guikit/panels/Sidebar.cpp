#include "guikit/panels/Sidebar.hpp"
#include "guikit/Context.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/panels/ScrollArea.hpp"

namespace guikit {

ContainerScope Sidebar::Open(
    Context& ctx,
    const SidebarOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool activeDisabled = opts.disabled || ctx.IsDisabled();
    const float activeScale = opts.scale.value_or(ctx.GetCurrentScale());
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);

    const UiTheme& theme = ctx.GetTheme();
    Node* node = ctx.CreateNode(NodeType::Sidebar, id);
    node->theme = pushedTheme;
    node->debugBoundsOverride = opts.debugBounds;
    node->inspectModeOverride = opts.inspectMode;
    node->direction = FlexDirection::Column;
    node->sidebarRight = (opts.side == SidebarSide::Right);
    node->sidebarScrollable = opts.scrollable;
    node->gap = theme.ResolveGap(opts.gap, activeSize);
    node->minWidth = theme.Scale(opts.minWidth);

    if (opts.width.has_value()) {
        node->widthMode = SizeMode::Fixed;
        node->fixedWidth = theme.Scale(*opts.width);
    } else {
        node->widthMode = SizeMode::Fit;
    }
    node->heightMode = SizeMode::Stretch;
    node->padding = theme.ResolvePadding(opts.padding, activeSize);
    node->cornerRadius = theme.ResolveRadius(opts.borderRadius, activeSize);
    node->clipChildren = opts.clipChildren;
    node->scrollOrientation = Orientation::Vertical;
    node->scrollbarPolicy = opts.scrollbar;
    node->uiSize = activeSize;
    node->disabled = activeDisabled;

    const auto& pal = theme.palette;
    node->backgroundColor = (opts.backgroundColor.a > 0) ? opts.backgroundColor : pal.surface;
    node->borderColor = (opts.borderColor.a > 0) ? opts.borderColor : pal.border;
    node->borderWidth = theme.Scale(opts.borderWidth);

    if (opts.scrollable) {
        node->postRenderFn = &ScrollArea::RenderScrollbar;

        auto& state = ctx.GetOrCreateState<ScrollState>(id);
        const Rect prevBounds = ctx.GetPrevFrameBounds(id);
        if (prevBounds.w > 0.0f && prevBounds.h > 0.0f && !node->disabled) {
            const Vec2 wheel = ctx.GetMouseWheel();
            if (prevBounds.Contains(ctx.GetMousePos()) && wheel.y != 0.0f) {
                const ScrollbarMetrics& sm = ctx.GetTheme().GetScrollbar(activeSize);
                state.offset.y -= wheel.y * sm.scrollStep;
                if (state.offset.y < 0.0f) state.offset.y = 0.0f;
            }
        }
        node->scrollOffset = state.offset;
    }

    ctx.PushScale(activeScale);
    ctx.PushSize(activeSize);
    ctx.PushDisabled(activeDisabled);
    ctx.PushContainer(node);
    return ContainerScope(&ctx, node);
}

} // namespace guikit
