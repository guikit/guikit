#pragma once

#include <optional>
#include <source_location>
#include "guikit/core/ContainerScope.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

struct UiTheme;
class Context;

/// Options for configuring a vertical Column layout.
struct ColumnOptions {
    std::optional<UiSize> gap{std::nullopt};
    std::optional<Padding> padding{std::nullopt};
    Alignment align{Alignment::Stretch};
    Justify justify{Justify::Start};
    SizeMode widthMode{SizeMode::Stretch};
    SizeMode heightMode{SizeMode::Stretch};
    std::optional<UiSize> size{std::nullopt};
    bool disabled{false};
    const UiTheme* theme{nullptr};
    std::optional<float> scale{std::nullopt};
};

/// Autonomous immediate mode Column layout entry point.
class Column {
public:
    [[nodiscard]] static ContainerScope Open(
        Context& ctx,
        const ColumnOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );
};

} // namespace guikit
