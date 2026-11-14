#pragma once

#include "guikit/core/container_scope.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"
#include "keel/core.h"
#include "keel/optional.h"
#include "keel/ref.h"

namespace guikit {

class Context;
class ICanvas;

/// Trigger mode for opening a ContextMenu popup.
enum class ContextMenuTrigger : keel::u8 {
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
    keel::Optional<float> minWidth{188.0f};
    keel::Optional<float> width{keel::kNullOpt};
    StateColor background{};
    Border border{.width = 1.0f, .radius = 8.0f};
    bool disabled{false};
    const UiTheme* theme{nullptr};
    keel::Optional<float> scale{keel::kNullOpt};
};

/// Extended response returned by `ui.ContextMenu(...)`.
struct ContextMenuResponse : public Response {
    keel::i32 clickedIndex{-1}; ///< Index of the menu item activated this frame (-1 if none)
    bool isOpen{false};   ///< True while the context menu popup is open
};

/// Floating ContextMenu / Dropdown action menu component.
class ContextMenu {
public:
    static ContextMenuResponse Run(
        Context& ctx,
        bool* externalOpen,
        keel::Optional<Vec2> explicitAnchorPos,
        Rect triggerRect,
        keel::ArrayRef<ContextMenuItem> items,
        const ContextMenuOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );
};

} // namespace guikit
