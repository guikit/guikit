#pragma once

#include <initializer_list>
#include <optional>
#include <span>
#include <source_location>
#include <string_view>
#include "guikit/core/container_scope.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"
#include "guikit/panels/context_menu.hpp"

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

/// Horizontal overflow policy for a Toolbar when tools exceed available width.
enum class ToolbarOverflow : uint8_t {
    AutoCompact, ///< Collapse Icon+Text tools into square Icon-only tools (with tooltips), then scroll if still overflowing
    Scroll,      ///< Keep labels intact and scroll horizontally with navigation chevrons and mouse wheel
    Wrap,        ///< Wrap overflowing tools onto subsequent rows and expand toolbar height
    Clip         ///< Clip overflowing tools to the toolbar content viewport
};

/// Configuration options for a ToolButton / Toolbar item.
struct ToolButtonOptions {
    UiText text{};
    UiText tooltip{};
    Icon icon{};
    StateColor color{StateColor::Ghost()};
    Placement iconPlacement{Placement::Leading};
    Align align{Align::Center};                 ///< Content horizontal alignment (Start = left, Center, End = right)
    StretchMode stretch{StretchMode::Compact};  ///< Sizing behavior: Compact, Space, Fill
    std::optional<float> width{std::nullopt};   ///< Explicit capsule width in pixels
    std::optional<float> minWidth{std::nullopt};///< Minimum capsule width in pixels
    bool disabled{false};
    bool active{false};                         ///< Toggle active/selected highlight state
    bool square{false};                         ///< Force square (defaultHeight x defaultHeight) button geometry
    std::optional<Color> iconColor{std::nullopt}; ///< Optional custom icon tint override
    UiText badge{};                             ///< Optional compact corner/trailing badge counter
};

/// Extended response returned by Toolbar SplitButton and SplitToggle controls.
struct SplitButtonResponse : public Response {
    int selectedIndex{-1};        ///< Selected item index (or clicked item index for stateless SplitButton)
    bool toggled{false};          ///< True when the main button action/toggle was triggered this frame
    bool selectionChanged{false}; ///< True when a dropdown item was selected this frame
    bool isOpen{false};           ///< True while the dropdown selector menu is open
};

/// Configuration options for Toolbar container.
struct ToolbarOptions {
    ToolbarStyle style{ToolbarStyle::Surface};              ///< Surface (default), Header (flush), or Floating (compact island)
    Orientation orientation{Orientation::Horizontal};       ///< Horizontal bar or Vertical tool palette
    ToolbarOverflow overflow{ToolbarOverflow::AutoCompact}; ///< Overflow behavior when tools exceed horizontal space
    std::optional<float> gap{std::nullopt};                 ///< Gap between toolbar items (nullopt = inherit size)
    std::optional<Padding> padding{std::nullopt};       ///< Internal toolbar padding token or explicit insets
    StateColor background{};
    Border border{};
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

