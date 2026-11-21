#include "guikit/widgets/date_picker.hpp"
#include "keel/math.h"
#include "keel/static_array.h"
#include "keel/time.h"
#include "guikit/context.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"
#include "guikit/graphics/icon.hpp"

namespace guikit {

DateTime DateTime::Today() noexcept {
    const keel::CalendarTime now = keel::local_time();
    return DateTime{
        .year = now.year,
        .month = now.month,
        .day = now.day,
        .hour = now.hour,
        .minute = now.minute,
        .second = now.second
    };
}

namespace {

template <typename Pal>
constexpr Color ResolveThemePrimary(const Pal& pal) noexcept {
    if constexpr (requires { pal.primary; }) {
        return pal.primary;
    } else {
        return pal.accent;
    }
}

Color ResolveAccentColor(const DatePickerOptions& opts, const UiTheme& theme, bool hovered = false, bool active = false, bool disabled = false) noexcept {
    const Color fallback = ResolveThemePrimary(theme.palette);
    if (opts.color.HasAny()) {
        return theme.ResolveColor(opts.color, hovered, active, disabled, fallback);
    }
    return disabled ? fallback.WithAlphaF(0.40f) : fallback;
}

constexpr keel::i32 CompareDateOnly(keel::i32 y1, keel::i32 m1, keel::i32 d1, keel::i32 y2, keel::i32 m2, keel::i32 d2) noexcept {
    if (y1 != y2) return (y1 < y2) ? -1 : 1;
    if (m1 != m2) return (m1 < m2) ? -1 : 1;
    if (d1 != d2) return (d1 < d2) ? -1 : 1;
    return 0;
}

constexpr keel::i32 CompareDateTimeByMode(const DateTime& a, const DateTime& b, DatePickerMode mode) noexcept {
    if (mode == DatePickerMode::Date) {
        return CompareDateOnly(a.year, a.month, a.day, b.year, b.month, b.day);
    }
    if (mode == DatePickerMode::Time) {
        if (a.hour != b.hour) return (a.hour < b.hour) ? -1 : 1;
        if (a.minute != b.minute) return (a.minute < b.minute) ? -1 : 1;
        if (a.second != b.second) return (a.second < b.second) ? -1 : 1;
        return 0;
    }
    const keel::i32 dc = CompareDateOnly(a.year, a.month, a.day, b.year, b.month, b.day);
    if (dc != 0) return dc;
    if (a.hour != b.hour) return (a.hour < b.hour) ? -1 : 1;
    if (a.minute != b.minute) return (a.minute < b.minute) ? -1 : 1;
    if (a.second != b.second) return (a.second < b.second) ? -1 : 1;
    return 0;
}

constexpr bool IsDayCellDisabled(keel::i32 year, keel::i32 month, keel::i32 day, const DatePickerOptions& opts) noexcept {
    if (opts.minDate.has_value() &&
        CompareDateOnly(year, month, day, opts.minDate->year, opts.minDate->month, opts.minDate->day) < 0) {
        return true;
    }
    if (opts.maxDate.has_value() &&
        CompareDateOnly(year, month, day, opts.maxDate->year, opts.maxDate->month, opts.maxDate->day) > 0) {
        return true;
    }
    return false;
}

void ClampDateTimeToBounds(DateTime& dt, const DatePickerOptions& opts) noexcept {
    if (opts.minDate.has_value() && CompareDateTimeByMode(dt, *opts.minDate, opts.mode) < 0) {
        if (opts.mode == DatePickerMode::Date) {
            dt.year = opts.minDate->year;
            dt.month = opts.minDate->month;
            dt.day = opts.minDate->day;
        } else if (opts.mode == DatePickerMode::Time) {
            dt.hour = opts.minDate->hour;
            dt.minute = opts.minDate->minute;
            dt.second = opts.minDate->second;
        } else {
            dt = *opts.minDate;
        }
    }
    if (opts.maxDate.has_value() && CompareDateTimeByMode(dt, *opts.maxDate, opts.mode) > 0) {
        if (opts.mode == DatePickerMode::Date) {
            dt.year = opts.maxDate->year;
            dt.month = opts.maxDate->month;
            dt.day = opts.maxDate->day;
        } else if (opts.mode == DatePickerMode::Time) {
            dt.hour = opts.maxDate->hour;
            dt.minute = opts.maxDate->minute;
            dt.second = opts.maxDate->second;
        } else {
            dt = *opts.maxDate;
        }
    }
}

ControlSpec EffectiveDatePickerControl(const DatePickerOptions& opts, UiSize size, const UiTheme& theme) noexcept {
    const SelectMetrics& m = theme.GetSelect(size);
    return Control::FromOptions(
        opts,
        SizeMode::Stretch,
        SizeMode::Fit,
        ElementLayout::Stacked,
        m.labelGap,
        m.descGap,
        m.paddingX
    );
}

constexpr keel::StringRef MonthName(keel::i32 month, keel::ArrayRef<keel::StringRef> customMonths = {}) noexcept {
    if (customMonths.size() >= 12 && month >= 1 && month <= 12 && !customMonths[keel::as<keel::usize>(month - 1)].empty()) {
        return customMonths[keel::as<keel::usize>(month - 1)];
    }
    switch (month) {
        case 1:  return "January";
        case 2:  return "February";
        case 3:  return "March";
        case 4:  return "April";
        case 5:  return "May";
        case 6:  return "June";
        case 7:  return "July";
        case 8:  return "August";
        case 9:  return "September";
        case 10: return "October";
        case 11: return "November";
        case 12: return "December";
        default: return "January";
    }
}

constexpr keel::StringRef ResolveTodayLabel(const DatePickerOptions& opts) noexcept {
    if (opts.mode == DatePickerMode::Date) {
        return opts.labels.today.empty() ? keel::StringRef("Today") : opts.labels.today;
    }
    return opts.labels.now.empty() ? keel::StringRef("Now") : opts.labels.now;
}

constexpr keel::StringRef ResolveDoneLabel(const DatePickerOptions& opts) noexcept {
    return opts.labels.done.empty() ? keel::StringRef("Done") : opts.labels.done;
}

constexpr keel::StringRef ResolveTimeSuffix(keel::usize unitIdx, const DatePickerOptions& opts) noexcept {
    if (unitIdx < opts.labels.timeSuffixes.size() && !opts.labels.timeSuffixes[unitIdx].empty()) {
        return opts.labels.timeSuffixes[unitIdx];
    }
    switch (unitIdx) {
        case 0:  return "h";
        case 1:  return "m";
        case 2:  return "s";
        default: return {};
    }
}

void StepMonth(keel::i32& ioYear, keel::i32& ioMonth, keel::i32 deltaMonths) noexcept {
    keel::i32 total = (ioYear * 12 + (ioMonth - 1)) + deltaMonths;
    ioYear = total / 12;
    keel::i32 m = total % 12;
    if (m < 0) {
        m += 12;
        ioYear -= 1;
    }
    ioMonth = m + 1;
}

struct DayCellEntry {
    keel::i32 year{2026};
    keel::i32 month{10};
    keel::i32 day{1};
    bool isCurrentMonth{true};
};

keel::StaticArray<DayCellEntry, 42> BuildCalendarGrid(keel::i32 viewYear, keel::i32 viewMonth, bool firstDayMonday) noexcept {
    keel::StaticArray<DayCellEntry, 42> cells{};
    const keel::i32 clampedMonth = keel::clamp(viewMonth, 1, 12);
    const keel::i32 firstDow = DatePicker::DayOfWeek(viewYear, clampedMonth, 1); // 0 = Sun .. 6 = Sat
    const keel::i32 leading = firstDayMonday ? ((firstDow + 6) % 7) : firstDow;
    const keel::i32 daysInCurr = DatePicker::DaysInMonth(viewYear, clampedMonth);

    keel::i32 prevYear = viewYear;
    keel::i32 prevMonth = clampedMonth;
    StepMonth(prevYear, prevMonth, -1);
    const keel::i32 daysInPrev = DatePicker::DaysInMonth(prevYear, prevMonth);

    keel::i32 nextYear = viewYear;
    keel::i32 nextMonth = clampedMonth;
    StepMonth(nextYear, nextMonth, +1);

    for (keel::i32 i = 0; i < 42; ++i) {
        if (i < leading) {
            cells[keel::as<keel::usize>(i)] = DayCellEntry{
                .year = prevYear,
                .month = prevMonth,
                .day = daysInPrev - leading + 1 + i,
                .isCurrentMonth = false
            };
        } else if (i < leading + daysInCurr) {
            cells[keel::as<keel::usize>(i)] = DayCellEntry{
                .year = viewYear,
                .month = clampedMonth,
                .day = i - leading + 1,
                .isCurrentMonth = true
            };
        } else {
            cells[keel::as<keel::usize>(i)] = DayCellEntry{
                .year = nextYear,
                .month = nextMonth,
                .day = i - (leading + daysInCurr) + 1,
                .isCurrentMonth = false
            };
        }
    }
    return cells;
}

void DrawCalendarIcon(ICanvas& canvas, Rect r, Color col) {
    if (r.w <= 0.0f || r.h <= 0.0f) return;
    const keel::f32 bodyY = r.y + r.h * 0.14f;
    const keel::f32 bodyH = r.h * 0.82f;
    const Rect bodyRect(r.x + r.w * 0.08f, bodyY, r.w * 0.84f, bodyH);
    canvas.DrawRoundRect(bodyRect, 2.5f, Color::Transparent(), col, 1.4f);

    // Top rings
    const keel::f32 ringTop = r.y + r.h * 0.04f;
    const keel::f32 ringBot = bodyY + r.h * 0.10f;
    canvas.DrawLine({r.x + r.w * 0.30f, ringTop}, {r.x + r.w * 0.30f, ringBot}, col, 1.4f);
    canvas.DrawLine({r.x + r.w * 0.70f, ringTop}, {r.x + r.w * 0.70f, ringBot}, col, 1.4f);

    // Header divider line
    const keel::f32 sepY = bodyY + bodyH * 0.32f;
    canvas.DrawLine({bodyRect.x + 1.0f, sepY}, {bodyRect.Right() - 1.0f, sepY}, col, 1.2f);

    // Grid dots
    const keel::f32 dotSz = keel::max(1.6f, r.w * 0.11f);
    const keel::f32 row1Y = sepY + (bodyRect.Bottom() - sepY) * 0.30f - dotSz * 0.5f;
    const keel::f32 row2Y = sepY + (bodyRect.Bottom() - sepY) * 0.68f - dotSz * 0.5f;
    for (keel::i32 colIdx = 0; colIdx < 3; ++colIdx) {
        const keel::f32 cx = bodyRect.x + bodyRect.w * (0.25f + keel::as<keel::f32>(colIdx) * 0.25f) - dotSz * 0.5f;
        canvas.DrawRoundRect(Rect(cx, row1Y, dotSz, dotSz), 0.8f, col);
        canvas.DrawRoundRect(Rect(cx, row2Y, dotSz, dotSz), 0.8f, col);
    }
}

void DrawClockIcon(ICanvas& canvas, Rect r, Color col) {
    if (r.w <= 0.0f || r.h <= 0.0f) return;
    const keel::f32 sz = keel::min(r.w, r.h) * 0.86f;
    const Rect circleRect(r.x + (r.w - sz) * 0.5f, r.y + (r.h - sz) * 0.5f, sz, sz);
    canvas.DrawRoundRect(circleRect, sz * 0.5f, Color::Transparent(), col, 1.4f);

    const Vec2 center(circleRect.x + sz * 0.5f, circleRect.y + sz * 0.5f);
    canvas.DrawLine(center, {center.x, center.y - sz * 0.28f}, col, 1.4f);
    canvas.DrawLine(center, {center.x + sz * 0.22f, center.y + sz * 0.08f}, col, 1.4f);
}

struct DatePickerPopoverLayout {
    Rect popupRect{};
    bool hasCalendar{true};
    bool hasTime{false};

