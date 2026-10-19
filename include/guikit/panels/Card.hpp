#pragma once

#include <optional>
#include <source_location>
#include "guikit/core/ContainerScope.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

struct UiTheme;
class Context;

/// Options for configuring a visual Card surface container.
struct CardOptions {
    UiVariant variant{UiVariant::Default};
    float minWidth{0.0f};                           ///< Minimum card width constraint (0 = auto content intrinsic)
    std::optional<float> gap{std::nullopt};        ///< Gap between child elements inside card (nullopt = from theme)
    std::optional<Padding> padding{std::nullopt};   ///< Card padding (nullopt = auto from theme)
    StateColor background{};
    Border border{.width = 1.0f};
    bool clipChildren{true};                        ///< Clip children to prevent content bleeding outside rounded card
    SizeMode widthMode{SizeMode::Stretch};
    SizeMode heightMode{SizeMode::Fit};
    bool disabled{false};
    const UiTheme* theme{nullptr};
    std::optional<float> scale{std::nullopt};
    std::optional<bool> debugBounds{std::nullopt};
    std::optional<bool> inspectMode{std::nullopt};
};

/// Autonomous immediate mode Card surface container entry point.
class Card {
public:
    [[nodiscard]] static ContainerScope Open(
        Context& ctx,
        const CardOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );
};

} // namespace guikit
