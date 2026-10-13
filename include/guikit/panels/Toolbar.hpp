#pragma once

#include <optional>
#include <source_location>
#include <string_view>
#include "guikit/core/ContainerScope.hpp"
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/graphics/Icon.hpp"

namespace guikit {

class Context;
class ICanvas;
class Node;

/// Configuration options for a ToolButton.
struct ToolButtonOptions {
    std::string_view label{};
    Icon icon{};
    UiVariant variant{UiVariant::Ghost};
    Placement iconPlacement{Placement::Leading};
    Align align{Align::Center};  ///< Content horizontal alignment (Start = left, Center, End = right)
    LabelVariants labelVariants{};
    StretchMode stretch{StretchMode::Compact};  ///< Sizing behavior: Compact, Between, Fill
    std::optional<float> width{std::nullopt};       ///< Explicit capsule width in pixels
    std::optional<float> minWidth{std::nullopt};    ///< Minimum capsule width in pixels
    bool disabled{false};
    bool active{false};         ///< Toggle active/selected highlight state
    std::optional<UiSize> size{std::nullopt}; ///< Local size override (nullopt = inherit toolbar size)
};

/// Configuration options for Toolbar container.
struct ToolbarOptions {
    std::optional<UiSize> size{std::nullopt};       ///< Size token determining height, icon scale, and density
    std::optional<float> gap{std::nullopt};        ///< Gap between toolbar items (nullopt = inherit size)
    std::optional<Padding> padding{std::nullopt};   ///< Internal toolbar padding token or explicit insets (nullopt = standard symmetric padding)
    std::optional<float> radius{std::nullopt};     ///< Corner radius in pixels (nullopt = inherit size)
    Color backgroundColor{Color::Transparent()};
    Color borderColor{Color::Transparent()};
    bool disabled{false};
};

/// RAII scope for Toolbar container.
class [[nodiscard]] ToolbarScope : public ContainerScope {
public:
    using ContainerScope::ContainerScope;

    /// Adds a vertical separator bar between tool groups.
    void Separator(float heightRatio = 0.55f, std::source_location loc = std::source_location::current());

    /// Adds an elastic spacer pushing subsequent tools to the far right.
    void Spacer(std::source_location loc = std::source_location::current());

    /// Adds a compact tool button with icon and optional label.
    Response ToolButton(
        ToolButtonOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Direct access to underlying Context.
    [[nodiscard]] Context& Ctx() noexcept { return *m_ctx; }
};

/// Autonomous Toolbar widget logic (measuring, rendering, and tool button utilities).
class Toolbar {
public:
    [[nodiscard]] static ToolbarScope Open(
        Context& ctx,
        ToolbarOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    static Response ToolButton(
        Context& ctx,
        const ToolButtonOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    static void RenderSeparator(
        ICanvas& canvas,
        Rect bounds,
        Color color = Color::Transparent(),
        const UiTheme& theme = UiTheme::Dark()
    );
};

} // namespace guikit