    // Calendar header navigation buttons & title
    Rect prevYearRect{};
    Rect prevMonthRect{};
    Rect titleRect{};
    Rect nextMonthRect{};
    Rect nextYearRect{};

    // Weekday & 6x7 day grid
    Rect weekdayRowRect{};
    Rect gridRect{};
    keel::f32 cellW{0.0f};
    keel::f32 cellH{0.0f};

    // Time selector bar
    Rect timeBarRect{};
    keel::i32 timeUnitCount{0};
    keel::StaticArray<Rect, 3> timeUnitRects{};
    keel::StaticArray<Rect, 3> timeMinusRects{};
    keel::StaticArray<Rect, 3> timeValueRects{};
    keel::StaticArray<Rect, 3> timePlusRects{};

    // Footer bar
    Rect footerRect{};
    Rect todayBtnRect{};
    Rect doneBtnRect{};
};

DatePickerPopoverLayout ComputePopoverLayout(
    Rect boxRect,
    const DatePickerOptions& opts,
    const UiTheme& theme,
    Rect vpRect,
    ICanvas* canvas
) noexcept {
    DatePickerPopoverLayout l{};
    l.hasCalendar = (opts.mode != DatePickerMode::Time);
    l.hasTime = (opts.mode == DatePickerMode::DateTime || opts.mode == DatePickerMode::Time);

    keel::f32 pad = 10.0f;
    keel::f32 headerH = l.hasCalendar ? 30.0f : 0.0f;
    keel::f32 weekdayH = l.hasCalendar ? 22.0f : 0.0f;
    keel::f32 cellH = l.hasCalendar ? 28.0f : 0.0f;
    keel::f32 timeH = l.hasTime ? 42.0f : 0.0f;
    keel::f32 footerH = 36.0f;
    keel::f32 headerGap = 4.0f;
    keel::f32 sectionGap = 6.0f;

    const keel::f32 smallFont = theme.ResolveFontSize(UiText{}, TextRole::LabelSmall, opts.size);
    const keel::StringRef todayLabel = ResolveTodayLabel(opts);
    const keel::StringRef doneLabel = ResolveDoneLabel(opts);
    constexpr keel::f32 kFooterBtnPadX = 12.0f;
    keel::f32 todayW = opts.showTodayButton
        ? keel::max(64.0f, keel::ceil(TextUtil::MeasureWidth(todayLabel, smallFont, canvas)) + kFooterBtnPadX * 2.0f)
        : 0.0f;
    keel::f32 doneW = keel::max(62.0f, keel::ceil(TextUtil::MeasureWidth(doneLabel, smallFont, canvas)) + kFooterBtnPadX * 2.0f);
    const keel::f32 footerMinInnerW = opts.showTodayButton ? (todayW + 8.0f + doneW) : doneW;

    keel::f32 popupW = l.hasCalendar
        ? 264.0f
        : (opts.showSeconds ? 258.0f : 196.0f);
    popupW = keel::max(popupW, footerMinInnerW + pad * 2.0f);
    if (vpRect.w > 40.0f && popupW > vpRect.w - 8.0f) {
        popupW = keel::max(140.0f, vpRect.w - 8.0f);
    }

    keel::f32 gridH = l.hasCalendar ? (cellH * 6.0f) : 0.0f;
    keel::f32 totalH = pad * 2.0f + footerH;
    if (l.hasCalendar) {
        totalH += headerH + headerGap + weekdayH + gridH + sectionGap;
    }
    if (l.hasTime) {
        totalH += timeH + sectionGap;
    }

    if (vpRect.h > 60.0f && totalH > vpRect.h - 12.0f) {
        const keel::f32 fitScale = keel::clamp((vpRect.h - 12.0f) / totalH, 0.56f, 1.0f);
        pad = keel::floor(pad * fitScale);
        headerH = keel::floor(headerH * fitScale);
        weekdayH = keel::floor(weekdayH * fitScale);
        cellH = keel::floor(cellH * fitScale);
        gridH = l.hasCalendar ? (cellH * 6.0f) : 0.0f;
        timeH = keel::floor(timeH * fitScale);
        footerH = keel::floor(footerH * fitScale);
        headerGap = keel::floor(headerGap * fitScale);
        sectionGap = keel::floor(sectionGap * fitScale);

        totalH = pad * 2.0f + footerH;
        if (l.hasCalendar) {
            totalH += headerH + headerGap + weekdayH + gridH + sectionGap;
        }
        if (l.hasTime) {
            totalH += timeH + sectionGap;
        }
    }

    keel::f32 popupX = boxRect.x;
    keel::f32 popupY = boxRect.y + boxRect.h + 4.0f;
    if (vpRect.w > 0.0f && vpRect.h > 0.0f) {
        const keel::f32 margin = 6.0f;
        const keel::f32 downY = boxRect.Bottom() + 4.0f;
        const keel::f32 upY = boxRect.y - 4.0f - totalH;
        const bool fitsDown = (downY + totalH <= vpRect.Bottom() - margin);
        const bool fitsUp = (upY >= vpRect.y + margin);

        if (fitsDown) {
            popupY = downY;
        } else if (fitsUp) {
            popupY = upY;
        } else {
            const keel::f32 rightX = boxRect.Right() + 6.0f;
            const keel::f32 leftX = boxRect.x - 6.0f - popupW;
            if (rightX + popupW <= vpRect.Right() - margin) {
                popupX = rightX;
                popupY = boxRect.y;
            } else if (leftX >= vpRect.x + margin) {
                popupX = leftX;
                popupY = boxRect.y;
            } else {
                const keel::f32 availDown = vpRect.Bottom() - downY - margin;
                const keel::f32 availUp = boxRect.y - 4.0f - (vpRect.y + margin);
                popupY = (availUp > availDown) ? upY : downY;
            }
        }
        popupX = keel::clamp(popupX, vpRect.x + 4.0f, keel::max(vpRect.x + 4.0f, vpRect.Right() - popupW - 4.0f));
        popupY = keel::clamp(popupY, vpRect.y + 4.0f, keel::max(vpRect.y + 4.0f, vpRect.Bottom() - totalH - 4.0f));
    }

    l.popupRect = Rect(popupX, popupY, popupW, totalH);

    keel::f32 curY = popupY + pad;
    const keel::f32 innerX = popupX + pad;
    const keel::f32 innerW = popupW - pad * 2.0f;

    if (l.hasCalendar) {
        const keel::f32 navBtnSz = keel::min(headerH, 26.0f);
        const keel::f32 navY = curY + (headerH - navBtnSz) * 0.5f;
        l.prevYearRect = Rect(innerX, navY, navBtnSz, navBtnSz);
        l.prevMonthRect = Rect(innerX + navBtnSz + 2.0f, navY, navBtnSz, navBtnSz);
        l.nextYearRect = Rect(innerX + innerW - navBtnSz, navY, navBtnSz, navBtnSz);
        l.nextMonthRect = Rect(innerX + innerW - (navBtnSz * 2.0f + 2.0f), navY, navBtnSz, navBtnSz);
        const keel::f32 titleX = l.prevMonthRect.Right() + 4.0f;
        l.titleRect = Rect(titleX, curY, keel::max(0.0f, l.nextMonthRect.x - 4.0f - titleX), headerH);
        curY += headerH + headerGap;

        l.weekdayRowRect = Rect(innerX, curY, innerW, weekdayH);
        curY += weekdayH;

        l.cellW = innerW / 7.0f;
        l.cellH = cellH;
        l.gridRect = Rect(innerX, curY, innerW, gridH);
        curY += gridH + sectionGap;
    }

    if (l.hasTime) {
        l.timeBarRect = Rect(innerX, curY, innerW, timeH);
        l.timeUnitCount = opts.showSeconds ? 3 : 2;
        const keel::f32 unitGap = 8.0f;
        const keel::f32 unitH = keel::min(keel::max(16.0f, timeH - 2.0f), 28.0f);
        const keel::f32 unitY = curY + (timeH - unitH) * 0.5f;
        const keel::f32 totalGap = unitGap * keel::as<keel::f32>(l.timeUnitCount - 1);
        const keel::f32 unitW = (innerW - totalGap) / keel::as<keel::f32>(l.timeUnitCount);
        const keel::f32 stepBtnW = 22.0f;

        for (keel::i32 u = 0; u < l.timeUnitCount; ++u) {
            const keel::f32 ux = innerX + keel::as<keel::f32>(u) * (unitW + unitGap);
            l.timeUnitRects[keel::as<keel::usize>(u)] = Rect(ux, unitY, unitW, unitH);
            l.timeMinusRects[keel::as<keel::usize>(u)] = Rect(ux, unitY, stepBtnW, unitH);
            l.timePlusRects[keel::as<keel::usize>(u)] = Rect(ux + unitW - stepBtnW, unitY, stepBtnW, unitH);
            l.timeValueRects[keel::as<keel::usize>(u)] = Rect(ux + stepBtnW, unitY, keel::max(0.0f, unitW - stepBtnW * 2.0f), unitH);
        }
        curY += timeH + sectionGap;
    }

    l.footerRect = Rect(innerX, popupY + totalH - pad - footerH, innerW, footerH);
    const keel::f32 btnH = keel::min(keel::max(16.0f, footerH - 2.0f), 26.0f);
    const keel::f32 btnY = l.footerRect.y + (footerH - btnH) * 0.5f + 1.0f;
    if (opts.showTodayButton) {
        const keel::f32 maxButtonsW = keel::max(0.0f, innerW - 6.0f);
        if (todayW + doneW > maxButtonsW && maxButtonsW > 0.0f) {
            const keel::f32 ratio = maxButtonsW / (todayW + doneW);
            todayW = keel::floor(todayW * ratio);
            doneW = keel::max(24.0f, maxButtonsW - todayW);
        }
        l.todayBtnRect = Rect(innerX, btnY, todayW, btnH);
    } else {
        doneW = keel::min(doneW, innerW);
    }
    l.doneBtnRect = Rect(innerX + innerW - doneW, btnY, doneW, btnH);

    return l;
}

struct DatePickerPopupRenderData {
    DatePickerPopoverLayout layout{};
    DateTime value{};
    keel::i32 viewYear{2026};
    keel::i32 viewMonth{10};
    DatePickerOptions opts{};
    UiSize size{UiSize::Md};
    UiTheme theme{};
    keel::i32 hoveredNavBtn{-1};
    keel::i32 hoveredDayCell{-1};
    keel::i32 hoveredTimeBtn{-1};
    bool todayHovered{false};
    bool doneHovered{false};
};

void RenderDatePickerPopover(ICanvas& canvas, const DatePickerPopupRenderData& d) {
    const Rect& popupRect = d.layout.popupRect;
    if (popupRect.w <= 0.0f || popupRect.h <= 0.0f) return;

    const SelectMetrics& m = d.theme.GetSelect(d.size);
    const auto& pal = d.theme.palette;
    const Color accentCol = ResolveAccentColor(d.opts, d.theme);

    // Outer subtle drop shadow and elevated card surface
    canvas.DrawRoundRect(Rect(popupRect.x, popupRect.y + 2.0f, popupRect.w, popupRect.h), m.cornerRadius + 2.0f, Color(0, 0, 0, 56));
    canvas.DrawRoundRect(popupRect, m.cornerRadius + 1.0f, pal.surfaceElevated, pal.border, 1.0f);

    const keel::f32 bodyFont = d.theme.ResolveFontSize(UiText{}, TextRole::LabelMedium, d.size);
    const keel::f32 smallFont = d.theme.ResolveFontSize(UiText{}, TextRole::LabelSmall, d.size);

    if (d.layout.hasCalendar) {
        // Navigation buttons: «, ‹, ›, »
        const keel::StaticArray<Rect, 4> navRects = {
            d.layout.prevYearRect,
            d.layout.prevMonthRect,
            d.layout.nextMonthRect,
            d.layout.nextYearRect
        };
        const keel::StaticArray<keel::StringRef, 4> navLabels = {"\xC2\xAB", "\xE2\x80\xB9", "\xE2\x80\xBA", "\xC2\xBB"};
        for (keel::i32 i = 0; i < 4; ++i) {
            const bool hov = (d.hoveredNavBtn == i);
            if (hov) {
                canvas.DrawRoundRect(navRects[keel::as<keel::usize>(i)], 4.0f, pal.surfaceHover);
            }
            const Color navCol = hov ? pal.textPrimary : pal.textSecondary;
            canvas.DrawTextBox(navLabels[keel::as<keel::usize>(i)], navRects[keel::as<keel::usize>(i)], bodyFont * 1.15f, navCol, 0.5f, 0.45f);
        }

        // Month & year title ("October 2026")
        char titleBuf[96];
        const keel::StringRef mName = MonthName(d.viewMonth, d.opts.labels.monthNames);
        const auto titleStr = keel::format_buf(titleBuf, "%s %04d", mName, d.viewYear);
        canvas.DrawTextBox(titleStr, d.layout.titleRect, bodyFont, pal.textPrimary, 0.5f, 0.5f);

        // Weekday column headers (indexed 0 = Sunday .. 6 = Saturday)
        static constexpr keel::StaticArray<keel::StringRef, 7> kDefaultWeekdaysSun = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
        for (keel::i32 col = 0; col < 7; ++col) {
            const keel::i32 dow = (col + (d.opts.firstDayMonday ? 1 : 0)) % 7;
            const keel::StringRef wLabel = (d.opts.labels.weekdayNames.size() >= 7 && !d.opts.labels.weekdayNames[keel::as<keel::usize>(dow)].empty())
                ? d.opts.labels.weekdayNames[keel::as<keel::usize>(dow)]
                : kDefaultWeekdaysSun[keel::as<keel::usize>(dow)];
            const Rect wRect(
                d.layout.weekdayRowRect.x + keel::as<keel::f32>(col) * d.layout.cellW,
                d.layout.weekdayRowRect.y,
                d.layout.cellW,
                d.layout.weekdayRowRect.h
            );
            canvas.DrawTextBox(wLabel, wRect, smallFont, pal.textMuted, 0.5f, 0.5f);
        }

        // 6x7 day grid (42 cells)
        const DateTime todayRef = d.opts.todayOverride.value_or(DateTime::Today());
        const auto cells = BuildCalendarGrid(d.viewYear, d.viewMonth, d.opts.firstDayMonday);
        for (keel::i32 idx = 0; idx < 42; ++idx) {
            const keel::i32 row = idx / 7;
            const keel::i32 col = idx % 7;
            const Rect cellRect(
                d.layout.gridRect.x + keel::as<keel::f32>(col) * d.layout.cellW,
                d.layout.gridRect.y + keel::as<keel::f32>(row) * d.layout.cellH,
                d.layout.cellW,
                d.layout.cellH
            );
            const Rect pillRect(
                cellRect.x + 2.0f,
                cellRect.y + 1.5f,
                keel::max(0.0f, cellRect.w - 4.0f),
                keel::max(0.0f, cellRect.h - 3.0f)
            );

            const DayCellEntry& cell = cells[keel::as<keel::usize>(idx)];
            const bool cellDisabled = IsDayCellDisabled(cell.year, cell.month, cell.day, d.opts);
            const bool isSelected = (cell.year == d.value.year && cell.month == d.value.month && cell.day == d.value.day);
            const bool isToday = (cell.year == todayRef.year && cell.month == todayRef.month && cell.day == todayRef.day);
            const bool isHovered = !cellDisabled && (d.hoveredDayCell == idx);

            if (isSelected) {
                canvas.DrawRoundRect(pillRect, 5.0f, cellDisabled ? accentCol.WithAlphaF(0.45f) : accentCol);
            } else if (isHovered) {
                canvas.DrawRoundRect(pillRect, 5.0f, pal.surfaceHover);
                if (isToday) {
                    canvas.DrawRoundRect(pillRect, 5.0f, Color::Transparent(), accentCol.WithAlphaF(0.65f), 1.0f);
                }
            } else if (isToday) {
                canvas.DrawRoundRect(pillRect, 5.0f, accentCol.WithAlphaF(0.10f), accentCol.WithAlphaF(0.55f), 1.0f);
            }

            Color dayCol = pal.textPrimary;
            if (isSelected) {
                dayCol = Color(255, 255, 255, 255);
            } else if (cellDisabled) {
                dayCol = pal.textMuted.WithAlphaF(0.32f);
            } else if (!cell.isCurrentMonth) {
                dayCol = pal.textMuted.WithAlphaF(0.55f);
            } else if (isToday) {
                dayCol = accentCol;
            }

            char dayBuf[16];
            const auto dayStr = keel::format_buf(dayBuf, "%d", cell.day);
            canvas.DrawTextBox(dayStr, cellRect, smallFont, dayCol, 0.5f, 0.5f);
        }
    }

    if (d.layout.hasTime) {
        if (d.layout.hasCalendar) {
            const keel::f32 sepY = d.layout.timeBarRect.y - 2.0f;
            canvas.DrawLine(
                {d.layout.timeBarRect.x, sepY},
                {d.layout.timeBarRect.Right(), sepY},
                pal.borderSubtle,
                1.0f
            );
        }

        const keel::StaticArray<keel::i32, 3> vals = {
            keel::clamp(d.value.hour, 0, 23),
            keel::clamp(d.value.minute, 0, 59),
            keel::clamp(d.value.second, 0, 59)
        };
        const keel::StaticArray<keel::StringRef, 3> suffixes = {
            ResolveTimeSuffix(0, d.opts),
            ResolveTimeSuffix(1, d.opts),
            ResolveTimeSuffix(2, d.opts)
        };

        for (keel::i32 u = 0; u < d.layout.timeUnitCount; ++u) {
            const keel::usize su = keel::as<keel::usize>(u);
            const Rect& unitRect = d.layout.timeUnitRects[su];
            const Rect& minusRect = d.layout.timeMinusRects[su];
            const Rect& valRect = d.layout.timeValueRects[su];
            const Rect& plusRect = d.layout.timePlusRects[su];

            canvas.DrawRoundRect(unitRect, 5.0f, pal.inputBackground, pal.inputBorder, 1.0f);

            const bool minusHov = (d.hoveredTimeBtn == u * 2);
            const bool plusHov = (d.hoveredTimeBtn == u * 2 + 1);
            if (minusHov) {
                canvas.DrawRoundRect(minusRect, 4.0f, pal.surfaceHover);
            }
            if (plusHov) {
                canvas.DrawRoundRect(plusRect, 4.0f, pal.surfaceHover);
            }

            canvas.DrawTextBox("-", minusRect, bodyFont, minusHov ? pal.textPrimary : pal.textSecondary, 0.5f, 0.5f);
            canvas.DrawTextBox("+", plusRect, bodyFont, plusHov ? pal.textPrimary : pal.textSecondary, 0.5f, 0.5f);

            char valBuf[16];
            const auto valStr = keel::format_buf(valBuf, "%02d%s", vals[su], suffixes[su]);
            canvas.DrawTextBox(valStr, valRect, smallFont, pal.textPrimary, 0.5f, 0.5f);

            if (u + 1 < d.layout.timeUnitCount) {
                const keel::f32 colonX = unitRect.Right() + (d.layout.timeUnitRects[su + 1].x - unitRect.Right()) * 0.5f;
                const Rect colonRect(colonX - 4.0f, unitRect.y, 8.0f, unitRect.h);
                canvas.DrawTextBox(":", colonRect, bodyFont, pal.textSecondary, 0.5f, 0.5f);
            }
        }
    }

    // Footer divider & buttons
    canvas.DrawLine(
        {d.layout.footerRect.x, d.layout.footerRect.y},
        {d.layout.footerRect.Right(), d.layout.footerRect.y},
        pal.borderSubtle,
        1.0f
    );

    if (d.opts.showTodayButton) {
        const Color todayBg = d.todayHovered ? accentCol.WithAlphaF(0.20f) : accentCol.WithAlphaF(0.10f);
        const Color todayBorder = accentCol.WithAlphaF(0.35f);
        canvas.DrawRoundRect(d.layout.todayBtnRect, 4.0f, todayBg, todayBorder, 1.0f);
        canvas.DrawTextBox(ResolveTodayLabel(d.opts), d.layout.todayBtnRect, smallFont, accentCol, 0.5f, 0.5f);
    }

    const Color doneBg = d.doneHovered ? accentCol.WithAlphaF(0.88f) : accentCol;
    canvas.DrawRoundRect(d.layout.doneBtnRect, 4.0f, doneBg);
    canvas.DrawTextBox(ResolveDoneLabel(d.opts), d.layout.doneBtnRect, smallFont, Color(255, 255, 255, 255), 0.5f, 0.5f);
}

} // namespace

keel::FixedString<32> DatePicker::FormatDateTime(const DateTime& dt, DatePickerMode mode, bool showSeconds) {
    const keel::i32 month = keel::clamp(dt.month, 1, 12);
    const keel::i32 maxDay = DaysInMonth(dt.year, month);
    const keel::i32 day = keel::clamp(dt.day, 1, maxDay);
    const keel::i32 hour = keel::clamp(dt.hour, 0, 23);
    const keel::i32 minute = keel::clamp(dt.minute, 0, 59);
    const keel::i32 second = keel::clamp(dt.second, 0, 59);

    char buf[32];
    keel::StringRef res;
    switch (mode) {
        case DatePickerMode::Date:
            res = keel::format_buf(buf, "%04d-%02d-%02d", dt.year, month, day);
            break;
        case DatePickerMode::DateTime:
            if (showSeconds) {
                res = keel::format_buf(buf, "%04d-%02d-%02d %02d:%02d:%02d", dt.year, month, day, hour, minute, second);
            } else {
                res = keel::format_buf(buf, "%04d-%02d-%02d %02d:%02d", dt.year, month, day, hour, minute);
            }
            break;
        case DatePickerMode::Time:
            if (showSeconds) {
                res = keel::format_buf(buf, "%02d:%02d:%02d", hour, minute, second);
            } else {
                res = keel::format_buf(buf, "%02d:%02d", hour, minute);
            }
            break;
    }
    return keel::FixedString<32>(res);
}

Vec2 DatePicker::Measure(
    const DateTime& value,
    const DatePickerOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const SelectMetrics& m = theme.GetSelect(size);
    const ControlSpec ctrl = EffectiveDatePickerControl(opts, size, theme);

    const keel::FixedString<32> formatted = FormatDateTime(value, opts.mode, opts.showSeconds);
    const keel::f32 font = theme.ResolveFontSize(UiText{}, TextRole::LabelMedium, size);
    const keel::f32 textW = TextUtil::MeasureWidth(formatted, font, canvas);
    const keel::f32 minContentW = m.paddingX * 2.0f + m.iconSize * 2.0f + 22.0f + textW;

    keel::f32 boxW = keel::max(m.minWidth, minContentW);
    if (opts.minWidth.has_value()) {
        boxW = keel::max(*opts.minWidth, boxW);
    }
    if (ctrl.width.IsFixed()) {
        boxW = ctrl.width.Dp();
    }
    const keel::f32 boxH = ctrl.height.IsFixed() ? ctrl.height.Dp() : m.defaultHeight;
    return Control::Measure({boxW, boxH}, ctrl, theme);
}

Vec2 DatePicker::Measure(
    const DateTime& value,
    const DatePickerOptions& opts,
    UiSize size,
    ICanvas* canvas
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(value, opts, size, canvas, defaultTheme);
}

Response DatePicker::Run(
    Context& ctx,
    DateTime& value,
    const DatePickerOptions& opts,
    keel::SourceLocation loc
) {

    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size;
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const bool wasOpen = ctx.IsPopupOpen(id);

    ctx.RegisterFocusable(id, effectiveDisabled);

    Response res = ctx.Interact(id);
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        res.focused = false;
    } else if (res.clicked) {
        ctx.RequestFocus(id);
    }

