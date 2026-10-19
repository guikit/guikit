#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "guikit/core/Theme.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/graphics/Icon.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Anchor corner/edge for the floating Toast notification stack.
enum class ToastPlacement : uint8_t {
    BottomRight = 0,
    TopRight,
    BottomLeft,
    TopLeft,
    TopCenter,
    BottomCenter
};

/// Options for spawning a single Toast notification via `ui.ShowToast(...)`.
struct ToastOptions {
    UiText title{};
    UiText message{};
    UiVariant variant{UiVariant::Info}; ///< Default, Primary, Secondary, Info, Success, Warning, Danger
    Icon icon{};                        ///< Optional icon (auto-selected from variant if empty)
    float duration{4.0f};               ///< Auto-dismiss duration in seconds (<= 0.0f = sticky until closed)
    bool showProgress{true};            ///< Render animated countdown bar along the bottom edge
    bool closable{true};                ///< Show [X] dismiss button
};

/// Configuration options for the Toast stack host (`ui.ToastHost(...)` or `ui.SetToastHostOptions(...)`).
struct ToastHostOptions {
    ToastPlacement placement{ToastPlacement::BottomRight};
    float width{320.0f};
    float gap{8.0f};
    float margin{16.0f};
    size_t maxVisible{5};
    bool pauseOnHover{true};
    bool confineToParent{false}; ///< Anchor inside current container bounds instead of full viewport
    const UiTheme* theme{nullptr};
    std::optional<float> scale{std::nullopt};
};

/// Active runtime state of a single queued Toast notification.
struct ToastEntry {
    uint64_t id{0};
    std::string title{};
    std::string message{};
    UiVariant variant{UiVariant::Info};
    Icon icon{};
    float duration{4.0f};
    bool showProgress{true};
    bool closable{true};
    double elapsedSec{0.0};
    double lastTickSec{0.0};
    bool hovered{false};
    bool closeHovered{false};
};

/// Autonomous Toast notification manager and overlay renderer.
class Toast {
public:
    /// Updates toast timers, processes hover/close interactions, and registers overlay rendering.
    static void RunHost(
        Context& ctx,
        const ToastHostOptions& opts = {}
    );

    /// Resolves default semantic icon for a toast variant when no explicit icon is provided.
    [[nodiscard]] static Icon ResolveDefaultIcon(UiVariant variant, Icon explicitIcon) noexcept;
};

} // namespace guikit
