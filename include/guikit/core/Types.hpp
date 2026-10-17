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
    Space,
    Left,
    Right,
    Home,
    End,
    Up,
    Down,
    PageUp,
    PageDown,
    A, C, D, F, G, H, K, V, X, Y, Z,
    Slash,
    Plus,
    Minus,
    Zero,
    F3,
    F5,
    F9,
    F10,
    F11
};

/// Keyboard modifier flags.
struct KeyModifiers {
    bool ctrl{false};
    bool shift{false};
    bool alt{false};

    [[nodiscard]] constexpr bool operator==(const KeyModifiers& other) const noexcept {
        return ctrl == other.ctrl && shift == other.shift && alt == other.alt;
    }
    [[nodiscard]] constexpr bool operator!=(const KeyModifiers& other) const noexcept {
        return !(*this == other);
    }
};

/// Semantic keyboard actions decoupled from physical key codes.
/// Widgets react to KeyAction rather than hardcoded Key + KeyModifiers combinations.
enum class KeyAction : uint8_t {
    None = 0,

    // Clipboard & Selection
    Copy,               ///< Copy selection to clipboard (default: Ctrl+C)
    Cut,                ///< Cut selection to clipboard (default: Ctrl+X)
    Paste,              ///< Paste from clipboard (default: Ctrl+V)
    SelectAll,          ///< Select all content (default: Ctrl+A)

    // Undo / Redo History
    Undo,               ///< Undo last edit (default: Ctrl+Z)
    Redo,               ///< Redo last undone edit (default: Ctrl+Y or Ctrl+Shift+Z)

    // Cursor Movement
    MoveLeft,           ///< Move cursor left by 1 character (default: Left)
    MoveRight,          ///< Move cursor right by 1 character (default: Right)
    MoveUp,             ///< Move cursor up by 1 line (default: Up)
    MoveDown,           ///< Move cursor down by 1 line (default: Down)
    MoveWordLeft,       ///< Move cursor to previous word boundary (default: Ctrl+Left)
    MoveWordRight,      ///< Move cursor to next word boundary (default: Ctrl+Right)
    MoveLineStart,      ///< Move cursor to start of line (default: Home)
    MoveLineEnd,        ///< Move cursor to end of line (default: End)
    MoveDocStart,       ///< Move cursor to start of document (default: Ctrl+Home)
    MoveDocEnd,         ///< Move cursor to end of document (default: Ctrl+End)
    MovePageUp,         ///< Move cursor / viewport up by 1 page (default: PageUp)
    MovePageDown,       ///< Move cursor / viewport down by 1 page (default: PageDown)

    // Cursor Selection Extension (Shift + Movement)
    SelectLeft,         ///< Extend selection left by 1 character (default: Shift+Left)
    SelectRight,        ///< Extend selection right by 1 character (default: Shift+Right)
    SelectUp,           ///< Extend selection up by 1 line (default: Shift+Up)
    SelectDown,         ///< Extend selection down by 1 line (default: Shift+Down)
    SelectWordLeft,     ///< Extend selection to previous word boundary (default: Ctrl+Shift+Left)
    SelectWordRight,    ///< Extend selection to next word boundary (default: Ctrl+Shift+Right)
    SelectLineStart,    ///< Extend selection to start of line (default: Shift+Home)
    SelectLineEnd,      ///< Extend selection to end of line (default: Shift+End)
    SelectDocStart,     ///< Extend selection to start of document (default: Ctrl+Shift+Home)
    SelectDocEnd,       ///< Extend selection to end of document (default: Ctrl+Shift+End)
    SelectPageUp,       ///< Extend selection up by 1 page (default: Shift+PageUp)
    SelectPageDown,     ///< Extend selection down by 1 page (default: Shift+PageDown)

    // Deletion & Editing Primitives
    DeletePrevChar,     ///< Delete character before cursor (default: Backspace)
    DeleteNextChar,     ///< Delete character after cursor (default: Delete)
    DeletePrevWord,     ///< Delete word before cursor (default: Ctrl+Backspace)
    DeleteNextWord,     ///< Delete word after cursor (default: Ctrl+Delete)
    Confirm,            ///< Confirm / Submit / Newline (default: Enter)
    Cancel,             ///< Cancel / Close overlay / Clear selection (default: Escape)
    Indent,             ///< Indent line/selection or focus next (default: Tab)
    Unindent,           ///< Unindent line/selection or focus prev (default: Shift+Tab)

    // Code Editor Line Operations
    DuplicateLine,      ///< Duplicate current line or select next occurrence (default: Ctrl+D)
    DeleteLine,         ///< Delete entire current line (default: Ctrl+Shift+K)
    ToggleComment,      ///< Toggle line comment on selection (default: Ctrl+/)
    MoveLineUp,         ///< Swap current line with line above (default: Alt+Up)
    MoveLineDown,       ///< Swap current line with line below (default: Alt+Down)