    // Clamp incoming date/time fields into valid ranges
    value.month = keel::clamp(value.month, 1, 12);
    value.day = keel::clamp(value.day, 1, DaysInMonth(value.year, value.month));
    value.hour = keel::clamp(value.hour, 0, 23);
    value.minute = keel::clamp(value.minute, 0, 59);
    value.second = keel::clamp(value.second, 0, 59);
    ClampDateTimeToBounds(value, opts);

    const UiTheme theme = ctx.GetTheme();
    const SelectMetrics& m = theme.GetSelect(activeSize);
    ICanvas* canvas = ctx.GetCanvas();

    DatePickerOptions optsCopy = opts;
    if (optsCopy.width.IsAuto()) {
        optsCopy.width = AxisSize(Control::ResolveWidthMode(ctx, optsCopy.width, SizeMode::Stretch));
    }
    auto copyLabelList = [&](keel::ArrayRef<keel::StringRef> src) -> keel::ArrayRef<keel::StringRef> {
        if (src.empty()) return {};
        auto* arr = ctx.GetArena().alloc_array<keel::StringRef>(src.size());
        for (keel::usize i = 0; i < src.size(); ++i) {
            arr[i] = ctx.AllocString(src[i]);
        }
        return keel::ArrayRef<keel::StringRef>(arr, src.size());
    };
    optsCopy.labels.monthNames = copyLabelList(opts.labels.monthNames);
    optsCopy.labels.weekdayNames = copyLabelList(opts.labels.weekdayNames);
    optsCopy.labels.timeSuffixes = copyLabelList(opts.labels.timeSuffixes);
    optsCopy.labels.today = ctx.AllocString(opts.labels.today);
    optsCopy.labels.now = ctx.AllocString(opts.labels.now);
    optsCopy.labels.done = ctx.AllocString(opts.labels.done);

