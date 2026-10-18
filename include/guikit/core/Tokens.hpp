#pragma once

#include <concepts>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include "guikit/core/Types.hpp"
#include "guikit/graphics/Icon.hpp"

namespace guikit {


/// Semantic visual variants for colors, states, and emphasis across all widgets.
enum class UiVariant : uint8_t {
    Default,    ///< Neutral base styling
    Primary,    ///< Primary brand/call-to-action color
    Secondary,  ///< Outline / subtle secondary action
    Success,    ///< Positive / confirmation / success (green)
    Warning,    ///< Caution / alert (amber)
    Danger,     ///< Destructive / emergency / error (red)
    Info,       ///< Informational / accent (blue/cyan)
    Ghost,      ///< Transparent background with hover highlighting
    Muted       ///< Subdued / secondary styling (slate-400)
};

/// Universal relative placement for companion elements (e.g. icons or value readouts inside controls).
enum class Placement : uint8_t {
    Leading,    ///< Placed before primary element (left / start)
    Trailing,   ///< Placed after primary element (right / end)
    Top,        ///< Placed above primary element
    Bottom,     ///< Placed below primary element
    Thumb       ///< Placed dynamically above the interactive thumb/handle (e.g. Slider)
};

/// Compound layout structure for UI elements with external labels/descriptions.
enum class ElementLayout : uint8_t {
    Stacked,  ///< Label on top, control in middle, Description on bottom
    Top,      ///< Label and Description above control
    Bottom,   ///< Label and Description below control
    Inline    ///< Horizontal inline row: Label and control side-by-side
};

/// Sequencing order between external label and interactive control in Inline layout.
enum class InlineOrder : uint8_t {
    LabelFirst,    ///< Label/text first (left / start), control second (right / end)
    ControlFirst   ///< Control first (left / start), label/text second (right / end)
};

/// Divider placement style for Element, Heading, and composite widgets.
enum class ElementDivider : uint8_t {
    None = 0,
    Bottom,   ///< Horizontal divider line underneath the element
    Trailing  ///< Inline horizontal divider line flanking/trailing the label bar
};

using HeadingDivider = ElementDivider;

/// 6x3 Typographic Role Matrix (Material 3 scale extended for desktop/engineering).
enum class TextRole : uint8_t {
    // Display (hero metrics, large gauges, KPI counters)
    DisplayLarge,   ///< 40px / LH 48px
    DisplayMedium,  ///< 32px / LH 40px
    DisplaySmall,   ///< 28px / LH 36px

    // Headline (primary window and screen titles)
    HeadlineLarge,  ///< 24px / LH 32px
    HeadlineMedium, ///< 22px / LH 28px
    HeadlineSmall,  ///< 20px / LH 26px

    // Title (section headers, card titles, modal titles)
    TitleLarge,     ///< 18px / LH 24px
    TitleMedium,    ///< 16px / LH 22px
    TitleSmall,     ///< 15px / LH 20px

    // Body (standard content, forms, table values)
    BodyLarge,      ///< 16px / LH 24px
    BodyMedium,     ///< 14px / LH 20px (Default)
    BodySmall,      ///< 13px / LH 18px (Dense)

    // Label (control captions, badges, toolbars, hints)
    LabelLarge,     ///< 14px / LH 18px
    LabelMedium,    ///< 12px / LH 16px
    LabelSmall,     ///< 11px / LH 14px

    // Code (monospace telemetry, registers, hex dumps, log traces)
    CodeLarge,      ///< 15px / LH 20px
    CodeMedium,     ///< 13px / LH 18px
    CodeSmall       ///< 11px / LH 15px
};

/// Per-state color specification for frames, borders, backgrounds, controls, and text.
/// Pure C++20 aggregate supporting designated initializers (`{.normal = ..., .hover = ...}`)
/// as well as semantic preset constants (`StateColor::Danger()`, `StateColor::Warning()`, etc.).
struct StateColor {
    std::optional<Color> normal{std::nullopt};   ///< Idle / resting / unchecked state color
    std::optional<Color> hover{std::nullopt};    ///< Mouse-hovered state color
    std::optional<Color> active{std::nullopt};   ///< Pressed / checked / focused / filled state color
    std::optional<Color> disabled{std::nullopt}; ///< Disabled state color

    [[nodiscard]] constexpr bool HasAny() const noexcept {
        return normal.has_value() || hover.has_value() || active.has_value() || disabled.has_value();
    }

    [[nodiscard]] static constexpr Color BrightenColor(Color c) noexcept {
        return Color(
            static_cast<uint8_t>(std::min(255, static_cast<int>(c.r) + (255 - static_cast<int>(c.r)) / 5)),
            static_cast<uint8_t>(std::min(255, static_cast<int>(c.g) + (255 - static_cast<int>(c.g)) / 5)),
            static_cast<uint8_t>(std::min(255, static_cast<int>(c.b) + (255 - static_cast<int>(c.b)) / 5)),
            c.a
        );
    }

    [[nodiscard]] static constexpr Color DarkenColor(Color c) noexcept {
        return Color(
            static_cast<uint8_t>((static_cast<int>(c.r) * 82) / 100),
            static_cast<uint8_t>((static_cast<int>(c.g) * 82) / 100),
            static_cast<uint8_t>((static_cast<int>(c.b) * 82) / 100),
            c.a
        );
    }

    /// Resolves the effective color for the given interaction state, falling back to `fallback*` colors when a state is nullopt.
    [[nodiscard]] constexpr Color Resolve(
        bool isHovered,
        bool isActive,
        bool isDisabled,
        Color fallbackNormal,
        Color fallbackHover,
        Color fallbackActive,
        Color fallbackDisabled
    ) const noexcept {
        if (isDisabled) {
            if (disabled.has_value()) return *disabled;
            if (normal.has_value()) return normal->WithAlphaF(0.40f);
            return fallbackDisabled;
        }
        if (isActive) {
            if (active.has_value()) return *active;
            if (hover.has_value()) return *hover;
            if (normal.has_value()) return DarkenColor(*normal);
            return fallbackActive;
        }
        if (isHovered) {
            if (hover.has_value()) return *hover;
            if (normal.has_value()) return BrightenColor(*normal);
            return fallbackHover;
        }
        return normal.value_or(fallbackNormal);
    }