    // Search & Navigation
    Find,               ///< Open Find bar (default: Ctrl+F)
    Replace,            ///< Open Find & Replace bar (default: Ctrl+H)
    FindNext,           ///< Jump to next search match (default: F3)
    FindPrev,           ///< Jump to previous search match (default: Shift+F3)
    GoToLine,           ///< Open Go to Line dialog (default: Ctrl+G)

    // Viewport & Typography Zoom
    ZoomIn,             ///< Increase zoom level (default: Ctrl+Plus)
    ZoomOut,            ///< Decrease zoom level (default: Ctrl+Minus)
    ZoomReset,          ///< Reset zoom level to 100% (default: Ctrl+0)

    // Debugger Controls
    DebugContinue,      ///< Continue execution (default: F5)
    DebugStop,          ///< Stop debugging session (default: Shift+F5)
    DebugStepOver,      ///< Step over current line (default: F10)
    DebugStepInto,      ///< Step into function call (default: F11)
    DebugStepOut,       ///< Step out of current function (default: Shift+F11)
    ToggleBreakpoint,   ///< Toggle breakpoint on current line (default: F9)

    Count
};

/// Type alias for semantic action enum.
using Action = KeyAction;

/// Represents a single keyboard shortcut chord (Key + exact KeyModifiers).
struct KeyChord {
    Key key{Key::None};
    KeyModifiers mods{};

    constexpr KeyChord() noexcept = default;
    constexpr KeyChord(Key k, KeyModifiers m = {}) noexcept : key(k), mods(m) {}
    constexpr KeyChord(Key k, bool ctrl, bool shift = false, bool alt = false) noexcept
        : key(k), mods{ctrl, shift, alt} {}

    [[nodiscard]] constexpr bool IsValid() const noexcept { return key != Key::None; }

    [[nodiscard]] constexpr bool Matches(Key k, KeyModifiers m) const noexcept {
        return key != Key::None && key == k && mods == m;
    }

    [[nodiscard]] constexpr bool operator==(const KeyChord& other) const noexcept {
        return key == other.key && mods == other.mods;
    }
    [[nodiscard]] constexpr bool operator!=(const KeyChord& other) const noexcept {
        return !(*this == other);
    }
};

/// Primary and optional secondary key chords bound to a semantic KeyAction.
struct KeyBinding {
    KeyChord primary{};
    KeyChord secondary{};

    [[nodiscard]] constexpr bool Matches(Key k, KeyModifiers m) const noexcept {
        return primary.Matches(k, m) || secondary.Matches(k, m);
    }
};

/// Centralized, user-configurable keybinding table mapping KeyAction to KeyChords.
class Keymap {
public:
    static constexpr size_t kActionCount = static_cast<size_t>(KeyAction::Count);

    constexpr Keymap() noexcept = default;

