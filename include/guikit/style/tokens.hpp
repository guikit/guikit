#pragma once

#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"
#include "keel/optional.h"
#include "keel/string.h"

namespace guikit {

/// Semantic visual variants for colors, states, and emphasis across all widgets.
enum class UiVariant : keel::u8 {
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

/// Visual emphasis style for buttons and action triggers (`actions/`).
enum class ButtonStyle : keel::u8 {
    Default = 0, ///< Standard neutral/semantic button following the active theme's `FillStyle` & `ChromeStyle`
    Solid,       ///< Saturated semantic emphasis fill with contrasting foreground ink
    Soft,        ///< Tonal tinted surface with matching semantic accent ink
    Outline,     ///< Transparent fill with crisp semantic border and tinted hover state
    Ghost        ///< Borderless transparent trigger revealing chrome on hover/active (toolbars, chrome icons)
};

/// Visual presentation style for text and selection input fields (`fields/`).
enum class FieldStyle : keel::u8 {
    Default = 0, ///< Theme-driven input well (outlined, bottom-accent, sunken 3D, or brutalist)
    Filled,      ///< Soft tinted surface fill with subtle hover/focus accent
    Borderless   ///< Minimal inline input well (no resting outline; subtle focus indicator)
};

/// Universal relative placement for companion elements (e.g. icons or value readouts inside controls).
enum class Placement : keel::u8 {
    Leading,    ///< Placed before primary element (left / start)
    Trailing,   ///< Placed after primary element (right / end)
    Top,        ///< Placed above primary element
    Bottom,     ///< Placed below primary element
    Thumb       ///< Placed dynamically above the interactive thumb/handle (e.g. Slider)
};

/// Placement of an `Element`'s label and description relative to its control slot.
enum class ElementLayout : keel::u8 {
    Stacked,   ///< Label on top, control slot in middle, Description on bottom
    Top,       ///< Label and Description stacked above control slot
    Bottom,    ///< Label and Description stacked below control slot
    Leading,   ///< Label and Description leading (left of) control slot (`Label ... [Control]`)
    Trailing   ///< Label and Description trailing (right of) control slot (`[Control] Label`)
};

/// Returns true if `layout` arranges text and control horizontally side-by-side (`Leading` or `Trailing`).
[[nodiscard]] constexpr bool IsInlineElementLayout(ElementLayout layout) noexcept {
    return layout == ElementLayout::Leading || layout == ElementLayout::Trailing;
}

/// Semantic heading hierarchy level for `Heading` and document typography (`H1`–`H6`, independent of `UiSize`).
enum class HeadingLevel : keel::u8 {
    H1 = 0, ///< Primary page / document title (`UiTheme::GetHeading(HeadingLevel::H1)`)
    H2 = 1, ///< Major section heading (`UiTheme::GetHeading(HeadingLevel::H2)`)
    H3 = 2, ///< Subsection / card heading (`UiTheme::GetHeading(HeadingLevel::H3)`, default)
    H4 = 3, ///< Group / panel heading (`UiTheme::GetHeading(HeadingLevel::H4)`)
    H5 = 4, ///< Sub-group / minor heading (`UiTheme::GetHeading(HeadingLevel::H5)`)
    H6 = 5  ///< Caption / overline heading (`UiTheme::GetHeading(HeadingLevel::H6)`)
};

/// Built-in separator placement for Heading.
enum class HeadingSeparator : keel::u8 {
    None = 0,
    Bottom,   ///< Horizontal separator line underneath the heading
    Trailing  ///< Inline horizontal separator line flanking/trailing the label bar
};

/// 6x3 Typographic Role Matrix (Material 3 scale extended for desktop/engineering).
enum class TextRole : keel::u8 {
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

/// Semantic theme token tag carried by `StateColor` presets so `UiTheme` can resolve Light/Dark palette variants.
enum class StateColorToken : keel::u8 {
    Custom = 0,
    Primary,
    Secondary,
    Success,
    Warning,
    Danger,
    Info,
    Ghost,
    Muted,
    TextPrimary,
    TextSecondary,
    TextMuted,
    Surface
};

/// Per-state color specification for frames, borders, backgrounds, controls, and text.
/// Pure C++20 aggregate supporting designated initializers (`{.normal = ..., .hover = ...}`)
/// as well as semantic preset constants (`StateColor::Danger()`, `StateColor::Warning()`, etc.).
struct StateColor {
    keel::Optional<Color> normal{keel::kNullOpt};   ///< Idle / resting / unchecked state color
    keel::Optional<Color> hover{keel::kNullOpt};    ///< Mouse-hovered state color
    keel::Optional<Color> active{keel::kNullOpt};   ///< Pressed / checked / focused / filled state color
    keel::Optional<Color> disabled{keel::kNullOpt}; ///< Disabled state color
    StateColorToken token{StateColorToken::Custom}; ///< Optional semantic theme token resolved via UiTheme
    keel::u8 surfaceTint{255};                       ///< Surface mix weight (255 = pure color, 0 = pure theme surface)
    keel::u8 alphaScale{255};                        ///< Post-resolve alpha multiplier (255 = 100% opacity, 0 = 0% opacity)

    [[nodiscard]] constexpr bool HasAny() const noexcept {
        return token != StateColorToken::Custom
            || normal.has_value()
            || hover.has_value()
            || active.has_value()
            || disabled.has_value();
    }

    /// Mixes this semantic/custom color onto the theme's opaque `surface` palette token (`0.0f` = pure surface, `1.0f` = full accent).
    /// Produces an opaque tonal surface (callout/well backdrop) that adapts automatically to both Light and Dark themes.
    [[nodiscard]] constexpr StateColor Tinted(keel::f32 amount = 0.14f) const noexcept {
        StateColor copy = *this;
        const keel::f32 clamped = keel::clamp(amount, 0.0f, 1.0f);
        copy.surfaceTint = keel::as<keel::u8>(clamped * 255.0f + 0.5f);
        return copy;
    }

    /// Scales the resolved alpha channel across all interaction states (`0.0f` = transparent, `1.0f` = unchanged) while keeping theme reactivity.
    [[nodiscard]] constexpr StateColor WithAlphaF(keel::f32 factor) const noexcept {
        StateColor copy = *this;
        const keel::f32 clamped = keel::clamp(factor, 0.0f, 1.0f);
        copy.alphaScale = keel::as<keel::u8>(clamped * 255.0f + 0.5f);
        return copy;
    }

    /// Scales the resolved alpha channel using an 8-bit multiplier (`0..255`).
    [[nodiscard]] constexpr StateColor WithAlpha(keel::u8 alpha) const noexcept {
        StateColor copy = *this;
        copy.alphaScale = alpha;
        return copy;
    }

    [[nodiscard]] constexpr bool IsTinted() const noexcept {
        return surfaceTint < 255;
    }

    [[nodiscard]] constexpr keel::f32 GetTint() const noexcept {
        return keel::as<keel::f32>(surfaceTint) / 255.0f;
    }

    [[nodiscard]] constexpr bool HasAlphaScale() const noexcept {
        return alphaScale < 255;
    }

    [[nodiscard]] constexpr keel::f32 GetAlphaF() const noexcept {
        return keel::as<keel::f32>(alphaScale) / 255.0f;
    }

    [[nodiscard]] static constexpr Color BrightenColor(Color c) noexcept {
        return Color(
            keel::as<keel::u8>(keel::min(255, keel::as<keel::i32>(c.r) + (255 - keel::as<keel::i32>(c.r)) / 5)),
            keel::as<keel::u8>(keel::min(255, keel::as<keel::i32>(c.g) + (255 - keel::as<keel::i32>(c.g)) / 5)),
            keel::as<keel::u8>(keel::min(255, keel::as<keel::i32>(c.b) + (255 - keel::as<keel::i32>(c.b)) / 5)),
            c.a
        );
    }

