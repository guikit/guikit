#pragma once

#include <initializer_list>
#include "keel/core.h"
#include "keel/math.h"
#include "keel/md_ref.h"
#include "keel/ref.h"
#include "keel/static_array.h"

namespace guikit {

static_assert(sizeof(keel::f32) == 4 && sizeof(keel::f64) == 8, "GuiKit requires 32-bit float and 64-bit double");

/// Concept matching any integral or enum index type except `bool`.
template <typename T>
concept IndexLike = (keel::IsIntegral<keel::RemoveCvRef<T>> || keel::IsEnum<keel::RemoveCvRef<T>>) &&
                    !keel::IsSame<keel::RemoveCvRef<T>, bool>;

/// Standard system mouse cursor shapes requested by interactive widgets and resize handles.
enum class CursorType : keel::u8 {
    Default = 0,
    Pointer,
    Text,
    ResizeEW,
    ResizeNS,
    ResizeNWSE,
    ResizeNESW,
    Move,
    NotAllowed
};

/// 2D floating-point vector for layout coordinates, sizes, and offsets.
struct Vec2 {
    keel::f32 x{0.0f};
    keel::f32 y{0.0f};

    constexpr Vec2() noexcept = default;
    constexpr Vec2(keel::f32 inX, keel::f32 inY) noexcept : x(inX), y(inY) {}

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

    [[nodiscard]] constexpr Vec2 operator*(keel::f32 scalar) const noexcept {
        return {x * scalar, y * scalar};
    }

    [[nodiscard]] constexpr Vec2 operator/(keel::f32 scalar) const noexcept {
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

    constexpr Vec2& operator*=(keel::f32 scalar) noexcept {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    [[nodiscard]] keel::f32 Length() const noexcept {
        return keel::sqrt(x * x + y * y);
    }
};

/// 2D rectangle defined by top-left position and dimensions.
struct Rect {
    keel::f32 x{0.0f};
    keel::f32 y{0.0f};
    keel::f32 w{0.0f};
    keel::f32 h{0.0f};

    constexpr Rect() noexcept = default;
    constexpr Rect(keel::f32 inX, keel::f32 inY, keel::f32 inW, keel::f32 inH) noexcept
        : x(inX), y(inY), w(inW), h(inH) {}
    constexpr Rect(Vec2 pos, Vec2 size) noexcept
        : x(pos.x), y(pos.y), w(size.x), h(size.y) {}

    [[nodiscard]] constexpr Vec2 Pos() const noexcept { return {x, y}; }
    [[nodiscard]] constexpr Vec2 Size() const noexcept { return {w, h}; }

    [[nodiscard]] constexpr keel::f32 Left() const noexcept { return x; }
    [[nodiscard]] constexpr keel::f32 Top() const noexcept { return y; }
    [[nodiscard]] constexpr keel::f32 Right() const noexcept { return x + w; }
    [[nodiscard]] constexpr keel::f32 Bottom() const noexcept { return y + h; }

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
        const keel::f32 nx = keel::max(x, other.x);
        const keel::f32 ny = keel::max(y, other.y);
        const keel::f32 nr = keel::min(Right(), other.Right());
        const keel::f32 nb = keel::min(Bottom(), other.Bottom());
        if (nr <= nx || nb <= ny) {
            return {nx, ny, 0.0f, 0.0f};
        }
        return {nx, ny, nr - nx, nb - ny};
    }

    [[nodiscard]] constexpr Rect Inset(keel::f32 amount) const noexcept {
        return {x + amount, y + amount, keel::max(0.0f, w - 2.0f * amount), keel::max(0.0f, h - 2.0f * amount)};
    }

    [[nodiscard]] constexpr Rect Inset(keel::f32 dx, keel::f32 dy) const noexcept {
        return {x + dx, y + dy, keel::max(0.0f, w - 2.0f * dx), keel::max(0.0f, h - 2.0f * dy)};
    }

    [[nodiscard]] constexpr keel::f32 Area() const noexcept {
        return (w > 0.0f && h > 0.0f) ? (w * h) : 0.0f;
    }

    [[nodiscard]] constexpr Rect Union(const Rect& other) const noexcept {
        if (w <= 0.0f || h <= 0.0f) return other;
        if (other.w <= 0.0f || other.h <= 0.0f) return *this;
        const keel::f32 nx = keel::min(x, other.x);
        const keel::f32 ny = keel::min(y, other.y);
        const keel::f32 nr = keel::max(Right(), other.Right());
        const keel::f32 nb = keel::max(Bottom(), other.Bottom());
        return {nx, ny, nr - nx, nb - ny};
    }

    [[nodiscard]] constexpr bool operator==(const Rect& other) const noexcept {
        return x == other.x && y == other.y && w == other.w && h == other.h;
    }

    [[nodiscard]] constexpr bool operator!=(const Rect& other) const noexcept {
        return !(*this == other);
    }
};

/// Uniform scale followed by a translation: `p -> p * scale + offset`. Maps a local coordinate space into its
/// parent's (or, composed along a branch, into the frame's). There is no rotation or shear by design: every
/// layout transform in the engine scales about a point.
struct Transform2D {
    keel::f32 scale{1.0f};
    Vec2 offset{0.0f, 0.0f};

    /// Scales by `s` about `origin`, which stays fixed: `origin + (p - origin) * s`.
    [[nodiscard]] static constexpr Transform2D ScaleAbout(Vec2 origin, keel::f32 s) noexcept {
        return Transform2D{s, Vec2{origin.x * (1.0f - s), origin.y * (1.0f - s)}};
    }

    [[nodiscard]] constexpr bool IsIdentity() const noexcept {
        return scale == 1.0f && offset.x == 0.0f && offset.y == 0.0f;
    }

    [[nodiscard]] constexpr Vec2 Apply(Vec2 p) const noexcept {
        return Vec2{p.x * scale + offset.x, p.y * scale + offset.y};
    }
    [[nodiscard]] constexpr Rect Apply(const Rect& r) const noexcept {
        return Rect(r.x * scale + offset.x, r.y * scale + offset.y, r.w * scale, r.h * scale);
    }
    [[nodiscard]] constexpr Vec2 Invert(Vec2 p) const noexcept {
        return Vec2{(p.x - offset.x) / scale, (p.y - offset.y) / scale};
    }
    [[nodiscard]] constexpr Rect Invert(const Rect& r) const noexcept {
        return Rect((r.x - offset.x) / scale, (r.y - offset.y) / scale, r.w / scale, r.h / scale);
    }