    /// Returns the standard desktop keymap with all default bindings populated.
    [[nodiscard]] static constexpr Keymap Default() noexcept {
        Keymap km;
        // Clipboard & Selection
        km.Bind(KeyAction::Copy,            KeyChord(Key::C, true, false, false));
        km.Bind(KeyAction::Cut,             KeyChord(Key::X, true, false, false));
        km.Bind(KeyAction::Paste,           KeyChord(Key::V, true, false, false));
        km.Bind(KeyAction::SelectAll,       KeyChord(Key::A, true, false, false));

        // Undo / Redo
        km.Bind(KeyAction::Undo,            KeyChord(Key::Z, true, false, false));
        km.Bind(KeyAction::Redo,            KeyChord(Key::Y, true, false, false), KeyChord(Key::Z, true, true, false));

        // Cursor Movement
        km.Bind(KeyAction::MoveLeft,        KeyChord(Key::Left, false, false, false));
        km.Bind(KeyAction::MoveRight,       KeyChord(Key::Right, false, false, false));
        km.Bind(KeyAction::MoveUp,          KeyChord(Key::Up, false, false, false));
        km.Bind(KeyAction::MoveDown,        KeyChord(Key::Down, false, false, false));
        km.Bind(KeyAction::MoveWordLeft,    KeyChord(Key::Left, true, false, false));
        km.Bind(KeyAction::MoveWordRight,   KeyChord(Key::Right, true, false, false));
        km.Bind(KeyAction::MoveLineStart,   KeyChord(Key::Home, false, false, false));
        km.Bind(KeyAction::MoveLineEnd,     KeyChord(Key::End, false, false, false));
        km.Bind(KeyAction::MoveDocStart,    KeyChord(Key::Home, true, false, false));
        km.Bind(KeyAction::MoveDocEnd,      KeyChord(Key::End, true, false, false));
        km.Bind(KeyAction::MovePageUp,      KeyChord(Key::PageUp, false, false, false));
        km.Bind(KeyAction::MovePageDown,    KeyChord(Key::PageDown, false, false, false));

        // Cursor Selection Extension
        km.Bind(KeyAction::SelectLeft,      KeyChord(Key::Left, false, true, false));
        km.Bind(KeyAction::SelectRight,     KeyChord(Key::Right, false, true, false));
        km.Bind(KeyAction::SelectUp,        KeyChord(Key::Up, false, true, false));
        km.Bind(KeyAction::SelectDown,      KeyChord(Key::Down, false, true, false));
        km.Bind(KeyAction::SelectWordLeft,  KeyChord(Key::Left, true, true, false));
        km.Bind(KeyAction::SelectWordRight, KeyChord(Key::Right, true, true, false));
        km.Bind(KeyAction::SelectLineStart, KeyChord(Key::Home, false, true, false));
        km.Bind(KeyAction::SelectLineEnd,   KeyChord(Key::End, false, true, false));
        km.Bind(KeyAction::SelectDocStart,  KeyChord(Key::Home, true, true, false));
        km.Bind(KeyAction::SelectDocEnd,    KeyChord(Key::End, true, true, false));
        km.Bind(KeyAction::SelectPageUp,    KeyChord(Key::PageUp, false, true, false));
        km.Bind(KeyAction::SelectPageDown,  KeyChord(Key::PageDown, false, true, false));

        // Deletion & Editing Primitives
        km.Bind(KeyAction::DeletePrevChar,  KeyChord(Key::Backspace, false, false, false));
        km.Bind(KeyAction::DeleteNextChar,  KeyChord(Key::Delete, false, false, false));
        km.Bind(KeyAction::DeletePrevWord,  KeyChord(Key::Backspace, true, false, false));
        km.Bind(KeyAction::DeleteNextWord,  KeyChord(Key::Delete, true, false, false));
        km.Bind(KeyAction::Confirm,         KeyChord(Key::Enter, false, false, false));
        km.Bind(KeyAction::Cancel,          KeyChord(Key::Escape, false, false, false));
        km.Bind(KeyAction::Indent,          KeyChord(Key::Tab, false, false, false));
        km.Bind(KeyAction::Unindent,        KeyChord(Key::Tab, false, true, false));

        // Code Editor Line Operations
        km.Bind(KeyAction::DuplicateLine,   KeyChord(Key::D, true, false, false));
        km.Bind(KeyAction::DeleteLine,      KeyChord(Key::K, true, true, false));
        km.Bind(KeyAction::ToggleComment,   KeyChord(Key::Slash, true, false, false));
        km.Bind(KeyAction::MoveLineUp,      KeyChord(Key::Up, false, false, true));
        km.Bind(KeyAction::MoveLineDown,    KeyChord(Key::Down, false, false, true));

        // Search & Navigation
        km.Bind(KeyAction::Find,            KeyChord(Key::F, true, false, false));
        km.Bind(KeyAction::Replace,         KeyChord(Key::H, true, false, false));
        km.Bind(KeyAction::FindNext,        KeyChord(Key::F3, false, false, false));
        km.Bind(KeyAction::FindPrev,        KeyChord(Key::F3, false, true, false));
        km.Bind(KeyAction::GoToLine,        KeyChord(Key::G, true, false, false));

        // Zoom
        km.Bind(KeyAction::ZoomIn,          KeyChord(Key::Plus, true, false, false), KeyChord(Key::Plus, true, true, false));
        km.Bind(KeyAction::ZoomOut,         KeyChord(Key::Minus, true, false, false), KeyChord(Key::Minus, true, true, false));
        km.Bind(KeyAction::ZoomReset,       KeyChord(Key::Zero, true, false, false));

        // Debugger
        km.Bind(KeyAction::DebugContinue,    KeyChord(Key::F5, false, false, false));
        km.Bind(KeyAction::DebugStop,        KeyChord(Key::F5, false, true, false));
        km.Bind(KeyAction::DebugStepOver,    KeyChord(Key::F10, false, false, false));
        km.Bind(KeyAction::DebugStepInto,    KeyChord(Key::F11, false, false, false));
        km.Bind(KeyAction::DebugStepOut,     KeyChord(Key::F11, false, true, false));
        km.Bind(KeyAction::ToggleBreakpoint, KeyChord(Key::F9, false, false, false));

        return km;
    }

    /// Binds a semantic action to a primary (and optional secondary) key chord.
    constexpr void Bind(KeyAction action, KeyChord primary, KeyChord secondary = {}) noexcept {
        const size_t idx = static_cast<size_t>(action);
        if (idx > 0 && idx < kActionCount) {
            m_bindings[idx] = KeyBinding{primary, secondary};
        }
    }