    [[nodiscard]] static constexpr Color DarkenColor(Color c) noexcept {
        return Color(
            keel::as<keel::u8>((keel::as<keel::i32>(c.r) * 82) / 100),
            keel::as<keel::u8>((keel::as<keel::i32>(c.g) * 82) / 100),
            keel::as<keel::u8>((keel::as<keel::i32>(c.b) * 82) / 100),
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
        auto applyScale = [this](Color c) constexpr noexcept -> Color {
            return (alphaScale < 255 && HasAny()) ? c.ScaleAlpha(alphaScale) : c;
        };
        if (isDisabled) {
            if (disabled.has_value()) return applyScale(*disabled);
            if (normal.has_value()) return applyScale(normal->WithAlphaF(0.40f));
            return fallbackDisabled;
        }
        if (isActive) {
            if (active.has_value()) return applyScale(*active);
            if (hover.has_value()) return applyScale(*hover);
            if (normal.has_value()) return applyScale(DarkenColor(*normal));
            return fallbackActive;
        }
        if (isHovered) {
            if (hover.has_value()) return applyScale(*hover);
            if (normal.has_value()) return applyScale(BrightenColor(*normal));
            return fallbackHover;
        }
        return normal.has_value() ? applyScale(*normal) : fallbackNormal;
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
            .disabled = Color::Hex(0x3B82F666),
            .token    = StateColorToken::Primary
        };
    }
    [[nodiscard]] static constexpr StateColor Secondary() noexcept {
        return {
            .normal   = Color::Hex(0x475569FF), // Slate-600
            .hover    = Color::Hex(0x64748BFF), // Slate-500
            .active   = Color::Hex(0x334155FF), // Slate-700
            .disabled = Color::Hex(0x47556966),
            .token    = StateColorToken::Secondary
        };
    }
    [[nodiscard]] static constexpr StateColor Success() noexcept {
        return {
            .normal   = Color::Hex(0x10B981FF), // Emerald-500
            .hover    = Color::Hex(0x34D399FF), // Emerald-400
            .active   = Color::Hex(0x059669FF), // Emerald-600
            .disabled = Color::Hex(0x10B98166),
            .token    = StateColorToken::Success
        };
    }
    [[nodiscard]] static constexpr StateColor Warning() noexcept {
        return {
            .normal   = Color::Hex(0xF59E0BFF), // Amber-500
            .hover    = Color::Hex(0xFBBF24FF), // Amber-400
            .active   = Color::Hex(0xD97706FF), // Amber-600
            .disabled = Color::Hex(0xF59E0B66),
            .token    = StateColorToken::Warning
        };
    }
    [[nodiscard]] static constexpr StateColor Danger() noexcept {
        return {
            .normal   = Color::Hex(0xEF4444FF), // Red-500
            .hover    = Color::Hex(0xF87171FF), // Red-400
            .active   = Color::Hex(0xDC2626FF), // Red-600
            .disabled = Color::Hex(0xEF444466),
            .token    = StateColorToken::Danger
        };
    }
    [[nodiscard]] static constexpr StateColor Info() noexcept {
        return {
            .normal   = Color::Hex(0x06B6D4FF), // Cyan-500
            .hover    = Color::Hex(0x22D3EEFF), // Cyan-400
            .active   = Color::Hex(0x0891B2FF), // Cyan-600
            .disabled = Color::Hex(0x06B6D466),
            .token    = StateColorToken::Info
        };
    }
    [[nodiscard]] static constexpr StateColor Ghost() noexcept {
        return {
            .normal   = Color::Hex(0x00000000),
            .hover    = Color::Hex(0x64748B38),
            .active   = Color::Hex(0x64748B58),
            .disabled = Color::Hex(0x00000000),
            .token    = StateColorToken::Ghost
        };
    }
    [[nodiscard]] static constexpr StateColor Muted() noexcept {
        return {
            .normal   = Color::Hex(0x64748BFF), // Slate-500
            .hover    = Color::Hex(0x94A3B8FF), // Slate-400
            .active   = Color::Hex(0x475569FF), // Slate-600
            .disabled = Color::Hex(0x64748B66),
            .token    = StateColorToken::Muted
        };
    }
    [[nodiscard]] static constexpr StateColor TextPrimary() noexcept {
        return {
            .normal   = Color::Hex(0xF8FAFCFF), // Slate-50
            .hover    = Color::Hex(0xFFFFFFFF),
            .active   = Color::Hex(0xE2E8F0FF),
            .disabled = Color::Hex(0xF8FAFC66),
            .token    = StateColorToken::TextPrimary
        };
    }
    [[nodiscard]] static constexpr StateColor TextSecondary() noexcept {
        return {
            .normal   = Color::Hex(0x94A3B8FF), // Slate-400
            .hover    = Color::Hex(0xCBD5E1FF), // Slate-300
            .active   = Color::Hex(0x64748BFF), // Slate-500
            .disabled = Color::Hex(0x94A3B866),
            .token    = StateColorToken::TextSecondary
        };
    }
    [[nodiscard]] static constexpr StateColor TextMuted() noexcept {
        return {
            .normal   = Color::Hex(0x64748BFF), // Slate-500
            .hover    = Color::Hex(0x94A3B8FF), // Slate-400
            .active   = Color::Hex(0x475569FF), // Slate-600
            .disabled = Color::Hex(0x64748B66),
            .token    = StateColorToken::TextMuted
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
            case UiVariant::Ghost:     return TextMuted();
        }
        return Default();
    }

    [[nodiscard]] constexpr bool IsNone() const noexcept {
        if (!HasAny() || token != StateColorToken::Custom) return false;
        if (normal.has_value() && normal->a > 0) return false;
        if (hover.has_value() && hover->a > 0) return false;
        if (active.has_value() && active->a > 0) return false;
        if (disabled.has_value() && disabled->a > 0) return false;
        return true;
    }

    [[nodiscard]] constexpr bool IsSurface() const noexcept {
        return token == StateColorToken::Surface;
    }

    /// Resolves whether a background fill is enabled for a widget whose default background state is `defaultEnabled`.
    /// - `StateColor::None()` (all specified states transparent) -> `false`
    /// - `StateColor::Default()` (`{}`) -> `defaultEnabled`
    /// - `StateColor::Surface()` or any custom/semantic color -> `true`
    [[nodiscard]] constexpr bool IsBackgroundEnabled(bool defaultEnabled = false) const noexcept {
        if (!HasAny()) {
            return defaultEnabled;
        }
        return !IsNone();
    }

    [[nodiscard]] static constexpr StateColor None() noexcept {
        return {
            .normal   = Color::Transparent(),
            .hover    = Color::Transparent(),
            .active   = Color::Transparent(),
            .disabled = Color::Transparent()
        };
    }

    [[nodiscard]] static constexpr StateColor Surface() noexcept {
        return {
            .normal   = Color::Hex(0x111722FF),
            .hover    = Color::Hex(0x1A2232FF),
            .active   = Color::Hex(0x232E42FF),
            .disabled = Color::Hex(0x11172266),
            .token    = StateColorToken::Surface
        };
    }

    [[nodiscard]] constexpr bool operator==(const StateColor& other) const noexcept = default;
};

/// Unified border and corner radius specification for Elements, Headings, and Controls.
/// Pure C++20 aggregate supporting designated initializers (`{.width = 1.0f, .radius = 8.0f, .color = StateColor::Danger()}`)
/// as well as concise positional initialization (`{1.0f}`) and static factory presets.
struct Border {
    keel::f32 width{0.0f};                           ///< Stroke thickness in points (0.0f = none / theme default)
    keel::Optional<keel::f32> radius{keel::kNullOpt}; ///< Corner radius override in points (nullopt = theme radius)
    StateColor color{};                          ///< Per-state border stroke color (normal, hover, active, disabled)

    [[nodiscard]] constexpr bool HasStroke() const noexcept {
        return width > 0.0f;
    }

    [[nodiscard]] constexpr bool HasExplicitStroke() const noexcept {
        return color.HasAny() || width > 1.5f;
    }

    [[nodiscard]] constexpr bool HasAny() const noexcept {
        return width > 0.0f || radius.has_value() || color.HasAny();
    }

    [[nodiscard]] static constexpr Border None() noexcept {
        return {};
    }

    [[nodiscard]] static constexpr Border Solid(
        keel::f32 w = 1.0f,
        StateColor col = {},
        keel::Optional<keel::f32> r = keel::kNullOpt
    ) noexcept {
        return {w, r, col};
    }

    [[nodiscard]] static constexpr Border Rounded(keel::f32 r) noexcept {
        return {0.0f, r, {}};
    }

    [[nodiscard]] constexpr bool operator==(const Border& other) const noexcept = default;
};

/// Section divider specification for `Heading`.
/// Supports implicit construction from `HeadingSeparator` (`.separator = HeadingSeparator::Bottom`)
/// as well as custom stroke color and thickness (`.separator = {HeadingSeparator::Bottom, StateColor::Primary(), 1.5f}`).
struct HeadingDivider {
    HeadingSeparator placement{HeadingSeparator::None}; ///< Divider placement (None, Bottom, or Trailing)
    StateColor color{};                                 ///< Per-state divider stroke color (default = theme borderSubtle)
    keel::f32 width{1.0f};                                  ///< Divider stroke thickness in points (<= 0.0f = disabled)

    constexpr HeadingDivider() noexcept = default;
    constexpr HeadingDivider(
        HeadingSeparator inPlacement,
        StateColor inColor = {},
        keel::f32 inWidth = 1.0f
    ) noexcept
        : placement(inPlacement), color(inColor), width(inWidth) {}

    [[nodiscard]] constexpr bool IsEnabled() const noexcept {
        return placement != HeadingSeparator::None && width > 0.0f;
    }

    [[nodiscard]] static constexpr HeadingDivider None() noexcept {
        return {};
    }

    [[nodiscard]] static constexpr HeadingDivider Bottom(StateColor col = {}, keel::f32 w = 1.0f) noexcept {
        return {HeadingSeparator::Bottom, col, w};
    }

