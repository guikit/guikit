#pragma once

#include <optional>
#include <source_location>
#include "guikit/core/ContainerScope.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

class Context;

/// Options for configuring a 12-column responsive cell grid layout.
struct GridOptions {
    int columns{12};                                ///< Total number of columns in the grid (default: 12)
    float minColWidth{0.0f};                        ///< Minimum column width before wrapping items (0 = auto content intrinsic)
    std::optional<UiSize> gap{std::nullopt};        ///< Horizontal gap between columns (nullopt = inherit size)
    std::optional<UiSize> runGap{std::nullopt};     ///< Vertical gap between wrapped rows (nullopt = match gap)
    std::optional<Padding> padding{std::nullopt};   ///< Container padding (nullopt = zero)
    Alignment align{Alignment::Stretch};
    Justify justify{Justify::Start};
    SizeMode widthMode{SizeMode::Stretch};
    SizeMode heightMode{SizeMode::Fit};
    std::optional<UiSize> size{std::nullopt};
    bool disabled{false};
};

/// Options for configuring an individual grid cell slot.
struct CellOptions {
    float minWidth{0.0f};                           ///< Minimum width constraint (0 = auto content intrinsic)
    std::optional<UiSize> gap{std::nullopt};        ///< Gap between stacked children inside this cell
    std::optional<Padding> padding{std::nullopt};   ///< Internal cell padding (nullopt = zero)
    Alignment align{Alignment::Stretch};
    Justify justify{Justify::Start};
    SizeMode heightMode{SizeMode::Fit};
    std::optional<UiSize> size{std::nullopt};
    bool disabled{false};
    bool clipChildren{false};                       ///< Clip cell children to cell boundaries (default: false)
};

/// Specialized RAII Scope guard for Grid containers providing direct cell slot helpers.
class GridScope : public ContainerScope {
public:
    using ContainerScope::ContainerScope;

    /// Arbitrary span cell slot (1..12 columns)
    ContainerScope Cell(int span, CellOptions opts = {}, std::source_location loc = std::source_location::current());

    /// Full-width 12-column cell slot
    ContainerScope Cell(CellOptions opts = {}, std::source_location loc = std::source_location::current()) {
        return Cell12(opts, loc);
    }

    ContainerScope Cell12(CellOptions opts = {}, std::source_location loc = std::source_location::current());
    ContainerScope Cell8(CellOptions opts = {}, std::source_location loc = std::source_location::current());
    ContainerScope Cell6(CellOptions opts = {}, std::source_location loc = std::source_location::current());
    ContainerScope Cell4(CellOptions opts = {}, std::source_location loc = std::source_location::current());
    ContainerScope Cell3(CellOptions opts = {}, std::source_location loc = std::source_location::current());
    ContainerScope Cell2(CellOptions opts = {}, std::source_location loc = std::source_location::current());
    ContainerScope Cell1(CellOptions opts = {}, std::source_location loc = std::source_location::current());
};

/// Autonomous immediate mode Grid layout entry point.
class Grid {
public:
    static GridScope Open(
        Context& ctx,
        const GridOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );
};

} // namespace guikit