    [[nodiscard]] constexpr Color Resolve(
        bool isHovered,
        bool isActive,
        bool isDisabled,
        Color fallback
    ) const noexcept {
        return Resolve(
            isHovered,
            isActive,
            isDisabled,
            fallback,
            fallback,
            fallback,
            fallback.WithAlphaF(0.40f)
        );
    }

    // --- Semantic Preset Constants ---
    [[nodiscard]] static constexpr StateColor Default() noexcept {
        return {};
    }
    [[nodiscard]] static constexpr StateColor FromColor(Color base) noexcept {
        return {
            .normal   = base,
            .hover    = BrightenColor(base),
            .active   = DarkenColor(base),
            .disabled = base.WithAlphaF(0.40f)
        };
    }
    [[nodiscard]] static constexpr StateColor Primary() noexcept {
        return {
            .normal   = Color::Hex(0x3B82F6FF), // Blue-500
            .hover    = Color::Hex(0x60A5FAFF), // Blue-400
            .active   = Color::Hex(0x2563EBFF), // Blue-600
            .disabled = Color::Hex(0x3B82F666)
        };
    }
    [[nodiscard]] static constexpr StateColor Secondary() noexcept {
        return {
            .normal   = Color::Hex(0x475569FF), // Slate-600
            .hover    = Color::Hex(0x64748BFF), // Slate-500
            .active   = Color::Hex(0x334155FF), // Slate-700
            .disabled = Color::Hex(0x47556966)
        };
    }
    [[nodiscard]] static constexpr StateColor Success() noexcept {
        return {
            .normal   = Color::Hex(0x10B981FF), // Emerald-500
            .hover    = Color::Hex(0x34D399FF), // Emerald-400
            .active   = Color::Hex(0x059669FF), // Emerald-600
            .disabled = Color::Hex(0x10B98166)
        };
    }
    [[nodiscard]] static constexpr StateColor Warning() noexcept {
        return {
            .normal   = Color::Hex(0xF59E0BFF), // Amber-500
            .hover    = Color::Hex(0xFBBF24FF), // Amber-400
            .active   = Color::Hex(0xD97706FF), // Amber-600
            .disabled = Color::Hex(0xF59E0B66)
        };
    }
    [[nodiscard]] static constexpr StateColor Danger() noexcept {
        return {
            .normal   = Color::Hex(0xEF4444FF), // Red-500
            .hover    = Color::Hex(0xF87171FF), // Red-400
            .active   = Color::Hex(0xDC2626FF), // Red-600
            .disabled = Color::Hex(0xEF444466)
        };
    }
    [[nodiscard]] static constexpr StateColor Info() noexcept {
        return {
            .normal   = Color::Hex(0x06B6D4FF), // Cyan-500
            .hover    = Color::Hex(0x22D3EEFF), // Cyan-400
            .active   = Color::Hex(0x0891B2FF), // Cyan-600
            .disabled = Color::Hex(0x06B6D466)
        };
    }
    [[nodiscard]] static constexpr StateColor Ghost() noexcept {
        return {
            .normal   = Color::Hex(0x00000000),
            .hover    = Color::Hex(0x64748B38),
            .active   = Color::Hex(0x64748B58),
            .disabled = Color::Hex(0x00000000)
        };
    }
    [[nodiscard]] static constexpr StateColor Muted() noexcept {
        return {
            .normal   = Color::Hex(0x64748BFF), // Slate-500
            .hover    = Color::Hex(0x94A3B8FF), // Slate-400
            .active   = Color::Hex(0x475569FF), // Slate-600
            .disabled = Color::Hex(0x64748B66)
        };
    }
    [[nodiscard]] static constexpr StateColor TextSecondary() noexcept {
        return {
            .normal   = Color::Hex(0x94A3B8FF), // Slate-400
            .hover    = Color::Hex(0xCBD5E1FF), // Slate-300
            .active   = Color::Hex(0x64748BFF), // Slate-500
            .disabled = Color::Hex(0x94A3B866)
        };
    }
    [[nodiscard]] static constexpr StateColor FromVariant(UiVariant v) noexcept {
        switch (v) {
            case UiVariant::Default:   return Default();
            case UiVariant::Primary:   return Primary();
            case UiVariant::Secondary: return Secondary();
            case UiVariant::Success:   return Success();
            case UiVariant::Warning:   return Warning();
            case UiVariant::Danger:    return Danger();
            case UiVariant::Info:      return Info();
            case UiVariant::Ghost:     return Ghost();
            case UiVariant::Muted:     return Muted();
        }
        return Default();
    }
    [[nodiscard]] static constexpr StateColor FromTextVariant(UiVariant v) noexcept {
        switch (v) {
            case UiVariant::Default:   return Default();
            case UiVariant::Primary:   return Primary();
            case UiVariant::Secondary:
            case UiVariant::Muted:     return TextSecondary();
            case UiVariant::Success:   return Success();
            case UiVariant::Warning:   return Warning();
            case UiVariant::Danger:    return Danger();
            case UiVariant::Info:      return Info();
            case UiVariant::Ghost:     return Muted();
        }
        return Default();
    }

    [[nodiscard]] constexpr bool operator==(const StateColor& other) const noexcept = default;
};

/// Unified font size specification for UiText.
/// Supports:
/// - Auto (`{}`): inherits the widget slot's default typographic size from `UiTheme`
/// - Explicit pixels (`TextSize::Px(14.0f)` or `14.0f` in `UiText`)
/// - Scale presets (`TextSize::Xs()`, `Sm()`, `Md()`, `Lg()`, `Xl()`, or `UiSize` in `UiText`)
/// - Typographic role (`TextRole::HeadlineSmall`, `TextRole::CodeSmall`, etc.)
struct TextSize {
    float px{0.0f};                             ///< Explicit pixel size (> 0.0f overrides theme scale)
    UiSize preset{UiSize::None};                ///< Relative scale preset (Xs, Sm, Md, Lg, Xl) resolved via UiTheme
    std::optional<TextRole> role{std::nullopt}; ///< Optional typographic role (Display, Headline, Title, Body, Label, Code)

