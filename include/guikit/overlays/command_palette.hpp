#pragma once

#include "guikit/context.hpp"
#include "guikit/core/response.hpp"
#include "guikit/style/theme.hpp"
#include "guikit/style/tokens.hpp"
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
    keel::Optional<keel::f32> width{keel::kNullOpt}; ///< Explicit width in dp (nullopt or <= 0 = auto-fit content within [minWidth..maxWidth])
    keel::f32 minWidth{480.0f};                      ///< Minimum width in dp when auto-fitting content
    keel::f32 maxWidth{780.0f};                      ///< Maximum width in dp when auto-fitting content
    keel::u32 maxVisibleItems{9};
    keel::f32 backdropOpacity{0.45f};
    keel::Optional<Color> backdropColor{keel::kNullOpt};
    bool closeOnBackdropClick{true};
    bool closeOnEscape{true};
    bool showFooterHints{true};
    bool confineToParent{false};
    StateColor background{};
    Border border{.width = 1.0f};
    UiSize size{UiSize::Md};
};

struct CommandPaletteResponse : public Response {
    keel::i32 selectedIndex{-1}; ///< Index in `items` triggered this frame (-1 if none)
};

namespace detail {

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

} // namespace detail

inline CommandPaletteResponse CommandPalette(
    Context& ui,
    bool& isOpen,
    keel::ArrayRef<CommandPaletteItem> items,
    const CommandPaletteOptions& opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    return detail::CommandPalette::Run(ui, isOpen, items, opts, loc);
}

} // namespace guikit
