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
    Icon icon{};
    UiVariant variant{UiVariant::Ghost};
    Placement iconPlacement{Placement::Leading};
    LabelVariants labelVariants{};
    bool stretch{false};        ///< Expand to fill available horizontal space in flex containers
    bool disabled{false};
    bool active{false};         ///< Toggle active/selected highlight state
    std::optional<UiSize> size{std::nullopt}; ///< Local size override (nullopt = inherit toolbar size)
};

/// Configuration options for Toolbar container.
struct ToolbarOptions {
    std::optional<UiSize> size{std::nullopt};       ///< Size token determining height, icon scale, and density
    std::optional<UiSize> gap{std::nullopt};        ///< Gap between toolbar items (nullopt = inherit size)
    std::optional<Padding> padding{std::nullopt};   ///< Internal toolbar padding token or explicit insets (nullopt = standard symmetric padding)
    std::optional<UiSize> radius{std::nullopt};     ///< Corner radius token (nullopt = inherit size)
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
        std::string_view label = {},
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
        std::string_view label,
        ToolButtonOptions opts,
        std::source_location loc = std::source_location::current()
    );

    static Vec2 MeasureToolButton(
        std::string_view label,
        const ToolButtonOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr
    );

    static void RenderToolButton(
        ICanvas& canvas,
        Rect bounds,
        std::string_view label,
        const ToolButtonOptions& opts,
        UiSize size,
        bool hovered,
        bool pressed,
        bool disabled
    );

    static void RenderSeparator(
        ICanvas& canvas,
        Rect bounds,
        Color color = Color::Transparent(),
        const UiTheme& theme = UiTheme::Dark()
    );
};

} // namespace guikit
