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
    node->widthMode = SizeMode::Stretch;
    node->heightMode = SizeMode::Stretch;
}

} // namespace guikit