    [[nodiscard]] static constexpr HeadingDivider Trailing(StateColor col = {}, keel::f32 w = 1.0f) noexcept {
        return {HeadingSeparator::Trailing, col, w};
    }

    [[nodiscard]] constexpr bool operator==(const HeadingDivider& other) const noexcept = default;
    [[nodiscard]] constexpr bool operator==(HeadingSeparator sep) const noexcept {
        return placement == sep;
    }
};

/// Unified font size specification for UiText.
/// Supports:
/// - Auto (`{}`): inherits the widget slot's default typographic size from `UiTheme`
/// - Explicit pixels (`TextSize::Px(14.0f)` or `14.0f` in `UiText`)
/// - Scale presets (`TextSize::Xs()`, `Sm()`, `Md()`, `Lg()`, `Xl()`, or `UiSize` in `UiText`)
/// - Typographic role (`TextRole::HeadlineSmall`, `TextRole::CodeSmall`, etc.)
struct TextSize {
    keel::f32 px{0.0f};                               ///< Explicit pixel size (> 0.0f overrides theme scale)
    UiSize preset{UiSize::None};                  ///< Relative scale preset (Xs, Sm, Md, Lg, Xl) resolved via UiTheme
    keel::Optional<TextRole> role{keel::kNullOpt}; ///< Optional typographic role (Display, Headline, Title, Body, Label, Code)

    constexpr TextSize() noexcept = default;
    constexpr TextSize(keel::f32 inPx) noexcept : px(inPx) {}
    constexpr TextSize(UiSize s) noexcept : preset(s) {}
    constexpr TextSize(TextRole r) noexcept : role(r) {}
    constexpr TextSize(TextRole r, UiSize s) noexcept : preset(s), role(r) {}

    [[nodiscard]] static constexpr TextSize Auto() noexcept { return {}; }
    [[nodiscard]] static constexpr TextSize Px(keel::f32 inPx) noexcept { return TextSize(inPx); }
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
    keel::u8 minLines{0}; ///< Minimum reserved vertical lines (0 = collapse when empty, >=1 = always reserve height)
    keel::u8 maxLines{1}; ///< Maximum visible lines (1 = single-line with "...", 0 = unlimited word-wrap, >1 = word-wrap capped at maxLines)
    bool rich{false};    ///< Opt-in inline rich-text formatting (**bold**, *italic*, ~~strike~~, `code`, [link](url))

    /// Returns true if multi-line word wrapping is active (`maxLines == 0` unlimited or `maxLines > 1`).
    [[nodiscard]] constexpr bool IsWordWrap() const noexcept {
        return maxLines != 1;
    }

    /// Single-line mode with "..." truncation on overflow. Optionally reserves `reserveLines` when empty.
    [[nodiscard]] static constexpr TextWrap SingleLine(keel::u8 reserveLines = 0) noexcept {
        return {.minLines = reserveLines, .maxLines = 1};
    }

    /// Multi-line word-wrapping mode (`maxLines = 0` means unlimited lines).
    [[nodiscard]] static constexpr TextWrap Word(keel::u8 minReserved = 0, keel::u8 maxVisible = 0) noexcept {
        return {.minLines = minReserved, .maxLines = maxVisible};
    }

    /// Multi-line word-wrapped inline rich-text mode (`**bold**`, `*italic*`, `` `code` ``, `~~strike~~`).
    [[nodiscard]] static constexpr TextWrap Rich(keel::u8 minReserved = 0, keel::u8 maxVisible = 0) noexcept {
        return {.minLines = minReserved, .maxLines = maxVisible, .rich = true};
    }

    /// Single-line inline rich-text mode.
    [[nodiscard]] static constexpr TextWrap RichSingleLine(keel::u8 reserveLines = 0) noexcept {
        return {.minLines = reserveLines, .maxLines = 1, .rich = true};
    }

    /// Reserves vertical height for at least `n` lines even when the text is empty (prevents vertical layout shift).
    /// When `n == 1`, defaults to single-line mode with "..." truncation. When `n > 1`, enables word wrapping.
    [[nodiscard]] static constexpr TextWrap Reserve(keel::u8 n = 1) noexcept {
        return {.minLines = n, .maxLines = (n == 1) ? keel::u8(1) : keel::u8(0)};
    }

    /// Limits multi-line word wrapping to at most `n` lines (truncating the last visible line with "..."),
    /// while collapsing to 0 height when the text is empty.
    [[nodiscard]] static constexpr TextWrap Max(keel::u8 n) noexcept {
        return {.minLines = 0, .maxLines = n};
    }

    /// Reserves and caps height at exactly `n` lines (`minLines = n`, `maxLines = n`).
    [[nodiscard]] static constexpr TextWrap Fixed(keel::u8 n = 1) noexcept {
        return {.minLines = n, .maxLines = n};
    }

    [[nodiscard]] constexpr bool operator==(const TextWrap& other) const noexcept = default;
};

using TextLines = TextWrap;

struct UiText;

namespace detail {
template <typename S>
concept UiStringLike =
    keel::IsConvertible<const S&, keel::StringRef> &&
    !keel::IsSame<keel::Decay<S>, UiText> &&
    !keel::IsSame<keel::Decay<S>, TextVariants>;

template <typename S>
concept UiStringViewLike =
    keel::IsSame<keel::Decay<S>, keel::StringRef> ||
    keel::IsConvertible<const S&, const char*> ||
    (keel::IsTriviallyCopyable<keel::Decay<S>> &&
     requires(const char* d, keel::usize n) {
         typename keel::Decay<S>::traits_type;
         keel::Decay<S>(d, n);
     });

template <typename S>
    requires UiStringLike<S>
[[nodiscard]] constexpr keel::StringRef ToUiStringView(const S& s) noexcept {
    if constexpr (keel::IsConvertible<const S&, const char*>) {
        const char* p = s;
        return p ? keel::StringRef(p) : keel::StringRef{};
    } else {
        return keel::StringRef(s);
    }
}

void AcceptTextVariants(TextVariants) noexcept;

template <typename T>
concept UiTextModifier =
    keel::IsSame<keel::Decay<T>, TextSize> ||
    keel::IsSame<keel::Decay<T>, keel::f32> ||
    keel::IsSame<keel::Decay<T>, UiSize> ||
    keel::IsSame<keel::Decay<T>, TextRole> ||
    keel::IsSame<keel::Decay<T>, StateColor> ||
    keel::IsSame<keel::Decay<T>, Color> ||
    keel::IsSame<keel::Decay<T>, UiVariant> ||
    requires(const T& v) { AcceptTextVariants(v); } ||
    keel::IsSame<keel::Decay<T>, TextWrap>;
} // namespace detail

/// Styled text wrapper combining text content with font size (`TextSize`), per-state color (`StateColor`),
/// wrapping/line reservation policy (`TextWrap`), and candidate width/height variants (`TextVariants`).
/// Implicitly constructible from string literals, `keel::StringRef`, `keel::String` (`ui.Format`), and external string types.
struct GUIKIT_LIFETIME_POINTER UiText {
    keel::StringRef text{};
    TextSize size{};
    StateColor color{};
    TextWrap wrap{};
    TextVariants variants{};

    constexpr void ApplyModifier(TextSize s) noexcept {
        if (s.px > 0.0f) size.px = s.px;
        if (s.preset != UiSize::None) size.preset = s.preset;
        if (s.role.has_value()) size.role = s.role;
    }
    constexpr void ApplyModifier(keel::f32 px) noexcept { size.px = px; }
    constexpr void ApplyModifier(UiSize s) noexcept { size.preset = s; }
    constexpr void ApplyModifier(TextRole r) noexcept { size.role = r; }
    constexpr void ApplyModifier(StateColor c) noexcept { color = c; }
    constexpr void ApplyModifier(Color c) noexcept { color = StateColor::FromColor(c); }
    constexpr void ApplyModifier(UiVariant v) noexcept { color = StateColor::FromTextVariant(v); }
    constexpr void ApplyModifier(TextVariants vars) noexcept { variants = vars; }
    constexpr void ApplyModifier(TextWrap w) noexcept {
        if (w.minLines > 0) wrap.minLines = w.minLines;
        wrap.maxLines = w.maxLines;
        if (w.rich) wrap.rich = true;
    }

    /// Convenience factory for inline rich-text (`**bold**`, `*italic*`, `` `code` ``, `~~strike~~`),
    /// defaulting to multi-line word wrap unless overridden by a `TextWrap` modifier.
    template <typename S, detail::UiTextModifier... Mods>
        requires detail::UiStringLike<S>
    [[nodiscard]] static constexpr UiText Rich(const S& s, const Mods&... mods) noexcept {
        UiText out;
        out.text = detail::ToUiStringView(s);
        out.wrap = TextWrap::Rich();
        (out.ApplyModifier(mods), ...);
        return out;
    }

