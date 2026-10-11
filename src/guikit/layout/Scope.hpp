#pragma once

#include <optional>
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/layout/LayoutTypes.hpp"
#include "guikit/layout/Node.hpp"

namespace guikit {

class Context;

/// Options for configuring a horizontal Row container.
struct RowOptions {
    std::optional<UiSize> gap{std::nullopt};
    Padding padding{};
    Alignment align{Alignment::Stretch};
    Justify justify{Justify::Start};
    SizeMode heightMode{SizeMode::Fit};
    std::optional<UiSize> size{std::nullopt};
    bool disabled{false};
};

/// Options for configuring a vertical Column container.
struct ColumnOptions {
    std::optional<UiSize> gap{std::nullopt};
    Padding padding{};
    Alignment align{Alignment::Stretch};
    Justify justify{Justify::Start};
    SizeMode widthMode{SizeMode::Stretch};
    SizeMode heightMode{SizeMode::Stretch};
    std::optional<UiSize> size{std::nullopt};
    bool disabled{false};
};

/// Options for configuring a visual Box container.
struct BoxOptions {
    std::optional<UiSize> gap{std::nullopt};
    SizeMode widthMode{SizeMode::Stretch};
    SizeMode heightMode{SizeMode::Fit};
    float fixedWidth{0.0f};
    float fixedHeight{0.0f};
    std::optional<Padding> padding{std::nullopt};
    Color backgroundColor{0, 0, 0, 0};
    Color borderColor{0, 0, 0, 0};
    float borderWidth{0.0f};
    std::optional<UiSize> radius{std::nullopt};
    bool clipChildren{false};
    std::optional<UiSize> size{std::nullopt};
    bool disabled{false};
};

/// Options for configuring an auto-wrapping flow container.
struct WrapOptions {
    std::optional<UiSize> gap{std::nullopt};        ///< Horizontal gap between items on same line
    std::optional<UiSize> runGap{std::nullopt};     ///< Vertical gap between consecutive wrapped lines
    Padding padding{};
    Alignment align{Alignment::Center};
    Justify justify{Justify::Start};
    SizeMode widthMode{SizeMode::Stretch};
    SizeMode heightMode{SizeMode::Fit};
    std::optional<UiSize> size{std::nullopt};
    bool disabled{false};
};

/// Options for configuring a 12-column responsive cell grid container.
struct CellGridOptions {
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

using GridOptions = CellGridOptions;

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

/// Options for configuring a visual Card surface container.
struct CardOptions {
    UiVariant variant{UiVariant::Default};
    float minWidth{0.0f};                           ///< Minimum card width constraint (0 = auto content intrinsic)
    std::optional<UiSize> gap{std::nullopt};        ///< Gap between child elements inside card
    std::optional<Padding> padding{std::nullopt};   ///< Card padding (nullopt = auto from style density)
    Color backgroundColor{Color::Transparent()};
    Color borderColor{Color::Transparent()};
    float borderWidth{1.0f};
    std::optional<UiSize> radius{std::nullopt};     ///< Corner radius token (nullopt = auto from style density)
    bool clipChildren{true};                        ///< Default true: clip children to prevent content from bleeding outside card
    SizeMode widthMode{SizeMode::Stretch};
    SizeMode heightMode{SizeMode::Fit};
    std::optional<UiSize> size{std::nullopt};
    bool disabled{false};
};

/// Options for configuring a ScrollArea viewport.
struct ScrollAreaOptions {
    Orientation orientation{Orientation::Vertical};
    VisibilityPolicy scrollbar{VisibilityPolicy::Auto};
    std::optional<UiSize> gap{std::nullopt};        ///< Gap between consecutive scroll items (nullopt = zero)
    std::optional<Padding> padding{std::nullopt};   ///< Viewport content padding (nullopt = zero)
    std::optional<UiSize> size{std::nullopt};
    bool disabled{false};
};

/// RAII Scope guard for hierarchical layout containers.
class ContainerScope {
public:
    ContainerScope(Context* ctx, Node* node) noexcept
        : m_ctx(ctx), m_node(node) {}

    ~ContainerScope();

    ContainerScope(const ContainerScope&) = delete;
    ContainerScope& operator=(const ContainerScope&) = delete;

    ContainerScope(ContainerScope&& other) noexcept
        : m_ctx(other.m_ctx), m_node(other.m_node) {
        other.m_ctx = nullptr;
        other.m_node = nullptr;
    }

    ContainerScope& operator=(ContainerScope&& other) noexcept {
        if (this != &other) {
            m_ctx = other.m_ctx;
            m_node = other.m_node;
            other.m_ctx = nullptr;
            other.m_node = nullptr;
        }
        return *this;
    }

    /// Evaluates to true allowing `if (auto row = ui.Row()) { ... }` syntax.
    [[nodiscard]] constexpr operator bool() const noexcept {
        return m_node != nullptr;
    }

    [[nodiscard]] Node* GetNode() const noexcept {
        return m_node;
    }

protected:
    Context* m_ctx{nullptr};
    Node* m_node{nullptr};
};

using CellScope = ContainerScope;

/// Specialized RAII Scope guard for CellGrid containers providing direct cell slot helpers.
class CellGridScope : public ContainerScope {
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

using GridScope = CellGridScope;
using RowScope = ContainerScope;
using ColumnScope = ContainerScope;
using BoxScope = ContainerScope;
using WrapScope = ContainerScope;
using CardScope = ContainerScope;
using ScrollAreaScope = ContainerScope;
using CarouselScope = ContainerScope;

} // namespace guikit