    /// The transform that applies `inner` first and then this one (inner space -> this transform's target).
    [[nodiscard]] constexpr Transform2D Then(const Transform2D& inner) const noexcept {
        return Transform2D{scale * inner.scale, Vec2{inner.offset.x * scale + offset.x, inner.offset.y * scale + offset.y}};
    }

    /// The transform that maps this transform's target space back into its source space.
    [[nodiscard]] constexpr Transform2D Inverse() const noexcept {
        return Transform2D{1.0f / scale, Vec2{-offset.x / scale, -offset.y / scale}};
    }
};

/// 32-bit RGBA color with linear blending utilities.
struct Color {
    keel::u8 r{0};
    keel::u8 g{0};
    keel::u8 b{0};
    keel::u8 a{255};

    constexpr Color() noexcept = default;
    constexpr Color(keel::u8 inR, keel::u8 inG, keel::u8 inB, keel::u8 inA = 255) noexcept
        : r(inR), g(inG), b(inB), a(inA) {}

    /// Pack as little-endian 32-bit integer (ABGR in memory / 0xAABBGGRR).
    [[nodiscard]] constexpr keel::u32 ToUint32() const noexcept {
        return (keel::as<keel::u32>(a) << 24) |
               (keel::as<keel::u32>(b) << 16) |
               (keel::as<keel::u32>(g) << 8) |
               keel::as<keel::u32>(r);
    }

    [[nodiscard]] constexpr bool operator==(const Color& other) const noexcept {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }

    [[nodiscard]] constexpr bool operator!=(const Color& other) const noexcept {
        return !(*this == other);
    }

    /// Construct from hexadecimal value (0xRRGGBBAA or 0xRRGGBB).
    [[nodiscard]] static constexpr Color Hex(keel::u32 hex, bool hasAlpha = true) noexcept {
        if (hasAlpha) {
            return {
                keel::as<keel::u8>((hex >> 24) & 0xFF),
                keel::as<keel::u8>((hex >> 16) & 0xFF),
                keel::as<keel::u8>((hex >> 8) & 0xFF),
                keel::as<keel::u8>(hex & 0xFF)
            };
        }
        return {
            keel::as<keel::u8>((hex >> 16) & 0xFF),
            keel::as<keel::u8>((hex >> 8) & 0xFF),
            keel::as<keel::u8>(hex & 0xFF),
            255
        };
    }

    [[nodiscard]] static constexpr Color Rgba(keel::u8 r, keel::u8 g, keel::u8 b, keel::u8 a = 255) noexcept {
        return {r, g, b, a};
    }

    [[nodiscard]] static constexpr Color Transparent() noexcept {
        return {0, 0, 0, 0};
    }

    [[nodiscard]] constexpr Color WithAlpha(keel::u8 newAlpha) const noexcept {
        return {r, g, b, newAlpha};
    }

    [[nodiscard]] constexpr Color WithAlphaF(keel::f32 factor) const noexcept {
        const keel::f32 clamped = keel::clamp(factor, 0.0f, 1.0f);
        return {r, g, b, keel::as<keel::u8>(keel::as<keel::f32>(a) * clamped)};
    }

    [[nodiscard]] constexpr Color ScaleAlpha(keel::u8 scale) const noexcept {
        if (scale == 255) return *this;
        return {
            r,
            g,
            b,
            keel::as<keel::u8>((keel::as<keel::u32>(a) * keel::as<keel::u32>(scale) + 127u) / 255u)
        };
    }

    [[nodiscard]] static constexpr Color Mix255(Color base, Color accent, keel::u8 weight) noexcept {
        const keel::u32 w = weight;
        const keel::u32 inv = 255u - w;
        return {
            keel::as<keel::u8>((keel::as<keel::u32>(base.r) * inv + keel::as<keel::u32>(accent.r) * w + 127u) / 255u),
            keel::as<keel::u8>((keel::as<keel::u32>(base.g) * inv + keel::as<keel::u32>(accent.g) * w + 127u) / 255u),
            keel::as<keel::u8>((keel::as<keel::u32>(base.b) * inv + keel::as<keel::u32>(accent.b) * w + 127u) / 255u),
            keel::as<keel::u8>((keel::as<keel::u32>(base.a) * inv + keel::as<keel::u32>(accent.a) * w + 127u) / 255u)
        };
    }

    [[nodiscard]] static constexpr Color Lerp(Color a, Color b, keel::f32 t) noexcept {
        const keel::f32 f = keel::clamp(t, 0.0f, 1.0f);
        auto lerpCh = [f](keel::u8 u0, keel::u8 u1) constexpr noexcept -> keel::u8 {
            const keel::f32 v0 = keel::as<keel::f32>(u0);
            const keel::f32 v1 = keel::as<keel::f32>(u1);
            return keel::as<keel::u8>(v0 + f * (v1 - v0));
        };
        return {
            lerpCh(a.r, b.r),
            lerpCh(a.g, b.g),
            lerpCh(a.b, b.b),
            lerpCh(a.a, b.a)
        };
    }
};

/// Logical key codes for keyboard navigation and text editing.
enum class Key : keel::u16 {
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

/// Last active interaction modality driving `:focus-visible` indicator behavior.
enum class InputModality : keel::u8 {
    Pointer = 0,  ///< Mouse / touch interaction (suppresses focus rings on non-text controls)
    Keyboard      ///< Keyboard navigation (Tab / Arrows / Space / Enter — shows focus rings)
};

/// Cause of a focus transition (following W3C `:focus-visible`, WinUI 3, and React Aria semantics).
enum class FocusReason : keel::u8 {
    Auto = 0,     ///< Inherits the current `InputModality` (`Pointer` vs `Keyboard`)
    Pointer,      ///< Focused via mouse / touch click (logical focus only; no ring on non-text controls)
    Keyboard,     ///< Focused via keyboard navigation (shows focus ring)
    Programmatic  ///< Focused imperatively by application code (inherits current modality)
};

/// Semantic keyboard actions decoupled from physical key codes.
/// Widgets react to KeyAction rather than hardcoded Key + KeyModifiers combinations.
enum class KeyAction : keel::u8 {
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
    static constexpr keel::usize kActionCount = keel::as<keel::usize>(KeyAction::Count);

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
        const keel::usize idx = keel::as<keel::usize>(action);
        if (idx > 0 && idx < kActionCount) {
            m_bindings[idx] = KeyBinding{primary, secondary};
        }
    }

