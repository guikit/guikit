#pragma once

#include "guikit/context.hpp"
#include "keel/core.h"
#include "keel/optional.h"
#include "keel/ref.h"
#include "keel/string.h"
#include "guikit/core/control.hpp"
#include "guikit/core/response.hpp"
#include "guikit/style/theme.hpp"
#include "guikit/core/types.hpp"

namespace guikit {

class Context;
class ICanvas;
struct Node;

/// Structured calendar date and time value for DatePicker.
struct DateTime {
    keel::i32 year{2026};
    keel::i32 month{10};  ///< Month [1..12]
    keel::i32 day{20};    ///< Day [1..31]
    keel::i32 hour{14};   ///< Hour [0..23]
    keel::i32 minute{30}; ///< Minute [0..59]
    keel::i32 second{0};  ///< Second [0..59]

    /// Returns the current local system date and time.
    [[nodiscard]] static DateTime Today() noexcept;

    constexpr bool operator==(const DateTime&) const noexcept = default;
};

/// Display and editing mode for DatePicker widget.
enum class DatePickerMode : keel::u8 {
    Date,     ///< YYYY-MM-DD
    DateTime, ///< YYYY-MM-DD HH:MM(:SS)
    Time      ///< HH:MM(:SS)
};

/// Localized text labels for DatePicker calendar headers, time units, and footer buttons.
struct DatePickerLabels {
    keel::ArrayRef<keel::StringRef> monthNames{};   ///< Optional 12 localized month names [January .. December]
    keel::ArrayRef<keel::StringRef> weekdayNames{}; ///< Optional 7 localized short weekday labels [Sunday .. Saturday]
    keel::ArrayRef<keel::StringRef> timeSuffixes{}; ///< Optional 3 localized time unit suffixes [h, m, s]
    keel::StringRef today{"Today"};                 ///< Footer button label in `DatePickerMode::Date`
    keel::StringRef now{"Now"};                     ///< Footer button label in `DateTime` / `Time` modes
    keel::StringRef done{"Done"};                   ///< Footer confirmation button label
};

/// Configuration options for DatePicker control widget.
struct DatePickerOptions {
    DatePickerMode mode{DatePickerMode::Date};
    bool showSeconds{false};
    bool showTodayButton{true};
    bool firstDayMonday{true};
    keel::Optional<DateTime> minDate{keel::kNullOpt};       ///< Optional lower bound for selectable date/time
    keel::Optional<DateTime> maxDate{keel::kNullOpt};       ///< Optional upper bound for selectable date/time
    keel::Optional<DateTime> todayOverride{keel::kNullOpt}; ///< Optional deterministic override for "Today"/"Now" (nullopt = DateTime::Today())
    DatePickerLabels labels{};                              ///< Optional localized calendar, time-unit, and button labels
    UiText tooltip{};                                       ///< Optional floating tooltip text on hover
    bool disabled{false};                                   ///< Interaction suppression
    UiSize size{UiSize::Md};                                ///< Control size token (`Xs`..`Xl`, `Dp(...)`, `Factor(...)`)
    AxisSize width{};                                       ///< Horizontal sizing policy (`Auto` -> `Stretch`, `Fit`, or explicit `dp`)
    AxisSize height{};                                      ///< Vertical sizing policy (`Auto` -> `Fit`, `Stretch`, or explicit `dp`)
    keel::Optional<keel::f32> minWidth{keel::kNullOpt};         ///< Optional minimum width in logical dp
    keel::Optional<keel::f32> maxWidth{keel::kNullOpt};         ///< Optional maximum width in logical dp
    keel::Optional<keel::f32> minHeight{keel::kNullOpt};        ///< Optional minimum height in logical dp
    keel::Optional<keel::f32> maxHeight{keel::kNullOpt};        ///< Optional maximum height in logical dp
    FieldStyle style{FieldStyle::Default};                  ///< Visual field presentation (Default, Filled, Borderless)
    UiVariant status{UiVariant::Default};                   ///< Validation / semantic accent status (Default, Success, Warning, Danger, Info)
    StateColor background{};                                ///< Optional background fill override (`{}` = theme default, `StateColor::None()` = transparent)
    Border border{};                                        ///< Optional border width, color, and radius override
};

/// Internal runtime state allocated in frame arena for DatePicker nodes.
struct DatePickerData {
    DateTime value{};
    DatePickerOptions opts{};
    UiText formattedText{};
    bool isOpen{false};
    bool chevronHovered{false};
};

/// Autonomous DatePicker (calendar date and time popover) widget logic.
namespace detail {

class DatePicker {
public:
    /// Primary immediate mode entry point for DatePicker.
    [[nodiscard]] static Response Run(
        Context& ctx,
        DateTime& value,
        const DatePickerOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Measures intrinsic dimensions for DatePicker control.
    [[nodiscard]] static Vec2 Measure(
        const DateTime& value,
        const DatePickerOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme
    );

    [[nodiscard]] static Vec2 Measure(
        const DateTime& value,
        const DatePickerOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr
    );

    /// Renders the closed DatePicker trigger node.
    static void Render(
        const Node* node,
        ICanvas& canvas,
        const UiTheme& theme
    );

    /// Returns true if the given Gregorian year is a leap year.
    [[nodiscard]] static constexpr bool IsLeapYear(keel::i32 year) noexcept {
        return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    }

    /// Returns the number of days [28..31] in the given month [1..12] of year.
    [[nodiscard]] static constexpr keel::i32 DaysInMonth(keel::i32 year, keel::i32 month) noexcept {
        switch (month) {
            case 1: case 3: case 5: case 7: case 8: case 10: case 12: return 31;
            case 4: case 6: case 9: case 11: return 30;
            case 2: return IsLeapYear(year) ? 29 : 28;
            default: return 30;
        }
    }

    /// Returns day of week in [0..6] where 0 = Sunday .. 6 = Saturday.
    [[nodiscard]] static constexpr keel::i32 DayOfWeek(keel::i32 year, keel::i32 month, keel::i32 day) noexcept {
        constexpr keel::i32 t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
        keel::i32 y = year;
        const keel::i32 m = keel::clamp(month, 1, 12);
        if (m < 3) {
            y -= 1;
        }
        const keel::i32 dow = (y + y / 4 - y / 100 + y / 400 + t[m - 1] + day) % 7;
        return (dow < 0) ? (dow + 7) : dow;
    }

    /// Formats a DateTime value according to mode and seconds visibility.
    [[nodiscard]] static keel::FixedString<32> FormatDateTime(
        const DateTime& dt,
        DatePickerMode mode,
        bool showSeconds = false
    );
};

} // namespace detail

inline Response DatePicker(
    Context& ui,
    DateTime& value,
    DatePickerOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    return detail::DatePicker::Run(ui, value, opts, loc);
}

} // namespace guikit
