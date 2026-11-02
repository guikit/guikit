#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <source_location>
#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "guikit/core/ui_string.hpp"
#include "guikit/graphics/icon.hpp"

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
    std::optional<float> width{std::nullopt}; ///< Explicit width in dp (nullopt or <= 0 = auto-fit content within [minWidth..maxWidth])
    float minWidth{480.0f};                   ///< Minimum width in dp when auto-fitting content
    float maxWidth{780.0f};                   ///< Maximum width in dp when auto-fitting content
    uint32_t maxVisibleItems{9};
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