    /// Removes all key bindings for the specified action.
    constexpr void Unbind(KeyAction action) noexcept {
        const keel::usize idx = keel::as<keel::usize>(action);
        if (idx > 0 && idx < kActionCount) {
            m_bindings[idx] = KeyBinding{};
        }
    }

    /// Returns the current binding for the specified action.
    [[nodiscard]] constexpr KeyBinding GetBinding(KeyAction action) const noexcept {
        const keel::usize idx = keel::as<keel::usize>(action);
        return (idx > 0 && idx < kActionCount) ? m_bindings[idx] : KeyBinding{};
    }

    /// Returns true if the given (key, mods) event matches the specified semantic action.
    [[nodiscard]] constexpr bool Matches(KeyAction action, Key key, KeyModifiers mods) const noexcept {
        const keel::usize idx = keel::as<keel::usize>(action);
        return (idx > 0 && idx < kActionCount) && m_bindings[idx].Matches(key, mods);
    }

    /// Resolves a physical (key, mods) event into its bound KeyAction, or KeyAction::None if unbound.
    [[nodiscard]] constexpr KeyAction Resolve(Key key, KeyModifiers mods) const noexcept {
        if (key == Key::None) return KeyAction::None;
        for (keel::usize i = 1; i < kActionCount; ++i) {
            if (m_bindings[i].Matches(key, mods)) {
                return static_cast<KeyAction>(i);
            }
        }
        return KeyAction::None;
    }

private:
    keel::StaticArray<KeyBinding, kActionCount> m_bindings{};
};

/// Visual blinking mode for input caret / cursor.
enum class CursorBlinkMode : keel::u8 {
    Blinking,  ///< Standard alternating visibility (~1 Hz)
    Solid,     ///< Always visible while focused (reduced motion / industrial UI)
    Hidden     ///< Never rendered
};

/// Timing and geometry configuration for text input cursor.
struct CursorConfig {
    CursorBlinkMode mode{CursorBlinkMode::Blinking};
    keel::f32 blinkPeriod{1.06f};   ///< Full cycle duration in seconds (530ms on, 530ms off)
    keel::f32 width{1.5f};          ///< Caret line width in pixels
};

/// Universal 1D/2D orientation for layout and components (ScrollArea, Splitter, Slider, Separator).
enum class Orientation : keel::u8 {
    Vertical,
    Horizontal,
    Both
};

/// Universal conditional visibility policy (Scrollbars, ClearButtons, Headers, Legends).
enum class VisibilityPolicy : keel::u8 {
    Auto,    ///< Context-dependent / shown when needed (e.g. on overflow, content presence, hover)
    Always,  ///< Unconditionally shown
    Never    ///< Suppressed / never rendered
};

/// Density-independent point unit (dp) wrapper.
struct Dp {
    keel::f32 value{0.0f};

    constexpr explicit Dp(keel::f32 v) noexcept : value(v) {}

    [[nodiscard]] constexpr keel::f32 ToPixels(keel::f32 scaleFactor) const noexcept {
        return keel::round(value * scaleFactor);
    }
};

#if defined(__clang__) || defined(_PREFAST_)
#define GUIKIT_LIFETIME_POINTER [[gsl::Pointer]]
#else
#define GUIKIT_LIFETIME_POINTER
#endif

/// Non-allocating 16-byte view wrapper for candidate text strings used in dynamic layout sizing.
class GUIKIT_LIFETIME_POINTER TextVariants {
public:
    constexpr TextVariants() noexcept = default;

    constexpr TextVariants(std::initializer_list<keel::StringRef> list KEEL_LIFETIMEBOUND) noexcept
        : m_span(list) {}

    constexpr TextVariants(keel::ArrayRef<keel::StringRef> s) noexcept
        : m_span(s) {}

    template <typename R>
        requires(!keel::IsSame<keel::Decay<R>, TextVariants> &&
                 !keel::IsSame<keel::Decay<R>, keel::ArrayRef<keel::StringRef>> &&
                 requires(const R& r) { keel::ArrayRef<keel::StringRef>(r); })
    constexpr TextVariants(const R& range) noexcept
        : m_span(range) {}

    [[nodiscard]] constexpr auto begin() const noexcept { return m_span.begin(); }
    [[nodiscard]] constexpr auto end() const noexcept { return m_span.end(); }
    [[nodiscard]] constexpr bool empty() const noexcept { return m_span.empty(); }
    [[nodiscard]] constexpr keel::usize size() const noexcept { return m_span.size(); }
    [[nodiscard]] constexpr keel::ArrayRef<keel::StringRef> span() const noexcept { return m_span; }

    [[nodiscard]] constexpr bool operator==(const TextVariants& other) const noexcept {
        return m_span == other.m_span;
    }

private:
    keel::ArrayRef<keel::StringRef> m_span{};
};
static_assert(sizeof(TextVariants) == 2 * sizeof(void*), "TextVariants must remain 2 * sizeof(void*) bytes");

/// Concept matching any dynamic `size_t` list (`keel::Array<size_t>`, `keel::SmallArray<size_t, N>`,
/// `keel::FixedArray<size_t, N>`, or external `std::vector<size_t>`) without including STL headers.
template <typename C>
concept WritableIndexList =
    !keel::IsConst<C> &&
    requires(C& c, const C& cc, keel::usize v) {
        { c.data() } -> keel::SameAs<keel::usize*>;
        { cc.data() } -> keel::SameAs<const keel::usize*>;
        { cc.size() } -> keel::SameAs<keel::usize>;
        c.push_back(v);
        c.pop_back();
        c.clear();
    };

/// Non-owning type-erased mutable adapter over a dynamic `size_t` list (`MultiSelect`, `TextAreaOptions::breakpoints`).
class IndexListSink {
public:
    struct Ops {
        keel::MutArrayRef<keel::usize> (*view_fn)(void* obj) noexcept;
        void (*push_fn)(void* obj, keel::usize val);
        void (*erase_at_fn)(void* obj, keel::usize idx);
        void (*clear_fn)(void* obj);
    };