    constexpr TextSize() noexcept = default;
    constexpr TextSize(float inPx) noexcept : px(inPx) {}
    constexpr TextSize(UiSize s) noexcept : preset(s) {}
    constexpr TextSize(TextRole r) noexcept : role(r) {}
    constexpr TextSize(TextRole r, UiSize s) noexcept : preset(s), role(r) {}

    [[nodiscard]] static constexpr TextSize Auto() noexcept { return {}; }
    [[nodiscard]] static constexpr TextSize Px(float inPx) noexcept { return TextSize(inPx); }
    [[nodiscard]] static constexpr TextSize Xs() noexcept { return TextSize(UiSize::Xs); }
    [[nodiscard]] static constexpr TextSize Sm() noexcept { return TextSize(UiSize::Sm); }
    [[nodiscard]] static constexpr TextSize Md() noexcept { return TextSize(UiSize::Md); }
    [[nodiscard]] static constexpr TextSize Lg() noexcept { return TextSize(UiSize::Lg); }
    [[nodiscard]] static constexpr TextSize Xl() noexcept { return TextSize(UiSize::Xl); }

    [[nodiscard]] constexpr bool IsAuto() const noexcept {
        return px <= 0.0f && preset == UiSize::None && !role.has_value();
    }
    [[nodiscard]] constexpr bool HasValue() const noexcept {
        return !IsAuto();
    }

    [[nodiscard]] constexpr bool operator==(const TextSize& other) const noexcept = default;
};

/// Text wrapping, line count constraints, and vertical space reservation policy for UiText.
/// Pure C++20 aggregate supporting designated initializers (`{.minLines = 1, .maxLines = 2}`)
/// as well as semantic factory presets (`TextWrap::SingleLine()`, `TextWrap::Word()`, `TextWrap::Reserve(n)`, etc.).
struct TextWrap {
    uint8_t minLines{0}; ///< Minimum reserved vertical lines (0 = collapse when empty, >=1 = always reserve height)
    uint8_t maxLines{1}; ///< Maximum visible lines (1 = single-line with "...", 0 = unlimited word-wrap, >1 = word-wrap capped at maxLines)

    /// Returns true if multi-line word wrapping is active (`maxLines == 0` unlimited or `maxLines > 1`).
    [[nodiscard]] constexpr bool IsWordWrap() const noexcept {
        return maxLines != 1;
    }

    /// Single-line mode with "..." truncation on overflow. Optionally reserves `reserveLines` when empty.
    [[nodiscard]] static constexpr TextWrap SingleLine(uint8_t reserveLines = 0) noexcept {
        return {.minLines = reserveLines, .maxLines = 1};
    }

    /// Multi-line word-wrapping mode (`maxLines = 0` means unlimited lines).
    [[nodiscard]] static constexpr TextWrap Word(uint8_t minReserved = 0, uint8_t maxVisible = 0) noexcept {
        return {.minLines = minReserved, .maxLines = maxVisible};
    }

    /// Reserves vertical height for at least `n` lines even when the text is empty (prevents vertical layout shift).
    /// When `n == 1`, defaults to single-line mode with "..." truncation. When `n > 1`, enables word wrapping.
    [[nodiscard]] static constexpr TextWrap Reserve(uint8_t n = 1) noexcept {
        return {.minLines = n, .maxLines = (n == 1) ? uint8_t(1) : uint8_t(0)};
    }

    /// Limits multi-line word wrapping to at most `n` lines (truncating the last visible line with "..."),
    /// while collapsing to 0 height when the text is empty.
    [[nodiscard]] static constexpr TextWrap Max(uint8_t n) noexcept {
        return {.minLines = 0, .maxLines = n};
    }

    /// Reserves and caps height at exactly `n` lines (`minLines = n`, `maxLines = n`).
    [[nodiscard]] static constexpr TextWrap Fixed(uint8_t n = 1) noexcept {
        return {.minLines = n, .maxLines = n};
    }

    [[nodiscard]] constexpr bool operator==(const TextWrap& other) const noexcept = default;
};

using TextLines = TextWrap;

struct UiText;

namespace detail {
template <typename S>
concept UiStringLike =
    std::convertible_to<const S&, std::string_view> &&
    !std::is_same_v<std::decay_t<S>, UiText> &&
    !std::is_same_v<std::decay_t<S>, TextVariants>;

template <typename S>
    requires UiStringLike<S>
[[nodiscard]] constexpr std::string_view ToUiStringView(const S& s) noexcept {
    if constexpr (std::is_convertible_v<const S&, const char*>) {
        const char* p = s;
        return p ? std::string_view(p) : std::string_view{};
    } else {
        return static_cast<std::string_view>(s);
    }
}

template <typename T>
concept UiTextModifier =
    std::is_same_v<std::decay_t<T>, TextSize> ||
    std::is_same_v<std::decay_t<T>, float> ||
    std::is_same_v<std::decay_t<T>, UiSize> ||
    std::is_same_v<std::decay_t<T>, TextRole> ||
    std::is_same_v<std::decay_t<T>, StateColor> ||
    std::is_same_v<std::decay_t<T>, Color> ||
    std::is_same_v<std::decay_t<T>, UiVariant> ||
    std::is_same_v<std::decay_t<T>, TextVariants> ||
    std::is_same_v<std::decay_t<T>, TextWrap>;
} // namespace detail

/// Styled text wrapper combining text content with font size (`TextSize`), per-state color (`StateColor`),
/// wrapping/line reservation policy (`TextWrap`), and candidate width/height variants (`TextVariants`).
/// Implicitly constructible from string literals, std::string_view, std::string, and BasicUiString (ui.Format).
struct UiText {
    std::string_view text{};
    TextSize size{};
    StateColor color{};
    TextWrap wrap{};
    TextVariants variants{};

