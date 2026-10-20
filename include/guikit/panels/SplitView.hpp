#pragma once

#include <optional>
#include <source_location>
#include "guikit/core/ContainerScope.hpp"
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/panels/Box.hpp"

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
    SizeMode widthMode{SizeMode::Stretch};
    SizeMode heightMode{SizeMode::Stretch};
    std::optional<float> width{std::nullopt};
    std::optional<float> height{std::nullopt};
    Padding padding{};
    StateColor background{};
    Border border{};
    bool clipChildren{true};
    bool disabled{false};
    const UiTheme* theme{nullptr};
    std::optional<float> scale{std::nullopt};
    std::optional<bool> debugBounds{std::nullopt};
    std::optional<bool> inspectMode{std::nullopt};
};

class [[nodiscard]] SplitViewScope : public ContainerScope {
public:
    Response response{};

    SplitViewScope() noexcept
        : ContainerScope(nullptr, nullptr), m_ratio(0.5f), m_dragging(false) {}

    SplitViewScope(Context* ctx, Node* node, Response res, float ratio, bool dragging) noexcept
        : ContainerScope(ctx, node), response(res), m_ratio(ratio), m_dragging(dragging) {}

    SplitViewScope(SplitViewScope&& other) noexcept
        : ContainerScope(std::move(other))
        , response(other.response)
        , m_ratio(other.m_ratio)
        , m_dragging(other.m_dragging) {
        other.m_dragging = false;
    }

    SplitViewScope& operator=(SplitViewScope&& other) noexcept {
        if (this != &other) {
            ContainerScope::operator=(std::move(other));
            response = other.response;
            m_ratio = other.m_ratio;
            m_dragging = other.m_dragging;
            other.m_dragging = false;
        }
        return *this;
    }

    [[nodiscard]] BoxScope First(
        const BoxOptions& opts = {.widthMode = SizeMode::Stretch, .heightMode = SizeMode::Stretch, .clipChildren = true},
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] BoxScope Second(
        const BoxOptions& opts = {.widthMode = SizeMode::Stretch, .heightMode = SizeMode::Stretch, .clipChildren = true},
        std::source_location loc = std::source_location::current()
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
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] static SplitViewScope Open(
        Context& ctx,
        float& ratio,
        const SplitViewOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] static SplitViewScope Open(
        Context& ctx,
        float* externalRatio,
        const SplitViewOptions& opts,
        std::source_location loc = std::source_location::current()
    );
};

} // namespace guikit