    constexpr UiText() noexcept = default;
    constexpr UiText(const char* s) noexcept : text(s ? s : "") {}

    template <typename S>
        requires (detail::UiStringLike<S> && !keel::IsConvertible<const S&, const char*> && detail::UiStringViewLike<S>)
    constexpr UiText(const S& s) noexcept : text(keel::StringRef(s)) {}

    template <typename S>
        requires (detail::UiStringLike<S> && !keel::IsConvertible<const S&, const char*> && !detail::UiStringViewLike<S>)
    constexpr UiText(const S& s KEEL_LIFETIMEBOUND) noexcept : text(keel::StringRef(s)) {}

    template <detail::UiTextModifier... Mods>
        requires (sizeof...(Mods) >= 1)
    constexpr UiText(const Mods&... mods) noexcept {
        (ApplyModifier(mods), ...);
    }

    template <typename S, detail::UiTextModifier... Mods>
        requires (detail::UiStringLike<S> && detail::UiStringViewLike<S> && sizeof...(Mods) >= 1)
    constexpr UiText(const S& s, const Mods&... mods) noexcept : text(detail::ToUiStringView(s)) {
        (ApplyModifier(mods), ...);
    }

    template <typename S, detail::UiTextModifier... Mods>
        requires (detail::UiStringLike<S> && !detail::UiStringViewLike<S> && sizeof...(Mods) >= 1)
    constexpr UiText(const S& s KEEL_LIFETIMEBOUND, const Mods&... mods) noexcept : text(detail::ToUiStringView(s)) {
        (ApplyModifier(mods), ...);
    }

    // Explicit TextVariants constructors for braced-initializer-list deduction (e.g. {"text", {"A", "B"}})
    constexpr UiText(TextVariants vars KEEL_LIFETIMEBOUND) noexcept : variants(vars) {}
    constexpr UiText(UiVariant v, TextVariants vars KEEL_LIFETIMEBOUND) noexcept : color(StateColor::FromTextVariant(v)), variants(vars) {}
    constexpr UiText(StateColor c, TextVariants vars KEEL_LIFETIMEBOUND) noexcept : color(c), variants(vars) {}
    constexpr UiText(TextSize s, TextVariants vars KEEL_LIFETIMEBOUND) noexcept : size(s), variants(vars) {}
    constexpr UiText(TextRole r, TextVariants vars KEEL_LIFETIMEBOUND) noexcept : size(r), variants(vars) {}
    constexpr UiText(TextRole r, UiVariant v, TextVariants vars KEEL_LIFETIMEBOUND) noexcept : size(r), color(StateColor::FromTextVariant(v)), variants(vars) {}
    constexpr UiText(UiVariant v, TextRole r, TextVariants vars KEEL_LIFETIMEBOUND) noexcept : size(r), color(StateColor::FromTextVariant(v)), variants(vars) {}
    constexpr UiText(TextWrap w, TextVariants vars KEEL_LIFETIMEBOUND) noexcept : wrap(w), variants(vars) {}
    constexpr UiText(UiVariant v, TextWrap w, TextVariants vars KEEL_LIFETIMEBOUND) noexcept : color(StateColor::FromTextVariant(v)), wrap(w), variants(vars) {}

    constexpr UiText(keel::StringRef s, TextVariants vars KEEL_LIFETIMEBOUND) noexcept : text(s), variants(vars) {}
    constexpr UiText(keel::StringRef s, UiVariant v, TextVariants vars KEEL_LIFETIMEBOUND) noexcept : text(s), color(StateColor::FromTextVariant(v)), variants(vars) {}
    constexpr UiText(keel::StringRef s, StateColor c, TextVariants vars KEEL_LIFETIMEBOUND) noexcept : text(s), color(c), variants(vars) {}
    constexpr UiText(keel::StringRef s, TextVariants vars KEEL_LIFETIMEBOUND, UiVariant v) noexcept : text(s), color(StateColor::FromTextVariant(v)), variants(vars) {}
    constexpr UiText(keel::StringRef s, TextSize sz, TextVariants vars KEEL_LIFETIMEBOUND) noexcept : text(s), size(sz), variants(vars) {}
    constexpr UiText(keel::StringRef s, TextRole r, TextVariants vars KEEL_LIFETIMEBOUND) noexcept : text(s), size(r), variants(vars) {}
    constexpr UiText(keel::StringRef s, TextVariants vars KEEL_LIFETIMEBOUND, TextRole r) noexcept : text(s), size(r), variants(vars) {}
    constexpr UiText(keel::StringRef s, TextRole r, UiVariant v, TextVariants vars KEEL_LIFETIMEBOUND) noexcept : text(s), size(r), color(StateColor::FromTextVariant(v)), variants(vars) {}
    constexpr UiText(keel::StringRef s, UiVariant v, TextRole r, TextVariants vars KEEL_LIFETIMEBOUND) noexcept : text(s), size(r), color(StateColor::FromTextVariant(v)), variants(vars) {}
    constexpr UiText(keel::StringRef s, TextWrap w, TextVariants vars KEEL_LIFETIMEBOUND) noexcept : text(s), wrap(w), variants(vars) {}
    constexpr UiText(keel::StringRef s, UiVariant v, TextWrap w, TextVariants vars KEEL_LIFETIMEBOUND) noexcept : text(s), color(StateColor::FromTextVariant(v)), wrap(w), variants(vars) {}

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
    [[nodiscard]] constexpr UiText WithRich(bool r = true) const noexcept {
        UiText copy = *this;
        copy.wrap.rich = r;
        return copy;
    }
    [[nodiscard]] constexpr UiText Reserve(keel::u8 n = 1) const noexcept {
        return WithWrap(TextWrap::Reserve(n));
    }

    [[nodiscard]] constexpr bool empty() const noexcept { return text.empty(); }
    /// Returns true if the text has visible characters, width/height candidate variants, or reserved minimum lines.
    [[nodiscard]] constexpr bool HasContentOrReserve() const noexcept {
        return !text.empty() || !variants.empty() || wrap.minLines > 0;
    }
    [[nodiscard]] constexpr keel::usize length() const noexcept { return text.size(); }
    [[nodiscard]] constexpr const char* data() const noexcept { return text.data(); }
    [[nodiscard]] constexpr operator keel::StringRef() const noexcept { return text; }

    [[nodiscard]] constexpr TextRole RoleOr(TextRole fallback) const noexcept {
        return size.role.value_or(fallback);
    }

    [[nodiscard]] constexpr bool operator==(const UiText& other) const noexcept = default;
    [[nodiscard]] constexpr bool operator==(keel::StringRef sv) const noexcept { return text == sv; }
    [[nodiscard]] constexpr bool operator==(const char* s) const noexcept {
        return text == (s ? keel::StringRef(s) : keel::StringRef{});
    }
    template <typename S>
        requires (detail::UiStringLike<S> && !keel::IsSame<keel::Decay<S>, keel::StringRef> &&
                  !keel::IsConvertible<const S&, const char*>)
    [[nodiscard]] constexpr bool operator==(const S& s) const noexcept {
        return text == keel::StringRef(s);
    }
};
static_assert(sizeof(UiText) <= 80, "UiText must remain compact (<= 80 bytes)");

/// Unified value token for leading/trailing affixes (.prefix and .suffix) supporting text, icon, or both.
struct GUIKIT_LIFETIME_POINTER UiAffix {
    UiText text{};
    Icon icon{};
    Placement iconPlacement{Placement::Leading}; ///< Leading: [Icon][Text], Trailing: [Text][Icon]

    constexpr UiAffix() noexcept = default;
    constexpr UiAffix(const char* s) noexcept : text(s) {}

    template <typename S>
        requires (detail::UiStringLike<S> && !keel::IsConvertible<const S&, const char*> && detail::UiStringViewLike<S>)
    constexpr UiAffix(const S& s) noexcept : text(s) {}

    template <typename S>
        requires (detail::UiStringLike<S> && !keel::IsConvertible<const S&, const char*> && !detail::UiStringViewLike<S>)
    constexpr UiAffix(const S& s KEEL_LIFETIMEBOUND) noexcept : text(s) {}

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
    constexpr UiAffix(TextVariants vars KEEL_LIFETIMEBOUND) noexcept : text(vars) {}
    constexpr UiAffix(keel::StringRef s, TextVariants vars KEEL_LIFETIMEBOUND) noexcept : text(s, vars) {}
    constexpr UiAffix(Icon ic, TextVariants vars KEEL_LIFETIMEBOUND) noexcept : text(vars), icon(ic), iconPlacement(Placement::Leading) {}

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

    [[nodiscard]] constexpr operator keel::StringRef() const noexcept { return text.text; }
    [[nodiscard]] constexpr operator const UiText&() const noexcept { return text; }

