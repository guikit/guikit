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

/// Text wrapping and overflow behavior for UiText elements (labels, descriptions, and standalone text).
enum class TextWrap : uint8_t {
    SingleLine = 0, ///< Single line; truncated with trailing "..." if it exceeds available width (default)
    Word            ///< Multi-line word wrapping; breaks by words/characters to fit available width
};

/// Line count constraints and vertical space reservation policy for UiText.
struct TextLines {
    uint8_t minLines{0}; ///< Minimum reserved vertical lines (0 = collapse when empty, >=1 = always reserve height)
    uint8_t maxLines{0}; ///< Maximum visible lines before "..." truncation (0 = unlimited when Word, 1 when SingleLine)
    TextWrap wrap{TextWrap::SingleLine};

    constexpr TextLines() noexcept = default;
    constexpr TextLines(uint8_t inMin, uint8_t inMax, TextWrap inWrap = TextWrap::Word) noexcept
        : minLines(inMin)
        , maxLines(inMax)
        , wrap((inMax == 1) ? TextWrap::SingleLine : inWrap) {}

    /// Reserves vertical height for at least `n` lines even when the text is empty (prevents vertical layout shift).
    /// When `n == 1`, defaults to single-line mode with "..." truncation. When `n > 1`, enables word wrapping.
    [[nodiscard]] static constexpr TextLines Reserve(uint8_t n = 1) noexcept {
        return TextLines(n, (n == 1) ? 1 : 0, (n > 1) ? TextWrap::Word : TextWrap::SingleLine);
    }

    /// Limits multi-line word wrapping to at most `n` lines (truncating the last visible line with "..."),
    /// while collapsing to 0 height when the text is empty.
    [[nodiscard]] static constexpr TextLines Max(uint8_t n) noexcept {
        return TextLines(0, n, (n == 1) ? TextWrap::SingleLine : TextWrap::Word);
    }

    /// Reserves and caps height at exactly `n` lines (`minLines = n`, `maxLines = n`).
    [[nodiscard]] static constexpr TextLines Fixed(uint8_t n = 1) noexcept {
        return TextLines(n, n, (n > 1) ? TextWrap::Word : TextWrap::SingleLine);
    }

    [[nodiscard]] constexpr bool operator==(const TextLines& other) const noexcept = default;
};

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
    std::is_same_v<std::decay_t<T>, TextRole> ||
    std::is_same_v<std::decay_t<T>, UiVariant> ||
    std::is_same_v<std::decay_t<T>, TextVariants> ||
    std::is_same_v<std::decay_t<T>, TextWrap> ||
    std::is_same_v<std::decay_t<T>, TextLines>;
} // namespace detail

/// Styled text wrapper combining text content with optional typographic role, color variant,
/// wrapping/line reservation policy, and candidate width/height variants.
/// Implicitly constructible from string literals, std::string_view, std::string, and BasicUiString (ui.Format).
struct UiText {
    std::string_view text{};
    std::optional<TextRole> role{std::nullopt};
    std::optional<UiVariant> variant{std::nullopt};
    TextWrap wrap{TextWrap::SingleLine};
    uint8_t minLines{0};
    uint8_t maxLines{0};
    TextVariants variants{};