    /// Removes all key bindings for the specified action.
    constexpr void Unbind(KeyAction action) noexcept {
        const size_t idx = static_cast<size_t>(action);
        if (idx > 0 && idx < kActionCount) {
            m_bindings[idx] = KeyBinding{};
        }
    }

    /// Returns the current binding for the specified action.
    [[nodiscard]] constexpr KeyBinding GetBinding(KeyAction action) const noexcept {
        const size_t idx = static_cast<size_t>(action);
        return (idx > 0 && idx < kActionCount) ? m_bindings[idx] : KeyBinding{};
    }

    /// Returns true if the given (key, mods) event matches the specified semantic action.
    [[nodiscard]] constexpr bool Matches(KeyAction action, Key key, KeyModifiers mods) const noexcept {
        const size_t idx = static_cast<size_t>(action);
        return (idx > 0 && idx < kActionCount) && m_bindings[idx].Matches(key, mods);
    }

    /// Resolves a physical (key, mods) event into its bound KeyAction, or KeyAction::None if unbound.
    [[nodiscard]] constexpr KeyAction Resolve(Key key, KeyModifiers mods) const noexcept {
        if (key == Key::None) return KeyAction::None;
        for (size_t i = 1; i < kActionCount; ++i) {
            if (m_bindings[i].Matches(key, mods)) {
                return static_cast<KeyAction>(i);
            }
        }
        return KeyAction::None;
    }

private:
    std::array<KeyBinding, kActionCount> m_bindings{};
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

/// Non-allocating 16-byte view wrapper for candidate text strings used in dynamic layout sizing.
class TextVariants {
public:
    constexpr TextVariants() noexcept = default;

    constexpr TextVariants(std::initializer_list<std::string_view> list) noexcept
        : m_span(list.begin(), list.size())
    {
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

    [[nodiscard]] constexpr auto begin() const noexcept { return m_span.begin(); }
    [[nodiscard]] constexpr auto end() const noexcept { return m_span.end(); }
    [[nodiscard]] constexpr bool empty() const noexcept { return m_span.empty(); }
    [[nodiscard]] constexpr size_t size() const noexcept { return m_span.size(); }
    [[nodiscard]] constexpr std::span<const std::string_view> span() const noexcept { return m_span; }

    [[nodiscard]] constexpr bool operator==(const TextVariants& other) const noexcept {
        if (m_span.size() != other.m_span.size()) {
            return false;
        }
        for (size_t i = 0; i < m_span.size(); ++i) {
            if (m_span[i] != other.m_span[i]) {
                return false;
            }
        }
        return true;
    }

private:
    std::span<const std::string_view> m_span{};
};
static_assert(sizeof(TextVariants) == 2 * sizeof(void*), "TextVariants must remain 2 * sizeof(void*) bytes");

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

/// Content fitting policy within a target bounding box (Image, Scene, Video).
enum class ObjectFit : uint8_t {
    Contain,    ///< Scale uniformly to fit entirely inside destination bounds (default)
    Cover,      ///< Scale uniformly to fill destination bounds completely (cropping excess)
    Fill,       ///< Stretch non-uniformly to match destination width and height
    ScaleDown,  ///< Like Contain, but never enlarges above 100% (1:1) natural size
    Original    ///< Keep 1:1 natural dimensions without scaling
};

/// Pure geometric helper computing the fitted destination rectangle for `srcSize` inside `dstRect`.
[[nodiscard]] constexpr Rect ComputeBoxFit(
    ObjectFit fit,
    Vec2 srcSize,
    Rect dstRect,
    float alignX = 0.5f,
    float alignY = 0.5f
) noexcept {
    if (dstRect.w <= 0.0f || dstRect.h <= 0.0f) {
        return {dstRect.x, dstRect.y, 0.0f, 0.0f};
    }
    if (srcSize.x <= 0.0f || srcSize.y <= 0.0f || fit == ObjectFit::Fill) {
        return dstRect;
    }

    float scale = 1.0f;
    const float scaleX = dstRect.w / srcSize.x;
    const float scaleY = dstRect.h / srcSize.y;

    switch (fit) {
        case ObjectFit::Contain:
            scale = std::min(scaleX, scaleY);
            break;
        case ObjectFit::Cover:
            scale = std::max(scaleX, scaleY);
            break;
        case ObjectFit::ScaleDown:
            scale = std::min(1.0f, std::min(scaleX, scaleY));
            break;
        case ObjectFit::Original:
            scale = 1.0f;
            break;
        case ObjectFit::Fill:
            return dstRect;
    }

    const float outW = srcSize.x * scale;
    const float outH = srcSize.y * scale;
    const float outX = dstRect.x + (dstRect.w - outW) * alignX;
    const float outY = dstRect.y + (dstRect.h - outH) * alignY;
    return {outX, outY, outW, outH};
}

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
