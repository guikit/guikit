#pragma once

#include <optional>
#include <source_location>
#include "guikit/core/ContainerScope.hpp"
#include "guikit/core/Response.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/graphics/Icon.hpp"

namespace guikit {

struct UiTheme;
class Context;
struct Node;

/// Visual surface style of a StatusBar container.
enum class StatusBarStyle : uint8_t {
    Surface,    ///< Standard subtle system status strip (surface) with a 1px top border
    Header,     ///< Distinct elevated strip (surfaceElevated) with a 1px top border
    Accent      ///< High-contrast primary accent strip (VS Code style)
};

/// Options for configuring a StatusBar container.
struct StatusBarOptions {
    StatusBarStyle style{StatusBarStyle::Surface};
    std::optional<float> height{std::nullopt};          ///< Explicit height in pixels (nullopt = auto compact height from theme, ~26px in Md)
    std::optional<float> gap{std::nullopt};             ///< Horizontal gap between status items (nullopt = auto compact gap)
    std::optional<Padding> padding{std::nullopt};       ///< Internal bar padding (nullopt = auto compact padding)
    Color backgroundColor{Color::Transparent()};        ///< Optional background color override
    Color borderColor{Color::Transparent()};            ///< Optional top border color override
    float borderWidth{1.0f};                            ///< Top border thickness in pixels
    std::optional<float> borderRadius{0.0f};            ///< Corner radius in pixels (0px by default for docked status bars)
    bool disabled{false};
    const UiTheme* theme{nullptr};
    std::optional<float> scale{std::nullopt};
    std::optional<bool> debugBounds{std::nullopt};
    std::optional<bool> inspectMode{std::nullopt};
};

/// Configuration for a static or interactive segment inside a StatusBar.
struct StatusItemOptions {
    UiText text{};                                      ///< Segment text label
    Icon icon{};                                        ///< Optional leading vector icon
    Color dotColor{Color::Transparent()};               ///< Optional leading status LED dot color
    Color textColor{Color::Transparent()};              ///< Optional explicit text/icon color override
    UiVariant variant{UiVariant::Secondary};            ///< Semantic color variant when textColor is not specified
    UiText badge{};                                     ///< Optional trailing pill badge text
    UiText tooltip{};                                   ///< Optional hover tooltip
    bool active{false};                                 ///< Highlight state for toggleable status actions
    bool disabled{false};
};

/// Scoped RAII guard for a StatusBar container.
class [[nodiscard]] StatusBarScope : public ContainerScope {
public:
    Response response{};

    StatusBarScope() noexcept : ContainerScope(nullptr, nullptr) {}
    StatusBarScope(Context* ctx, Node* node, Response res, const StatusBarOptions& opts) noexcept
        : ContainerScope(ctx, node), response(res), m_opts(opts) {}

    StatusBarScope(StatusBarScope&&) noexcept = default;
    StatusBarScope& operator=(StatusBarScope&&) noexcept = default;

    /// Renders a non-interactive status segment (optional LED dot, icon, text, badge, and tooltip).
    void Item(
        const StatusItemOptions& opts,
        std::source_location loc = std::source_location::current()
    );

    /// Renders an interactive clickable status segment with hover/press feedback.
    Response Action(
        const StatusItemOptions& opts,
        std::source_location loc = std::source_location::current()
    );

    /// Renders a vertical divider line between status segments.
    void Separator(
        float heightRatio = 0.55f,
        std::source_location loc = std::source_location::current()
    );

    /// Inserts an elastic horizontal spacer to push subsequent items to the right edge.
    void Spacer(std::source_location loc = std::source_location::current());

    [[nodiscard]] const Response* operator->() const noexcept { return &response; }

private:
    StatusBarOptions m_opts{};
};

/// Autonomous immediate mode StatusBar panel component.
class StatusBar {
public:
    [[nodiscard]] static StatusBarScope Open(
        Context& ctx,
        const StatusBarOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    static Response RenderSegment(
        Context& ctx,
        const StatusItemOptions& opts,
        StatusBarStyle barStyle,
        bool interactive,
        std::source_location loc = std::source_location::current()
    );
};

} // namespace guikit
