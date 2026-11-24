#pragma once

#include "guikit/style/theme.hpp"
#include "keel/optional.h"

namespace guikit {

/// Queries the host OS or browser preferred color scheme (`ThemeMode::Dark` or `ThemeMode::Light`),
/// or `keel::kNullOpt` if unavailable or indeterminate.
///
/// Stateless, zero-external-dependency implementation shared across all GuiKit backends:
/// - Web / Emscripten: `window.matchMedia('(prefers-color-scheme: ...)')`
/// - Windows: `HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize\AppsUseLightTheme`
/// - macOS: `AppleInterfaceStyle` via dynamic `CoreFoundation` lookup
/// - Linux / Unix: `GTK_THEME`, `gtk-3.0/settings.ini`, `gtk-4.0/settings.ini`, and `kdeglobals`
[[nodiscard]] keel::Optional<ThemeMode> QueryOsSystemTheme() noexcept;

} // namespace guikit
