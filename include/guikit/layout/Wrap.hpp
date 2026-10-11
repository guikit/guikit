#pragma once

#include <optional>
#include <source_location>
#include "guikit/core/ContainerScope.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

class Context;

/// Options for configuring an auto-wrapping flow layout.
struct WrapOptions {
    std::optional<UiSize> gap{std::nullopt};        ///< Horizontal gap between items on same line
    std::optional<UiSize> runGap{std::nullopt};     ///< Vertical gap between consecutive wrapped lines
    std::optional<Padding> padding{std::nullopt};
    Alignment align{Alignment::Center};
    Justify justify{Justify::Start};
    SizeMode widthMode{SizeMode::Stretch};
    SizeMode heightMode{SizeMode::Fit};
    std::optional<UiSize> size{std::nullopt};
    bool disabled{false};
};

/// Autonomous immediate mode Wrap layout entry point.
class Wrap {
public:
    static ContainerScope Open(
        Context& ctx,
        const WrapOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );
};

} // namespace guikit
