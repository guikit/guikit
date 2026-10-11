#pragma once

#include <optional>
#include <source_location>
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/layout/Types.hpp"
#include "guikit/layout/Scope.hpp"

namespace guikit {

class Context;

/// Options for configuring a 12-column responsive cell grid layout.
struct GridOptions {
    int columns{12};                                ///< Total number of columns in the grid (default: 12)
    float minColWidth{0.0f};                        ///< Minimum column width before wrapping items (0 = auto content intrinsic)
    std::optional<UiSize> gap{std::nullopt};        ///< Horizontal gap between columns (nullopt = inherit size)
    std::optional<UiSize> runGap{std::nullopt};     ///< Vertical gap between wrapped rows (nullopt = match gap)
    Padding padding{};                              ///< Container padding
    Alignment align{Alignment::Stretch};
    Justify justify{Justify::Start};
    SizeMode widthMode{SizeMode::Stretch};
    SizeMode heightMode{SizeMode::Fit};
    std::optional<UiSize> size{std::nullopt};
    bool disabled{false};
};

using CellGridOptions = GridOptions;

/// Options for configuring an individual grid cell slot.
struct CellOptions {
    float minWidth{0.0f};                           ///< Minimum width constraint (0 = auto content intrinsic)
    std::optional<UiSize> gap{std::nullopt};        ///< Gap between stacked children inside this cell
    Padding padding{};                              ///< Internal cell padding
    Alignment align{Alignment::Stretch};
    Justify justify{Justify::Start};
    SizeMode heightMode{SizeMode::Fit};
    std::optional<UiSize> size{std::nullopt};
    bool disabled{false};
};

using CellScope = ContainerScope;

/// Specialized RAII Scope guard for Grid containers providing direct cell slot helpers.
class GridScope : public ContainerScope {
public:
    using ContainerScope::ContainerScope;

    /// Full-width 12-column cell slot
    CellScope Cell(CellOptions opts = {}, std::source_location loc = std::source_location::current()) {
        return Cell12(opts, loc);
    }

    CellScope Cell12(CellOptions opts = {}, std::source_location loc = std::source_location::current());
    CellScope Cell8(CellOptions opts = {}, std::source_location loc = std::source_location::current());
    CellScope Cell6(CellOptions opts = {}, std::source_location loc = std::source_location::current());
    CellScope Cell4(CellOptions opts = {}, std::source_location loc = std::source_location::current());
    CellScope Cell3(CellOptions opts = {}, std::source_location loc = std::source_location::current());
    CellScope Cell2(CellOptions opts = {}, std::source_location loc = std::source_location::current());
    CellScope Cell1(CellOptions opts = {}, std::source_location loc = std::source_location::current());
};

using CellGridScope = GridScope;

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
