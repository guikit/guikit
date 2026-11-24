#pragma once

#include "guikit/context.hpp"
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
    keel::u32 siblingCount{1};       ///< Number of page buttons shown on each side of active page
    bool showFirstLast{true};        ///< Show « (First) and » (Last) buttons
    bool showPrevNext{true};         ///< Show ‹ (Prev) and › (Next) buttons
    bool showPageInfo{false};        ///< Show trailing "Page X of Y" summary label
    bool interactiveEllipsis{false}; ///< false = static "..." separator (MUI/Shadcn); true = fast-jump button on "..." (Ant Design)
    UiText tooltip{};                ///< Optional floating tooltip text on hover
    bool disabled{false};                            ///< Interaction suppression
    UiSize size{UiSize::Md};                         ///< Control size token (`Xs`..`Xl`, `Dp(...)`, `Factor(...)`)
    AxisSize width{};                                ///< Horizontal sizing policy (`Auto` -> `Fit`, `Stretch`, or explicit `dp`)
    AxisSize height{};                               ///< Vertical sizing policy (`Auto` -> `Fit`, `Stretch`, or explicit `dp`)
    keel::Optional<keel::f32> minWidth{keel::kNullOpt};  ///< Optional minimum width in logical dp
    keel::Optional<keel::f32> maxWidth{keel::kNullOpt};  ///< Optional maximum width in logical dp
    keel::Optional<keel::f32> minHeight{keel::kNullOpt}; ///< Optional minimum height in logical dp
    keel::Optional<keel::f32> maxHeight{keel::kNullOpt}; ///< Optional maximum height in logical dp
    StateColor color{};                              ///< Per-state semantic accent color override
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
namespace detail {

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

} // namespace detail

template <IndexLike IndexT, IndexLike CountT>
    requires (!keel::IsConst<IndexT>)
Response Pagination(
    Context& ui,
    IndexT& currentPage,
    CountT totalPages,
    PaginationOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    if constexpr (keel::IsSame<keel::Decay<IndexT>, keel::usize>) {
        return detail::Pagination::Run(ui, currentPage, keel::as<keel::usize>(totalPages), opts, loc);
    } else {
        keel::i32 page = keel::as<keel::i32>(currentPage);
        Response r = detail::Pagination::Run(ui, page, keel::as<keel::i32>(totalPages), opts, loc);
        if (r.changed && page >= 0) currentPage = detail::FromIndexLike<IndexT>(page);
        return r;
    }
}

} // namespace guikit