    constexpr IndexListSink() noexcept = default;
    constexpr IndexListSink(decltype(nullptr)) noexcept {}

    template <WritableIndexList C>
    constexpr IndexListSink(C* c) noexcept
        : m_obj(c), m_ops(c ? &ops_for<C>() : nullptr) {}

    template <WritableIndexList C>
        requires(!keel::IsSame<keel::Decay<C>, IndexListSink>)
    explicit constexpr IndexListSink(C& c) noexcept
        : m_obj(&c), m_ops(&ops_for<C>()) {}

    [[nodiscard]] constexpr bool has_target() const noexcept { return m_obj != nullptr && m_ops != nullptr; }
    [[nodiscard]] explicit constexpr operator bool() const noexcept { return has_target(); }
    [[nodiscard]] constexpr bool operator==(decltype(nullptr)) const noexcept { return !has_target(); }
    [[nodiscard]] constexpr bool operator!=(decltype(nullptr)) const noexcept { return has_target(); }

    [[nodiscard]] keel::ArrayRef<keel::usize> as_ref() const noexcept {
        if (!has_target()) return {};
        const keel::MutArrayRef<keel::usize> m = m_ops->view_fn(m_obj);
        return keel::ArrayRef<keel::usize>(m.data(), m.size());
    }

    [[nodiscard]] keel::MutArrayRef<keel::usize> as_mut_ref() const noexcept {
        return has_target() ? m_ops->view_fn(m_obj) : keel::MutArrayRef<keel::usize>{};
    }

    [[nodiscard]] keel::usize size() const noexcept { return as_ref().size(); }
    [[nodiscard]] bool empty() const noexcept { return as_ref().empty(); }
    [[nodiscard]] const keel::usize* begin() const noexcept { return as_ref().begin(); }
    [[nodiscard]] const keel::usize* end() const noexcept { return as_ref().end(); }
    [[nodiscard]] keel::usize& operator[](keel::usize i) const noexcept { return as_mut_ref()[i]; }

    void push_back(keel::usize val) const {
        if (has_target()) m_ops->push_fn(m_obj, val);
    }

    void erase_at(keel::usize idx) const {
        if (has_target()) m_ops->erase_at_fn(m_obj, idx);
    }

    void clear() const {
        if (has_target()) m_ops->clear_fn(m_obj);
    }

private:
    template <WritableIndexList C>
    static const Ops& ops_for() noexcept {
        static constexpr Ops kOps{
            +[](void* obj) noexcept -> keel::MutArrayRef<keel::usize> {
                auto& c = *static_cast<C*>(obj);
                return keel::MutArrayRef<keel::usize>(c.data(), c.size());
            },
            +[](void* obj, keel::usize val) {
                auto& c = *static_cast<C*>(obj);
                if constexpr (requires(const C& cc) { cc.full(); }) {
                    if (c.full()) return;
                }
                c.push_back(val);
            },
            +[](void* obj, keel::usize idx) {
                auto& c = *static_cast<C*>(obj);
                const keel::usize n = c.size();
                if (idx >= n) return;
                keel::usize* p = c.data();
                for (keel::usize i = idx + 1; i < n; ++i) {
                    p[i - 1] = p[i];
                }
                c.pop_back();
            },
            +[](void* obj) { static_cast<C*>(obj)->clear(); },
        };
        return kOps;
    }

    void* m_obj{nullptr};
    const Ops* m_ops{nullptr};
};

namespace literals {
    constexpr Dp operator""_dp(long double v) noexcept {
        return Dp(keel::as<keel::f32>(v));
    }
    constexpr Dp operator""_dp(unsigned long long v) noexcept {
        return Dp(keel::as<keel::f32>(v));
    }
} // namespace literals

namespace detail {
using DpValue = Dp;
} // namespace detail

/// Universal control size token (`None`, `Xs`, `Sm`, `Md`, `Lg`, `Xl`), explicit target height in `dp`,
/// or factor relative to the theme's `Md` baseline (`UiSize::Factor(1.25f)`).
struct UiSize {
    enum class Preset : keel::u8 {
        None = 0, ///< Zero / unpadded / no gap / square corners
        Xs,       ///< Extra Small (compact toolbars, inline badges, dense data tables)
        Sm,       ///< Small (secondary panels, compact forms)
        Md,       ///< Medium (standard default controls across the engine)
        Lg,       ///< Large (prominent action bars, touch-friendly interfaces)
        Xl,       ///< Extra Large (hero actions, emergency stops, kiosk displays)
        Dp,       ///< Custom target baseline height in logical dp
        Factor    ///< Custom multiplier relative to the theme's `Md` baseline
    };

private:
    static constexpr keel::i32 kPresetCount = 6;
    static constexpr keel::f32 kFixedScale = 1024.0f;
    static constexpr keel::f32 kMaxCustomVal = 1000000.0f;

    keel::i32 m_encoded{keel::as<keel::i32>(Preset::Md)};

    constexpr explicit UiSize(keel::i32 encoded) noexcept : m_encoded(encoded) {}

    [[nodiscard]] static constexpr keel::i32 EncodeDp(keel::f32 dp) noexcept {
        if (!(dp > 0.0f)) return keel::as<keel::i32>(Preset::None);
        const keel::f32 clamped = (dp > kMaxCustomVal) ? kMaxCustomVal : dp;
        return kPresetCount + keel::as<keel::i32>(clamped * kFixedScale + 0.5f);
    }

    [[nodiscard]] static constexpr keel::i32 EncodeFactor(keel::f32 factor) noexcept {
        if (!(factor > 0.0f)) return keel::as<keel::i32>(Preset::Md);
        const keel::f32 clamped = (factor > kMaxCustomVal) ? kMaxCustomVal : factor;
        const keel::i32 q = keel::as<keel::i32>(clamped * kFixedScale + 0.5f);
        return -keel::max<keel::i32>(1, q);
    }

public:
    constexpr UiSize() noexcept = default;

    constexpr UiSize(keel::f32 dp) noexcept : m_encoded(EncodeDp(dp)) {}
    constexpr UiSize(detail::DpValue dp) noexcept : m_encoded(EncodeDp(dp.value)) {}

    [[nodiscard]] static constexpr UiSize Dp(keel::f32 dp) noexcept { return UiSize(EncodeDp(dp)); }
    [[nodiscard]] static constexpr UiSize Factor(keel::f32 factor) noexcept { return UiSize(EncodeFactor(factor)); }

