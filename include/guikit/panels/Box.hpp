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
    std::optional<float> gap{std::nullopt};        ///< Gap between stacked children (nullopt = from theme)
    SizeMode widthMode{SizeMode::Stretch};
    SizeMode heightMode{SizeMode::Fit};
    float fixedWidth{0.0f};
    float fixedHeight{0.0f};
    std::optional<Padding> padding{std::nullopt};   ///< Container padding (nullopt = zero)
    StateColor background{};
    Border border{};
    bool clipChildren{false};
    bool disabled{false};
    const UiTheme* theme{nullptr};
    std::optional<float> scale{std::nullopt};
    std::optional<bool> debugBounds{std::nullopt};
    std::optional<bool> inspectMode{std::nullopt};
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
