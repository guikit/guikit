#pragma once

#include <optional>
#include <source_location>
#include "guikit/core/ContainerScope.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

struct UiTheme;
class Context;

/// Horizontal docking edge for a Sidebar container.
enum class SidebarSide : uint8_t {
    Left,   ///< Docks to the left side of the parent container (stacked left-to-right in call order)
    Right   ///< Docks to the right side of the parent container (stacked right-to-left in call order)
};

/// Options for configuring a dockable vertical Sidebar surface container.
struct SidebarOptions {
    SidebarSide side{SidebarSide::Left};            ///< Docking side (Left by default, or Right)
    std::optional<float> width{std::nullopt};       ///< Explicit sidebar width in pixels (nullopt = Fit to content)
    float minWidth{0.0f};                           ///< Minimum sidebar width constraint in pixels
    bool scrollable{true};                          ///< Automatically scroll vertically when content overflows
    VisibilityPolicy scrollbar{VisibilityPolicy::Auto};
    std::optional<float> gap{std::nullopt};         ///< Gap between child elements inside sidebar (nullopt = from theme)
    std::optional<Padding> padding{std::nullopt};   ///< Sidebar internal padding (nullopt = auto from theme)
    Color backgroundColor{Color::Transparent()};
    Color borderColor{Color::Transparent()};
    float borderWidth{1.0f};
    std::optional<float> borderRadius{std::nullopt}; ///< Corner radius in pixels (nullopt = auto from theme)
    bool clipChildren{true};
    std::optional<UiSize> size{std::nullopt};
    bool disabled{false};
    const UiTheme* theme{nullptr};
    std::optional<float> scale{std::nullopt};
    std::optional<bool> debugBounds{std::nullopt};
    std::optional<bool> inspectMode{std::nullopt};
};

/// Autonomous immediate mode dockable Sidebar panel container entry point.
class Sidebar {
public:
    [[nodiscard]] static ContainerScope Open(
        Context& ctx,
        const SidebarOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );
};

} // namespace guikit
