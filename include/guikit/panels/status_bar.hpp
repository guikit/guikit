#pragma once

#include "guikit/core/container_scope.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"
#include "keel/core.h"
#include "keel/optional.h"

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
    keel::Optional<float> height{keel::kNullOpt};          ///< Explicit height in pixels (nullopt = auto compact height from theme, ~26px in Md)
    keel::Optional<float> gap{keel::kNullOpt};             ///< Horizontal gap between status items (nullopt = auto compact gap)
    keel::Optional<Padding> padding{keel::kNullOpt};       ///< Internal bar padding (nullopt = auto compact padding)
    StateColor background{};
    Border border{.width = 1.0f};
    bool disabled{false};
    const UiTheme* theme{nullptr};
    keel::Optional<float> scale{keel::kNullOpt};
    keel::Optional<bool> debugBounds{keel::kNullOpt};
    keel::Optional<bool> inspectMode{keel::kNullOpt};
};

/// Configuration for a static or interactive segment inside a StatusBar.
struct StatusItemOptions {
    UiText text{};                                      ///< Segment text label (supports .Variants() width reservation)
    Icon icon{};                                        ///< Optional leading vector icon
    Color dotColor{Color::Transparent()};               ///< Optional leading status LED dot color
    StateColor color{};                                 ///< Optional text/icon color override (Default = Secondary)
    UiText badge{};                                     ///< Optional trailing pill badge text
    UiText tooltip{};                                   ///< Optional hover tooltip
    keel::Optional<float> minWidth{keel::kNullOpt};        ///< Optional minimum segment width in unscaled points
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
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Renders an interactive clickable status segment with hover/press feedback.
    Response Action(
        const StatusItemOptions& opts,
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Renders a vertical divider line between status segments.
    void Separator(
        float heightRatio = 0.55f,
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Inserts an elastic horizontal spacer to push subsequent items to the right edge.
    void Spacer(keel::SourceLocation loc = keel::SourceLocation::current());

    /// Inserts a fixed horizontal gap in logical dp.
    void Gap(float sizeDp = 8.0f, keel::SourceLocation loc = keel::SourceLocation::current());

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
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    static Response RenderSegment(
        Context& ctx,
        const StatusItemOptions& opts,
        StatusBarStyle barStyle,
        bool interactive,
        keel::SourceLocation loc = keel::SourceLocation::current()
    );
};

} // namespace guikit
