#pragma once

#include <initializer_list>
#include <optional>
#include <span>
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

/// Trigger mode for opening a ContextMenu popup.
enum class ContextMenuTrigger : uint8_t {
    RightClick = 0, ///< Opens on right mouse click within the trigger rect (positioned at cursor)
    LeftClick,      ///< Opens on left mouse click within the trigger rect (anchored below/above rect)
    Manual          ///< Controlled programmatically via external `bool& open`
};

/// Single entry inside a ContextMenu popup.
struct ContextMenuItem {
    UiText label{};
    UiText shortcut{};                     ///< Right-aligned keyboard shortcut hint (e.g. "Ctrl+C")
    Icon icon{};                           ///< Optional leading icon
    UiVariant variant{UiVariant::Default}; ///< Semantic variant (e.g. UiVariant::Danger for "Delete")
    bool separatorAfter{false};            ///< Draw a 1px horizontal divider below this item
    bool checked{false};                   ///< Checkmark state for toggleable items
    bool hasCheck{false};                  ///< Show/reserve leading checkmark slot
    bool disabled{false};
    bool isSeparatorOnly{false};           ///< Pure horizontal divider line

    [[nodiscard]] static constexpr ContextMenuItem Separator() noexcept {
        ContextMenuItem item{};
        item.isSeparatorOnly = true;
        return item;
    }
};

/// Configuration options for a ContextMenu popup.
struct ContextMenuOptions {
    ContextMenuTrigger trigger{ContextMenuTrigger::RightClick};
    std::optional<float> minWidth{188.0f};
    std::optional<float> width{std::nullopt};
    std::optional<float> borderRadius{8.0f};
    bool disabled{false};
    const UiTheme* theme{nullptr};
    std::optional<float> scale{std::nullopt};
};

/// Extended response returned by `ui.ContextMenu(...)`.
struct ContextMenuResponse : public Response {
    int clickedIndex{-1}; ///< Index of the menu item activated this frame (-1 if none)
    bool isOpen{false};   ///< True while the context menu popup is open
};

/// Floating ContextMenu / Dropdown action menu component.
class ContextMenu {
public:
    static ContextMenuResponse Run(
        Context& ctx,
        bool* externalOpen,
        std::optional<Vec2> explicitAnchorPos,
        Rect triggerRect,
        std::span<const ContextMenuItem> items,
        const ContextMenuOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );
};

} // namespace guikit
