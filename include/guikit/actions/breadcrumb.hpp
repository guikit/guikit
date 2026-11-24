#pragma once

#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/response.hpp"
#include "guikit/style/theme.hpp"
#include "guikit/style/tokens.hpp"
#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"
#include "keel/core.h"
#include "keel/optional.h"
#include "keel/ref.h"
#include "keel/string.h"

namespace guikit {

class Context;
class ICanvas;
struct Node;

/// Visual separator glyph style rendered between consecutive breadcrumb items.
enum class BreadcrumbSeparator : keel::u8 {
    Chevron, ///< › chevron vector icon
    Slash,   ///< / slash separator
    Arrow,   ///< → arrow vector icon
    Dot      ///< • bullet dot separator
};

/// Individual path segment definition for Breadcrumb navigation widget.
struct BreadcrumbItem {
    keel::StringRef label{};
    Icon icon{};
    keel::StringRef badge{};
};

/// Configuration options for Breadcrumb navigation widget.
struct BreadcrumbOptions {
    BreadcrumbSeparator separator{BreadcrumbSeparator::Chevron};
    keel::u32 maxVisibleItems{0}; ///< 0 = show all (auto-collapses if viewport is narrower than trail); >= 2 collapses middle items into an interactive "..." dropdown menu when items.size() > maxVisibleItems
    UiText tooltip{};             ///< Optional floating tooltip text on hover
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

/// Internal runtime payload allocated in frame arena for Breadcrumb nodes.
struct BreadcrumbData {
    BreadcrumbOptions opts{};
    keel::ArrayRef<BreadcrumbItem> items{};
    keel::usize activeIndex{0};
    bool popupOpen{false};
    keel::i32 hoveredToken{-1};
    keel::i32 pressedToken{-1};
};

/// Autonomous Breadcrumb path navigation widget (execution, measuring, and rendering).
namespace detail {

class Breadcrumb {
public:
    static Response Run(
        Context& ctx,
        keel::usize& activeIndex,
        keel::ArrayRef<keel::StringRef> items,
        const BreadcrumbOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    static Response Run(
        Context& ctx,
        keel::usize& activeIndex,
        keel::ArrayRef<BreadcrumbItem> items,
        const BreadcrumbOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    static Response Run(
        Context& ctx,
        keel::i32& activeIndex,
        keel::ArrayRef<keel::StringRef> items,
        const BreadcrumbOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    static Response Run(
        Context& ctx,
        keel::i32& activeIndex,
        keel::ArrayRef<BreadcrumbItem> items,
        const BreadcrumbOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    static Vec2 Measure(
        keel::ArrayRef<BreadcrumbItem> items,
        const BreadcrumbOptions& opts,
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

template <IndexLike IndexT>
    requires (!keel::IsConst<IndexT>)
Response Breadcrumb(
    Context& ui,
    IndexT& activeIndex,
    keel::ArrayRef<BreadcrumbItem> items,
    BreadcrumbOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    if constexpr (keel::IsSame<keel::Decay<IndexT>, keel::usize>) {
        return detail::Breadcrumb::Run(ui, activeIndex, items, opts, loc);
    } else {
        keel::i32 idx = keel::as<keel::i32>(activeIndex);
        Response r = detail::Breadcrumb::Run(ui, idx, items, opts, loc);
        if (r.changed && idx >= 0) activeIndex = detail::FromIndexLike<IndexT>(idx);
        return r;
    }
}

template <IndexLike IndexT>
    requires (!keel::IsConst<IndexT>)
Response Breadcrumb(
    Context& ui,
    IndexT& activeIndex,
    keel::ArrayRef<keel::StringRef> items,
    BreadcrumbOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    if constexpr (keel::IsSame<keel::Decay<IndexT>, keel::usize>) {
        return detail::Breadcrumb::Run(ui, activeIndex, items, opts, loc);
    } else {
        keel::i32 idx = keel::as<keel::i32>(activeIndex);
        Response r = detail::Breadcrumb::Run(ui, idx, items, opts, loc);
        if (r.changed && idx >= 0) activeIndex = detail::FromIndexLike<IndexT>(idx);
        return r;
    }
}

} // namespace guikit
