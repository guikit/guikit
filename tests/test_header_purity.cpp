#include "guikit/core.hpp"
#include "guikit/guikit.hpp"
#if defined(GUIKIT_HAS_SCENES) && GUIKIT_HAS_SCENES
#include "guikit/scenes.hpp"
#endif
#include "guikit/backend/sdl_canvas.hpp"
#include "guikit/backend/sdl_gl_shader_engine.hpp"
#include "guikit/backend/sdl_window_backend.hpp"
#include "guikit/backend/thor_canvas.hpp"

// 1. Verify zero OS / C / third-party header & macro pollution in public headers
#if defined(_WINDOWS_) || defined(_INC_WINDOWS)
#error "guikit public headers must not leak <windows.h>"
#endif

#if defined(SDL_h_) || defined(SDL_INIT_VIDEO)
#error "guikit public headers must not leak <SDL3/SDL.h>"
#endif

#if defined(THORVG_H) || defined(_THORVG_H_)
#error "guikit public headers must not leak thorvg.h"
#endif

#if defined(min) || defined(max) || defined(near) || defined(far) || defined(DrawText) || defined(RGB)
#error "guikit public headers must not leak C/Win32 macros"
#endif

// 2. Verify `guikit::geo` internal/GIS symbols do NOT leak into `namespace guikit`
namespace purity_probe {
struct PolygonTriangulator {
    int ok{1};
};
struct LayerStyle {
    int ok{1};
};
struct AttributeValue {
    int ok{1};
};
} // namespace purity_probe

namespace {
[[nodiscard]] int VerifyNamespaceHygiene() noexcept {
    using namespace guikit;
    using namespace purity_probe;

    // Would fail with ambiguous symbol error if `using namespace geo;` existed inside `namespace guikit`.
    const PolygonTriangulator a{};
    const LayerStyle b{};
    const AttributeValue c{};
#if defined(GUIKIT_HAS_SCENES) && GUIKIT_HAS_SCENES
    static_assert(std::is_same_v<guikit::GeoJsonIO, guikit::geo::GeoJsonIO>);
#endif
    return a.ok + b.ok + c.ok;
}
} // namespace

int main() {
    static_assert(guikit::Version::major >= 1);
    static_assert(sizeof(guikit::Node) <= 256);
    return (VerifyNamespaceHygiene() == 3) ? 0 : 1;
}
