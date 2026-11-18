#pragma once

#include "keel/core.h"
#include "keel/optional.h"

namespace keel { class Arena; }

namespace guikit {

struct UiTheme;

/// Resolved subtree environment inherited down container nodes (`Context::GetZone()` / `Node::zone`).
struct Zone {
    UiTheme* theme{nullptr};                          ///< Active theme (context theme or arena copy)
    float scale{1.0f};                                ///< Accumulated geometric scale (relative to root)
    float density{1.0f};                              ///< Accumulated spacing/gap/padding multiplier
    keel::Optional<bool> debugBounds{keel::kNullOpt}; ///< Debug bounding boxes (`kNullOpt` = the context setting)
    keel::Optional<bool> inspectMode{keel::kNullOpt}; ///< DevTools inspector (`kNullOpt` = the context setting)
};

/// Default fallback zone used when constructing standalone nodes outside a `Context`.
inline constexpr Zone kDefaultZone{};

/// Subtree environment options attached to containers via `.zone = {...}`.
/// Anything left unset inherits from the enclosing container; `scale` and `density` compose multiplicatively.
struct ZoneOptions {
    const UiTheme* theme{nullptr};                    ///< Replaces inherited theme (copied into the frame arena)
    keel::Optional<float> scale{keel::kNullOpt};      ///< Geometric subtree multiplier (layout, text, strokes, input)
    keel::Optional<float> density{keel::kNullOpt};    ///< Spacing/gap/padding multiplier for container layouts
    keel::Optional<bool> debugBounds{keel::kNullOpt}; ///< Shows (or hides) debug bounding boxes inside the subtree
    keel::Optional<bool> inspectMode{keel::kNullOpt}; ///< Enables (or disables) the DevTools inspector inside the subtree

    [[nodiscard]] constexpr bool IsEmpty() const noexcept {
        return theme == nullptr
            && !scale.has_value()
            && !density.has_value()
            && !debugBounds.has_value()
            && !inspectMode.has_value();
    }
};

/// Resolves child zone state from `parent` + `opts` into `arena` (returns `parent` unchanged when `opts.IsEmpty()`).
[[nodiscard]] const Zone* ResolveZone(
    const Zone* parent,
    const ZoneOptions& opts,
    keel::Arena& arena
);

} // namespace guikit