    [[nodiscard]] constexpr bool operator==(const UiAffix& other) const noexcept = default;
    [[nodiscard]] constexpr bool operator==(keel::StringRef sv) const noexcept {
        return !icon.IsValid() && text.text == sv;
    }
    [[nodiscard]] constexpr bool operator==(const char* s) const noexcept {
        return !icon.IsValid() && text == s;
    }
    template <typename S>
        requires (detail::UiStringLike<S> && !keel::IsSame<keel::Decay<S>, keel::StringRef> &&
                  !keel::IsConvertible<const S&, const char*>)
    [[nodiscard]] constexpr bool operator==(const S& s) const noexcept {
        return !icon.IsValid() && text.text == keel::StringRef(s);
    }
    [[nodiscard]] constexpr bool operator==(const Icon& ic) const noexcept {
        return text.empty() && icon == ic;
    }
};

/// Typographic metrics ratio representing line-height to font-size for standard UI text.
inline constexpr keel::f32 kDefaultLineHeightMultiplier = 1.35f;

/// Typographic metrics ratio representing average character advance to font-size for proportional text fallback.
inline constexpr keel::f32 kDefaultCharAdvanceRatio = 0.58f;

/// Typographic metrics ratio representing average character advance to font-size for monospace text fallback.
inline constexpr keel::f32 kMonospaceCharAdvanceRatio = 0.62f;

/// Metrics associated with a TextRole.
struct TypographyMetrics {
    keel::f32 fontSize;
    keel::f32 lineHeight;
    bool isMonospace;
};

/// Standardized component metrics associated with each UiSize.
struct ComponentMetrics {
    keel::f32 defaultHeight;
    keel::f32 fontSize;             ///< Primary label / control text size (LabelLarge level)
    keel::f32 companionFontSize;    ///< Companion / inner control text size (LabelMedium level)
    keel::f32 descFontSize;         ///< Helper description / caption text size (LabelSmall level)
    keel::f32 paddingX;
    keel::f32 paddingY;
    keel::f32 cornerRadius;
    keel::f32 iconSize;
    keel::f32 minWidth{48.0f};
    keel::f32 defaultWidth{60.0f};
    keel::f32 badgeFontSize{11.0f}; ///< Pill badge text size inside controls and form labels
    keel::f32 itemGap{4.5f};        ///< Horizontal gap between leading icon, label, and inline items
    keel::f32 badgeGap{6.0f};       ///< Horizontal gap before trailing pill badge
    keel::f32 lineGap{2.25f};       ///< Vertical gap between stacked label and description lines
};

/// Standardized metrics for scrollbars derived from UiSize tokens.
struct ScrollbarMetrics {
    keel::f32 trackWidth;      ///< Thickness of the scrollbar track and thumb in points
    keel::f32 minThumbLength;  ///< Minimum thumb size preventing it from becoming unclickable
    keel::f32 cornerRadius;    ///< Rounding radius of the thumb pill
    keel::f32 scrollStep;      ///< Pixels / points scrolled per mouse wheel tick
    keel::f32 margin;          ///< Margin between track and viewport edge / content
    keel::f32 gutter;          ///< Total reserved gutter (trackWidth + 2 * margin)
};

/// Standardized metrics for Carousel navigation arrows and ribbon scrolling derived from UiSize tokens.
struct CarouselMetrics {
    keel::f32 arrowButtonSize; ///< Width & height of the navigation arrow square button
    keel::f32 iconSize;        ///< Size of the vector chevron arrow icon
    keel::f32 cornerRadius;    ///< Rounding radius of the arrow button
    keel::f32 margin;          ///< Outer margin between arrow button and container edge
    keel::f32 gap;             ///< Inner gap between arrow button and scrollable viewport
    keel::f32 gutter;          ///< Total reserved gutter (margin + arrowButtonSize + gap)
    keel::f32 scrollStep;      ///< Scroll distance in points per arrow button click
};

/// Standardized metrics for Slider and StepSlider widgets according to UiSize.
struct SliderMetrics {
    keel::f32 defaultHeight;  ///< Height of the interactive track/thumb row
    keel::f32 trackHeight;    ///< Height/thickness of the horizontal track
    keel::f32 thumbSize;      ///< Diameter/size of the circular thumb knob
    keel::f32 cornerRadius;   ///< Rounding radius of track and thumb
    keel::f32 tickSize;       ///< Diameter of tick marks on StepSlider track
    keel::f32 labelGap;       ///< Vertical gap between header label and track
    keel::f32 tickGap;        ///< Vertical gap between track and step labels
    keel::f32 descGap;        ///< Vertical gap between slider and description
    keel::f32 defaultWidth{135.0f}; ///< Default track length in points
};

/// Standardized metrics for Switch widget according to UiSize.
struct SwitchMetrics {
    Vec2 trackSize;
    keel::f32 knobDiameter;
    keel::f32 knobPadding;
    keel::f32 gap;
    keel::f32 labelGap;
    keel::f32 descGap;
};

/// Standardized metrics for Checkbox widget according to UiSize.
struct CheckboxMetrics {
    keel::f32 boxSize;
    keel::f32 cornerRadius;
    keel::f32 gap;
    keel::f32 labelGap;
    keel::f32 descGap;
    keel::f32 strokeWidth;
};

/// Standardized metrics for InputField widget according to UiSize.
struct InputFieldMetrics {
    keel::f32 defaultHeight;
    keel::f32 cornerRadius;
    keel::f32 paddingX;
    keel::f32 labelGap;
    keel::f32 descGap;
    keel::f32 strokeWidth;
    keel::f32 iconSize;
    keel::f32 defaultWidth{120.0f};
    keel::f32 minWidth{60.0f};
};

/// Standardized metrics for Select widget according to UiSize.
struct SelectMetrics {
    keel::f32 defaultHeight;
    keel::f32 cornerRadius;
    keel::f32 paddingX;
    keel::f32 labelGap;
    keel::f32 descGap;
    keel::f32 strokeWidth;
    keel::f32 iconSize;
    keel::f32 itemHeight;
    keel::f32 defaultWidth{120.0f};
    keel::f32 minWidth{60.0f};
};

/// Standardized metrics for section `Heading` typography according to `HeadingLevel` (`H1`–`H6`).
struct HeadingMetrics {
    keel::f32 titleFontSize;
    keel::f32 subtitleFontSize;
    keel::f32 badgeFontSize;
    keel::f32 iconSize;
    keel::f32 gap;
    keel::f32 badgeGap;
    keel::f32 lineGap;
};

/// Universal color theme modes.
enum class ThemeMode : keel::u8 {
    Dark,
    Light
};

/// Curated built-in design system theme presets.
enum class ThemePreset : keel::u8 {
    Dark = 0,         ///< Modern Slate Dark (default)
    Light,            ///< Crisp Slate Light
    Nord,             ///< Arctic bluish dark (Polar Night + Frost)
    Dracula,          ///< Vibrant purple-charcoal dark
    CatppuccinMocha,  ///< Warm pastel dark (Catppuccin Mocha)
    CatppuccinLatte,  ///< Warm cream light (Catppuccin Latte)
    Industrial,       ///< High-contrast amber-on-obsidian SCADA / cockpit (Dark)
    IndustrialLight,  ///< Daylight brushed-steel & amber SCADA / engineering HMI (Light)
    HighContrastDark, ///< WCAG AAA high-contrast dark (sharp borders, max legibility)
    HighContrastLight,///< WCAG AAA high-contrast light
    Windows98         ///< Classic retro Windows 98 silver-gray & navy blue (0dp sharp corners)
};

inline constexpr keel::usize kThemePresetCount = 11u;

/// Human-readable display name for a `ThemePreset`.
[[nodiscard]] constexpr keel::StringRef ThemePresetName(ThemePreset preset) noexcept {
    switch (preset) {
        case ThemePreset::Dark:              return "Dark";
        case ThemePreset::Light:             return "Light";
        case ThemePreset::Nord:              return "Nord";
        case ThemePreset::Dracula:           return "Dracula";
        case ThemePreset::CatppuccinMocha:   return "Catppuccin Mocha";
        case ThemePreset::CatppuccinLatte:   return "Catppuccin Latte";
        case ThemePreset::Industrial:        return "Industrial Dark";
        case ThemePreset::IndustrialLight:   return "Industrial Light";
        case ThemePreset::HighContrastDark:  return "High Contrast Dark";
        case ThemePreset::HighContrastLight: return "High Contrast Light";
        case ThemePreset::Windows98:         return "Windows 98";
    }
    return "Dark";
}

/// Unified visual surface role shared across `surfaces/`, `overlays/`, `views/`, `Node`, and `IStylePainter`.
enum class SurfaceRole : keel::u8 {
    None = 0,       ///< Transparent layout container without surface chrome
    Flat,           ///< Standard card / panel backdrop (etched 3D groove in Classic3D, 1px hairline in Modern/Fluent)
    Sunken,         ///< Recessed well / input tray / list / table / tree container (sunken 3D in Classic3D)
    Raised,         ///< Elevated card / floating dialog / modal surface (shadow in Modern/Fluent, raised 3D in Classic3D, hard shadow in Brutalist)
    Popup,          ///< Floating dropdown / context menu / toast / command palette overlay
    DockEdge        ///< Application shell dock panel (1-sided seam divider in Docked mode, full frame in Floating mode)
};

/// Edge orientation of a `SurfaceRole::DockEdge` seam (for Sidebar, Toolbar, MenuBar, BottomPanel, StatusBar).
enum class DockSeam : keel::u8 {
    None = 0,
    Left,
    Right,
    Top,
    Bottom
};

/// Elevation levels for theme-controlled drop shadows.
enum class Elevation : keel::u8 {
    None = 0, ///< Flat (no shadow)
    Sm   = 1, ///< Subtle lift (tooltips, readouts, inline callouts)
    Md   = 2, ///< Cards, dropdown selects, popovers
    Lg   = 3  ///< Context menus, date pickers, command palettes, toasts
};

/// Theme-resolved drop shadow parameters passed to `ICanvas::DrawShadow`.
struct ShadowStyle {
    keel::f32 blur{0.0f};
    Vec2 offset{0.0f, 0.0f};
    Color color{0, 0, 0, 0};