    const Vec2 intrinsicDims = DatePicker::Measure(value, optsCopy, activeSize, canvas, theme);

    const ControlSpec ctrl = EffectiveDatePickerControl(optsCopy, activeSize, theme);
    const Rect prevBounds = ctx.GetPrevFrameBounds(id);
    const Rect boxRect = (prevBounds.w > 0.0f && prevBounds.h > 0.0f)
        ? prevBounds
        : Rect({0.0f, 0.0f}, intrinsicDims);

    struct DatePickerState {
        keel::i32 viewYear{2026};
        keel::i32 viewMonth{10};
        keel::i32 syncedYear{0};
        keel::i32 syncedMonth{0};
        bool initialized{false};
    };
    auto& state = ctx.GetOrCreatePersistentState<DatePickerState>(id);
    if (!state.initialized || value.year != state.syncedYear || value.month != state.syncedMonth) {
        state.viewYear = value.year;
        state.viewMonth = value.month;
        state.syncedYear = value.year;
        state.syncedMonth = value.month;
        state.initialized = true;
    }

    const bool isFocused = ctx.IsFocused(id) && !effectiveDisabled;
    bool isOpen = wasOpen;

    if (!effectiveDisabled) {
        if (!wasOpen) {
            if (res.clicked || (isFocused && (ctx.ConsumeKey(Key::Space) || ctx.ConsumeKey(Key::Enter) || ctx.ConsumeKey(Key::Down)))) {
                isOpen = true;
                state.viewYear = value.year;
                state.viewMonth = value.month;
                ctx.RequestFocus(id);
            }
        } else {
            if (res.clicked) {
                ctx.ClosePopup();
                isOpen = false;
            }
        }
    }

