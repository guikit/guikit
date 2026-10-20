#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <source_location>
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/core/UiString.hpp"
#include "guikit/graphics/Icon.hpp"

namespace guikit {

class Context;

struct CommandPaletteItem {
    UiString label{};       ///< Primary command title
    UiString category{};    ///< Optional category group/prefix (e.g. "Navigation", "View", "DSP")
    UiString description{}; ///< Optional secondary description
    UiString shortcut{};    ///< Optional keyboard shortcut badge (e.g. "Ctrl+K", "F5")
    Icon icon{};            ///< Optional leading icon
    bool disabled{false};
};

struct CommandPaletteOptions {
    UiString placeholder{"Type a command or search..."};
    float width{520.0f};
    uint32_t maxVisibleItems{8};
    float backdropOpacity{0.45f};
    bool closeOnBackdropClick{true};
    bool closeOnEscape{true};
    bool showFooterHints{true};
    bool confineToParent{false};
    StateColor background{};
    Border border{.width = 1.0f, .radius = 10.0f};
    const UiTheme* theme{nullptr};
    std::optional<float> scale{std::nullopt};
};

struct CommandPaletteResponse : public Response {
    int selectedIndex{-1}; ///< Index in `items` triggered this frame (-1 if none)
};

class CommandPalette {
public:
    [[nodiscard]] static CommandPaletteResponse Run(
        Context& ctx,
        bool& isOpen,
        std::span<const CommandPaletteItem> items,
        const CommandPaletteOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );
};

} // namespace guikit