    static const UiSize None;
    static const UiSize Xs;
    static const UiSize Sm;
    static const UiSize Md;
    static const UiSize Lg;
    static const UiSize Xl;

    [[nodiscard]] constexpr Preset GetPreset() const noexcept {
        if (m_encoded >= 0 && m_encoded < kPresetCount) {
            return static_cast<Preset>(m_encoded);
        }
        return (m_encoded < 0) ? Preset::Factor : Preset::Dp;
    }

    [[nodiscard]] constexpr bool IsNone() const noexcept { return m_encoded == keel::as<keel::i32>(Preset::None); }
    [[nodiscard]] constexpr bool IsPreset() const noexcept { return m_encoded >= 0 && m_encoded < kPresetCount; }
    [[nodiscard]] constexpr bool IsDp() const noexcept { return m_encoded >= kPresetCount; }
    [[nodiscard]] constexpr bool IsFactor() const noexcept { return m_encoded < 0; }
    [[nodiscard]] constexpr bool IsCustom() const noexcept { return !IsPreset(); }

    [[nodiscard]] constexpr keel::f32 CustomValue() const noexcept {
        if (m_encoded >= kPresetCount) {
            return keel::as<keel::f32>(m_encoded - kPresetCount) / kFixedScale;
        }
        if (m_encoded < 0) {
            return keel::as<keel::f32>(-m_encoded) / kFixedScale;
        }
        return 0.0f;
    }

    [[nodiscard]] constexpr keel::usize TableIndex() const noexcept {
        return IsPreset() ? keel::as<keel::usize>(m_encoded) : keel::as<keel::usize>(Preset::Md);
    }

    [[nodiscard]] constexpr keel::f32 ResolveFactor(keel::f32 mdHeightDp) const noexcept {
        if (m_encoded >= kPresetCount) {
            const keel::f32 targetDp = keel::as<keel::f32>(m_encoded - kPresetCount) / kFixedScale;
            return (mdHeightDp > 0.0f) ? (targetDp / mdHeightDp) : 1.0f;
        }
        if (m_encoded < 0) {
            return keel::as<keel::f32>(-m_encoded) / kFixedScale;
        }
        return 1.0f;
    }

    [[nodiscard]] constexpr keel::i32 Encoded() const noexcept { return m_encoded; }
    [[nodiscard]] static constexpr UiSize FromEncoded(keel::i32 raw) noexcept { return UiSize(raw); }

    [[nodiscard]] constexpr bool operator==(const UiSize& other) const noexcept = default;
};

inline constexpr UiSize UiSize::None{keel::as<keel::i32>(UiSize::Preset::None)};
inline constexpr UiSize UiSize::Xs{keel::as<keel::i32>(UiSize::Preset::Xs)};
inline constexpr UiSize UiSize::Sm{keel::as<keel::i32>(UiSize::Preset::Sm)};
inline constexpr UiSize UiSize::Md{keel::as<keel::i32>(UiSize::Preset::Md)};
inline constexpr UiSize UiSize::Lg{keel::as<keel::i32>(UiSize::Preset::Lg)};
inline constexpr UiSize UiSize::Xl{keel::as<keel::i32>(UiSize::Preset::Xl)};
static_assert(sizeof(UiSize) == 4, "UiSize must remain 4 bytes");

/// Direction of primary layout axis.
enum class FlexDirection : keel::u8 {
    Row,    ///< Children laid out horizontally from left to right
    Column, ///< Children laid out vertically from top to bottom
    Wrap,   ///< Children laid out horizontally and wrapped to subsequent lines upon overflow
    Grid    ///< 12-column responsive grid with automatic line wrapping
};

/// Sizing policy along an axis.
enum class SizeMode : keel::u8 {
    Stretch,    ///< Expands to fill available space allocated by parent container
    Fit,        ///< Shrinks to hug content / intrinsic child sizes
    Fixed       ///< Explicit dimension in points
};

/// Unified layout sizing policy along a single axis (`width` or `height`).
///
/// Combines symbolic sizing modes (`Auto`, `Fit`, `Stretch`) and explicit dimensions in logical `dp` (`Fixed`)
/// into a single 8-byte value type so mutually exclusive policies cannot conflict:
/// - `AxisSize::Auto()` / `{}`: contextual default resolved by the widget or container
/// - `SizeMode::Fit` / `AxisSize::Fit()`: shrink-wraps intrinsic content
/// - `SizeMode::Stretch` / `AxisSize::Stretch(weight = 1.0f)`: expands to fill available parent space (with optional proportional flex weight)
/// - `180.0f` / `AxisSize::Fixed(180.0f)`: exact dimension in logical `dp` (`> 0.0f`)
struct AxisSize {
    enum class Kind : keel::u8 {
        Auto = 0,
        Stretch,
        Fit,
        Fixed
    };

private:
    keel::f32 m_value{0.0f}; ///< Fixed dimension in dp (when `Fixed`) or proportional flex weight (when `Stretch`)
    Kind m_kind{Kind::Auto};

    constexpr AxisSize(Kind kind, keel::f32 value) noexcept : m_value(value), m_kind(kind) {}

public:
    constexpr AxisSize() noexcept = default;

    constexpr AxisSize(SizeMode mode) noexcept
        : m_value(mode == SizeMode::Stretch ? 1.0f : 0.0f),
          m_kind(mode == SizeMode::Stretch ? Kind::Stretch : Kind::Fit) {}

    template <typename T>
        requires ((keel::IsFloatingPoint<keel::RemoveCvRef<T>> || keel::IsIntegral<keel::RemoveCvRef<T>>) &&
                  !keel::IsSame<keel::RemoveCvRef<T>, bool> &&
                  !keel::IsSame<keel::RemoveCvRef<T>, char>)
    constexpr AxisSize(T dp) noexcept
        : m_value(dp > static_cast<T>(0) ? keel::as<keel::f32>(dp) : 0.0f),
          m_kind(dp > static_cast<T>(0) ? Kind::Fixed : Kind::Auto) {}