    constexpr void ApplyModifier(TextSize s) noexcept {
        if (s.px > 0.0f) size.px = s.px;
        if (s.preset != UiSize::None) size.preset = s.preset;
        if (s.role.has_value()) size.role = s.role;
    }
    constexpr void ApplyModifier(float px) noexcept { size.px = px; }
    constexpr void ApplyModifier(UiSize s) noexcept { size.preset = s; }
    constexpr void ApplyModifier(TextRole r) noexcept { size.role = r; }
    constexpr void ApplyModifier(StateColor c) noexcept { color = c; }
    constexpr void ApplyModifier(Color c) noexcept { color = StateColor::FromColor(c); }
    constexpr void ApplyModifier(UiVariant v) noexcept { color = StateColor::FromTextVariant(v); }
    constexpr void ApplyModifier(TextVariants vars) noexcept { variants = vars; }
    constexpr void ApplyModifier(TextWrap w) noexcept {
        if (w.minLines > 0) wrap.minLines = w.minLines;
        wrap.maxLines = w.maxLines;
    }

    constexpr UiText() noexcept = default;
    constexpr UiText(const char* s) noexcept : text(s ? s : "") {}

    template <typename S>
        requires (detail::UiStringLike<S> && !std::is_convertible_v<const S&, const char*>)
    constexpr UiText(const S& s) noexcept : text(static_cast<std::string_view>(s)) {}

    template <detail::UiTextModifier... Mods>
        requires (sizeof...(Mods) >= 1)
    constexpr UiText(Mods... mods) noexcept {
        (ApplyModifier(mods), ...);
    }

    template <typename S, detail::UiTextModifier... Mods>
        requires (detail::UiStringLike<S> && sizeof...(Mods) >= 1)
    constexpr UiText(const S& s, Mods... mods) noexcept : text(detail::ToUiStringView(s)) {
        (ApplyModifier(mods), ...);
    }

    // Explicit TextVariants constructors for braced-initializer-list deduction (e.g. {"text", {"A", "B"}})
    constexpr UiText(TextVariants vars) noexcept : variants(vars) {}
    constexpr UiText(UiVariant v, TextVariants vars) noexcept : color(StateColor::FromTextVariant(v)), variants(vars) {}
    constexpr UiText(StateColor c, TextVariants vars) noexcept : color(c), variants(vars) {}
    constexpr UiText(TextSize s, TextVariants vars) noexcept : size(s), variants(vars) {}
    constexpr UiText(TextRole r, TextVariants vars) noexcept : size(r), variants(vars) {}
    constexpr UiText(TextRole r, UiVariant v, TextVariants vars) noexcept : size(r), color(StateColor::FromTextVariant(v)), variants(vars) {}
    constexpr UiText(UiVariant v, TextRole r, TextVariants vars) noexcept : size(r), color(StateColor::FromTextVariant(v)), variants(vars) {}
    constexpr UiText(TextWrap w, TextVariants vars) noexcept : wrap(w), variants(vars) {}
    constexpr UiText(UiVariant v, TextWrap w, TextVariants vars) noexcept : color(StateColor::FromTextVariant(v)), wrap(w), variants(vars) {}

    constexpr UiText(std::string_view s, TextVariants vars) noexcept : text(s), variants(vars) {}
    constexpr UiText(std::string_view s, UiVariant v, TextVariants vars) noexcept : text(s), color(StateColor::FromTextVariant(v)), variants(vars) {}
    constexpr UiText(std::string_view s, StateColor c, TextVariants vars) noexcept : text(s), color(c), variants(vars) {}
    constexpr UiText(std::string_view s, TextVariants vars, UiVariant v) noexcept : text(s), color(StateColor::FromTextVariant(v)), variants(vars) {}
    constexpr UiText(std::string_view s, TextSize sz, TextVariants vars) noexcept : text(s), size(sz), variants(vars) {}
    constexpr UiText(std::string_view s, TextRole r, TextVariants vars) noexcept : text(s), size(r), variants(vars) {}
    constexpr UiText(std::string_view s, TextVariants vars, TextRole r) noexcept : text(s), size(r), variants(vars) {}
    constexpr UiText(std::string_view s, TextRole r, UiVariant v, TextVariants vars) noexcept : text(s), size(r), color(StateColor::FromTextVariant(v)), variants(vars) {}
    constexpr UiText(std::string_view s, UiVariant v, TextRole r, TextVariants vars) noexcept : text(s), size(r), color(StateColor::FromTextVariant(v)), variants(vars) {}
    constexpr UiText(std::string_view s, TextWrap w, TextVariants vars) noexcept : text(s), wrap(w), variants(vars) {}
    constexpr UiText(std::string_view s, UiVariant v, TextWrap w, TextVariants vars) noexcept : text(s), color(StateColor::FromTextVariant(v)), wrap(w), variants(vars) {}

    [[nodiscard]] constexpr UiText WithSize(TextSize s) const noexcept {
        UiText copy = *this;
        copy.size = s;
        return copy;
    }
    [[nodiscard]] constexpr UiText WithRole(TextRole r) const noexcept {
        UiText copy = *this;
        copy.size.role = r;
        return copy;
    }
    [[nodiscard]] constexpr UiText WithColor(StateColor c) const noexcept {
        UiText copy = *this;
        copy.color = c;
        return copy;
    }
    [[nodiscard]] constexpr UiText WithVariant(UiVariant v) const noexcept {
        UiText copy = *this;
        copy.color = StateColor::FromTextVariant(v);
        return copy;
    }
    [[nodiscard]] constexpr UiText WithWrap(TextWrap w) const noexcept {
        UiText copy = *this;
        copy.wrap = w;
        return copy;
    }
    [[nodiscard]] constexpr UiText WithLines(TextWrap w) const noexcept {
        UiText copy = *this;
        copy.wrap = w;
        return copy;
    }
    [[nodiscard]] constexpr UiText Reserve(uint8_t n = 1) const noexcept {
        return WithWrap(TextWrap::Reserve(n));
    }

