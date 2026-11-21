#include "guikit/layout/spacer.hpp"
#include "guikit/context.hpp"
#include "guikit/core/node.hpp"

namespace guikit {

void Spacer::Run(
    Context& ctx,
    keel::f32 flex,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    Node* node = ctx.CreateNode(NodeType::Spacer, id);
    node->isSpacer = true;
    node->flexGrow = flex;
    if (node->parent && node->parent->type == NodeType::Toolbar && node->parent->direction == FlexDirection::Column) {
        node->widthMode = SizeMode::Fit;
        node->heightMode = SizeMode::Stretch;
    } else if (node->parent && (node->parent->type == NodeType::Toolbar || node->parent->type == NodeType::StatusBar) &&
               node->parent->direction == FlexDirection::Row) {
        node->widthMode = SizeMode::Stretch;
        node->heightMode = SizeMode::Fit;
    } else {
        node->widthMode = SizeMode::Stretch;
        node->heightMode = SizeMode::Stretch;
    }
    node->measureFn = [](const Node*, keel::f32, ICanvas*, const UiTheme&) noexcept -> Vec2 {
        return {0.0f, 0.0f};
    };
}

void Spacer::Gap(
    Context& ctx,
    keel::f32 sizeDp,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    Node* node = ctx.CreateNode(NodeType::Spacer, id);
    node->isSpacer = false;
    node->flexGrow = 0.0f;
    const keel::f32 scaled = keel::max(0.0f, sizeDp * ctx.GetDensity());
    const bool horizontalParent = node->parent != nullptr &&
        (node->parent->direction == FlexDirection::Row || node->parent->direction == FlexDirection::Wrap);
    node->widthMode = SizeMode::Fixed;
    node->heightMode = SizeMode::Fixed;
    node->fixedWidth = horizontalParent ? scaled : 0.0f;
    node->fixedHeight = horizontalParent ? 0.0f : scaled;
    node->measureFn = [](const Node* n, keel::f32, ICanvas*, const UiTheme&) noexcept -> Vec2 {
        return {n->fixedWidth, n->fixedHeight};
    };
}

} // namespace guikit
