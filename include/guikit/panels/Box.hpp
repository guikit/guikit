#pragma once
 
#include <optional>
#include <source_location>
#include "guikit/core/ContainerScope.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

struct UiTheme;
class Context;

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
    const UiTheme* theme{nullptr};
    std::optional<float> scale{std::nullopt};
};

/// Autonomous immediate mode Box container entry point.
class Box {
public:
    [[nodiscard]] static ContainerScope Open(
        Context& ctx,
        const BoxOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );
};

} // namespace guikit