    [[nodiscard]] constexpr bool empty() const noexcept { return text.empty(); }
    /// Returns true if the text has visible characters, width/height candidate variants, or reserved minimum lines.
    [[nodiscard]] constexpr bool HasContentOrReserve() const noexcept {
        return !text.empty() || !variants.empty() || wrap.minLines > 0;
    }
    [[nodiscard]] constexpr size_t length() const noexcept { return text.length(); }
    [[nodiscard]] constexpr const char* data() const noexcept { return text.data(); }
    [[nodiscard]] constexpr operator std::string_view() const noexcept { return text; }

    [[nodiscard]] constexpr TextRole RoleOr(TextRole fallback) const noexcept {
        return size.role.value_or(fallback);
    }

    [[nodiscard]] constexpr bool operator==(const UiText& other) const noexcept = default;
    [[nodiscard]] constexpr bool operator==(std::string_view sv) const noexcept { return text == sv; }
    [[nodiscard]] constexpr bool operator==(const char* s) const noexcept { return text == (s ? std::string_view(s) : std::string_view{}); }
};
static_assert(sizeof(UiText) <= 80, "UiText must remain compact (<= 80 bytes)");

/// Unified value token for leading/trailing affixes (.prefix and .suffix) supporting text, icon, or both.
struct UiAffix {
    UiText text{};
    Icon icon{};
    Placement iconPlacement{Placement::Leading}; ///< Leading: [Icon][Text], Trailing: [Text][Icon]

    constexpr UiAffix() noexcept = default;
    constexpr UiAffix(const char* s) noexcept : text(s) {}

    template <typename S>
        requires (detail::UiStringLike<S> && !std::is_convertible_v<const S&, const char*>)
    constexpr UiAffix(const S& s) noexcept : text(s) {}

    constexpr UiAffix(UiText t) noexcept : text(t) {}
    constexpr UiAffix(Icon ic) noexcept : icon(ic) {}
    constexpr UiAffix(IconId id) noexcept : icon(id) {}

    template <detail::UiTextModifier... Mods>
        requires (sizeof...(Mods) >= 1)
    constexpr UiAffix(Mods... mods) noexcept : text(mods...) {}

    template <typename S, detail::UiTextModifier... Mods>
        requires (detail::UiStringLike<S> && sizeof...(Mods) >= 1)
    constexpr UiAffix(const S& s, Mods... mods) noexcept : text(s, mods...) {}

    // Icon + modifiers (e.g. {Icon::Settings(), UiVariant::Info})
    template <detail::UiTextModifier... Mods>
        requires (sizeof...(Mods) >= 1)
    constexpr UiAffix(Icon ic, Mods... mods) noexcept : text(mods...), icon(ic) {}

    // Icon + Text (Icon Leading): {Icon::Search(), "Filter", UiVariant::Secondary, ...}
    template <typename S, detail::UiTextModifier... Mods>
        requires (detail::UiStringLike<S>)
    constexpr UiAffix(Icon ic, const S& s, Mods... mods) noexcept
        : text(detail::ToUiStringView(s), mods...), icon(ic), iconPlacement(Placement::Leading) {}

    constexpr UiAffix(Icon ic, UiText t, Placement placement = Placement::Leading) noexcept
        : text(t), icon(ic), iconPlacement(placement) {}

    // Text + Icon (Icon Trailing): {"Pro", Icon::Check(), UiVariant::Warning, ...}
    template <typename S, detail::UiTextModifier... Mods>
        requires (detail::UiStringLike<S>)
    constexpr UiAffix(const S& s, Icon ic, Mods... mods) noexcept
        : text(detail::ToUiStringView(s), mods...), icon(ic), iconPlacement(Placement::Trailing) {}

    constexpr UiAffix(UiText t, Icon ic, Placement placement = Placement::Trailing) noexcept
        : text(t), icon(ic), iconPlacement(placement) {}

    // TextVariants support
    constexpr UiAffix(TextVariants vars) noexcept : text(vars) {}
    constexpr UiAffix(std::string_view s, TextVariants vars) noexcept : text(s, vars) {}
    constexpr UiAffix(Icon ic, TextVariants vars) noexcept : text(vars), icon(ic), iconPlacement(Placement::Leading) {}

    [[nodiscard]] constexpr UiAffix WithIcon(Icon ic, Placement placement = Placement::Leading) const noexcept {
        UiAffix copy = *this;
        copy.icon = ic;
        copy.iconPlacement = placement;
        return copy;
    }
    [[nodiscard]] constexpr UiAffix WithText(UiText t) const noexcept {
        UiAffix copy = *this;
        copy.text = t;
        return copy;
    }
    [[nodiscard]] constexpr UiAffix WithColor(StateColor c) const noexcept {
        UiAffix copy = *this;
        copy.text.color = c;
        return copy;
    }
    [[nodiscard]] constexpr UiAffix WithVariant(UiVariant v) const noexcept {
        UiAffix copy = *this;
        copy.text.color = StateColor::FromTextVariant(v);
        return copy;
    }
    [[nodiscard]] constexpr UiAffix WithSize(TextSize s) const noexcept {
        UiAffix copy = *this;
        copy.text.size = s;
        return copy;
    }
    [[nodiscard]] constexpr UiAffix WithRole(TextRole r) const noexcept {
        UiAffix copy = *this;
        copy.text.size.role = r;
        return copy;
    }

    [[nodiscard]] constexpr bool empty() const noexcept {
        return text.empty() && !icon.IsValid();
    }
    [[nodiscard]] constexpr bool HasContentOrReserve() const noexcept {
        return text.HasContentOrReserve() || icon.IsValid();
    }
    [[nodiscard]] constexpr bool HasText() const noexcept {
        return !text.empty() || !text.variants.empty();
    }
    [[nodiscard]] constexpr bool HasIcon() const noexcept {
        return icon.IsValid();
    }

    [[nodiscard]] constexpr operator std::string_view() const noexcept { return text.text; }
    [[nodiscard]] constexpr operator const UiText&() const noexcept { return text; }

