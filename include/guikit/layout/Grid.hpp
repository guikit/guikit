#pragma once

#include <optional>
#include <source_location>
#include "guikit/core/ContainerScope.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

struct UiTheme;
class Context;

/// Options for configuring a 12-column responsive cell grid layout.
struct GridOptions {
    int columns{12};                                ///< Total number of columns in the grid (default: 12)
    float minColWidth{0.0f};                        ///< Minimum column width before wrapping items (0 = auto content intrinsic)
    std::optional<float> gap{std::nullopt};        ///< Horizontal gap between columns (nullopt = from theme)
    std::optional<float> runGap{std::nullopt};     ///< Vertical gap between wrapped rows (nullopt = match gap)
    std::optional<Padding> padding{std::nullopt};   ///< Container padding (nullopt = zero)
    Alignment align{Alignment::Stretch};
    Justify justify{Justify::Start};
    SizeMode widthMode{SizeMode::Stretch};
    SizeMode heightMode{SizeMode::Fit};
    bool disabled{false};
    const UiTheme* theme{nullptr};
    std::optional<float> scale{std::nullopt};
};

/// Options for configuring an individual grid cell slot.
struct CellOptions {
    float minWidth{0.0f};                           ///< Minimum width constraint (0 = auto content intrinsic)
    std::optional<float> gap{std::nullopt};        ///< Gap between stacked children inside this cell
    std::optional<Padding> padding{std::nullopt};   ///< Internal cell padding (nullopt = zero)
    Alignment align{Alignment::Stretch};
    Justify justify{Justify::Start};
    SizeMode heightMode{SizeMode::Fit};
    bool disabled{false};
    bool clipChildren{false};                       ///< Clip cell children to cell boundaries (default: false)
    const UiTheme* theme{nullptr};
    std::optional<float> scale{std::nullopt};
};

/// Specialized RAII Scope guard for Grid containers providing direct cell slot helpers.
class [[nodiscard]] GridScope : public ContainerScope {
public:
    using ContainerScope::ContainerScope;

    /// Arbitrary span cell slot (1..12 columns)
    [[nodiscard]] ContainerScope Cell(int span, CellOptions opts = {}, std::source_location loc = std::source_location::current());

    /// Full-width 12-column cell slot
    [[nodiscard]] ContainerScope Cell(CellOptions opts = {}, std::source_location loc = std::source_location::current()) {
        return Cell12(opts, loc);
    }

    [[nodiscard]] ContainerScope Cell12(CellOptions opts = {}, std::source_location loc = std::source_location::current());
    [[nodiscard]] ContainerScope Cell8(CellOptions opts = {}, std::source_location loc = std::source_location::current());
    [[nodiscard]] ContainerScope Cell6(CellOptions opts = {}, std::source_location loc = std::source_location::current());
    [[nodiscard]] ContainerScope Cell4(CellOptions opts = {}, std::source_location loc = std::source_location::current());
    [[nodiscard]] ContainerScope Cell3(CellOptions opts = {}, std::source_location loc = std::source_location::current());
    [[nodiscard]] ContainerScope Cell2(CellOptions opts = {}, std::source_location loc = std::source_location::current());
    [[nodiscard]] ContainerScope Cell1(CellOptions opts = {}, std::source_location loc = std::source_location::current());
};

/// Autonomous immediate mode Grid layout entry point.
class Grid {
public:
    [[nodiscard]] static GridScope Open(
        Context& ctx,
        const GridOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );
};

} // namespace guikit
