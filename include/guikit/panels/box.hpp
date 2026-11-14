#pragma once
 
#include <optional>
#include <source_location>
#include "guikit/core/container_scope.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"

namespace guikit {

struct UiTheme;
class Context;

/// Options for configuring a visual Box container.
struct BoxOptions {
    std::optional<float> gap{std::nullopt};        ///< Gap between stacked children (nullopt = from theme)
    SizeMode widthMode{SizeMode::Stretch};
    SizeMode heightMode{SizeMode::Fit};
    std::optional<float> width{std::nullopt};
    std::optional<float> height{std::nullopt};
    std::optional<float> minWidth{std::nullopt};
    std::optional<float> maxWidth{std::nullopt};
    std::optional<float> minHeight{std::nullopt};
    std::optional<float> maxHeight{std::nullopt};
    float flexGrow{1.0f};
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