    /// Adds a fixed non-stretching gap in logical dp along the toolbar's main axis.
    void Gap(float sizeDp = 8.0f, std::source_location loc = std::source_location::current());

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
        if (isSelected && (!opts.color.HasAny() || opts.color == StateColor::Ghost())) {
            opts.color = StateColor::Primary();
        } else if (!isSelected) {
            opts.color = StateColor::Ghost();
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

    /// Adds a split tool button: left zone triggers the primary action, right chevron opens a ContextMenu.
    SplitButtonResponse SplitButton(
        ToolButtonOptions opts,
        std::span<const ContextMenuItem> menuItems,
        ContextMenuOptions menuOpts = {},
        std::source_location loc = std::source_location::current()
    );

    SplitButtonResponse SplitButton(
        ToolButtonOptions opts,
        std::initializer_list<ContextMenuItem> menuItems,
        ContextMenuOptions menuOpts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return SplitButton(
            std::move(opts),
            std::span<const ContextMenuItem>(menuItems.begin(), menuItems.size()),
            std::move(menuOpts),
            loc
        );
    }

    /// Adds a split toggle button: left zone toggles `active`, right chevron selects `selectedIndex` from `items`.
    SplitButtonResponse SplitToggle(
        bool& active,
        int& selectedIndex,
        std::span<const std::string_view> items,
        ToolButtonOptions opts = {},
        ContextMenuOptions menuOpts = {},
        std::source_location loc = std::source_location::current()
    );

    template <IndexLike IndexT>
        requires(!std::is_same_v<std::remove_cv_t<IndexT>, int>)
    SplitButtonResponse SplitToggle(
        bool& active,
        IndexT& selectedIndex,
        std::span<const std::string_view> items,
        ToolButtonOptions opts = {},
        ContextMenuOptions menuOpts = {},
        std::source_location loc = std::source_location::current()
    ) {
        int raw = static_cast<int>(selectedIndex);
        const SplitButtonResponse r = SplitToggle(active, raw, items, std::move(opts), std::move(menuOpts), loc);
        if (r.selectionChanged && raw >= 0) {
            selectedIndex = static_cast<IndexT>(raw);
        }
        return r;
    }

    template <IndexLike IndexT>
    SplitButtonResponse SplitToggle(
        bool& active,
        IndexT& selectedIndex,
        std::initializer_list<std::string_view> items,
        ToolButtonOptions opts = {},
        ContextMenuOptions menuOpts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return SplitToggle(
            active,
            selectedIndex,
            std::span<const std::string_view>(items.begin(), items.size()),
            std::move(opts),
            std::move(menuOpts),
            loc
        );
    }

    /// Adds a split toggle button with rich ContextMenuItem entries (supporting icons, shortcuts, disabled items).
    SplitButtonResponse SplitToggle(
        bool& active,
        int& selectedIndex,
        std::span<const ContextMenuItem> items,
        ToolButtonOptions opts = {},
        ContextMenuOptions menuOpts = {},
        std::source_location loc = std::source_location::current()
    );

    template <IndexLike IndexT>
        requires(!std::is_same_v<std::remove_cv_t<IndexT>, int>)
    SplitButtonResponse SplitToggle(
        bool& active,
        IndexT& selectedIndex,
        std::span<const ContextMenuItem> items,
        ToolButtonOptions opts = {},
        ContextMenuOptions menuOpts = {},
        std::source_location loc = std::source_location::current()
    ) {
        int raw = static_cast<int>(selectedIndex);
        const SplitButtonResponse r = SplitToggle(active, raw, items, std::move(opts), std::move(menuOpts), loc);
        if (r.selectionChanged && raw >= 0) {
            selectedIndex = static_cast<IndexT>(raw);
        }
        return r;
    }

    template <IndexLike IndexT>
    SplitButtonResponse SplitToggle(
        bool& active,
        IndexT& selectedIndex,
        std::initializer_list<ContextMenuItem> items,
        ToolButtonOptions opts = {},
        ContextMenuOptions menuOpts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return SplitToggle(
            active,
            selectedIndex,
            std::span<const ContextMenuItem>(items.begin(), items.size()),
            std::move(opts),
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

    static SplitButtonResponse SplitButton(
        Context& ctx,
        const ToolButtonOptions& opts,
        std::span<const ContextMenuItem> menuItems,
        ContextMenuOptions menuOpts = {},
        std::source_location loc = std::source_location::current()
    );

    static void RenderSeparator(
        ICanvas& canvas,
        Rect bounds,
        Color color = Color::Transparent(),
        const UiTheme& theme = UiTheme::Dark(),
        Orientation orientation = Orientation::Horizontal
    );

    static void Arrange(
        Node* node,
        const Rect& availableRect,
        const UiTheme& theme = UiTheme::Dark()
    );

    [[nodiscard]] static Rect GetContentClipRect(const Node* node) noexcept;
};

} // namespace guikit