    [[nodiscard]] constexpr bool IsVisible() const noexcept {
        return color.a > 0u && (blur > 0.0f || offset.x != 0.0f || offset.y != 0.0f);
    }

    [[nodiscard]] constexpr ShadowStyle Scaled(keel::f32 geomScale) const noexcept {
        return ShadowStyle{blur * geomScale, {offset.x * geomScale, offset.y * geomScale}, color};
    }
};

/// Architectural framing style for application shell chrome (Toolbar, Sidebar, ActivityBar, BottomPanel, StatusBar).
enum class ShellStyle : keel::u8 {
    Docked = 0, ///< Flush edge-to-edge panels (0px outer padding, 0px panel gap, 0px radius, 1px inner-seam divider)
    Floating    ///< Detached floating islands (outer window padding, inter-panel gap, rounded corners, 4-sided border)
};

/// Structural visual language / look-and-feel (zero hardcoded colors; orthogonal to `ThemePalette`).
enum class ChromeStyle : keel::u8 {
    Modern = 0,  ///< Flat vector surfaces, soft ambient shadows, rounded corners, overlay scrollbars
    Fluent,      ///< Directional 1px lighting, bottom-accent input fields, left-accent selection bars (Win11 style)
    Classic3D,   ///< 2px raised/sunken/etched 3D bevels, sharp corners, 1dp press shift, 3D controls (Win98 style)
    Brutalist,   ///< Bold 2px borders, hard offset shadows, sharp corners, flat controls
    Glass        ///< Frosted & refractive translucent glass with GPU backdrop blur and specular rim lighting
};

inline constexpr keel::usize kChromeStyleCount = 5u;

/// Human-readable display name for a `ChromeStyle`.
[[nodiscard]] constexpr keel::StringRef ChromeStyleName(ChromeStyle style) noexcept {
    switch (style) {
        case ChromeStyle::Modern:    return "Modern";
        case ChromeStyle::Fluent:    return "Fluent";
        case ChromeStyle::Classic3D: return "Classic 3D";
        case ChromeStyle::Brutalist: return "Brutalist";
        case ChromeStyle::Glass:     return "Glass";
    }
    return "Modern";
}

/// Semantic accent fill treatment applied to Primary / Success / Warning / Danger / Info variants.
enum class FillStyle : keel::u8 {
    Solid = 0,   ///< Saturated semantic fills with high-contrast foreground text (classic default)
    Soft,        ///< Material 3 / Radix tonal tinted surfaces with matching vibrant ink and hairline borders
    Outline      ///< Minimalist transparent fill with crisp semantic borders and tinted hover states
};

inline constexpr keel::usize kFillStyleCount = 3u;

/// Human-readable display name for a `FillStyle`.
[[nodiscard]] constexpr keel::StringRef FillStyleName(FillStyle style) noexcept {
    switch (style) {
        case FillStyle::Solid:   return "Solid";
        case FillStyle::Soft:    return "Soft (Tonal)";
        case FillStyle::Outline: return "Outline";
    }
    return "Solid";
}

/// Curated syntax highlighting scheme selectable independently from UI theme preset.
enum class SyntaxPreset : keel::u8 {
    Auto = 0,       ///< Match the active `ThemePreset` syntax palette
    VsCodeDark,     ///< VS Code Dark+ Modern
    Monokai,        ///< Monokai Pro
    Dracula,        ///< Dracula
    OneDark,        ///< Atom One Dark Pro
    Solarized,      ///< Solarized Dark
    GitHubLight,    ///< GitHub Light
    MatlabClassic   ///< MATLAB Classic Light
};

inline constexpr keel::usize kSyntaxPresetCount = 8u;

/// Human-readable display name for a `SyntaxPreset`.
[[nodiscard]] constexpr keel::StringRef SyntaxPresetName(SyntaxPreset preset) noexcept {
    switch (preset) {
        case SyntaxPreset::Auto:          return "Auto (Match Preset)";
        case SyntaxPreset::VsCodeDark:    return "VS Code Dark+";
        case SyntaxPreset::Monokai:       return "Monokai Pro";
        case SyntaxPreset::Dracula:       return "Dracula";
        case SyntaxPreset::OneDark:       return "One Dark Pro";
        case SyntaxPreset::Solarized:     return "Solarized Dark";
        case SyntaxPreset::GitHubLight:   return "GitHub Light";
        case SyntaxPreset::MatlabClassic: return "MATLAB Classic";
    }
    return "Auto (Match Preset)";
}

/// 4-tone 3D bevel color palette dynamically derived from `ThemePalette` (for both Dark and Light palettes).
struct BevelPalette {
    Color highlightOuter{255, 255, 255, 255}; ///< Outer top/left bright edge
    Color highlightInner{223, 223, 223, 255}; ///< Inner top/left light edge
    Color shadowInner{128, 128, 128, 255};    ///< Inner bottom/right shadow edge
    Color shadowOuter{0, 0, 0, 255};          ///< Outer bottom/right deep edge
};

/// Structural frame treatment applied by the active `IStylePainter` to surfaces and buttons.
enum class FrameTreatment : keel::u8 {
    Flat = 0,       ///< Clean vector fill + uniform stroke (Modern)
    Directional1px, ///< Subtle 1px top-bright / bottom-dark directional lighting (Fluent / Win11)
    Bevel3D,        ///< Classic 2px raised/sunken/etched 3D bevel frames (Classic3D / Win98)
    HardShadow,     ///< High-contrast crisp 2px border + hard non-blurred offset shadow (Brutalist)
    Glass           ///< Refractive & frosted backdrop-blur glass with specular rim lighting (Glass)
};

/// Structural treatment for editable input & selection field wells (`fields/`).
enum class FieldTreatment : keel::u8 {
    Outlined = 0,   ///< Standard uniform border around input slot (Modern / Brutalist)
    BottomAccent,   ///< Subtle outline with emphasized bottom accent bar on focus (Fluent)
    Sunken3D,       ///< Classic 2px sunken 3D input well (Classic3D)
    Filled          ///< Soft tonal background fill with minimal resting border
};

/// Structural treatment for inline/embedded field action buttons (`Select` chevron, `DatePicker`, `SpinBox` steppers).
enum class FieldButtonStyle : keel::u8 {
    Ghost = 0,      ///< Transparent inline trigger with subtle hover tint (Modern / Fluent)
    Framed,         ///< Crisp outlined button inside/beside field (Brutalist)
    Raised3D        ///< Classic raised 3D bevel trigger button (Classic3D)
};

/// Keyboard focus indicator treatment rendered by `IStylePainter`.
enum class FocusStyle : keel::u8 {
    OuterRing = 0,  ///< Standard outer focus ring stroke (Modern / Fluent)
    InnerFrame,     ///< Classic inset focus outline inside control bounds (Classic3D)
    BorderAccent    ///< Bold border accent highlight (Brutalist)
};

/// Selected/hovered row highlight style for menus, popups, lists, sidebars, tables, and trees.
enum class SelectionHighlightStyle : keel::u8 {
    Tonal = 0,      ///< Soft surfaceHover / subtle accent tint preserving primary text ink (Modern)
    LeftAccentBar,  ///< Soft tonal fill + crisp vertical accent pill bar on leading edge (Fluent)
    SolidAccent     ///< Full solid accent fill with `palette.accentText` foreground ink (Classic3D / Brutalist)
};

/// Structural treatment for separators and docked panel seams.
enum class DividerStyle : keel::u8 {
    Hairline = 0,   ///< Clean 1px hairline stroke (Modern / Fluent)
    Etched3D,       ///< 2px etched 3D groove (Classic3D)
    Bold            ///< 2px high-contrast stroke (Brutalist)
};

/// Structural treatment for `Checkbox` and `Radio` indicators.
enum class CheckControlStyle : keel::u8 {
    FilledAccent = 0, ///< Solid accent fill with contrasting check/dot ink when checked (Modern / Fluent / Brutalist)
    SunkenInk         ///< Sunken inputBackground well with primary-text check/dot ink (Classic3D)
};

/// Structural geometry of `Slider` / `StepSlider` / `RangeSlider` thumb knobs.
enum class SliderThumbShape : keel::u8 {
    Circle = 0,     ///< Modern circular thumb knob (`Modern`)
    Ring,           ///< Concentric outer ring with responsive accent inner core (`Fluent` / Win11)
    Square,         ///< Sharp rectangular thumb with bold border & hard shadow (`Brutalist` or zero radius)
    Pointer3D,      ///< Smooth vector pentagonal 3D pointer thumb with bevel edges (`Classic3D`)
    Bar             ///< Tactile cross-axis fader handle with center indicator notch (DAW / SCADA / CAD)
};

inline constexpr keel::usize kSliderThumbShapeCount = 5u;

/// Human-readable display name for a `SliderThumbShape`.
[[nodiscard]] constexpr keel::StringRef SliderThumbShapeName(SliderThumbShape shape) noexcept {
    switch (shape) {
        case SliderThumbShape::Circle:    return "Circle";
        case SliderThumbShape::Ring:      return "Ring (Fluent)";
        case SliderThumbShape::Square:    return "Square";
        case SliderThumbShape::Pointer3D: return "Pointer 3D";
        case SliderThumbShape::Bar:       return "Bar (Fader)";
    }
    return "Circle";
}

/// Structural fill treatment of slider tracks.
enum class SliderTrackStyle : keel::u8 {
    Filled = 0,     ///< Active track range filled with semantic accent color
    NeutralGroove   ///< Neutral recessed groove unless explicit accent is set (Classic3D)
};

/// Structural fill style for `ProgressBar`.
enum class ProgressFillStyle : keel::u8 {
    Smooth = 0,     ///< Continuous bar (rounded or sharp according to theme radius)
    Segmented       ///< Classic sunken trough with segmented rectangular blocks (Classic3D)
};

/// Structural rendering style for `TabBar`.
enum class TabBarChrome : keel::u8 {
    Modern = 0,     ///< Standard GuiKit Underline / Card / Pill / Folder tabs
    Classic3D       ///< Contiguous chamfered 3D property-sheet tabs (zero gap)
};

/// Structural corner shape for badges, counters, and tags.
enum class BadgeShape : keel::u8 {
    Pill = 0,       ///< Fully rounded capsule (height * 0.5)
    Rounded,        ///< Subtle rounded corners (Fluent)
    Sharp           ///< 0dp sharp rectangular corners (Classic3D / Brutalist)
};

/// Text rendering treatment for disabled controls.
enum class DisabledTextStyle : keel::u8 {
    Opacity = 0,    ///< Standard alpha attenuation (`theme.disabledOpacity`)
    Embossed3D      ///< Classic 1px highlight-drop + shadow-ink embossing (derived from `BevelPalette`)
};

/// Structural chrome style for scrollbar track, thumb, and optional stepper buttons.
enum class ScrollbarChromeStyle : keel::u8 {
    Modern = 0,     ///< Rounded pill thumb without stepper buttons (Modern / Fluent)
    Flat,           ///< Wide sharp 0dp flat rectangular track, thumb & borderless chevron buttons (Windows 10 / VS Code)
    ClassicBevel,   ///< 3D raised bevel buttons & thumb, sunken track (Classic3D)
    Framed          ///< Sharp 0dp geometric scrollbar with bold rail divider, bordered thumb & chevron buttons (Brutalist)
};

inline constexpr keel::usize kScrollbarChromeStyleCount = 4u;

/// Human-readable display name for a `ScrollbarChromeStyle`.
[[nodiscard]] constexpr keel::StringRef ScrollbarChromeStyleName(ScrollbarChromeStyle style) noexcept {
    switch (style) {
        case ScrollbarChromeStyle::Modern:       return "Modern";
        case ScrollbarChromeStyle::Flat:         return "Flat";
        case ScrollbarChromeStyle::ClassicBevel: return "Classic 3D";
        case ScrollbarChromeStyle::Framed:       return "Framed";
    }
    return "Modern";
}

/// 16-field declarative structural look-and-feel matrix consumed by `IStylePainter` (zero colors).
struct ChromeTokens {
    FrameTreatment          frame{FrameTreatment::Flat};
    FieldTreatment          field{FieldTreatment::Outlined};
    FieldButtonStyle        fieldButton{FieldButtonStyle::Ghost};
    FocusStyle              focus{FocusStyle::OuterRing};
    SelectionHighlightStyle selection{SelectionHighlightStyle::Tonal};
    DividerStyle            divider{DividerStyle::Hairline};
    CheckControlStyle       checkControl{CheckControlStyle::FilledAccent};
    SliderThumbShape        sliderThumb{SliderThumbShape::Circle};
    SliderTrackStyle        sliderTrack{SliderTrackStyle::Filled};
    ProgressFillStyle       progressFill{ProgressFillStyle::Smooth};
    TabBarChrome            tabChrome{TabBarChrome::Modern};
    BadgeShape              badgeShape{BadgeShape::Pill};
    DisabledTextStyle       disabledText{DisabledTextStyle::Opacity};
    ScrollbarChromeStyle    scrollbar{ScrollbarChromeStyle::Modern};
    keel::f32               buttonPressOffset{0.0f}; ///< Content shift in dp on pressed/active buttons
    keel::f32               tabGapOverride{-1.0f};   ///< >= 0 overrides default TabBar item gap (e.g. 0.0f for Classic3D)

