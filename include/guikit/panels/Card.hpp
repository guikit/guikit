#pragma once

#include <optional>
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

/// Options for configuring a visual Card surface container.
struct CardOptions {
    UiVariant variant{UiVariant::Default};
    float minWidth{0.0f};                           ///< Minimum card width constraint (0 = auto content intrinsic)
    std::optional<UiSize> gap{std::nullopt};        ///< Gap between child elements inside card (nullopt = from theme)
    std::optional<Padding> padding{std::nullopt};   ///< Card padding (nullopt = auto from theme)
    Color backgroundColor{Color::Transparent()};
    Color borderColor{Color::Transparent()};
    float borderWidth{1.0f};
    std::optional<UiSize> radius{std::nullopt};     ///< Corner radius token (nullopt = auto from theme)
    bool clipChildren{true};                        ///< Clip children to prevent content bleeding outside rounded card
    SizeMode widthMode{SizeMode::Stretch};
    SizeMode heightMode{SizeMode::Fit};
    std::optional<UiSize> size{std::nullopt};
    bool disabled{false};
};

} // namespace guikit