    [[nodiscard]] constexpr bool operator==(const UiAffix& other) const noexcept = default;
    [[nodiscard]] constexpr bool operator==(std::string_view sv) const noexcept {
        return !icon.IsValid() && text.text == sv;
    }
    [[nodiscard]] constexpr bool operator==(const char* s) const noexcept {
        return !icon.IsValid() && text == s;
    }
    [[nodiscard]] constexpr bool operator==(const Icon& ic) const noexcept {
        return text.empty() && icon == ic;
    }
};

/// Typographic metrics ratio representing line-height to font-size for standard UI text.
inline constexpr float kDefaultLineHeightMultiplier = 1.35f;

/// Typographic metrics ratio representing average character advance to font-size for proportional text fallback.
inline constexpr float kDefaultCharAdvanceRatio = 0.58f;

/// Typographic metrics ratio representing average character advance to font-size for monospace text fallback.
inline constexpr float kMonospaceCharAdvanceRatio = 0.62f;

/// Metrics associated with a TextRole.
struct TypographyMetrics {
    float fontSize;
    float lineHeight;
    bool isMonospace;
};

/// Standardized component metrics associated with each UiSize.
struct ComponentMetrics {
    float defaultHeight;
    float fontSize;          ///< Primary label / control text size (LabelLarge level)
    float companionFontSize; ///< Companion / inner control text size (LabelMedium level)
    float descFontSize;      ///< Helper description / caption text size (LabelSmall level)
    float paddingX;
    float paddingY;
    float cornerRadius;
    float iconSize;
    float minWidth{64.0f};
    float defaultWidth{80.0f};
};

/// Standardized metrics for scrollbars derived from UiSize tokens.
struct ScrollbarMetrics {
    float trackWidth;      ///< Thickness of the scrollbar track and thumb in points
    float minThumbLength;  ///< Minimum thumb size preventing it from becoming unclickable
    float cornerRadius;    ///< Rounding radius of the thumb pill
    float scrollStep;      ///< Pixels / points scrolled per mouse wheel tick
    float margin;          ///< Margin between track and viewport edge / content
    float gutter;          ///< Total reserved gutter (trackWidth + 2 * margin)
};

/// Standardized metrics for Carousel navigation arrows and ribbon scrolling derived from UiSize tokens.
struct CarouselMetrics {
    float arrowButtonSize; ///< Width & height of the navigation arrow square button
    float iconSize;        ///< Size of the vector chevron arrow icon
    float cornerRadius;    ///< Rounding radius of the arrow button
    float margin;          ///< Outer margin between arrow button and container edge
    float gap;             ///< Inner gap between arrow button and scrollable viewport
    float gutter;          ///< Total reserved gutter (margin + arrowButtonSize + gap)
    float scrollStep;      ///< Scroll distance in points per arrow button click
};

/// Standardized metrics for Slider and StepSlider widgets according to UiSize.
struct SliderMetrics {
    float defaultHeight;  ///< Height of the interactive track/thumb row
    float trackHeight;    ///< Height/thickness of the horizontal track
    float thumbSize;      ///< Diameter/size of the circular thumb knob
    float cornerRadius;   ///< Rounding radius of track and thumb
    float tickSize;       ///< Diameter of tick marks on StepSlider track
    float labelGap;       ///< Vertical gap between header label and track
    float tickGap;        ///< Vertical gap between track and step labels
    float descGap;        ///< Vertical gap between slider and description
    float defaultWidth{180.0f}; ///< Default track length in points
};

/// Standardized metrics for Switch widget according to UiSize.
struct SwitchMetrics {
    Vec2 trackSize;
    float knobDiameter;
    float knobPadding;
    float gap;
    float descGap;
};

/// Standardized metrics for Checkbox widget according to UiSize.
struct CheckboxMetrics {
    float boxSize;
    float cornerRadius;
    float gap;
    float descGap;
    float strokeWidth;
};

/// Standardized metrics for InputField widget according to UiSize.
struct InputFieldMetrics {
    float defaultHeight;
    float cornerRadius;
    float paddingX;
    float labelGap;
    float descGap;
    float strokeWidth;
    float iconSize;
    float defaultWidth{160.0f};
    float minWidth{80.0f};
};

/// Standardized metrics for Select widget according to UiSize.
struct SelectMetrics {
    float defaultHeight;
    float cornerRadius;
    float paddingX;
    float labelGap;
    float descGap;
    float strokeWidth;
    float iconSize;
    float itemHeight;
    float defaultWidth{160.0f};
    float minWidth{80.0f};
};

/// Standardized metrics for Heading widget according to UiSize.
struct HeadingMetrics {
    float titleFontSize;
    float subtitleFontSize;
    float badgeFontSize;
    float iconSize;
    float gap;
    float badgeGap;
    float lineGap;
};

/// Density scale for component metrics and touch targets.
enum class Density : uint8_t {
    Standard,  ///< Standard desktop productivity scale
    Compact,   ///< High-density engineering data / dense tabular workflows
    Touch      ///< Touch-friendly enlarged targets and generous spacing
};

/// Universal color theme modes.
enum class ThemeMode : uint8_t {
    Dark,
    Light
};

/// Color palette definition for an entire theme.
struct ThemePalette {
    // 1. Surface and viewport backgrounds
    Color background;       ///< Main viewport clear color (#0B0F19 dark / #F1F5F9 light)
    Color surface;          ///< Card / container fill (#1E293B dark / #FFFFFF light)
    Color surfaceHover;     ///< Hovered container / list item fill (#33415580 dark / #F1F5F9FF light)
    Color surfaceActive;    ///< Active / pressed item fill (#334155FF dark / #E2E8F0FF light)
    Color surfaceElevated;  ///< Dropdowns, floating popups, toolbars (#1E293B dark / #FFFFFF light)

