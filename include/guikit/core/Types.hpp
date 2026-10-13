#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace guikit {

/// 2D floating-point vector for layout coordinates, sizes, and offsets.
struct Vec2 {
    float x{0.0f};
    float y{0.0f};

    constexpr Vec2() noexcept = default;
    constexpr Vec2(float inX, float inY) noexcept : x(inX), y(inY) {}

    [[nodiscard]] constexpr bool operator==(const Vec2& other) const noexcept {
        return x == other.x && y == other.y;
    }

    [[nodiscard]] constexpr bool operator!=(const Vec2& other) const noexcept {
        return !(*this == other);
    }

    [[nodiscard]] constexpr Vec2 operator+(const Vec2& other) const noexcept {
        return {x + other.x, y + other.y};
    }

    [[nodiscard]] constexpr Vec2 operator-(const Vec2& other) const noexcept {
        return {x - other.x, y - other.y};
    }

    [[nodiscard]] constexpr Vec2 operator*(float scalar) const noexcept {
        return {x * scalar, y * scalar};
    }

    [[nodiscard]] constexpr Vec2 operator/(float scalar) const noexcept {
        return {x / scalar, y / scalar};
    }

    constexpr Vec2& operator+=(const Vec2& other) noexcept {
        x += other.x;
        y += other.y;
        return *this;
    }

