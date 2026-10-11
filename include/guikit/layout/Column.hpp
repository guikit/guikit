#pragma once

#include <optional>
#include <source_location>
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/layout/Types.hpp"
#include "guikit/layout/Scope.hpp"

namespace guikit {

class Context;

/// Options for configuring a vertical Column layout.
struct ColumnOptions {
    std::optional<UiSize> gap{std::nullopt};
    Padding padding{};
    Alignment align{Alignment::Stretch};
    Justify justify{Justify::Start};
    SizeMode widthMode{SizeMode::Stretch};
    SizeMode heightMode{SizeMode::Stretch};
    std::optional<UiSize> size{std::nullopt};
    bool disabled{false};
};

using ColumnScope = ContainerScope;

/// Autonomous immediate mode Column layout entry point.
class Column {
public:
    static ColumnScope Open(
        Context& ctx,
        const ColumnOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );
};

} // namespace guikit
