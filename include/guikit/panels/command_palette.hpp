#pragma once

#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"
#include "keel/core.h"
#include "keel/optional.h"
#include "keel/ref.h"

namespace guikit {

class Context;

struct CommandPaletteItem {
    keel::StringRef label{};       ///< Primary command title
    keel::StringRef category{};    ///< Optional category group/prefix (e.g. "Navigation", "View", "DSP")
    keel::StringRef description{}; ///< Optional secondary description
    keel::StringRef shortcut{};    ///< Optional keyboard shortcut badge (e.g. "Ctrl+K", "F5")
    Icon icon{};                   ///< Optional leading icon
    bool disabled{false};
};

struct CommandPaletteOptions {
    UiText placeholder{"Type a command or search..."};
    keel::Optional<float> width{keel::kNullOpt}; ///< Explicit width in dp (nullopt or <= 0 = auto-fit content within [minWidth..maxWidth])
    float minWidth{480.0f};                      ///< Minimum width in dp when auto-fitting content
    float maxWidth{780.0f};                      ///< Maximum width in dp when auto-fitting content
    keel::u32 maxVisibleItems{9};
    float backdropOpacity{0.45f};
    keel::Optional<Color> backdropColor{keel::kNullOpt};
    bool closeOnBackdropClick{true};
    bool closeOnEscape{true};
    bool showFooterHints{true};
    bool confineToParent{false};
    StateColor background{};
    Border border{.width = 1.0f, .radius = 10.0f};
    const UiTheme* theme{nullptr};
    keel::Optional<float> scale{keel::kNullOpt};
};

struct CommandPaletteResponse : public Response {
    keel::i32 selectedIndex{-1}; ///< Index in `items` triggered this frame (-1 if none)
};

class CommandPalette {
public:
    [[nodiscard]] static CommandPaletteResponse Run(
        Context& ctx,
        bool& isOpen,
        keel::ArrayRef<CommandPaletteItem> items,
        const CommandPaletteOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );
};

} // namespace guikit