    constexpr Vec2& operator-=(const Vec2& other) noexcept {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    constexpr Vec2& operator*=(float scalar) noexcept {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    [[nodiscard]] float Length() const noexcept {
        return std::sqrt(x * x + y * y);
    }
};

/// 2D rectangle defined by top-left position and dimensions.
struct Rect {
    float x{0.0f};
    float y{0.0f};
    float w{0.0f};
    float h{0.0f};

    constexpr Rect() noexcept = default;
    constexpr Rect(float inX, float inY, float inW, float inH) noexcept
        : x(inX), y(inY), w(inW), h(inH) {}
    constexpr Rect(Vec2 pos, Vec2 size) noexcept
        : x(pos.x), y(pos.y), w(size.x), h(size.y) {}

    [[nodiscard]] constexpr Vec2 Pos() const noexcept { return {x, y}; }
    [[nodiscard]] constexpr Vec2 Size() const noexcept { return {w, h}; }

    [[nodiscard]] constexpr float Left() const noexcept { return x; }
    [[nodiscard]] constexpr float Top() const noexcept { return y; }
    [[nodiscard]] constexpr float Right() const noexcept { return x + w; }
    [[nodiscard]] constexpr float Bottom() const noexcept { return y + h; }

    [[nodiscard]] constexpr Vec2 Center() const noexcept {
        return {x + w * 0.5f, y + h * 0.5f};
    }

    [[nodiscard]] constexpr bool Contains(Vec2 pt) const noexcept {
        return w > 0.0f && h > 0.0f && pt.x >= x && pt.x <= (x + w) && pt.y >= y && pt.y <= (y + h);
    }

    [[nodiscard]] constexpr bool Intersects(const Rect& other) const noexcept {
        return x < other.Right() && Right() > other.x &&
               y < other.Bottom() && Bottom() > other.y;
    }

    [[nodiscard]] constexpr Rect Intersect(const Rect& other) const noexcept {
        const float nx = std::max(x, other.x);
        const float ny = std::max(y, other.y);
        const float nr = std::min(Right(), other.Right());
        const float nb = std::min(Bottom(), other.Bottom());
        if (nr <= nx || nb <= ny) {
            return {nx, ny, 0.0f, 0.0f};
        }
        return {nx, ny, nr - nx, nb - ny};
    }

    [[nodiscard]] constexpr Rect Inset(float amount) const noexcept {
        return {x + amount, y + amount, std::max(0.0f, w - 2.0f * amount), std::max(0.0f, h - 2.0f * amount)};
    }

    [[nodiscard]] constexpr Rect Inset(float dx, float dy) const noexcept {
        return {x + dx, y + dy, std::max(0.0f, w - 2.0f * dx), std::max(0.0f, h - 2.0f * dy)};
    }

    [[nodiscard]] constexpr float Area() const noexcept {
        return (w > 0.0f && h > 0.0f) ? (w * h) : 0.0f;
    }

    [[nodiscard]] constexpr Rect Union(const Rect& other) const noexcept {
        if (w <= 0.0f || h <= 0.0f) return other;
        if (other.w <= 0.0f || other.h <= 0.0f) return *this;
        const float nx = std::min(x, other.x);
        const float ny = std::min(y, other.y);
        const float nr = std::max(Right(), other.Right());
        const float nb = std::max(Bottom(), other.Bottom());
        return {nx, ny, nr - nx, nb - ny};
    }

    [[nodiscard]] constexpr bool operator==(const Rect& other) const noexcept {
        return x == other.x && y == other.y && w == other.w && h == other.h;
    }

    [[nodiscard]] constexpr bool operator!=(const Rect& other) const noexcept {
        return !(*this == other);
    }
};

/// 32-bit RGBA color with linear blending utilities.
struct Color {
    uint8_t r{0};
    uint8_t g{0};
    uint8_t b{0};
    uint8_t a{255};

    constexpr Color() noexcept = default;
    constexpr Color(uint8_t inR, uint8_t inG, uint8_t inB, uint8_t inA = 255) noexcept
        : r(inR), g(inG), b(inB), a(inA) {}

    /// Pack as little-endian 32-bit integer (ABGR in memory / 0xAABBGGRR).
    [[nodiscard]] constexpr uint32_t ToUint32() const noexcept {
        return (static_cast<uint32_t>(a) << 24) |
               (static_cast<uint32_t>(b) << 16) |
               (static_cast<uint32_t>(g) << 8) |
               static_cast<uint32_t>(r);
    }

    [[nodiscard]] constexpr bool operator==(const Color& other) const noexcept {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }

    [[nodiscard]] constexpr bool operator!=(const Color& other) const noexcept {
        return !(*this == other);
    }

    /// Construct from hexadecimal value (0xRRGGBBAA or 0xRRGGBB).
    [[nodiscard]] static constexpr Color Hex(uint32_t hex, bool hasAlpha = true) noexcept {
        if (hasAlpha) {
            return {
                static_cast<uint8_t>((hex >> 24) & 0xFF),
                static_cast<uint8_t>((hex >> 16) & 0xFF),
                static_cast<uint8_t>((hex >> 8) & 0xFF),
                static_cast<uint8_t>(hex & 0xFF)
            };
        }
        return {
            static_cast<uint8_t>((hex >> 16) & 0xFF),
            static_cast<uint8_t>((hex >> 8) & 0xFF),
            static_cast<uint8_t>(hex & 0xFF),
            255
        };
    }

    [[nodiscard]] static constexpr Color Rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) noexcept {
        return {r, g, b, a};
    }

    [[nodiscard]] static constexpr Color Transparent() noexcept {
        return {0, 0, 0, 0};
    }

    [[nodiscard]] constexpr Color WithAlpha(uint8_t newAlpha) const noexcept {
        return {r, g, b, newAlpha};
    }

    [[nodiscard]] constexpr Color WithAlphaF(float factor) const noexcept {
        const float clamped = std::clamp(factor, 0.0f, 1.0f);
        return {r, g, b, static_cast<uint8_t>(static_cast<float>(a) * clamped)};
    }

    [[nodiscard]] static Color Lerp(Color a, Color b, float t) noexcept {
        const float f = std::clamp(t, 0.0f, 1.0f);
        return {
            static_cast<uint8_t>(std::lerp(static_cast<float>(a.r), static_cast<float>(b.r), f)),
            static_cast<uint8_t>(std::lerp(static_cast<float>(a.g), static_cast<float>(b.g), f)),
            static_cast<uint8_t>(std::lerp(static_cast<float>(a.b), static_cast<float>(b.b), f)),
            static_cast<uint8_t>(std::lerp(static_cast<float>(a.a), static_cast<float>(b.a), f))
        };
    }
};

/// Logical key codes for keyboard navigation and text editing.
enum class Key : uint16_t {
    None = 0,
    Backspace,
    Delete,
    Enter,
    Escape,
    Tab,
    Left,
    Right,
    Home,
    End,
    Up,
    Down,
    A, C, V, X, Z
};

/// Keyboard modifier flags.
struct KeyModifiers {
    bool ctrl{false};
    bool shift{false};
    bool alt{false};
};

/// Visual blinking mode for input caret / cursor.
enum class CursorBlinkMode : uint8_t {
    Blinking,  ///< Standard alternating visibility (~1 Hz)
    Solid,     ///< Always visible while focused (reduced motion / industrial UI)
    Hidden     ///< Never rendered
};

/// Timing and geometry configuration for text input cursor.
struct CursorConfig {
    CursorBlinkMode mode{CursorBlinkMode::Blinking};
    float blinkPeriod{1.06f};   ///< Full cycle duration in seconds (530ms on, 530ms off)
    float width{1.5f};          ///< Caret line width in pixels
};



/// Universal 1D/2D orientation for layout and components (ScrollArea, Splitter, Slider, Separator).
enum class Orientation : uint8_t {
    Vertical,
    Horizontal,
    Both
};

/// Universal conditional visibility policy (Scrollbars, ClearButtons, Headers, Legends).
enum class VisibilityPolicy : uint8_t {
    Auto,    ///< Context-dependent / shown when needed (e.g. on overflow, content presence, hover)
    Always,  ///< Unconditionally shown
    Never    ///< Suppressed / never rendered
};

/// Density-independent point unit (dp) wrapper.
struct Dp {
    float value{0.0f};

