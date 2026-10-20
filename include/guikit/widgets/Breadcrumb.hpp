#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <source_location>
#include <string_view>
#include "guikit/core/Control.hpp"
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/core/UiString.hpp"
#include "guikit/graphics/Icon.hpp"

namespace guikit {

class Context;
class ICanvas;
struct Node;

/// Visual separator glyph style rendered between consecutive breadcrumb items.
enum class BreadcrumbSeparator : uint8_t {
    Chevron, ///< › chevron vector icon
    Slash,   ///< / slash separator
    Arrow,   ///< → arrow vector icon
    Dot      ///< • bullet dot separator
};

/// Individual path segment definition for Breadcrumb navigation widget.
struct BreadcrumbItem {
    UiString label{};
    Icon icon{};
    UiString badge{};
};

/// Configuration options for Breadcrumb navigation widget.
struct BreadcrumbOptions {
    BreadcrumbSeparator separator{BreadcrumbSeparator::Chevron};
    uint32_t maxVisibleItems{0}; ///< 0 = show all; > 0 collapses middle items into an interactive "..." button when items.size() > maxVisibleItems
    StateColor color{};
    StretchMode stretch{StretchMode::Compact};
    Align align{Align::Start};
    std::optional<float> width{std::nullopt};
    std::optional<float> height{std::nullopt};
    StateColor background{};
    Border border{};
    UiString tooltip{};
    bool disabled{false};
    const UiTheme* theme{nullptr};
    std::optional<float> scale{std::nullopt};
    std::optional<bool> debugBounds{std::nullopt};
    std::optional<bool> inspectMode{std::nullopt};
};

/// Internal runtime payload allocated in frame arena for Breadcrumb nodes.
struct BreadcrumbData {
    BreadcrumbOptions opts{};
    std::span<const BreadcrumbItem> items{};
    size_t activeIndex{0};
    bool expanded{false};
    int hoveredToken{-1};
    int pressedToken{-1};
};

/// Autonomous Breadcrumb path navigation widget (execution, measuring, and rendering).
class Breadcrumb {
public:
    static Response Run(
        Context& ctx,
        size_t& activeIndex,
        std::span<const std::string_view> items,
        const BreadcrumbOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    static Response Run(
        Context& ctx,
        size_t& activeIndex,
        std::span<const BreadcrumbItem> items,
        const BreadcrumbOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    static Response Run(
        Context& ctx,
        int& activeIndex,
        std::span<const std::string_view> items,
        const BreadcrumbOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    static Response Run(
        Context& ctx,
        int& activeIndex,
        std::span<const BreadcrumbItem> items,
        const BreadcrumbOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    static Vec2 Measure(
        std::span<const BreadcrumbItem> items,
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