    [[nodiscard]] static constexpr AxisSize Auto() noexcept { return {}; }
    [[nodiscard]] static constexpr AxisSize Fit() noexcept { return AxisSize(Kind::Fit, 0.0f); }
    [[nodiscard]] static constexpr AxisSize Stretch(keel::f32 weight = 1.0f) noexcept {
        return AxisSize(Kind::Stretch, (weight > 0.0f) ? weight : 1.0f);
    }
    [[nodiscard]] static constexpr AxisSize Fixed(keel::f32 dp) noexcept {
        return (dp > 0.0f) ? AxisSize(Kind::Fixed, dp) : AxisSize{};
    }

    [[nodiscard]] constexpr Kind GetKind() const noexcept { return m_kind; }
    [[nodiscard]] constexpr bool IsAuto() const noexcept { return m_kind == Kind::Auto; }
    [[nodiscard]] constexpr bool IsFit() const noexcept { return m_kind == Kind::Fit; }
    [[nodiscard]] constexpr bool IsStretch() const noexcept { return m_kind == Kind::Stretch; }
    [[nodiscard]] constexpr bool IsFixed() const noexcept { return m_kind == Kind::Fixed; }
    [[nodiscard]] constexpr keel::f32 Dp() const noexcept { return (m_kind == Kind::Fixed) ? m_value : 0.0f; }
    [[nodiscard]] constexpr keel::f32 StretchWeight() const noexcept {
        return (m_kind == Kind::Stretch && m_value > 0.0f) ? m_value : 1.0f;
    }
    [[nodiscard]] constexpr bool HasCustomStretchWeight() const noexcept {
        return m_kind == Kind::Stretch && m_value > 0.0f && m_value != 1.0f;
    }

    [[nodiscard]] constexpr SizeMode ResolveMode(SizeMode autoDefault) const noexcept {
        switch (m_kind) {
            case Kind::Auto:    return autoDefault;
            case Kind::Stretch: return SizeMode::Stretch;
            case Kind::Fit:     return SizeMode::Fit;
            case Kind::Fixed:   return SizeMode::Fixed;
        }
        return autoDefault;
    }

    [[nodiscard]] constexpr bool operator==(const AxisSize& other) const noexcept {
        return m_kind == other.m_kind && m_value == other.m_value;
    }
    [[nodiscard]] constexpr bool operator==(SizeMode mode) const noexcept {
        if (mode == SizeMode::Stretch) return m_kind == Kind::Stretch;
        if (mode == SizeMode::Fit) return m_kind == Kind::Fit;
        return m_kind == Kind::Fixed;
    }
};
static_assert(sizeof(AxisSize) == 8, "AxisSize must remain compact (8 bytes)");

/// Alignment along the cross axis (perpendicular to FlexDirection).
enum class Alignment : keel::u8 {
    Start,      ///< Align to start of cross axis (Top in Row, Left in Column)
    Center,     ///< Center along cross axis
    End,        ///< Align to end of cross axis (Bottom in Row, Right in Column)
    Stretch     ///< Stretch to fill full cross-axis dimension
};

/// Type alias for backward compatibility.
using Align = Alignment;

/// Distribution along the main axis (parallel to FlexDirection) when space is not fully stretched.
enum class Justify : keel::u8 {
    Start,          ///< Pack children at start of main axis
    Center,         ///< Center children along main axis
    End,            ///< Pack children at end of main axis
    SpaceBetween    ///< Distribute items evenly; first item at start, last at end
};

/// Content fitting policy within a target bounding box (Image, Scene, Video).
enum class ObjectFit : keel::u8 {
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
    keel::f32 alignX = 0.5f,
    keel::f32 alignY = 0.5f
) noexcept {
    if (dstRect.w <= 0.0f || dstRect.h <= 0.0f) {
        return {dstRect.x, dstRect.y, 0.0f, 0.0f};
    }
    if (srcSize.x <= 0.0f || srcSize.y <= 0.0f || fit == ObjectFit::Fill) {
        return dstRect;
    }

    keel::f32 scale = 1.0f;
    const keel::f32 scaleX = dstRect.w / srcSize.x;
    const keel::f32 scaleY = dstRect.h / srcSize.y;

    switch (fit) {
        case ObjectFit::Contain:
            scale = keel::min(scaleX, scaleY);
            break;
        case ObjectFit::Cover:
            scale = keel::max(scaleX, scaleY);
            break;
        case ObjectFit::ScaleDown:
            scale = keel::min(1.0f, keel::min(scaleX, scaleY));
            break;
        case ObjectFit::Original:
            scale = 1.0f;
            break;
        case ObjectFit::Fill:
            return dstRect;
    }

    const keel::f32 outW = srcSize.x * scale;
    const keel::f32 outH = srcSize.y * scale;
    const keel::f32 outX = dstRect.x + (dstRect.w - outW) * alignX;
    const keel::f32 outY = dstRect.y + (dstRect.h - outH) * alignY;
    return {outX, outY, outW, outH};
}

/// Inset margins or internal padding on 4 sides, supporting both explicit pixel insets and semantic design tokens.
struct Padding {
    static constexpr keel::f32 kTokenSentinel = -3.402823466e+38f;

    keel::f32 left{0.0f};
    keel::f32 top{0.0f};
    keel::f32 right{0.0f};
    keel::f32 bottom{0.0f};

    constexpr Padding() noexcept = default;

    /// Uniform explicit pixel padding on all 4 sides.
    constexpr Padding(keel::f32 all) noexcept
        : left(all), top(all), right(all), bottom(all) {}

    /// Explicit pixel padding on 4 individual sides.
    constexpr Padding(keel::f32 l, keel::f32 t, keel::f32 r, keel::f32 b) noexcept
        : left(l), top(t), right(r), bottom(b) {}

    /// Semantic design token padding dynamically resolved via UiTheme and UI density scale.
    constexpr Padding(UiSize size) noexcept
        : left(kTokenSentinel)
        , top(keel::as<keel::f32>(keel::as<keel::u8>(size.GetPreset())))
        , right(size.IsCustom() ? size.CustomValue() : kTokenSentinel)
        , bottom(kTokenSentinel) {}

    [[nodiscard]] static constexpr Padding All(keel::f32 val) noexcept {
        return Padding(val);
    }

    [[nodiscard]] static constexpr Padding Symmetric(keel::f32 horizontal, keel::f32 vertical) noexcept {
        return Padding(horizontal, vertical, horizontal, vertical);
    }

    [[nodiscard]] constexpr Padding ScaledBy(keel::f32 factor) const noexcept {
        return IsToken() ? *this : Padding(left * factor, top * factor, right * factor, bottom * factor);
    }