    [[nodiscard]] static constexpr ChromeTokens FromStyle(ChromeStyle style) noexcept {
        switch (style) {
            case ChromeStyle::Modern:
                return ChromeTokens{};
            case ChromeStyle::Fluent:
                return ChromeTokens{
                    .frame             = FrameTreatment::Directional1px,
                    .field             = FieldTreatment::BottomAccent,
                    .fieldButton       = FieldButtonStyle::Ghost,
                    .focus             = FocusStyle::OuterRing,
                    .selection         = SelectionHighlightStyle::LeftAccentBar,
                    .divider           = DividerStyle::Hairline,
                    .checkControl      = CheckControlStyle::FilledAccent,
                    .sliderThumb       = SliderThumbShape::Ring,
                    .sliderTrack       = SliderTrackStyle::Filled,
                    .progressFill      = ProgressFillStyle::Smooth,
                    .tabChrome         = TabBarChrome::Modern,
                    .badgeShape        = BadgeShape::Rounded,
                    .disabledText      = DisabledTextStyle::Opacity,
                    .scrollbar         = ScrollbarChromeStyle::Modern,
                    .buttonPressOffset = 0.0f,
                    .tabGapOverride    = -1.0f
                };
            case ChromeStyle::Classic3D:
                return ChromeTokens{
                    .frame             = FrameTreatment::Bevel3D,
                    .field             = FieldTreatment::Sunken3D,
                    .fieldButton       = FieldButtonStyle::Raised3D,
                    .focus             = FocusStyle::InnerFrame,
                    .selection         = SelectionHighlightStyle::SolidAccent,
                    .divider           = DividerStyle::Etched3D,
                    .checkControl      = CheckControlStyle::SunkenInk,
                    .sliderThumb       = SliderThumbShape::Pointer3D,
                    .sliderTrack       = SliderTrackStyle::NeutralGroove,
                    .progressFill      = ProgressFillStyle::Segmented,
                    .tabChrome         = TabBarChrome::Classic3D,
                    .badgeShape        = BadgeShape::Sharp,
                    .disabledText      = DisabledTextStyle::Embossed3D,
                    .scrollbar         = ScrollbarChromeStyle::ClassicBevel,
                    .buttonPressOffset = 1.0f,
                    .tabGapOverride    = 0.0f
                };
            case ChromeStyle::Brutalist:
                return ChromeTokens{
                    .frame             = FrameTreatment::HardShadow,
                    .field             = FieldTreatment::Outlined,
                    .fieldButton       = FieldButtonStyle::Framed,
                    .focus             = FocusStyle::BorderAccent,
                    .selection         = SelectionHighlightStyle::SolidAccent,
                    .divider           = DividerStyle::Bold,
                    .checkControl      = CheckControlStyle::FilledAccent,
                    .sliderThumb       = SliderThumbShape::Square,
                    .sliderTrack       = SliderTrackStyle::Filled,
                    .progressFill      = ProgressFillStyle::Smooth,
                    .tabChrome         = TabBarChrome::Modern,
                    .badgeShape        = BadgeShape::Sharp,
                    .disabledText      = DisabledTextStyle::Opacity,
                    .scrollbar         = ScrollbarChromeStyle::Framed,
                    .buttonPressOffset = 1.0f,
                    .tabGapOverride    = 0.0f
                };
            case ChromeStyle::Glass:
                return ChromeTokens{
                    .frame             = FrameTreatment::Glass,
                    .field             = FieldTreatment::Filled,
                    .fieldButton       = FieldButtonStyle::Ghost,
                    .focus             = FocusStyle::OuterRing,
                    .selection         = SelectionHighlightStyle::Tonal,
                    .divider           = DividerStyle::Hairline,
                    .checkControl      = CheckControlStyle::FilledAccent,
                    .sliderThumb       = SliderThumbShape::Circle,
                    .sliderTrack       = SliderTrackStyle::Filled,
                    .progressFill      = ProgressFillStyle::Smooth,
                    .tabChrome         = TabBarChrome::Modern,
                    .badgeShape        = BadgeShape::Pill,
                    .disabledText      = DisabledTextStyle::Opacity,
                    .scrollbar         = ScrollbarChromeStyle::Modern,
                    .buttonPressOffset = 0.0f,
                    .tabGapOverride    = -1.0f
                };
        }
        return ChromeTokens{};
    }
};

/// Visual style of scrollbar stepper arrow glyphs (Up/Down or Left/Right).
enum class ScrollbarArrowStyle : keel::u8 {
    None = 0,       ///< Buttonless scrollbar
    Triangle,       ///< Solid filled triangle (Classic3D)
    Chevron         ///< Vector V-chevron (Framed / Brutalist)
};

/// Structural scrollbar configuration tokens in `UiTheme` (zero hardcoded colors; all colors derive from `ThemePalette`).
struct ScrollbarStyleTokens {
    ScrollbarChromeStyle chrome{ScrollbarChromeStyle::Modern};
    ScrollbarArrowStyle  arrowStyle{ScrollbarArrowStyle::None};
    bool                 showButtons{false};     ///< Show Up/Down (or Left/Right) stepper buttons
    bool                 alwaysShowTrack{true};  ///< Render track background even when not hovered
    bool                 showCornerBox{false};   ///< Fill bottom-right intersection when both V & H scrollbars are visible
    keel::f32            thumbInset{0.0f};       ///< Cross-axis inset of thumb inside track in dp
    keel::f32            buttonSize{0.0f};       ///< Stepper button length along scroll axis (0 = square matching trackWidth)

