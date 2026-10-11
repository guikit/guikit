#pragma once

#include <optional>
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/layout/Types.hpp"
#include "guikit/containers/Container.hpp"

namespace guikit {

/// Options for configuring a visual Box container.
struct BoxOptions {
    std::optional<UiSize> gap{std::nullopt};        ///< Gap between stacked children (nullopt = from theme)
    SizeMode widthMode{SizeMode::Stretch};
    SizeMode heightMode{SizeMode::Fit};
    float fixedWidth{0.0f};
    float fixedHeight{0.0f};
    std::optional<Padding> padding{std::nullopt};   ///< Container padding (nullopt = zero)
    Color backgroundColor{0, 0, 0, 0};
    Color borderColor{0, 0, 0, 0};
    float borderWidth{0.0f};
    std::optional<UiSize> radius{std::nullopt};     ///< Corner radius token (nullopt = zero / sharp)
    bool clipChildren{false};
    std::optional<UiSize> size{std::nullopt};
    bool disabled{false};
};

using BoxScope = ContainerScope;

} // namespace guikit