    [[nodiscard]] constexpr bool IsToken() const noexcept {
        return left == kTokenSentinel && bottom == kTokenSentinel;
    }

    [[nodiscard]] constexpr UiSize GetToken() const noexcept {
        if (!IsToken()) return UiSize::None;
        const auto preset = static_cast<UiSize::Preset>(keel::as<keel::u8>(top));
        switch (preset) {
            case UiSize::Preset::None:   return UiSize::None;
            case UiSize::Preset::Xs:     return UiSize::Xs;
            case UiSize::Preset::Sm:     return UiSize::Sm;
            case UiSize::Preset::Md:     return UiSize::Md;
            case UiSize::Preset::Lg:     return UiSize::Lg;
            case UiSize::Preset::Xl:     return UiSize::Xl;
            case UiSize::Preset::Dp:     return UiSize::Dp(right);
            case UiSize::Preset::Factor: return UiSize::Factor(right);
        }
        return UiSize::None;
    }

    [[nodiscard]] constexpr keel::f32 Horizontal() const noexcept {
        return IsToken() ? 0.0f : (left + right);
    }

    [[nodiscard]] constexpr keel::f32 Vertical() const noexcept {
        return IsToken() ? 0.0f : (top + bottom);
    }

    [[nodiscard]] constexpr bool operator==(const Padding& other) const noexcept {
        if (IsToken() || other.IsToken()) {
            return IsToken() == other.IsToken() && GetToken() == other.GetToken();
        }
        return left == other.left && top == other.top && right == other.right && bottom == other.bottom;
    }
};

static_assert(sizeof(Padding) == 16, "Padding must remain exactly 16 bytes for cache line layout");

struct StridedPixelRef;
struct MutStridedPixelRef;

/// Non-owning contiguous 32-bit ABGR8888 2D raster view (`width x height`, row-major top-to-bottom).
struct PixelRef {
    keel::MdRef<keel::u32, 2> md{};

    constexpr PixelRef() noexcept = default;
    constexpr PixelRef(decltype(nullptr)) noexcept {}
    constexpr PixelRef(keel::ArrayRef<keel::u32> pixels, keel::u32 width, keel::u32 height) noexcept
        : md(pixels, height, width) {}
    explicit constexpr PixelRef(keel::MdRef<keel::u32, 2> matrix) noexcept : md(matrix) {}

    [[nodiscard]] constexpr bool Empty() const noexcept { return md.empty(); }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return !md.empty(); }
    [[nodiscard]] constexpr keel::u32 Width() const noexcept { return keel::as<keel::u32>(md.extent(1)); }
    [[nodiscard]] constexpr keel::u32 Height() const noexcept { return keel::as<keel::u32>(md.extent(0)); }
    [[nodiscard]] constexpr keel::u32 RowStride() const noexcept { return Width(); }
    [[nodiscard]] constexpr keel::usize Size() const noexcept { return md.size(); }
    [[nodiscard]] constexpr keel::ArrayRef<keel::u32> Flat() const noexcept { return md.flat(); }
    [[nodiscard]] constexpr const keel::u32* Data() const noexcept { return md.data(); }
    [[nodiscard]] constexpr keel::ArrayRef<keel::u32> Row(keel::u32 y) const noexcept { return md.row(y); }
    [[nodiscard]] constexpr keel::u32 At(keel::u32 x, keel::u32 y) const noexcept { return md(y, x); }
    [[nodiscard]] constexpr keel::StridedMdRef<keel::u32, 2> Strided() const noexcept { return md.strided(); }
    [[nodiscard]] constexpr StridedPixelRef Subrect(
        keel::u32 x,
        keel::u32 y,
        keel::u32 w,
        keel::u32 h
    ) const noexcept;
};

/// Mutable non-owning contiguous 32-bit ABGR8888 2D raster view (`width x height`, row-major top-to-bottom).
struct MutPixelRef {
    keel::MutMdRef<keel::u32, 2> md{};

    constexpr MutPixelRef() noexcept = default;
    constexpr MutPixelRef(decltype(nullptr)) noexcept {}
    constexpr MutPixelRef(keel::MutArrayRef<keel::u32> pixels, keel::u32 width, keel::u32 height) noexcept
        : md(pixels, height, width) {}
    explicit constexpr MutPixelRef(keel::MutMdRef<keel::u32, 2> matrix) noexcept : md(matrix) {}

    [[nodiscard]] constexpr operator PixelRef() const noexcept { return PixelRef(md.as_ref()); }
    [[nodiscard]] constexpr bool Empty() const noexcept { return md.empty(); }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return !md.empty(); }
    [[nodiscard]] constexpr keel::u32 Width() const noexcept { return keel::as<keel::u32>(md.extent(1)); }
    [[nodiscard]] constexpr keel::u32 Height() const noexcept { return keel::as<keel::u32>(md.extent(0)); }
    [[nodiscard]] constexpr keel::u32 RowStride() const noexcept { return Width(); }
    [[nodiscard]] constexpr keel::usize Size() const noexcept { return md.size(); }
    [[nodiscard]] constexpr keel::MutArrayRef<keel::u32> Flat() const noexcept { return md.flat(); }
    [[nodiscard]] constexpr keel::u32* Data() const noexcept { return md.data(); }
    [[nodiscard]] constexpr keel::MutArrayRef<keel::u32> Row(keel::u32 y) const noexcept { return md.row(y); }
    [[nodiscard]] constexpr keel::u32& At(keel::u32 x, keel::u32 y) const noexcept { return md(y, x); }
    [[nodiscard]] constexpr keel::MutStridedMdRef<keel::u32, 2> Strided() const noexcept { return md.strided(); }
    [[nodiscard]] constexpr MutStridedPixelRef Subrect(
        keel::u32 x,
        keel::u32 y,
        keel::u32 w,
        keel::u32 h
    ) const noexcept;
    constexpr void Fill(keel::u32 color) const noexcept { md.fill(color); }
};

/// Non-owning 32-bit ABGR8888 2D raster view with explicit row pitch (`rowStride >= width`, contiguous rows).
struct StridedPixelRef {
    keel::StridedMdRef<keel::u32, 2> md{};

