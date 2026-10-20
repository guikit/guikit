#pragma once

#include <optional>
#include <source_location>
#include "guikit/core/ContainerScope.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

struct UiTheme;
class Context;

/// Options for configuring a horizontal Row layout.
struct RowOptions {
    std::optional<float> gap{std::nullopt};
    std::optional<Padding> padding{std::nullopt};
    Alignment align{Alignment::Stretch};
    Justify justify{Justify::Start};
    std::optional<SizeMode> widthMode{std::nullopt};
    SizeMode heightMode{SizeMode::Fit};
    bool equalWidths{false};
    bool disabled{false};
    const UiTheme* theme{nullptr};
    std::optional<float> scale{std::nullopt};
};

/// Autonomous immediate mode Row layout entry point.
class Row {
public:
    [[nodiscard]] static ContainerScope Open(
        Context& ctx,
        const RowOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );
};

} // namespace guikit
