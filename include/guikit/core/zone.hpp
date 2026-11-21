#pragma once

#include "keel/core.h"
#include "keel/optional.h"

namespace keel { class Arena; }

namespace guikit {

class Context;
struct UiTheme;

/// Resolved subtree environment inherited down UI nodes (`Context::GetZone()` / `Node::zone`).
struct Zone {
    UiTheme* theme{nullptr};                          ///< Active theme (context theme or arena copy)
    keel::f32 scale{1.0f};                                ///< Accumulated geometric scale (relative to root)
    keel::f32 density{1.0f};                              ///< Accumulated spacing/gap/padding multiplier
    keel::Optional<bool> debugBounds{keel::kNullOpt}; ///< Debug bounding boxes (`kNullOpt` = the context setting)
    keel::Optional<bool> inspectMode{keel::kNullOpt}; ///< DevTools inspector (`kNullOpt` = the context setting)
};

/// Default fallback zone used when constructing standalone nodes outside a `Context`.
inline constexpr Zone kDefaultZone{};

/// Subtree environment options passed to `ui.Zone({...})`.
/// Anything left unset inherits from the enclosing zone; `scale` and `density` compose multiplicatively.
struct ZoneOptions {
    keel::Optional<const UiTheme&> theme{keel::kNullOpt}; ///< Replaces inherited theme (copied into the frame arena)
    keel::Optional<keel::f32> scale{keel::kNullOpt};          ///< Geometric subtree multiplier (layout, text, strokes, input)
    keel::Optional<keel::f32> density{keel::kNullOpt};        ///< Spacing/gap/padding multiplier for container layouts
    keel::Optional<bool> debugBounds{keel::kNullOpt};     ///< Shows (or hides) debug bounding boxes inside the subtree
    keel::Optional<bool> inspectMode{keel::kNullOpt};     ///< Enables (or disables) the DevTools inspector inside the subtree

    [[nodiscard]] constexpr bool IsEmpty() const noexcept {
        return !theme.has_value()
            && !scale.has_value()
            && !density.has_value()
            && !debugBounds.has_value()
            && !inspectMode.has_value();
    }
};

/// Transparent RAII scope guard returned by `ui.Zone(...)` that applies environment overrides
/// (`theme`, `scale`, `density`, `debugBounds`, `inspectMode`) to widgets and containers in its lexical block
/// without creating a layout node.
class [[nodiscard]] ZoneScope {
public:
    ZoneScope() noexcept = default;
    explicit ZoneScope(Context* ctx) noexcept : m_ctx(ctx) {}
    ~ZoneScope() noexcept;

    ZoneScope(const ZoneScope&) = delete;
    ZoneScope& operator=(const ZoneScope&) = delete;

    ZoneScope(ZoneScope&& other) noexcept : m_ctx(other.m_ctx) {
        other.m_ctx = nullptr;
    }

    ZoneScope& operator=(ZoneScope&& other) noexcept {
        if (this != &other) {
            PopIfNeeded();
            m_ctx = other.m_ctx;
            other.m_ctx = nullptr;
        }
        return *this;
    }

    explicit operator bool() const noexcept { return m_ctx != nullptr; }

private:
    void PopIfNeeded() noexcept;
    Context* m_ctx{nullptr};
};

/// Resolves child zone state from `parent` + `opts` into `arena` (returns `parent` unchanged when `opts.IsEmpty()`).
[[nodiscard]] const Zone* ResolveZone(
    const Zone* parent,
    const ZoneOptions& opts,
    keel::Arena& arena
);

} // namespace guikit
