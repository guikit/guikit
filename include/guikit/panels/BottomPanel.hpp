#pragma once

#include <optional>
#include <source_location>
#include "guikit/core/ContainerScope.hpp"
#include "guikit/core/Response.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

struct UiTheme;
class Context;
struct Node;

/// Display and sizing state of a BottomPanel container.
enum class BottomPanelMode : uint8_t {
    Expanded,   ///< Open mode (explicit/user-resized height or Fit to content)
    Collapsed   ///< Minimized mode (0px layout height with interactive top edge / toggle handle)
};

/// Options for configuring a dockable bottom panel surface container.
struct BottomPanelOptions {
    BottomPanelMode defaultMode{BottomPanelMode::Expanded}; ///< Initial mode when managed by Context
    std::optional<float> height{220.0f};                    ///< Explicit panel height in Expanded mode (nullopt = Fit to content)
    float minHeight{100.0f};                                ///< Minimum panel height constraint in pixels
    float maxHeight{560.0f};                                ///< Maximum panel height when resizing via mouse drag
    bool resizable{true};                                   ///< Allow dragging the top border (sash) up/down to resize or snap-collapse
    bool allowCollapse{true};                               ///< Support Collapsed (completely minimized 0px) mode via drag-snap or toggle
    bool showToggleButton{false};                           ///< Optional floating edge pill button on the top border
    bool scrollable{false};                                 ///< Automatically scroll vertically when content overflows
    VisibilityPolicy scrollbar{VisibilityPolicy::Auto};
    std::optional<float> gap{0.0f};                         ///< Gap between direct children (0px by default so inner Toolbar docks flush)
    std::optional<Padding> padding{Padding::All(0.0f)};     ///< Internal padding (0px by default so inner Toolbar/views fill edge-to-edge)
    Color backgroundColor{Color::Transparent()};
    Color borderColor{Color::Transparent()};
    float borderWidth{1.0f};
    std::optional<float> borderRadius{0.0f};                ///< Corner radius in pixels (0px by default for docked panels)
    bool clipChildren{true};
    bool disabled{false};
    const UiTheme* theme{nullptr};
    std::optional<float> scale{std::nullopt};
    std::optional<bool> debugBounds{std::nullopt};
    std::optional<bool> inspectMode{std::nullopt};
};

/// Persistent state stored per BottomPanel in Context.
struct BottomPanelPersistState {
    bool initialized{false};
    BottomPanelMode mode{BottomPanelMode::Expanded};
    float customHeight{0.0f};
    float lastExpandedHeight{220.0f};
    bool draggingSash{false};
    float dragStartMouseY{0.0f};
    float dragStartHeight{0.0f};
    float dragStartCustomHeight{0.0f};
    BottomPanelMode dragStartMode{BottomPanelMode::Expanded};
    bool sashHovered{false};
    bool toggleHovered{false};
    bool togglePressed{false};
};

/// RAII scope returned by `ui.BottomPanel(...)`.
/// Evaluates to `true` (`operator bool()`) when in `Expanded` mode,
/// and `false` when in `Collapsed` mode (skipping child execution when minimized to 0px).
class [[nodiscard]] BottomPanelScope : public ContainerScope {
public:
    Response response{};

    BottomPanelScope() noexcept : ContainerScope(nullptr, nullptr) {}
    BottomPanelScope(Context* ctx, Node* node, Response res, BottomPanelMode mode, float height) noexcept
        : ContainerScope(ctx, node), response(res), m_mode(mode), m_height(height) {}

    BottomPanelScope(BottomPanelScope&& other) noexcept
        : ContainerScope(std::move(other))
        , response(other.response)
        , m_mode(other.m_mode)
        , m_height(other.m_height) {
        other.m_mode = BottomPanelMode::Collapsed;
    }

    BottomPanelScope& operator=(BottomPanelScope&& other) noexcept {
        if (this != &other) {
            ContainerScope::operator=(std::move(other));
            response = other.response;
            m_mode = other.m_mode;
            m_height = other.m_height;
            other.m_mode = BottomPanelMode::Collapsed;
        }
        return *this;
    }

    /// Evaluates to true when the bottom panel is Expanded; false when Collapsed (0px).
    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return m_node != nullptr && m_mode != BottomPanelMode::Collapsed;
    }

    [[nodiscard]] constexpr BottomPanelMode Mode() const noexcept { return m_mode; }
    [[nodiscard]] constexpr bool IsExpanded() const noexcept { return m_mode == BottomPanelMode::Expanded; }
    [[nodiscard]] constexpr bool IsCollapsed() const noexcept { return m_mode == BottomPanelMode::Collapsed; }
    [[nodiscard]] constexpr float Height() const noexcept { return m_height; }
    [[nodiscard]] const Response* operator->() const noexcept { return &response; }

private:
    BottomPanelMode m_mode{BottomPanelMode::Expanded};
    float m_height{0.0f};
};

/// Autonomous immediate mode dockable BottomPanel container entry point.
class BottomPanel {
public:
    [[nodiscard]] static BottomPanelScope Open(
        Context& ctx,
        const BottomPanelOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Open(ctx, static_cast<BottomPanelMode*>(nullptr), opts, loc);
    }

    [[nodiscard]] static BottomPanelScope Open(
        Context& ctx,
        bool* open,
        const BottomPanelOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] static BottomPanelScope Open(
        Context& ctx,
        BottomPanelMode* externalMode,
        const BottomPanelOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );
};

} // namespace guikit
