#pragma once

#include <cstdint>
#include <optional>
#include <source_location>
#include "guikit/core/control.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "keel/string.h"

namespace guikit {

class Context;
class ICanvas;
struct Node;

/// Configuration options for Pagination bar widget.
struct PaginationOptions {
    uint32_t siblingCount{1};        ///< Number of page buttons shown on each side of active page
    bool showFirstLast{true};        ///< Show « (First) and » (Last) buttons
    bool showPrevNext{true};         ///< Show ‹ (Prev) and › (Next) buttons
    bool showPageInfo{false};        ///< Show trailing "Page X of Y" summary label
    bool interactiveEllipsis{false}; ///< false = static "..." separator (MUI/Shadcn); true = fast-jump button on "..." (Ant Design)
    StateColor color{};
    StretchMode stretch{StretchMode::Compact};
    Align align{Align::Start};
    std::optional<float> width{std::nullopt};
    std::optional<float> height{std::nullopt};
    StateColor background{};
    Border border{};
    keel::StringRef tooltip{};
    bool disabled{false};
    const UiTheme* theme{nullptr};
    std::optional<float> scale{std::nullopt};
    std::optional<bool> debugBounds{std::nullopt};
    std::optional<bool> inspectMode{std::nullopt};
};

/// Internal runtime payload allocated in frame arena for Pagination nodes.
struct PaginationData {
    PaginationOptions opts{};
    size_t currentPage{0};
    size_t totalPages{1};
    int hoveredToken{-1};
    int pressedToken{-1};
};

/// Autonomous Pagination control widget (execution, measuring, and rendering).
class Pagination {
public:
    static Response Run(
        Context& ctx,
        size_t& currentPage,
        size_t totalPages,
        const PaginationOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    static Response Run(
        Context& ctx,
        int& currentPage,
        int totalPages,
        const PaginationOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    static Vec2 Measure(
        size_t currentPage,
        size_t totalPages,
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
