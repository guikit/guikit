#pragma once

#include <optional>
#include <source_location>
#include "guikit/core/container_scope.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"

namespace guikit {

struct UiTheme;
class Context;

/// Options for configuring a vertical Column layout.
struct ColumnOptions {
    std::optional<float> gap{std::nullopt};
    std::optional<Padding> padding{std::nullopt};
    Alignment align{Alignment::Stretch};
    Justify justify{Justify::Start};
    SizeMode widthMode{SizeMode::Stretch};
    SizeMode heightMode{SizeMode::Stretch};
    std::optional<float> width{std::nullopt};
    std::optional<float> height{std::nullopt};
    std::optional<float> minWidth{std::nullopt};
    std::optional<float> maxWidth{std::nullopt};
    std::optional<float> minHeight{std::nullopt};
    std::optional<float> maxHeight{std::nullopt};
    float flexGrow{1.0f};
    bool disabled{false};
    const UiTheme* theme{nullptr};
    std::optional<float> scale{std::nullopt};
    std::optional<bool> debugBounds{std::nullopt};
    std::optional<bool> inspectMode{std::nullopt};
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
