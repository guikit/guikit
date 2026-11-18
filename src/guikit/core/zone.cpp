#include "guikit/core/zone.hpp"
#include "guikit/core/theme.hpp"
#include "keel/arena.h"

namespace guikit {

// Zone resolution helper; keeps translation unit linked for build targets.
const Zone* ResolveZone(
    const Zone* parent,
    const ZoneOptions& opts,
    keel::Arena& arena
) {
    const Zone* base = (parent != nullptr) ? parent : &kDefaultZone;
    if (opts.IsEmpty()) {
        return base;
    }
    Zone* resolved = arena.create<Zone>(*base);
    if (opts.theme.has_value()) {
        resolved->theme = arena.create<UiTheme>(*opts.theme);
    }
    if (opts.scale.has_value() && *opts.scale > 0.0f) {
        resolved->scale *= (*opts.scale);
    }
    if (opts.density.has_value() && *opts.density > 0.0f) {
        resolved->density *= (*opts.density);
    }
    if (opts.debugBounds.has_value()) {
        resolved->debugBounds = opts.debugBounds;
    }
    if (opts.inspectMode.has_value()) {
        resolved->inspectMode = opts.inspectMode;
    }
    return resolved;
}

} // namespace guikit