    // 2. Borders and dividers
    Color border;           ///< Card / container border (#334155 dark / #E2E8F0 light)
    Color borderHover;      ///< Hovered control / card border (#64748B dark / #94A3B8 light)
    Color borderSubtle;     ///< Subtle separators and dividers (#33415580 dark / #E2E8F0FF light)
    Color focusRing;        ///< Focus outline glow (#38BDF8FF Sky-400 / #0284C7FF Sky-600)

    // 3. Text hierarchy
    Color textPrimary;      ///< High-contrast primary text (#F8FAFC dark / #0F172A light)
    Color textSecondary;    ///< Medium-contrast muted text (#94A3B8 dark / #64748B light)
    Color textMuted;        ///< Low-contrast subtle text (#64748B dark / #94A3B8 light)
    Color textDisabled;     ///< Disabled interaction text (#64748B80 dark / #94A3B880 light)

    // 4. Text input fields
    Color inputBackground;  ///< Textbox / input slot fill (#0F172A dark / #F8FAFC light)
    Color inputBorder;      ///< Textbox idle border (#334155 dark / #CBD5E1 light)
    Color caret;            ///< Text cursor caret (#38BDF8FF dark / #0284C7FF light)

    // 5. Interactive controls (Switch, Checkbox, Slider, Radio)
    Color controlTrack;        ///< Idle track fill of switch / checkbox (#1E293B dark / #E2E8F0 light)
    Color controlTrackHover;   ///< Hovered track fill (#334155 dark / #CBD5E1 light)
    Color controlKnob;         ///< Idle switch thumb / checkbox uncheck (#94A3B8 dark / #F8FAFC light)
    Color controlKnobHover;    ///< Hovered switch thumb (#E2E8F0 dark / #FFFFFF light)
    Color controlKnobActive;   ///< Checked thumb / active marker (#FFFFFF dark / #FFFFFF light)

    // 6. Accent brand
    Color accent;           ///< Primary brand / highlight color (#38BDF8 Sky-400 / #0284C7 Sky-600)
    Color accentHover;      ///< Primary brand hover color (#7DD3FC Sky-300 / #0369A1 Sky-700)
    Color accentText;       ///< Foreground text on accent fill (#0F172A dark / #FFFFFF light)
};

/// Color set for rendering a widget variant across its interaction states.
struct VariantColors {
    Color bg;
    Color bgHover;
    Color bgActive;
    Color text;
    Color border;
    float borderWidth{0.0f};
};

/// Color palette for configurable code syntax highlighting and editor overlays.
struct SyntaxPalette {
    Color keyword;        ///< Control flow / language keywords (if, return, struct, const...)
    Color type;           ///< Built-in and standard types (int, float, std, string, UiTheme...)
    Color function;       ///< Function & method calls (Button, Format, DrawRect...)
    Color string;         ///< String and character literals ("...", '...')
    Color number;         ///< Numeric literals (42, 3.14f, 0xFF...)
    Color comment;        ///< Line and block comments (//..., /*...*/)
    Color preprocessor;   ///< Preprocessor directives (#include, #pragma...)
    Color punctuation;    ///< Operators, braces, parentheses, semicolons
    Color identifier;     ///< Default identifiers and plain code text
    Color lineHighlight;  ///< Subtle background tint for the active cursor line
    Color selectionBg;    ///< Background fill for selected text ranges
    Color editorBg{0, 0, 0, 0}; ///< Optional editor canvas background (alpha > 0 overrides theme inputBackground)
    Color gutterBg{0, 0, 0, 0}; ///< Optional gutter, minimap & status bar surface (alpha > 0 overrides theme default)

    /// VS Code Dark+ Modern color scheme.
    [[nodiscard]] static constexpr SyntaxPalette VsCodeDark() noexcept {
        return SyntaxPalette{
            .keyword       = Color(197, 134, 192), // #C586C0 Orchid Purple
            .type          = Color(78, 201, 176),  // #4EC9B0 Teal Cyan
            .function      = Color(220, 220, 170), // #DCDCAA Soft Yellow
            .string        = Color(206, 145, 120), // #CE9178 Terracotta Orange
            .number        = Color(181, 206, 168), // #B5CEA8 Sage Green
            .comment       = Color(106, 153, 85),  // #6A9955 Olive Green
            .preprocessor  = Color(86, 156, 214),  // #569CD6 VS Code Blue
            .punctuation   = Color(212, 212, 212), // #D4D4D4 Light Silver
            .identifier    = Color(156, 220, 254), // #9CDCFE Ice Blue
            .lineHighlight = Color(255, 255, 255, 16),
            .selectionBg   = Color(38, 79, 120, 170),
            .editorBg      = Color(30, 30, 30),    // #1E1E1E VS Code Dark+ Editor
            .gutterBg      = Color(23, 23, 23)     // #171717 VS Code Dark+ Gutter
        };
    }

    /// Monokai Pro color scheme.
    [[nodiscard]] static constexpr SyntaxPalette Monokai() noexcept {
        return SyntaxPalette{
            .keyword       = Color(255, 97, 136),  // #FF6188 Monokai Pink/Red
            .type          = Color(120, 220, 232), // #78DCE8 Cyan
            .function      = Color(169, 220, 118), // #A9DC76 Lime Green
            .string        = Color(255, 216, 102), // #FFD866 Warm Yellow
            .number        = Color(171, 157, 242), // #AB9DF2 Lavender Purple
            .comment       = Color(114, 112, 114), // #727072 Warm Grey
            .preprocessor  = Color(252, 152, 103), // #FC9867 Coral Orange
            .punctuation   = Color(147, 146, 147), // #939293 Muted Silver
            .identifier    = Color(252, 252, 250), // #FCFCFA Warm White
            .lineHighlight = Color(255, 255, 255, 16),
            .selectionBg   = Color(255, 97, 136, 65),
            .editorBg      = Color(39, 40, 34),    // #272822 Authentic Monokai Olive-Charcoal
            .gutterBg      = Color(31, 32, 27)     // #1F201B Monokai Gutter
        };
    }