    const DatePickerPopoverLayout popLayout = ComputePopoverLayout(boxRect, optsCopy, theme, ctx.GetPopupViewportRect(), canvas);
    if (isOpen) {
        ctx.OpenPopup(id, popLayout.popupRect);
        ctx.RegisterOverlayHitRegion(id, popLayout.popupRect);
    }

    keel::i32 hoveredNavBtn = -1;
    keel::i32 hoveredDayCell = -1;
    keel::i32 hoveredTimeBtn = -1;
    bool todayHovered = false;
    bool doneHovered = false;

    if (isOpen && wasOpen && !effectiveDisabled) {
        auto hitScope = ctx.ScopedOverlayHitCheck(id);
        const Vec2 mouse = ctx.GetMousePos();
        const bool mouseClicked = ctx.IsMouseClicked();

        if (popLayout.hasCalendar) {
            const keel::StaticArray<Rect, 4> navRects = {
                popLayout.prevYearRect,
                popLayout.prevMonthRect,
                popLayout.nextMonthRect,
                popLayout.nextYearRect
            };
            for (keel::i32 i = 0; i < 4; ++i) {
                if (navRects[keel::as<keel::usize>(i)].Contains(mouse)) {
                    hoveredNavBtn = i;
                    if (mouseClicked) {
                        if (i == 0) state.viewYear -= 1;
                        else if (i == 1) StepMonth(state.viewYear, state.viewMonth, -1);
                        else if (i == 2) StepMonth(state.viewYear, state.viewMonth, +1);
                        else if (i == 3) state.viewYear += 1;
                        ctx.ConsumeMouseClick();
                    }
                    break;
                }
            }

            if (popLayout.gridRect.Contains(mouse)) {
                const keel::i32 col = keel::clamp(keel::as<keel::i32>((mouse.x - popLayout.gridRect.x) / popLayout.cellW), 0, 6);
                const keel::i32 row = keel::clamp(keel::as<keel::i32>((mouse.y - popLayout.gridRect.y) / popLayout.cellH), 0, 5);
                const keel::i32 cellIdx = row * 7 + col;
                const auto cells = BuildCalendarGrid(state.viewYear, state.viewMonth, optsCopy.firstDayMonday);
                const DayCellEntry& chosen = cells[keel::as<keel::usize>(cellIdx)];
                const bool cellDisabled = IsDayCellDisabled(chosen.year, chosen.month, chosen.day, optsCopy);
                if (!cellDisabled) {
                    hoveredDayCell = cellIdx;
                }
                if (mouseClicked) {
                    if (!cellDisabled) {
                        DateTime candidate = value;
                        candidate.year = chosen.year;
                        candidate.month = chosen.month;
                        candidate.day = chosen.day;
                        ClampDateTimeToBounds(candidate, optsCopy);
                        if (value != candidate) {
                            value = candidate;
                            res.changed = true;
                        }
                        state.viewYear = value.year;
                        state.viewMonth = value.month;
                        state.syncedYear = value.year;
                        state.syncedMonth = value.month;
                    }
                    ctx.ConsumeMouseClick();
                }
            }
        }

        if (popLayout.hasTime) {
            const Vec2 wheel = ctx.GetMouseWheel();
            for (keel::i32 u = 0; u < popLayout.timeUnitCount; ++u) {
                const keel::usize su = keel::as<keel::usize>(u);
                if (popLayout.timeMinusRects[su].Contains(mouse)) {
                    hoveredTimeBtn = u * 2;
                    if (mouseClicked) {
                        const DateTime prev = value;
                        if (u == 0) value.hour = (value.hour + 23) % 24;
                        else if (u == 1) value.minute = (value.minute + 59) % 60;
                        else value.second = (value.second + 59) % 60;
                        ClampDateTimeToBounds(value, optsCopy);
                        if (value != prev) res.changed = true;
                        ctx.ConsumeMouseClick();
                    }
                } else if (popLayout.timePlusRects[su].Contains(mouse)) {
                    hoveredTimeBtn = u * 2 + 1;
                    if (mouseClicked) {
                        const DateTime prev = value;
                        if (u == 0) value.hour = (value.hour + 1) % 24;
                        else if (u == 1) value.minute = (value.minute + 1) % 60;
                        else value.second = (value.second + 1) % 60;
                        ClampDateTimeToBounds(value, optsCopy);
                        if (value != prev) res.changed = true;
                        ctx.ConsumeMouseClick();
                    }
                }

                if (wheel.y != 0.0f && popLayout.timeUnitRects[su].Contains(mouse)) {
                    const DateTime prev = value;
                    const keel::i32 step = (wheel.y > 0.0f) ? 1 : -1;
                    if (u == 0) value.hour = (value.hour + 24 + step) % 24;
                    else if (u == 1) value.minute = (value.minute + 60 + step) % 60;
                    else value.second = (value.second + 60 + step) % 60;
                    ClampDateTimeToBounds(value, optsCopy);
                    if (value != prev) res.changed = true;
                    ctx.ConsumeMouseWheel();
                }
            }
        }

        if (optsCopy.showTodayButton && popLayout.todayBtnRect.Contains(mouse)) {
            todayHovered = true;
            if (mouseClicked) {
                const DateTime todayVal = optsCopy.todayOverride.value_or(DateTime::Today());
                DateTime candidate = value;
                if (optsCopy.mode == DatePickerMode::Date) {
                    candidate.year = todayVal.year;
                    candidate.month = todayVal.month;
                    candidate.day = todayVal.day;
                } else if (optsCopy.mode == DatePickerMode::Time) {
                    candidate.hour = todayVal.hour;
                    candidate.minute = todayVal.minute;
                    candidate.second = todayVal.second;
                } else {
                    candidate = todayVal;
                }
                ClampDateTimeToBounds(candidate, optsCopy);
                if (value != candidate) {
                    value = candidate;
                    res.changed = true;
                }
                state.viewYear = value.year;
                state.viewMonth = value.month;
                state.syncedYear = value.year;
                state.syncedMonth = value.month;
                ctx.ConsumeMouseClick();
            }
        }

        if (popLayout.doneBtnRect.Contains(mouse)) {
            doneHovered = true;
            if (mouseClicked) {
                res.submitted = true;
                ctx.ClosePopup();
                isOpen = false;
                ctx.ConsumeMouseClick();
                ctx.RequestFocus(id);
            }
        }

        if (isOpen && ctx.ConsumeKey(Key::Escape)) {
            ctx.ClosePopup();
            isOpen = false;
            ctx.RequestFocus(id);
        } else if (isOpen && ctx.ConsumeKey(Key::Enter)) {
            res.submitted = true;
            ctx.ClosePopup();
            isOpen = false;
            ctx.RequestFocus(id);
        }
    }

