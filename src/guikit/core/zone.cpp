#include "guikit/core/zone.hpp"
#include "guikit/core/theme.hpp"
#include "keel/arena.h"

namespace guikit {

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
    if (opts.HasThemeOverride()) {
        UiTheme* arenaTheme = nullptr;
        if (opts.theme.has_value()) {
            arenaTheme = arena.create<UiTheme>(*opts.theme);
        } else if (opts.preset.has_value()) {
            arenaTheme = arena.create<UiTheme>(UiTheme::FromPreset(*opts.preset));
            if (base->theme != nullptr) {
                arenaTheme->SetShellStyle(base->theme->shellStyle);
            }
        } else if (base->theme != nullptr) {
            arenaTheme = arena.create<UiTheme>(*base->theme);
        } else {
            arenaTheme = arena.create<UiTheme>(UiTheme::Dark());
        }

        if (opts.preset.has_value() && opts.theme.has_value()) {
            arenaTheme->SetPreset(*opts.preset);
        }
        if (opts.mode.has_value() && arenaTheme->mode != *opts.mode) {
            arenaTheme->SetMode(*opts.mode);
        }
        if (opts.accent.has_value()) {
            arenaTheme->SetAccent(*opts.accent);
        }
        if (opts.surfaceStyle.has_value()) {
            arenaTheme->SetSurfaceStyle(*opts.surfaceStyle);
        }
        if (opts.radiusScale.has_value()) {
            arenaTheme->SetRadiusScale(*opts.radiusScale);
        }
        if (opts.shadowIntensity.has_value()) {
            arenaTheme->SetShadowIntensity(*opts.shadowIntensity);
        }
        resolved->theme = arenaTheme;
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