    constexpr void ApplyModifier(TextRole r) noexcept { role = r; }
    constexpr void ApplyModifier(UiVariant v) noexcept { variant = v; }
    constexpr void ApplyModifier(TextVariants vars) noexcept { variants = vars; }
    constexpr void ApplyModifier(TextWrap w) noexcept { wrap = w; }
    constexpr void ApplyModifier(TextLines l) noexcept {
        minLines = l.minLines;
        maxLines = l.maxLines;
        wrap = l.wrap;
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
    constexpr UiText(UiVariant v, TextVariants vars) noexcept : variant(v), variants(vars) {}
    constexpr UiText(TextRole r, TextVariants vars) noexcept : role(r), variants(vars) {}
    constexpr UiText(TextRole r, UiVariant v, TextVariants vars) noexcept : role(r), variant(v), variants(vars) {}
    constexpr UiText(UiVariant v, TextRole r, TextVariants vars) noexcept : role(r), variant(v), variants(vars) {}
    constexpr UiText(TextWrap w, TextVariants vars) noexcept : wrap(w), variants(vars) {}
    constexpr UiText(UiVariant v, TextWrap w, TextVariants vars) noexcept : variant(v), wrap(w), variants(vars) {}
    constexpr UiText(TextLines l, TextVariants vars) noexcept
        : wrap(l.wrap), minLines(l.minLines), maxLines(l.maxLines), variants(vars) {}
    constexpr UiText(UiVariant v, TextLines l, TextVariants vars) noexcept
        : variant(v), wrap(l.wrap), minLines(l.minLines), maxLines(l.maxLines), variants(vars) {}

    constexpr UiText(std::string_view s, TextVariants vars) noexcept : text(s), variants(vars) {}
    constexpr UiText(std::string_view s, UiVariant v, TextVariants vars) noexcept : text(s), variant(v), variants(vars) {}
    constexpr UiText(std::string_view s, TextVariants vars, UiVariant v) noexcept : text(s), variant(v), variants(vars) {}
    constexpr UiText(std::string_view s, TextRole r, TextVariants vars) noexcept : text(s), role(r), variants(vars) {}
    constexpr UiText(std::string_view s, TextVariants vars, TextRole r) noexcept : text(s), role(r), variants(vars) {}
    constexpr UiText(std::string_view s, TextRole r, UiVariant v, TextVariants vars) noexcept : text(s), role(r), variant(v), variants(vars) {}
    constexpr UiText(std::string_view s, UiVariant v, TextRole r, TextVariants vars) noexcept : text(s), role(r), variant(v), variants(vars) {}
    constexpr UiText(std::string_view s, TextWrap w, TextVariants vars) noexcept : text(s), wrap(w), variants(vars) {}
    constexpr UiText(std::string_view s, UiVariant v, TextWrap w, TextVariants vars) noexcept : text(s), variant(v), wrap(w), variants(vars) {}
    constexpr UiText(std::string_view s, TextLines l, TextVariants vars) noexcept
        : text(s), wrap(l.wrap), minLines(l.minLines), maxLines(l.maxLines), variants(vars) {}
    constexpr UiText(std::string_view s, UiVariant v, TextLines l, TextVariants vars) noexcept
        : text(s), variant(v), wrap(l.wrap), minLines(l.minLines), maxLines(l.maxLines), variants(vars) {}

    [[nodiscard]] constexpr UiText WithRole(TextRole r) const noexcept {
        UiText copy = *this;
        copy.role = r;
        return copy;
    }
    [[nodiscard]] constexpr UiText WithVariant(UiVariant v) const noexcept {
        UiText copy = *this;
        copy.variant = v;
        return copy;
    }
    [[nodiscard]] constexpr UiText WithWrap(TextWrap w) const noexcept {
        UiText copy = *this;
        copy.wrap = w;
        return copy;
    }
    [[nodiscard]] constexpr UiText WithLines(TextLines l) const noexcept {
        UiText copy = *this;
        copy.ApplyModifier(l);
        return copy;
    }
    [[nodiscard]] constexpr UiText Reserve(uint8_t n = 1) const noexcept {
        return WithLines(TextLines::Reserve(n));
    }

    [[nodiscard]] constexpr bool empty() const noexcept { return text.empty(); }
    /// Returns true if the text has visible characters, width/height candidate variants, or reserved minimum lines.
    [[nodiscard]] constexpr bool HasContentOrReserve() const noexcept {
        return !text.empty() || !variants.empty() || minLines > 0;
    }
    [[nodiscard]] constexpr size_t length() const noexcept { return text.length(); }
    [[nodiscard]] constexpr size_t size() const noexcept { return text.size(); }
    [[nodiscard]] constexpr const char* data() const noexcept { return text.data(); }
    [[nodiscard]] constexpr operator std::string_view() const noexcept { return text; }

    [[nodiscard]] constexpr TextRole RoleOr(TextRole fallback) const noexcept {
        return role.value_or(fallback);
    }
    [[nodiscard]] constexpr UiVariant VariantOr(UiVariant fallback) const noexcept {
        return variant.value_or(fallback);
    }

    [[nodiscard]] constexpr bool operator==(const UiText& other) const noexcept = default;
    [[nodiscard]] constexpr bool operator==(std::string_view sv) const noexcept { return text == sv; }
    [[nodiscard]] constexpr bool operator==(const char* s) const noexcept { return text == (s ? std::string_view(s) : std::string_view{}); }
};
static_assert(sizeof(UiText) <= 40, "UiText must remain compact (<= 40 bytes)");

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
    [[nodiscard]] constexpr UiAffix WithVariant(UiVariant v) const noexcept {
        UiAffix copy = *this;
        copy.text.variant = v;
        return copy;
    }
    [[nodiscard]] constexpr UiAffix WithRole(TextRole r) const noexcept {
        UiAffix copy = *this;
        copy.text.role = r;
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
};

} // namespace guikit
