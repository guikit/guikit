#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <source_location>
#include <string>
#include "guikit/core/Control.hpp"
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

class Context;
class ICanvas;
struct Node;

/// Structured calendar date and time value for DatePicker.
struct DateTime {
    int year{2026};
    int month{10};  ///< Month [1..12]
    int day{20};    ///< Day [1..31]
    int hour{14};   ///< Hour [0..23]
    int minute{30}; ///< Minute [0..59]
    int second{0};  ///< Second [0..59]

    constexpr bool operator==(const DateTime&) const noexcept = default;
};

/// Display and editing mode for DatePicker widget.
enum class DatePickerMode : uint8_t {
    Date,     ///< YYYY-MM-DD
    DateTime, ///< YYYY-MM-DD HH:MM(:SS)
    Time      ///< HH:MM(:SS)
};

/// Configuration options for DatePicker control widget.
struct DatePickerOptions {
    DatePickerMode mode{DatePickerMode::Date};
    bool showSeconds{false};
    bool showTodayButton{true};
    bool firstDayMonday{true};
    StateColor color{};
    StretchMode stretch{StretchMode::Compact};
    Align align{Align::Start};
    std::optional<float> width{std::nullopt};
    std::optional<float> height{std::nullopt};
    StateColor background{StateColor::Surface()};
    Border border{};
    UiText tooltip{};
    bool disabled{false};
    const UiTheme* theme{nullptr};
    std::optional<float> scale{std::nullopt};
    std::optional<bool> debugBounds{std::nullopt};
    std::optional<bool> inspectMode{std::nullopt};
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
class DatePicker {
public:
    /// Primary immediate mode entry point for DatePicker.
    [[nodiscard]] static Response Run(
        Context& ctx,
        DateTime& value,
        const DatePickerOptions& opts = {},
        std::source_location loc = std::source_location::current()
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
    [[nodiscard]] static constexpr bool IsLeapYear(int year) noexcept {
        return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    }

    /// Returns the number of days [28..31] in the given month [1..12] of year.
    [[nodiscard]] static constexpr int DaysInMonth(int year, int month) noexcept {
        switch (month) {
            case 1: case 3: case 5: case 7: case 8: case 10: case 12: return 31;
            case 4: case 6: case 9: case 11: return 30;
            case 2: return IsLeapYear(year) ? 29 : 28;
            default: return 30;
        }
    }

    /// Returns day of week in [0..6] where 0 = Sunday .. 6 = Saturday.
    [[nodiscard]] static constexpr int DayOfWeek(int year, int month, int day) noexcept {
        constexpr int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
        int y = year;
        const int m = std::clamp(month, 1, 12);
        if (m < 3) {
            y -= 1;
        }
        const int dow = (y + y / 4 - y / 100 + y / 400 + t[m - 1] + day) % 7;
        return (dow < 0) ? (dow + 7) : dow;
    }

    /// Formats a DateTime value according to mode and seconds visibility.
    [[nodiscard]] static std::string FormatDateTime(
        const DateTime& dt,
        DatePickerMode mode,
        bool showSeconds = false
    );
};

} // namespace guikit
