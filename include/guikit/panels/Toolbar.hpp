#pragma once

#include <initializer_list>
#include <optional>
#include <span>
#include <source_location>
#include <string_view>
#include "guikit/core/ContainerScope.hpp"
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/graphics/Icon.hpp"
#include "guikit/panels/ContextMenu.hpp"

namespace guikit {

class Context;
class ICanvas;
struct Node;

/// Visual presentation and sizing mode for a Toolbar container.
enum class ToolbarStyle : uint8_t {
    Surface,    ///< Standard rounded surface bar stretching across the parent container
    Header,     ///< Flush edge-to-edge header strip (0px corner radius, 1px subtle border)
    Floating    ///< Compact fit-to-content floating pill/island with elevated surface & border
};

/// Configuration options for a ToolButton / Toolbar item.
struct ToolButtonOptions {
    UiText label{};
    UiText tooltip{};
    Icon icon{};
    UiVariant variant{UiVariant::Ghost};
    Placement iconPlacement{Placement::Leading};
    Align controlAlign{Align::Center};          ///< Content horizontal alignment (Start = left, Center, End = right)
    StretchMode stretch{StretchMode::Compact};  ///< Sizing behavior: Compact, Space, Fill
    std::optional<float> width{std::nullopt};   ///< Explicit capsule width in pixels
    std::optional<float> minWidth{std::nullopt};///< Minimum capsule width in pixels
    bool disabled{false};
    bool active{false};                         ///< Toggle active/selected highlight state
    bool square{false};                         ///< Force square (defaultHeight x defaultHeight) button geometry
    std::optional<Color> iconColor{std::nullopt}; ///< Optional custom icon tint override
    UiText badge{};                             ///< Optional compact corner/trailing badge counter
};

/// Configuration options for Toolbar container.
struct ToolbarOptions {
    ToolbarStyle style{ToolbarStyle::Surface};          ///< Surface (default), Header (flush), or Floating (compact island)
    Orientation orientation{Orientation::Horizontal};   ///< Horizontal bar or Vertical tool palette
    std::optional<float> gap{std::nullopt};             ///< Gap between toolbar items (nullopt = inherit size)
    std::optional<Padding> padding{std::nullopt};       ///< Internal toolbar padding token or explicit insets
    std::optional<float> borderRadius{std::nullopt};    ///< Corner radius in pixels (nullopt = inherit style/size)
    Color backgroundColor{Color::Transparent()};
    Color borderColor{Color::Transparent()};
    bool disabled{false};
    const UiTheme* theme{nullptr};
    std::optional<float> scale{std::nullopt};
    std::optional<bool> debugBounds{std::nullopt};
    std::optional<bool> inspectMode{std::nullopt};
};

/// RAII scope for Toolbar container.
class [[nodiscard]] ToolbarScope : public ContainerScope {
public:
    Response response{};

    ToolbarScope() noexcept : ContainerScope(nullptr, nullptr) {}
    ToolbarScope(Context* ctx, Node* node, Response res = {}, const ToolbarOptions& opts = {}) noexcept
        : ContainerScope(ctx, node), response(res), m_opts(opts) {}

    /// Adds a separator line between tool groups (vertical in Horizontal toolbars, horizontal in Vertical toolbars).
    void Separator(float heightRatio = 0.55f, std::source_location loc = std::source_location::current());

    /// Adds an elastic spacer pushing subsequent tools to the far end of the toolbar.
    void Spacer(std::source_location loc = std::source_location::current());

    /// Adds a compact tool button with icon and optional label.
    Response ToolButton(
        ToolButtonOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Alias for ToolButton representing a stateless command action.
    Response Action(
        ToolButtonOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return ToolButton(std::move(opts), loc);
    }

    /// Adds a stateful toggle button bound to `active`, highlighting when active and flipping `active` on click.
    Response Toggle(
        bool& active,
        ToolButtonOptions opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Adds a mutually exclusive tool selector bound to `selectedValue` (enum, int, size_t).
    template <typename T>
    Response Item(
        T& selectedValue,
        T itemValue,
        ToolButtonOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        const bool isSelected = (selectedValue == itemValue);
        opts.active = isSelected;
        if (isSelected && opts.variant == UiVariant::Ghost) {
            opts.variant = UiVariant::Primary;
        } else if (!isSelected) {
            opts.variant = UiVariant::Ghost;
        }
        Response res = ToolButton(std::move(opts), loc);
        if (res.clicked && !isSelected) {
            selectedValue = itemValue;
            res.changed = true;
            InvalidateContext();
        }
        return res;
    }

    /// Adds a dropdown tool button that opens a floating ContextMenu on click.
    ContextMenuResponse Dropdown(
        ToolButtonOptions opts,
        std::span<const ContextMenuItem> menuItems,
        ContextMenuOptions menuOpts = {},
        std::source_location loc = std::source_location::current()
    );

    ContextMenuResponse Dropdown(
        ToolButtonOptions opts,
        std::initializer_list<ContextMenuItem> menuItems,
        ContextMenuOptions menuOpts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Dropdown(
            std::move(opts),
            std::span<const ContextMenuItem>(menuItems.begin(), menuItems.size()),
            std::move(menuOpts),
            loc
        );
    }

    /// Direct access to underlying Context.
    [[nodiscard]] Context& Ctx() noexcept { return *m_ctx; }
    [[nodiscard]] const Response* operator->() const noexcept { return &response; }

private:
    void InvalidateContext() noexcept;
    ToolbarOptions m_opts{};
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
        const UiTheme& theme = UiTheme::Dark(),
        Orientation orientation = Orientation::Horizontal
    );
};

} // namespace guikit