    if (isOpen) {
        DatePickerPopupRenderData renderData{
            .layout = popLayout,
            .value = value,
            .viewYear = state.viewYear,
            .viewMonth = state.viewMonth,
            .opts = optsCopy,
            .size = activeSize,
            .theme = theme,
            .hoveredNavBtn = hoveredNavBtn,
            .hoveredDayCell = hoveredDayCell,
            .hoveredTimeBtn = hoveredTimeBtn,
            .todayHovered = todayHovered,
            .doneHovered = doneHovered
        };

        ctx.SetOpenPopupVisualHash(detail::HashArgs(
            popLayout.popupRect.x,
            popLayout.popupRect.y,
            popLayout.popupRect.w,
            popLayout.popupRect.h,
            value.year,
            value.month,
            value.day,
            value.hour,
            value.minute,
            value.second,
            state.viewYear,
            state.viewMonth,
            keel::as<keel::u8>(optsCopy.mode),
            optsCopy.showSeconds,
            optsCopy.showTodayButton,
            optsCopy.firstDayMonday,
            hoveredNavBtn,
            hoveredDayCell,
            hoveredTimeBtn,
            todayHovered,
            doneHovered,
            detail::HashArgs(ResolveTodayLabel(optsCopy), ResolveDoneLabel(optsCopy))
        ));

        ctx.RegisterOverlay([renderData](ICanvas& overlayCanvas) {
            RenderDatePickerPopover(overlayCanvas, renderData);
        });
    }

