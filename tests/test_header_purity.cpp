#include "guikit/core.hpp"
#include "guikit/guikit.hpp"
#include "guikit/backend/i_platform_window.hpp"
#include "guikit/backend/null_canvas.hpp"
#include "guikit/backend/text_compositor.hpp"
#include "guikit/core/shell_controls.hpp"
#include "guikit/core/text_buffer.hpp"

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

#if defined(FT_FREETYPE_H) || defined(FT2BUILD_H_)
#error "guikit public headers must not leak FreeType headers"
#endif

#if defined(min) || defined(max) || defined(near) || defined(far) || defined(DrawText) || defined(RGB)
#error "guikit public headers must not leak C/Win32 macros"
#endif

// 2. Verify zero STL container / utility header pollution in public headers (keel-only baseline)
#if defined(_GLIBCXX_STRING) || defined(_LIBCPP_STRING) || defined(_STRING_)
#error "guikit public headers must not leak <string>"
#endif

#if defined(_GLIBCXX_STRING_VIEW) || defined(_LIBCPP_STRING_VIEW) || defined(_STRING_VIEW_)
#error "guikit public headers must not leak <string_view>"
#endif

#if defined(_GLIBCXX_VECTOR) || defined(_LIBCPP_VECTOR) || defined(_VECTOR_)
#error "guikit public headers must not leak <vector>"
#endif

#if defined(_GLIBCXX_SPAN) || defined(_LIBCPP_SPAN) || defined(_SPAN_)
#error "guikit public headers must not leak <span>"
#endif

#if defined(_GLIBCXX_OPTIONAL) || defined(_LIBCPP_OPTIONAL) || defined(_OPTIONAL_)
#error "guikit public headers must not leak <optional>"
#endif

#if defined(_GLIBCXX_ARRAY) || defined(_LIBCPP_ARRAY) || defined(_ARRAY_)
#error "guikit public headers must not leak <array>"
#endif

#if defined(_GLIBCXX_FUNCTIONAL) || defined(_LIBCPP_FUNCTIONAL) || defined(_FUNCTIONAL_)
#error "guikit public headers must not leak <functional>"
#endif

#if defined(_GLIBCXX_MEMORY) || defined(_LIBCPP_MEMORY) || defined(_MEMORY_)
#error "guikit public headers must not leak <memory>"
#endif

#if defined(_GLIBCXX_UNORDERED_MAP) || defined(_LIBCPP_UNORDERED_MAP) || defined(_UNORDERED_MAP_)
#error "guikit public headers must not leak <unordered_map>"
#endif

#if defined(_GLIBCXX_VARIANT) || defined(_LIBCPP_VARIANT) || defined(_VARIANT_)
#error "guikit public headers must not leak <variant>"
#endif

#if defined(_GLIBCXX_CHRONO) || defined(_LIBCPP_CHRONO) || defined(_CHRONO_)
#error "guikit public headers must not leak <chrono>"
#endif

#if defined(_GLIBCXX_SOURCE_LOCATION) || defined(_LIBCPP_SOURCE_LOCATION) || defined(_SOURCE_LOCATION_)
#error "guikit public headers must not leak <source_location>"
#endif

#if defined(_GLIBCXX_TYPEINDEX) || defined(_LIBCPP_TYPEINDEX) || defined(_TYPEINDEX_)
#error "guikit public headers must not leak <typeindex>"
#endif

#if defined(_GLIBCXX_ALGORITHM) || defined(_LIBCPP_ALGORITHM) || defined(_ALGORITHM_)
#error "guikit public headers must not leak <algorithm>"
#endif

#if defined(_GLIBCXX_CMATH) || defined(_LIBCPP_CMATH) || defined(_CMATH_)
#error "guikit public headers must not leak <cmath> — use keel/math.h"
#endif

// 3. Verify `guikit::geo` internal/GIS symbols do NOT leak into `namespace guikit`
namespace purity_probe {
struct PolygonTriangulator {
    keel::i32 ok{1};
};
struct LayerStyle {
    keel::i32 ok{1};
};
struct AttributeValue {
    keel::i32 ok{1};
};
} // namespace purity_probe

namespace {
[[nodiscard]] keel::i32 VerifyNamespaceHygiene() noexcept {
    using namespace guikit;
    using namespace purity_probe;

    // Would fail with ambiguous symbol error if `using namespace geo;` existed inside `namespace guikit`.
    const PolygonTriangulator a{};
    const LayerStyle b{};
    const AttributeValue c{};
    return a.ok + b.ok + c.ok;
}
} // namespace

int main() {
    static_assert(guikit::Version::major >= 1);
    static_assert(sizeof(guikit::Node) <= 256);
    static_assert(sizeof(keel::i8) == 1 && sizeof(keel::u8) == 1);
    static_assert(sizeof(keel::i16) == 2 && sizeof(keel::u16) == 2);
    static_assert(sizeof(keel::i32) == 4 && sizeof(keel::u32) == 4);
    static_assert(sizeof(keel::i64) == 8 && sizeof(keel::u64) == 8);
    static_assert(sizeof(keel::isize) == sizeof(void*) && sizeof(keel::usize) == sizeof(void*));
    static_assert(sizeof(keel::f32) == 4 && sizeof(keel::f64) == 8);
    return (VerifyNamespaceHygiene() == 3) ? 0 : 1;
}
