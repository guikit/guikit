#pragma once

#include <optional>
#include <source_location>
#include "guikit/core/ContainerScope.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

struct UiTheme;
class Context;

/// Options for configuring an auto-wrapping flow layout.
struct WrapOptions {
    std::optional<float> gap{std::nullopt};        ///< Horizontal gap between items on same line
    std::optional<float> runGap{std::nullopt};     ///< Vertical gap between consecutive wrapped lines
    std::optional<Padding> padding{std::nullopt};
    Alignment align{Alignment::Center};
    Justify justify{Justify::Start};
    SizeMode widthMode{SizeMode::Stretch};
    SizeMode heightMode{SizeMode::Fit};
    bool disabled{false};
    const UiTheme* theme{nullptr};
    std::optional<float> scale{std::nullopt};
};

/// Autonomous immediate mode Wrap layout entry point.
class Wrap {
public:
    [[nodiscard]] static ContainerScope Open(
        Context& ctx,
        const WrapOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );
};

} // namespace guikit