    if (res.hovered && !isOpen && !opts.tooltip.empty()) {
        ctx.SetTooltip(opts.tooltip, prevBounds);
    }

    const keel::f32 btnSize = m.iconSize + 4.0f;
    const Rect chevronRect(
        boxRect.x + boxRect.w - m.paddingX - m.iconSize - 2.0f,
        boxRect.y + (boxRect.h - btnSize) * 0.5f,
        btnSize,
        btnSize
    );
    const bool chevronHovered = res.hovered && !effectiveDisabled && chevronRect.Contains(ctx.GetMousePos());
    const bool finalFocused = isOpen || ctx.IsFocused(id);

    const keel::FixedString<32> formattedStr = FormatDateTime(value, optsCopy.mode, optsCopy.showSeconds);

    Node* node = ctx.CreateNode(NodeType::DatePicker, id);
    optsCopy.tooltip = ctx.AllocText(optsCopy.tooltip);

    auto* data = ctx.AllocPayload<DatePickerData>();
    data->value = value;
    data->opts = optsCopy;
    data->formattedText = ctx.AllocText(UiText{formattedStr});
    data->isOpen = isOpen;
    data->chevronHovered = chevronHovered;

    node->text = data->formattedText.text;
    node->SetPayload(data);
    node->visualHash = detail::HashArgs(
        keel::as<keel::u8>(optsCopy.mode),
        optsCopy.showSeconds,
        optsCopy.showTodayButton,
        optsCopy.firstDayMonday,
        optsCopy.width,
        optsCopy.height,
        optsCopy.minWidth,
        optsCopy.maxWidth,
        optsCopy.minHeight,
        optsCopy.maxHeight,
        optsCopy.color,
        optsCopy.background,
        optsCopy.border,
        data->formattedText,
        data->isOpen,
        data->chevronHovered
    );
    node->measureFn = [](const Node* n, keel::f32 /*availableWidth*/, ICanvas* c, const UiTheme& t) noexcept -> Vec2 {
        const auto* d = n->As<DatePickerData>();
        return d
            ? DatePicker::Measure(d->value, d->opts, n->uiSize, c, t)
            : DatePicker::Measure(DateTime{}, DatePickerOptions{}, n->uiSize, c, t);
    };
    node->renderFn = &DatePicker::Render;
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->active = isOpen;
    node->focused = isOpen || ctx.IsFocusVisible(id);
    res.focused = finalFocused;

