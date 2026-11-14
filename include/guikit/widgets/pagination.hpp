#pragma once

#include "guikit/core/control.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "keel/core.h"
#include "keel/optional.h"
#include "keel/string.h"

namespace guikit {

class Context;
class ICanvas;
struct Node;

/// Configuration options for Pagination bar widget.
struct PaginationOptions {
    keel::u32 siblingCount{1};        ///< Number of page buttons shown on each side of active page
    bool showFirstLast{true};        ///< Show « (First) and » (Last) buttons
    bool showPrevNext{true};         ///< Show ‹ (Prev) and › (Next) buttons
    bool showPageInfo{false};        ///< Show trailing "Page X of Y" summary label
    bool interactiveEllipsis{false}; ///< false = static "..." separator (MUI/Shadcn); true = fast-jump button on "..." (Ant Design)
    StateColor color{};
    StretchMode stretch{StretchMode::Compact};
    Align align{Align::Start};
    keel::Optional<float> width{keel::kNullOpt};
    keel::Optional<float> widthRatio{keel::kNullOpt};
    keel::Optional<float> height{keel::kNullOpt};
    StateColor background{};
    Border border{};
    UiText tooltip{};
    bool disabled{false};
    const UiTheme* theme{nullptr};
    keel::Optional<float> scale{keel::kNullOpt};
    keel::Optional<bool> debugBounds{keel::kNullOpt};
    keel::Optional<bool> inspectMode{keel::kNullOpt};
};

/// Internal runtime payload allocated in frame arena for Pagination nodes.
struct PaginationData {
    PaginationOptions opts{};
    keel::usize currentPage{0};
    keel::usize totalPages{1};
    keel::i32 hoveredToken{-1};
    keel::i32 pressedToken{-1};
};

/// Autonomous Pagination control widget (execution, measuring, and rendering).
class Pagination {
public:
    static Response Run(
        Context& ctx,
        keel::usize& currentPage,
        keel::usize totalPages,
        const PaginationOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    static Response Run(
        Context& ctx,
        keel::i32& currentPage,
        keel::i32 totalPages,
        const PaginationOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    static Vec2 Measure(
        keel::usize currentPage,
        keel::usize totalPages,
        const PaginationOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme
    );

    static void Render(
        const Node* node,
        ICanvas& canvas,
        const UiTheme& theme
    );
};

} // namespace guikit