    constexpr explicit Dp(float v) noexcept : value(v) {}

    [[nodiscard]] inline float ToPixels(float scaleFactor) const noexcept {
        return std::round(value * scaleFactor);
    }
};

/// Non-allocating view wrapper for candidate text strings used in dynamic layout sizing.
class TextVariants {
public:
    constexpr TextVariants() noexcept = default;

    constexpr TextVariants(std::initializer_list<std::string_view> list) noexcept {
        const size_t n = std::min(list.size(), m_inline.size());
        auto it = list.begin();
        for (size_t i = 0; i < n; ++i, ++it) {
            m_inline[i] = *it;
        }
        m_count = n;
        m_span = std::span<const std::string_view>(m_inline.data(), m_count);
    }

    constexpr TextVariants(std::span<const std::string_view> s) noexcept
        : m_span(s)
    {
    }

    template <size_t N>
    constexpr TextVariants(const std::array<std::string_view, N>& arr) noexcept
        : m_span(arr.data(), arr.size())
    {
    }

    TextVariants(const std::vector<std::string_view>& vec) noexcept
        : m_span(vec.data(), vec.size())
    {
    }

    constexpr TextVariants(const TextVariants& other) noexcept
        : m_inline(other.m_inline)
        , m_count(other.m_count)
    {
        if (other.m_span.data() == other.m_inline.data()) {
            m_span = std::span<const std::string_view>(m_inline.data(), m_count);
        } else {
            m_span = other.m_span;
        }
    }

    constexpr TextVariants& operator=(const TextVariants& other) noexcept {
        if (this != &other) {
            m_inline = other.m_inline;
            m_count = other.m_count;
            if (other.m_span.data() == other.m_inline.data()) {
                m_span = std::span<const std::string_view>(m_inline.data(), m_count);
            } else {
                m_span = other.m_span;
            }
        }
        return *this;
    }

    constexpr TextVariants(TextVariants&& other) noexcept
        : m_inline(other.m_inline)
        , m_count(other.m_count)
    {
        if (other.m_span.data() == other.m_inline.data()) {
            m_span = std::span<const std::string_view>(m_inline.data(), m_count);
        } else {
            m_span = other.m_span;
        }
    }

    constexpr TextVariants& operator=(TextVariants&& other) noexcept {
        if (this != &other) {
            m_inline = other.m_inline;
            m_count = other.m_count;
            if (other.m_span.data() == other.m_inline.data()) {
                m_span = std::span<const std::string_view>(m_inline.data(), m_count);
            } else {
                m_span = other.m_span;
            }
        }
        return *this;
    }

