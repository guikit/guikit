#pragma once

#include "guikit/core/container_scope.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "keel/core.h"

namespace guikit {

struct UiTheme;
class Context;

/// Options for configuring a 12-column responsive cell grid layout.
struct GridOptions {
    keel::i32 columns{12};                                  ///< Total number of columns in the grid (default: 12)
    keel::f32 minColWidth{0.0f};                          ///< Minimum column width before wrapping items (0 = auto content intrinsic)
    keel::Optional<keel::f32> gap{keel::kNullOpt};        ///< Horizontal gap between columns (nullopt = from theme)
    keel::Optional<keel::f32> runGap{keel::kNullOpt};     ///< Vertical gap between wrapped rows (nullopt = match gap)
    keel::Optional<Padding> padding{keel::kNullOpt};  ///< Container padding (nullopt = zero)
    Alignment align{Alignment::Stretch};
    Justify justify{Justify::Start};
    AxisSize width{SizeMode::Stretch};
    AxisSize height{SizeMode::Fit};
    keel::Optional<keel::f32> minWidth{keel::kNullOpt};
    keel::Optional<keel::f32> maxWidth{keel::kNullOpt};
    keel::Optional<keel::f32> minHeight{keel::kNullOpt};
    keel::Optional<keel::f32> maxHeight{keel::kNullOpt};
    keel::f32 flexGrow{1.0f};
    bool disabled{false};
};

/// Options for configuring an individual grid cell slot.
struct CellOptions {
    keel::Optional<keel::f32> minWidth{keel::kNullOpt};   ///< Minimum width constraint (nullopt = auto content intrinsic)
    keel::Optional<keel::f32> maxWidth{keel::kNullOpt};
    AxisSize height{SizeMode::Fit};
    keel::Optional<keel::f32> minHeight{keel::kNullOpt};
    keel::Optional<keel::f32> maxHeight{keel::kNullOpt};
    keel::Optional<keel::f32> gap{keel::kNullOpt};        ///< Gap between stacked children inside this cell
    keel::Optional<Padding> padding{keel::kNullOpt};  ///< Internal cell padding (nullopt = zero)
    Alignment align{Alignment::Stretch};
    Justify justify{Justify::Start};
    bool clipChildren{false};                         ///< Clip cell children to cell boundaries (default: false)
    bool disabled{false};
};

/// Specialized RAII Scope guard for Grid containers providing direct cell slot helpers.
class [[nodiscard]] GridScope : public ContainerScope {
public:
    using ContainerScope::ContainerScope;

    /// Arbitrary span cell slot (1..12 columns)
    [[nodiscard]] ContainerScope Cell(keel::i32 span, CellOptions opts = {}, keel::SourceLocation loc = keel::SourceLocation::current());

    /// Full-width cell slot spanning all columns of this Grid
    [[nodiscard]] ContainerScope Cell(CellOptions opts = {}, keel::SourceLocation loc = keel::SourceLocation::current());

    [[nodiscard]] ContainerScope Cell12(CellOptions opts = {}, keel::SourceLocation loc = keel::SourceLocation::current());
    [[nodiscard]] ContainerScope Cell8(CellOptions opts = {}, keel::SourceLocation loc = keel::SourceLocation::current());
    [[nodiscard]] ContainerScope Cell6(CellOptions opts = {}, keel::SourceLocation loc = keel::SourceLocation::current());
    [[nodiscard]] ContainerScope Cell4(CellOptions opts = {}, keel::SourceLocation loc = keel::SourceLocation::current());
    [[nodiscard]] ContainerScope Cell3(CellOptions opts = {}, keel::SourceLocation loc = keel::SourceLocation::current());
    [[nodiscard]] ContainerScope Cell2(CellOptions opts = {}, keel::SourceLocation loc = keel::SourceLocation::current());
    [[nodiscard]] ContainerScope Cell1(CellOptions opts = {}, keel::SourceLocation loc = keel::SourceLocation::current());
};

/// Autonomous immediate mode Grid layout entry point.
class Grid {
public:
    [[nodiscard]] static GridScope Open(
        Context& ctx,
        const GridOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );
};

} // namespace guikit
