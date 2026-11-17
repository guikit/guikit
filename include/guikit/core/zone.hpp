#pragma once

#include "guikit/core/types.hpp"
#include "keel/core.h"
#include "keel/optional.h"

namespace guikit {

struct UiTheme;
class Context;

/// Ambient state that every widget inherits from the zone it is built in.
///
/// A `Context` frame starts in a *root zone* (the context theme, `UiSize::Md`, enabled). Containers, widgets
/// with a `.theme` / `.scale` override, `ui.Size(...)` and `ui.Zone(...)` each enter a child zone on a
/// per-frame stack and leave it again when their scope ends. Nothing is allocated on the heap.
struct ZoneState {
    UiTheme* theme{nullptr};     ///< Active theme; `theme->scale` is the zone's scale in layout units
    UiSize size{UiSize::Md};     ///< Active semantic control size
    bool disabled{false};        ///< Inherited disabled flag (a disabled zone cannot be re-enabled by a child)
};

/// What a child zone changes relative to its parent. Anything left unset is inherited.
struct ZoneOptions {
    /// Replaces the inherited theme (copied into the frame arena, then scaled to the zone's scale).
    const UiTheme* theme{nullptr};

    /// Zone scale in layout units, i.e. relative to the frame's pixels-per-dp factor. This is what the
    /// `.scale` option of containers and widgets resolves to. Mutually exclusive with `zoom`.
    keel::Optional<float> scale{keel::kNullOpt};

    /// Zone zoom relative to the *display* scale: `1.0f` is 100% of the OS scale whatever the frame's
    /// root zoom (the shell's user zoom) is, so the zone keeps its pixel size when the user zooms the UI.
    /// Mutually exclusive with `scale`.
    keel::Optional<float> zoom{keel::kNullOpt};

    keel::Optional<UiSize> size{keel::kNullOpt}; ///< Replaces the inherited control size (`None` = inherit)
    bool disabled{false};                        ///< ORed with the inherited disabled flag
};

/// RAII guard that enters a child zone and leaves it on destruction.
///
/// ```cpp
/// if (auto zone = ui.Zone({.theme = &previewTheme, .zoom = 1.25f})) {
///     ui.Button({.text = "Rendered at 125% of the display scale, whatever the shell zoom is"});
/// }
/// ```
class [[nodiscard]] ZoneScope {
public:
    ZoneScope() noexcept = default;
    ZoneScope(Context& ctx, const ZoneOptions& opts);
    ~ZoneScope();

    ZoneScope(const ZoneScope&) = delete;
    ZoneScope& operator=(const ZoneScope&) = delete;

    ZoneScope(ZoneScope&& other) noexcept
        : m_ctx(other.m_ctx), m_theme(other.m_theme), m_depth(other.m_depth) {
        other.m_ctx = nullptr;
        other.m_theme = nullptr;
    }

    ZoneScope& operator=(ZoneScope&& other) noexcept;

    /// Evaluates to true so `if (auto zone = ui.Zone(...)) { ... }` reads like the other scopes.
    [[nodiscard]] explicit constexpr operator bool() const noexcept { return m_ctx != nullptr; }

    /// The arena-owned theme this zone created, or `nullptr` when it inherited its parent's theme.
    /// Nodes store this as `Node::theme` so layout and rendering see the zone's theme.
    [[nodiscard]] UiTheme* GetTheme() const noexcept { return m_theme; }

    /// Removes this zone from the stack without leaving the zones entered after it and disarms the guard.
    /// Zones hold a full copy of their parent's state, so a container opened inside this zone can take over
    /// its role (this is how `ui.Size(...).Row(...)` hands the size to the row instead of nesting twice).
    void Detach() noexcept;

private:
    Context* m_ctx{nullptr};
    UiTheme* m_theme{nullptr};
    keel::usize m_depth{0}; ///< Zone stack size right after this zone was entered
};

} // namespace guikit
