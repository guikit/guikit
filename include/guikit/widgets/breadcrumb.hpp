#pragma once

#include "guikit/core/control.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/tokens.hpp"
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
    StateColor color{};
    AxisSize width{};  ///< Horizontal sizing policy (`Auto` -> `Fit`, `Stretch`, or `Fixed(dp)`)
    AxisSize height{}; ///< Vertical sizing policy (`Auto` -> `Fit`, `Stretch`, or `Fixed(dp)`)
    UiText tooltip{};
    bool disabled{false};
    const UiTheme* theme{nullptr};
    keel::Optional<float> scale{keel::kNullOpt};
    keel::Optional<bool> debugBounds{keel::kNullOpt};
    keel::Optional<bool> inspectMode{keel::kNullOpt};
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

} // namespace guikit
