#include "guikit/layout/Spacer.hpp"
#include "guikit/Context.hpp"
#include "guikit/core/Node.hpp"

namespace guikit {

void Spacer::Run(
    Context& ctx,
    float flex,
    std::source_location loc
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
    node->measureFn = [](const Node*, float, ICanvas*, const UiTheme&) noexcept -> Vec2 {
        return {0.0f, 0.0f};
    };
}

} // namespace guikit
