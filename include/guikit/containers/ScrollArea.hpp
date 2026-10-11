#pragma once

#include <optional>
#include "guikit/core/Theme.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/layout/Types.hpp"
#include "guikit/layout/Scope.hpp"

namespace guikit {

class ICanvas;
struct Node;

/// Options for configuring a ScrollArea viewport.
struct ScrollAreaOptions {
    Orientation orientation{Orientation::Vertical};
    VisibilityPolicy scrollbar{VisibilityPolicy::Auto};
    std::optional<UiSize> gap{std::nullopt};        ///< Gap between consecutive scroll items (nullopt = zero)
    std::optional<Padding> padding{std::nullopt};   ///< Viewport content padding (nullopt = zero)
    std::optional<UiSize> size{std::nullopt};
    bool disabled{false};
};

using ScrollAreaScope = ContainerScope;

/// ScrollArea container logic and scrollbar rendering.
class ScrollArea {
public:
    /// Renders scrollbar thumb and track after children (postRenderFn signature).
    static void RenderScrollbar(
        const Node* node,
        ICanvas& canvas,
        const UiTheme& theme = UiTheme::Dark()
    );
};

} // namespace guikit
