#pragma once

#include "guikit/context.hpp"
#include "guikit/core/container_scope.hpp"
#include "guikit/core/response.hpp"
#include "guikit/style/theme.hpp"
#include "guikit/style/tokens.hpp"
#include "guikit/core/types.hpp"
#include "guikit/surfaces/box.hpp"
#include "keel/core.h"
#include "keel/optional.h"

namespace guikit {

class Context;
class ICanvas;
struct Node;

using BoxScope = ContainerScope;

struct SplitViewOptions {
    Orientation orientation{Orientation::Horizontal}; ///< Horizontal = Left | Right panes; Vertical = Top / Bottom panes
    keel::f32 defaultRatio{0.5f};                         ///< Initial split ratio (0.0 .. 1.0) and double-click reset target
    keel::f32 minRatio{0.10f};                            ///< Minimum ratio for first pane
    keel::f32 maxRatio{0.90f};                            ///< Maximum ratio for first pane
    keel::f32 minFirstSize{48.0f};                        ///< Minimum size of first pane in logical dp
    keel::f32 minSecondSize{48.0f};                       ///< Minimum size of second pane in logical dp
    keel::f32 sashSize{6.0f};                             ///< Thickness of interactive splitter sash in logical dp
    bool showGrip{true};                              ///< Draw grip handle pill/dots in center of sash
    bool collapsible{false};                          ///< Snap pane to 0% / 100% when dragged near edge
    AxisSize width{SizeMode::Stretch};
    AxisSize height{SizeMode::Stretch};
    Padding padding{};
    StateColor background{};
    Border border{};
    bool clipChildren{true};
    bool disabled{false};
};

class [[nodiscard]] SplitViewScope : public ContainerScope {
public:
    Response response{};

    SplitViewScope() noexcept
        : ContainerScope(nullptr, nullptr), m_ratio(0.5f), m_dragging(false) {}

    SplitViewScope(Context* ctx, Node* node, Response res, keel::f32 ratio, bool dragging) noexcept
        : ContainerScope(ctx, node), response(res), m_ratio(ratio), m_dragging(dragging) {}

    SplitViewScope(SplitViewScope&& other) noexcept
        : ContainerScope(keel::move(other))
        , response(other.response)
        , m_ratio(other.m_ratio)
        , m_dragging(other.m_dragging) {
        other.m_dragging = false;
    }

    SplitViewScope& operator=(SplitViewScope&& other) noexcept {
        if (this != &other) {
            ContainerScope::operator=(keel::move(other));
            response = other.response;
            m_ratio = other.m_ratio;
            m_dragging = other.m_dragging;
            other.m_dragging = false;
        }
        return *this;
    }

    [[nodiscard]] BoxScope First(
        const BoxOptions& opts = {.width = SizeMode::Stretch, .height = SizeMode::Stretch, .clipChildren = true},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] BoxScope Second(
        const BoxOptions& opts = {.width = SizeMode::Stretch, .height = SizeMode::Stretch, .clipChildren = true},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] constexpr keel::f32 Ratio() const noexcept { return m_ratio; }
    [[nodiscard]] constexpr bool IsDragging() const noexcept { return m_dragging; }
    [[nodiscard]] const Response* operator->() const noexcept { return &response; }

private:
    keel::f32 m_ratio{0.5f};
    bool m_dragging{false};
};

namespace detail {

class SplitView {
public:
    [[nodiscard]] static SplitViewScope Open(
        Context& ctx,
        const SplitViewOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] static SplitViewScope Open(
        Context& ctx,
        keel::f32& ratio,
        const SplitViewOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] static SplitViewScope Open(
        Context& ctx,
        keel::f32* externalRatio,
        const SplitViewOptions& opts,
        keel::SourceLocation loc = keel::SourceLocation::current()
    );
};

} // namespace detail

[[nodiscard]] inline SplitViewScope SplitView(
    Context& ui,
    const SplitViewOptions& opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return detail::SplitView::Open(ui, opts, loc);
}

[[nodiscard]] inline SplitViewScope SplitView(
    Context& ui,
    keel::f32& ratio,
    const SplitViewOptions& opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return detail::SplitView::Open(ui, ratio, opts, loc);
}

} // namespace guikit