    constexpr StridedPixelRef() noexcept = default;
    constexpr StridedPixelRef(decltype(nullptr)) noexcept {}
    constexpr StridedPixelRef(PixelRef contiguous) noexcept : md(contiguous.md) {}
    constexpr StridedPixelRef(
        keel::ArrayRef<keel::u32> pixels,
        keel::u32 width,
        keel::u32 height,
        keel::u32 rowStride
    ) noexcept {
        if (rowStride >= width) {
            md = keel::StridedMdRef<keel::u32, 2>(pixels, {height, width}, {rowStride, 1});
        }
    }
    explicit constexpr StridedPixelRef(keel::StridedMdRef<keel::u32, 2> matrix) noexcept {
        if (!matrix.empty() && matrix.stride(1) == 1 && matrix.stride(0) >= matrix.extent(1)) {
            md = matrix;
        }
    }

    [[nodiscard]] constexpr bool Empty() const noexcept { return md.empty(); }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return !md.empty(); }
    [[nodiscard]] constexpr keel::u32 Width() const noexcept { return keel::as<keel::u32>(md.extent(1)); }
    [[nodiscard]] constexpr keel::u32 Height() const noexcept { return keel::as<keel::u32>(md.extent(0)); }
    [[nodiscard]] constexpr keel::u32 RowStride() const noexcept { return keel::as<keel::u32>(md.stride(0)); }
    [[nodiscard]] constexpr keel::usize Size() const noexcept { return md.size(); }
    [[nodiscard]] constexpr const keel::u32* Data() const noexcept { return md.data(); }
    [[nodiscard]] constexpr bool IsContiguous() const noexcept { return md.is_contiguous(); }
    [[nodiscard]] constexpr PixelRef AsContiguous() const noexcept { return PixelRef(md.as_contiguous()); }
    [[nodiscard]] constexpr keel::ArrayRef<keel::u32> Row(keel::u32 y) const noexcept {
        KEEL_BOUNDS_ASSERT(!Empty() && y < Height());
        return keel::ArrayRef<keel::u32>(md.data() + keel::as<keel::usize>(y) * md.stride(0), Width());
    }
    [[nodiscard]] constexpr keel::u32 At(keel::u32 x, keel::u32 y) const noexcept { return md(y, x); }
    [[nodiscard]] constexpr keel::StridedMdRef<keel::u32, 2> Strided() const noexcept { return md; }
    [[nodiscard]] constexpr StridedPixelRef Subrect(
        keel::u32 x,
        keel::u32 y,
        keel::u32 w,
        keel::u32 h
    ) const noexcept {
        return StridedPixelRef(md.slice_axis(0, y, h).slice_axis(1, x, w));
    }
};

/// Mutable non-owning 32-bit ABGR8888 2D raster view with explicit row pitch (`rowStride >= width`, contiguous rows).
struct MutStridedPixelRef {
    keel::MutStridedMdRef<keel::u32, 2> md{};

    constexpr MutStridedPixelRef() noexcept = default;
    constexpr MutStridedPixelRef(decltype(nullptr)) noexcept {}
    constexpr MutStridedPixelRef(MutPixelRef contiguous) noexcept : md(contiguous.md) {}
    constexpr MutStridedPixelRef(
        keel::MutArrayRef<keel::u32> pixels,
        keel::u32 width,
        keel::u32 height,
        keel::u32 rowStride
    ) noexcept {
        if (rowStride >= width) {
            md = keel::MutStridedMdRef<keel::u32, 2>(pixels, {height, width}, {rowStride, 1});
        }
    }
    explicit constexpr MutStridedPixelRef(keel::MutStridedMdRef<keel::u32, 2> matrix) noexcept {
        if (!matrix.empty() && matrix.stride(1) == 1 && matrix.stride(0) >= matrix.extent(1)) {
            md = matrix;
        }
    }

    [[nodiscard]] constexpr operator StridedPixelRef() const noexcept { return StridedPixelRef(md.as_ref()); }
    [[nodiscard]] constexpr bool Empty() const noexcept { return md.empty(); }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return !md.empty(); }
    [[nodiscard]] constexpr keel::u32 Width() const noexcept { return keel::as<keel::u32>(md.extent(1)); }
    [[nodiscard]] constexpr keel::u32 Height() const noexcept { return keel::as<keel::u32>(md.extent(0)); }
    [[nodiscard]] constexpr keel::u32 RowStride() const noexcept { return keel::as<keel::u32>(md.stride(0)); }
    [[nodiscard]] constexpr keel::usize Size() const noexcept { return md.size(); }
    [[nodiscard]] constexpr keel::u32* Data() const noexcept { return md.data(); }
    [[nodiscard]] constexpr bool IsContiguous() const noexcept { return md.is_contiguous(); }
    [[nodiscard]] constexpr MutPixelRef AsContiguous() const noexcept { return MutPixelRef(md.as_contiguous()); }
    [[nodiscard]] constexpr keel::MutArrayRef<keel::u32> Row(keel::u32 y) const noexcept {
        KEEL_BOUNDS_ASSERT(!Empty() && y < Height());
        return keel::MutArrayRef<keel::u32>(md.data() + keel::as<keel::usize>(y) * md.stride(0), Width());
    }
    [[nodiscard]] constexpr keel::u32& At(keel::u32 x, keel::u32 y) const noexcept { return md(y, x); }
    [[nodiscard]] constexpr keel::MutStridedMdRef<keel::u32, 2> Strided() const noexcept { return md; }
    [[nodiscard]] constexpr MutStridedPixelRef Subrect(
        keel::u32 x,
        keel::u32 y,
        keel::u32 w,
        keel::u32 h
    ) const noexcept {
        return MutStridedPixelRef(md.slice_axis(0, y, h).slice_axis(1, x, w));
    }
    constexpr void Fill(keel::u32 color) const noexcept { md.fill(color); }
};

constexpr StridedPixelRef PixelRef::Subrect(
    keel::u32 x,
    keel::u32 y,
    keel::u32 w,
    keel::u32 h
) const noexcept {
    return StridedPixelRef(md.strided().slice_axis(0, y, h).slice_axis(1, x, w));
}

constexpr MutStridedPixelRef MutPixelRef::Subrect(
    keel::u32 x,
    keel::u32 y,
    keel::u32 w,
    keel::u32 h
) const noexcept {
    return MutStridedPixelRef(md.strided().slice_axis(0, y, h).slice_axis(1, x, w));
}

} // namespace guikit
