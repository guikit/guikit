#include "guikit/core/control.hpp"
#include "guikit/context.hpp"
#include "guikit/core/element.hpp"
#include "guikit/core/node.hpp"

namespace guikit {

SizeMode Control::ResolveWidthMode(
    const Context& ctx,
    AxisSize width,
    SizeMode defaultMode
) noexcept {
    if (!width.IsAuto()) {
        return width.ResolveMode(defaultMode);
    }
    const Node* parent = ctx.GetCurrentContainer();
    if (parent != nullptr && parent->type == NodeType::Toolbar && parent->direction == FlexDirection::Row) {
        return SizeMode::Fit;
    }
    return defaultMode;
}

Vec2 Control::Measure(
    Vec2 intrinsicSize,
    const ControlSpec& spec,
    const UiTheme& theme
) noexcept {
    Vec2 result = intrinsicSize;
    if (spec.minWidth.has_value() && *spec.minWidth > 0.0f) {
        result.x = keel::max(result.x, theme.Scale(*spec.minWidth));
    }
    if (spec.minHeight.has_value() && *spec.minHeight > 0.0f) {
        result.y = keel::max(result.y, theme.Scale(*spec.minHeight));
    }
    if (spec.width.IsFixed()) {
        result.x = theme.Scale(spec.width.Dp());
    }
    if (spec.height.IsFixed()) {
        result.y = theme.Scale(spec.height.Dp());
    }
    return result;
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
    const ControlSpec& spec
) noexcept {
    if (!node) {
        return;
    }

    const UiTheme& nodeTheme = (node->theme != nullptr) ? *node->theme : ctx.GetTheme();
    if (spec.minWidth.has_value() && *spec.minWidth > 0.0f) {
        node->minWidth = keel::max(node->minWidth, nodeTheme.Scale(*spec.minWidth));
    }
    if (spec.minHeight.has_value() && *spec.minHeight > 0.0f) {
        node->minHeight = keel::max(node->minHeight, nodeTheme.Scale(*spec.minHeight));
    }

    if (spec.width.IsFixed()) {
        node->widthMode = SizeMode::Fixed;
        node->fixedWidth = nodeTheme.Scale(spec.width.Dp());
    } else {
        node->widthMode = ResolveWidthMode(ctx, spec.width, spec.defaultWidthMode);
        node->fixedWidth = 0.0f;
    }

    if (spec.height.IsFixed()) {
        node->heightMode = SizeMode::Fixed;
        node->fixedHeight = nodeTheme.Scale(spec.height.Dp());
    } else {
        node->heightMode = ResolveHeightMode(spec.height, spec.defaultHeightMode);
        node->fixedHeight = 0.0f;
    }

    Node* parent = node->parent;
    if (parent && parent->type == NodeType::Element) {
        if (auto* ep = parent->As<ElementPayload>()) {
            if (ep->controlChildCount == 0) {
                ep->firstChildSpec = spec;
            }
            ++ep->controlChildCount;
            if (node->widthMode == SizeMode::Stretch) {
                ep->anyChildStretchesW = true;
            }
            if (node->heightMode == SizeMode::Stretch) {
                ep->anyChildStretchesH = true;
            }
            Element::SyncWithChildren(ctx, parent);
        }
        return;
    }

    if (parent && (parent->type == NodeType::TreeItem ||
                   (parent->parent && parent->parent->type == NodeType::TreeItem))) {
        parent->visualHash = detail::HashArgs(
            parent->visualHash,
            node->visualHash,
            static_cast<keel::u8>(node->widthMode),
            static_cast<keel::u8>(node->heightMode),
            node->fixedWidth,
            node->fixedHeight
        );
        if (parent->parent && parent->parent->type == NodeType::TreeItem) {
            parent->parent->visualHash = detail::HashArgs(parent->parent->visualHash, parent->visualHash);
        }
        return;
    }

    ctx.AdaptChildToContainer(node);
    if (spec.height.IsFit()) {
        node->heightMode = SizeMode::Fit;
    }
}

} // namespace guikit
