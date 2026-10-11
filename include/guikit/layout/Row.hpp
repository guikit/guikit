#pragma once

#include <optional>
#include <source_location>
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/layout/Types.hpp"
#include "guikit/layout/Scope.hpp"

namespace guikit {

class Context;

/// Options for configuring a horizontal Row layout.
struct RowOptions {
    std::optional<UiSize> gap{std::nullopt};
    Padding padding{};
    Alignment align{Alignment::Stretch};
    Justify justify{Justify::Start};
    SizeMode heightMode{SizeMode::Fit};
    std::optional<UiSize> size{std::nullopt};
    bool disabled{false};
};

using RowScope = ContainerScope;

/// Autonomous immediate mode Row layout entry point.
class Row {
public:
    static RowScope Open(
        Context& ctx,
        const RowOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );
};

} // namespace guikit