    [[nodiscard]] constexpr auto begin() const noexcept { return m_span.begin(); }
    [[nodiscard]] constexpr auto end() const noexcept { return m_span.end(); }
    [[nodiscard]] constexpr bool empty() const noexcept { return m_span.empty(); }
    [[nodiscard]] constexpr size_t size() const noexcept { return m_span.size(); }
    [[nodiscard]] constexpr std::span<const std::string_view> span() const noexcept { return m_span; }

private:
    std::span<const std::string_view> m_span{};
    std::array<std::string_view, 8> m_inline{};
    size_t m_count{0};
};

namespace literals {
    constexpr Dp operator""_dp(long double v) noexcept {
        return Dp(static_cast<float>(v));
    }
    constexpr Dp operator""_dp(unsigned long long v) noexcept {
        return Dp(static_cast<float>(v));
    }
} // namespace literals

/// Universal design token scale for density, typography, spacing, and component dimensions.
enum class UiSize : uint8_t {
    None = 0, ///< Zero / unpadded / no gap / square corners
    Xs,       ///< Extra Small (compact toolbars, inline badges, dense data tables)
    Sm,       ///< Small (secondary panels, compact forms)
    Md,       ///< Medium (standard default controls across the engine)
    Lg,       ///< Large (prominent action bars, touch-friendly interfaces)
    Xl        ///< Extra Large (hero actions, emergency stops, kiosk displays)
};

/// Direction of primary layout axis.
enum class FlexDirection : uint8_t {
    Row,    ///< Children laid out horizontally from left to right
    Column, ///< Children laid out vertically from top to bottom
    Wrap,   ///< Children laid out horizontally and wrapped to subsequent lines upon overflow
    Grid    ///< 12-column responsive grid with automatic line wrapping
};

/// Sizing policy along an axis.
enum class SizeMode : uint8_t {
    Stretch,    ///< Expands to fill available space allocated by parent container
    Fit,        ///< Shrinks to hug content / intrinsic child sizes
    Fixed       ///< Explicit dimension in points
};

/// Stretch layout behavior for composite widgets containing labels and interactive controls.
enum class StretchMode : uint8_t {
    Compact, ///< Compact intrinsic size based on content (no row stretching)
    Space,   ///< Row stretches full width; control stays compact and extra space becomes air/gap
    Fill     ///< Row stretches full width; control stretches to fill all remaining available space
};

/// Alignment along the cross axis (perpendicular to FlexDirection).
enum class Alignment : uint8_t {
    Start,      ///< Align to start of cross axis (Top in Row, Left in Column)
    Center,     ///< Center along cross axis
    End,        ///< Align to end of cross axis (Bottom in Row, Right in Column)
    Stretch     ///< Stretch to fill full cross-axis dimension
};

/// Type alias for backward compatibility.
using Align = Alignment;

/// Distribution along the main axis (parallel to FlexDirection) when space is not fully stretched.
enum class Justify : uint8_t {
    Start,          ///< Pack children at start of main axis
    Center,         ///< Center children along main axis
    End,            ///< Pack children at end of main axis
    SpaceBetween    ///< Distribute items evenly; first item at start, last at end
};

/// Inset margins or internal padding on 4 sides, supporting both explicit pixel insets and semantic design tokens.
struct Padding {
    float left{0.0f};
    float top{0.0f};
    float right{0.0f};
    float bottom{0.0f};

    constexpr Padding() noexcept = default;

    /// Uniform explicit pixel padding on all 4 sides.
    constexpr Padding(float all) noexcept
        : left(all), top(all), right(all), bottom(all) {}

    /// Explicit pixel padding on 4 individual sides.
    constexpr Padding(float l, float t, float r, float b) noexcept
        : left(l), top(t), right(r), bottom(b) {}

    /// Semantic design token padding dynamically resolved via UiTheme and UI density scale.
    constexpr Padding(UiSize size) noexcept
        : left(-1.0f), top(static_cast<float>(size)), right(0.0f), bottom(0.0f) {}

    [[nodiscard]] static constexpr Padding All(float val) noexcept {
        return Padding(val);
    }

    [[nodiscard]] static constexpr Padding Symmetric(float horizontal, float vertical) noexcept {
        return Padding(horizontal, vertical, horizontal, vertical);
    }

    [[nodiscard]] constexpr bool IsToken() const noexcept {
        return left < 0.0f;
    }

    [[nodiscard]] constexpr UiSize GetToken() const noexcept {
        return (left < 0.0f) ? static_cast<UiSize>(top) : UiSize::None;
    }

    [[nodiscard]] constexpr float Horizontal() const noexcept {
        return (left < 0.0f) ? 0.0f : (left + right);
    }

    [[nodiscard]] constexpr float Vertical() const noexcept {
        return (left < 0.0f) ? 0.0f : (top + bottom);
    }

    [[nodiscard]] constexpr bool operator==(const Padding& other) const noexcept {
        if (IsToken() || other.IsToken()) {
            return IsToken() == other.IsToken() && GetToken() == other.GetToken();
        }
        return left == other.left && top == other.top && right == other.right && bottom == other.bottom;
    }
};

static_assert(sizeof(Padding) == 16, "Padding must remain exactly 16 bytes for cache line layout");

} // namespace guikit
