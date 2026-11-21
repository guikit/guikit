#pragma once

#include "guikit/core/theme.hpp"
#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"
#include "keel/optional.h"
#include "keel/string.h"

namespace guikit {

class Context;
class ICanvas;

/// Anchor corner/edge for the floating Toast notification stack.
enum class ToastPlacement : keel::u8 {
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
    keel::f32 duration{4.0f};               ///< Auto-dismiss duration in seconds (<= 0.0f = sticky until closed)
    bool showProgress{true};            ///< Render animated countdown bar along the bottom edge
    bool closable{true};                ///< Show [X] dismiss button
};

/// Configuration options for the Toast stack host (`ui.ToastHost(...)` or `ui.SetToastHostOptions(...)`).
struct ToastHostOptions {
    ToastPlacement placement{ToastPlacement::BottomRight};
    keel::f32 width{320.0f};
    keel::f32 gap{8.0f};
    keel::f32 margin{16.0f};
    keel::usize maxVisible{5};
    bool pauseOnHover{true};
    bool confineToParent{false}; ///< Anchor inside current container bounds instead of full viewport
};

/// Active runtime state of a single queued Toast notification.
struct ToastEntry {
    keel::u64 id{0};
    keel::String title{};
    keel::String message{};
    TextWrap titleWrap{};
    TextWrap messageWrap{};
    keel::String iconPath{};
    UiVariant variant{UiVariant::Info};
    Icon icon{};
    keel::f32 duration{4.0f};
    bool showProgress{true};
    bool closable{true};
    keel::f64 elapsedSec{0.0};
    keel::f64 lastTickSec{0.0};
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
