#pragma once

#include "guikit/core/container_scope.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "guikit/panels/box.hpp"
#include "keel/core.h"
#include "keel/optional.h"

namespace guikit {

class Context;
class ICanvas;
struct Node;

using BoxScope = ContainerScope;

struct SplitViewOptions {
    Orientation orientation{Orientation::Horizontal}; ///< Horizontal = Left | Right panes; Vertical = Top / Bottom panes
    float defaultRatio{0.5f};                         ///< Initial split ratio (0.0 .. 1.0) and double-click reset target
    float minRatio{0.10f};                            ///< Minimum ratio for first pane
    float maxRatio{0.90f};                            ///< Maximum ratio for first pane
    float minFirstSize{48.0f};                        ///< Minimum size of first pane in logical dp
    float minSecondSize{48.0f};                       ///< Minimum size of second pane in logical dp
    float sashSize{6.0f};                             ///< Thickness of interactive splitter sash in logical dp
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

    SplitViewScope(Context* ctx, Node* node, Response res, float ratio, bool dragging) noexcept
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

    [[nodiscard]] constexpr float Ratio() const noexcept { return m_ratio; }
    [[nodiscard]] constexpr bool IsDragging() const noexcept { return m_dragging; }
    [[nodiscard]] const Response* operator->() const noexcept { return &response; }

private:
    float m_ratio{0.5f};
    bool m_dragging{false};
};

class SplitView {
public:
    [[nodiscard]] static SplitViewScope Open(
        Context& ctx,
        const SplitViewOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] static SplitViewScope Open(
        Context& ctx,
        float& ratio,
        const SplitViewOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] static SplitViewScope Open(
        Context& ctx,
        float* externalRatio,
        const SplitViewOptions& opts,
        keel::SourceLocation loc = keel::SourceLocation::current()
    );
};

} // namespace guikit