    Control::SetupNode(ctx, node, ctrl);

    return res;
}

void DatePicker::Render(const Node* node, ICanvas& canvas, const UiTheme& theme) {
    if (!node) return;
    const Rect bounds = node->computedRect;
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    const auto* d = node->As<DatePickerData>();
    const DatePickerOptions opts = d ? d->opts : DatePickerOptions{};
    const UiSize size = node->uiSize;
    const bool hovered = node->hovered;
    const bool open = node->active || (d && d->isOpen);
    const bool disabled = node->disabled;
    const bool focused = node->focused;

    const SelectMetrics& m = theme.GetSelect(size);
    const auto& pal = theme.palette;
    const Color accentCol = ResolveAccentColor(opts, theme, hovered, open || focused, disabled);

    const bool hasBg = opts.background.IsBackgroundEnabled(true);
    Color boxBg = hasBg
        ? ((hovered && !disabled) ? pal.surface : pal.inputBackground)
        : Color::Transparent();
    Color boxBorder;
    keel::f32 strokeW = (opts.border.width > 0.0f) ? opts.border.width : m.strokeWidth;

    if ((open || focused) && !disabled) {
        boxBorder = opts.color.HasAny() ? accentCol : pal.focusRing;
        if (opts.border.width > 0.0f || m.strokeWidth > 0.0f) strokeW += 0.5f;
    } else if (hovered && !disabled) {
        boxBorder = pal.borderHover;
    } else {
        boxBorder = pal.inputBorder;
    }

    Color chevronCol = open ? accentCol : (((d && d->chevronHovered) && !disabled) ? pal.textPrimary : pal.textSecondary);
    Color leadIconCol = open ? accentCol : (disabled ? pal.textSecondary.WithAlphaF(0.40f) : accentCol);

    if (disabled) {
        boxBg = boxBg.WithAlphaF(0.40f);
        boxBorder = boxBorder.WithAlphaF(0.40f);
        chevronCol = chevronCol.WithAlphaF(0.40f);
    }

    if (hasBg && opts.background.HasAny()) {
        if (!opts.background.IsSurface()) {
            boxBg = theme.ResolveColor(opts.background, hovered, open || focused, disabled, boxBg);
        } else if (opts.background.HasAlphaScale()) {
            boxBg = boxBg.ScaleAlpha(opts.background.alphaScale);
        }
    }
    if (opts.border.color.HasAny()) {
        boxBorder = theme.ResolveColor(opts.border.color, hovered, open || focused, disabled, boxBorder);
    }

    const keel::f32 radius = opts.border.radius.has_value() ? *opts.border.radius : m.cornerRadius;
    canvas.DrawRoundRect(bounds, radius, boxBg, boxBorder, strokeW);

    // Left vector calendar / clock icon
    keel::f32 leftX = bounds.x + m.paddingX;
    const Rect iconRect(leftX, bounds.y + (bounds.h - m.iconSize) * 0.5f, m.iconSize, m.iconSize);
    if (opts.mode == DatePickerMode::Time) {
        DrawClockIcon(canvas, iconRect, leadIconCol);
    } else {
        DrawCalendarIcon(canvas, iconRect, leadIconCol);
    }
    leftX += m.iconSize + 8.0f;

    // Right chevron icon
    const keel::f32 rightX = bounds.x + bounds.w - m.paddingX - m.iconSize;
    const Rect chevronRect(rightX, bounds.y + (bounds.h - m.iconSize) * 0.5f, m.iconSize, m.iconSize);
    const Icon chevron = open ? Icon::ChevronUp() : Icon::ChevronDown();
    chevron.Draw(canvas, chevronRect, chevronCol);

    // Formatted date/time text
    const keel::f32 availW = keel::max(0.0f, (rightX - 6.0f) - leftX);
    const Rect textRect(leftX, bounds.y, availW, bounds.h);
    canvas.PushClip(textRect);

    const UiText displayTxt = d ? d->formattedText : UiText{"2026-10-20"};
    const keel::f32 font = theme.ResolveFontSize(displayTxt, TextRole::LabelMedium, size);
    const Color textCol = disabled ? pal.textPrimary.WithAlphaF(0.40f) : pal.textPrimary;
    TextUtil::DrawUiText(canvas, displayTxt, textRect, font, bounds.h, textCol, 0.0f, 0.5f, &theme);

    canvas.PopClip();
}

} // namespace guikit