    [[nodiscard]] static constexpr ScrollbarStyleTokens Modern() noexcept {
        return ScrollbarStyleTokens{};
    }

    /// Palette-pure scrollbar tokens driven by `ScrollbarChromeStyle` (zero hardcoded colors; adapts to any Dark/Light theme).
    [[nodiscard]] static constexpr ScrollbarStyleTokens ForChrome(ScrollbarChromeStyle c) noexcept {
        ScrollbarStyleTokens s{};
        s.chrome = c;
        switch (c) {
            case ScrollbarChromeStyle::Modern:
                return s;
            case ScrollbarChromeStyle::Flat:
                s.arrowStyle      = ScrollbarArrowStyle::Chevron;
                s.showButtons     = true;
                s.alwaysShowTrack = true;
                s.showCornerBox   = true;
                s.thumbInset      = 0.0f;
                return s;
            case ScrollbarChromeStyle::ClassicBevel:
                s.arrowStyle      = ScrollbarArrowStyle::Triangle;
                s.showButtons     = true;
                s.alwaysShowTrack = true;
                s.showCornerBox   = true;
                return s;
            case ScrollbarChromeStyle::Framed:
                s.arrowStyle      = ScrollbarArrowStyle::Chevron;
                s.showButtons     = true;
                s.alwaysShowTrack = true;
                s.showCornerBox   = true;
                s.thumbInset      = 2.0f;
                return s;
        }
        return s;
    }
};

/// Compact declarative recipe for generating a complete `UiTheme` via `UiTheme::Build(spec)`.
/// Supports seed-based accent/neutral derivation, surface appearance styles, layer contrast,
/// radius/spacing/font/border/shadow/accessibility scaling, syntax scheme selection,
/// and zero-dependency JSON export/import (`ToJson` / `FromJson`).
struct ThemeSpec {
    ThemePreset base{ThemePreset::Dark};                     ///< Starting curated preset (palette + syntax + default chrome)
    keel::Optional<ThemeMode> mode{keel::kNullOpt};          ///< Optional Dark/Light mode override
    keel::Optional<ChromeStyle> chrome{keel::kNullOpt};      ///< Optional structural chrome override (`Modern`, `Fluent`, `Classic3D`, `Brutalist`)
    keel::Optional<FillStyle> fillStyle{keel::kNullOpt};     ///< Optional semantic fill treatment override (`Solid`, `Soft`, `Outline`)
    SyntaxPreset syntaxPreset{SyntaxPreset::Auto};           ///< Independent code editor syntax color scheme (`Auto` = match preset)
    keel::Optional<Color> accent{keel::kNullOpt};    ///< Optional seed brand accent (auto-derives Primary, hover, active, contrast text, focusRing, caret)
    keel::Optional<Color> neutral{keel::kNullOpt};   ///< Optional neutral tint seed blended into surfaces & borders
    keel::Optional<Color> background{keel::kNullOpt};///< Optional direct viewport background color override
    keel::Optional<Color> surface{keel::kNullOpt};   ///< Optional direct card/surface fill color override
    keel::Optional<Color> border{keel::kNullOpt};    ///< Optional direct border color override
    keel::Optional<Color> textPrimary{keel::kNullOpt};///< Optional direct primary text ink color override
    keel::f32 surfaceContrast{1.0f};                 ///< Layer separation contrast multiplier (0.5 = subtle/flat, 1.0 = default, 1.8 = crisp)
    keel::f32 radiusScale{1.0f};                     ///< Corner radius scale (0.0 = sharp, 1.0 = default, 1.6 = soft/pill)
    keel::f32 spacingScale{1.0f};                    ///< Layout spacing & padding density multiplier (0.75..1.4)
    keel::f32 fontScale{1.0f};                       ///< Global typography size multiplier (0.8..1.5)
    keel::f32 lineHeightMultiplier{kDefaultLineHeightMultiplier}; ///< Paragraph / multi-line leading multiplier (1.1..1.8)
    keel::f32 borderScale{1.0f};                     ///< Stroke thickness scale (0.0 = borderless, 1.0 = default, 2.0 = bold)
    keel::f32 shadowIntensity{1.0f};                 ///< Elevation shadow strength (0.0 = flat/no shadows, 1.0 = default, 1.6 = deep)
    keel::f32 focusRingWidth{2.0f};                  ///< Keyboard focus outline thickness in dp (1.0..4.0)
    keel::f32 disabledOpacity{0.50f};                ///< Disabled widget opacity multiplier (0.2..0.8)
    keel::f32 animationScale{1.0f};                  ///< Motion scale (0.0 = reduced motion, 1.0 = default)
    ShellStyle shellStyle{ShellStyle::Docked};       ///< Application chrome framing (`Docked` or `Floating`)
    keel::Optional<Color> success{keel::kNullOpt};   ///< Optional semantic Success color override
    keel::Optional<Color> warning{keel::kNullOpt};   ///< Optional semantic Warning color override
    keel::Optional<Color> danger{keel::kNullOpt};    ///< Optional semantic Danger color override
    keel::Optional<Color> info{keel::kNullOpt};      ///< Optional semantic Info color override
    keel::Optional<ScrollbarChromeStyle> scrollbarChrome{keel::kNullOpt}; ///< Optional scrollbar chrome override (`Modern`, `Flat`, `ClassicBevel`, `Framed`)
    keel::Optional<keel::f32> scrollbarWidth{keel::kNullOpt};             ///< Optional Md scrollbar track width in dp (4..28)
    keel::Optional<SliderThumbShape> sliderThumb{keel::kNullOpt};         ///< Optional slider thumb shape override (`Circle`, `Ring`, `Square`, `Pointer3D`, `Bar`)

    /// Serializes this `ThemeSpec` to a compact, human-readable JSON object.
    [[nodiscard]] keel::String ToJson() const;

    /// Parses a `ThemeSpec` from JSON produced by `ToJson()` (returns `kNullOpt` on invalid input).
    [[nodiscard]] static keel::Optional<ThemeSpec> FromJson(keel::StringRef json) noexcept;
};

/// Theme-resolved geometric tokens for application shell framing.
struct ShellMetrics {
    Padding windowPadding{0.0f};   ///< Outer viewport inset around root shell containers
    keel::f32   panelGap{0.0f};        ///< Gap between adjacent shell panels (Toolbar, Sidebar, BottomPanel, StatusBar, Canvas)
    keel::f32   panelRadius{0.0f};     ///< Default corner radius of shell chrome panels
    keel::f32   panelBorderWidth{1.0f};///< Default border/divider stroke thickness of shell chrome panels
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
    keel::f32 borderWidth{0.0f};
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