    /// Dracula color scheme.
    [[nodiscard]] static constexpr SyntaxPalette Dracula() noexcept {
        return SyntaxPalette{
            .keyword       = Color(255, 121, 198), // #FF79C6 Neon Pink
            .type          = Color(139, 233, 253), // #8BE9FD Cyan
            .function      = Color(80, 250, 123),  // #50FA7B Emerald Green
            .string        = Color(241, 250, 140), // #F1FA8C Pale Yellow
            .number        = Color(189, 147, 249), // #BD93F9 Violet
            .comment       = Color(98, 114, 164),  // #6272A4 Slate Blue-Grey
            .preprocessor  = Color(255, 184, 108), // #FFB86C Pastel Orange
            .punctuation   = Color(248, 248, 242), // #F8F8F2 Foreground
            .identifier    = Color(248, 248, 242), // #F8F8F2 Foreground
            .lineHighlight = Color(68, 71, 90, 95),
            .selectionBg   = Color(68, 71, 90, 185),
            .editorBg      = Color(40, 42, 54),    // #282A36 Authentic Dracula Background
            .gutterBg      = Color(33, 34, 44)     // #21222C Dracula Gutter
        };
    }

    /// Atom One Dark Pro color scheme.
    [[nodiscard]] static constexpr SyntaxPalette OneDark() noexcept {
        return SyntaxPalette{
            .keyword       = Color(198, 120, 221), // #C678DD Purple
            .type          = Color(229, 192, 123), // #E5C07B Gold
            .function      = Color(97, 175, 239),  // #61AFEF Blue
            .string        = Color(152, 195, 121), // #98C379 Green
            .number        = Color(209, 154, 102), // #D19A66 Orange
            .comment       = Color(92, 99, 112),   // #5C6370 Cool Grey
            .preprocessor  = Color(224, 108, 117), // #E06C75 Crimson Red
            .punctuation   = Color(171, 178, 191), // #ABB2BF Light Grey
            .identifier    = Color(224, 108, 117), // #E06C75 Coral Identifier
            .lineHighlight = Color(44, 49, 60, 120),
            .selectionBg   = Color(62, 68, 81, 185),
            .editorBg      = Color(40, 44, 52),    // #282C34 Authentic One Dark Background
            .gutterBg      = Color(33, 37, 43)     // #21252B One Dark Gutter
        };
    }

    /// MATLAB Editor Classic color scheme (authentic light desktop editor).
    [[nodiscard]] static constexpr SyntaxPalette MatlabClassic() noexcept {
        return SyntaxPalette{
            .keyword       = Color(0, 0, 255),     // #0000FF Classic MATLAB Blue
            .type          = Color(0, 114, 189),   // #0072BD MATLAB Ocean Blue
            .function      = Color(15, 23, 42),    // #0F172A Crisp Dark Function Call
            .string        = Color(160, 32, 240),  // #A020F0 Classic MATLAB Purple ('string')
            .number        = Color(180, 83, 9),    // #B45309 Deep Amber Numeric
            .comment       = Color(34, 139, 34),   // #228B22 Classic MATLAB Forest Green (% comment)
            .preprocessor  = Color(161, 98, 7),    // #A16207 Section Header (%%)
            .punctuation   = Color(51, 65, 85),    // #334155 Slate Operators
            .identifier    = Color(15, 23, 42),    // #0F172A Crisp Black/Charcoal
            .lineHighlight = Color(254, 249, 195, 160), // Subtle MATLAB warm section/line tint
            .selectionBg   = Color(0, 114, 189, 55),
            .editorBg      = Color(255, 255, 255), // #FFFFFF Classic MATLAB White Canvas
            .gutterBg      = Color(240, 242, 245)  // #F0F2F5 Classic MATLAB Light Grey Gutter
        };
    }

    /// Solarized Dark color scheme.
    [[nodiscard]] static constexpr SyntaxPalette Solarized() noexcept {
        return SyntaxPalette{
            .keyword       = Color(133, 153, 0),   // #859900 Solarized Green
            .type          = Color(181, 137, 0),   // #B58900 Solarized Yellow
            .function      = Color(38, 139, 210),  // #268BD2 Solarized Blue
            .string        = Color(42, 161, 152),  // #2AA198 Solarized Cyan
            .number        = Color(211, 54, 130),  // #D33682 Solarized Magenta
            .comment       = Color(88, 110, 117),  // #586E75 Base01
            .preprocessor  = Color(203, 75, 22),   // #CB4B16 Solarized Orange
            .punctuation   = Color(147, 161, 161), // #93A1A1 Base1
            .identifier    = Color(131, 148, 150), // #839496 Base0
            .lineHighlight = Color(7, 54, 66, 180),
            .selectionBg   = Color(38, 139, 210, 80),
            .editorBg      = Color(0, 43, 54),     // #002B36 Authentic Solarized Base03
            .gutterBg      = Color(7, 54, 66)      // #073642 Authentic Solarized Base02
        };
    }

    /// GitHub Light high-contrast color scheme.
    [[nodiscard]] static constexpr SyntaxPalette GitHubLight() noexcept {
        return SyntaxPalette{
            .keyword       = Color(207, 34, 46),   // #CF222E Crimson
            .type          = Color(149, 56, 0),    // #953800 Rust Orange
            .function      = Color(130, 80, 223),  // #8250DF Royal Purple
            .string        = Color(10, 48, 105),   // #0A3069 Deep Navy
            .number        = Color(5, 80, 174),    // #0550AE Cobalt Blue
            .comment       = Color(110, 119, 129), // #6E7781 Slate Grey
            .preprocessor  = Color(130, 80, 223),  // #8250DF Purple
            .punctuation   = Color(36, 41, 47),    // #24292F Charcoal
            .identifier    = Color(36, 41, 47),    // #24292F Charcoal
            .lineHighlight = Color(234, 245, 255, 190),
            .selectionBg   = Color(9, 105, 218, 55),
            .editorBg      = Color(255, 255, 255), // #FFFFFF GitHub Light Canvas
            .gutterBg      = Color(246, 248, 250)  // #F6F8FA GitHub Light Subtle Gutter
        };
    }
};

} // namespace guikit
