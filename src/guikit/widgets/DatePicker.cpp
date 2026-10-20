#include "guikit/widgets/DatePicker.hpp"
#include <algorithm>
#include <array>
#include <format>
#include <string_view>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"
#include "guikit/graphics/Icon.hpp"

namespace guikit {

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
        return opts.color.Resolve(hovered, active, disabled, fallback);
    }
    return disabled ? fallback.WithAlphaF(0.40f) : fallback;
}

ControlSpec EffectiveDatePickerControl(const DatePickerOptions& opts, UiSize size, const UiTheme& theme) noexcept {
    const SelectMetrics& m = theme.GetSelect(size);
    ControlSpec c{};
    c.width = opts.width;
    c.height = opts.height;
    c.align = opts.align;
    c.background = opts.background;
    c.border = opts.border;
    c.defaultLayout = ElementLayout::Stacked;
    c.defaultInlineOrder = InlineOrder::LabelFirst;
    c.labelGap = m.labelGap;
    c.descGap = m.descGap;
    c.inlineGap = m.paddingX;
    c.stretchable = true;
    c.stretchHeightWhenStandalone = false;
    c.hasIntrinsicBox = true;
    return c;
}

constexpr std::string_view MonthName(int month) noexcept {
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

void StepMonth(int& ioYear, int& ioMonth, int deltaMonths) noexcept {
    int total = (ioYear * 12 + (ioMonth - 1)) + deltaMonths;
    ioYear = total / 12;
    int m = total % 12;
    if (m < 0) {
        m += 12;
        ioYear -= 1;
    }
    ioMonth = m + 1;
}

struct DayCellEntry {
    int year{2026};
    int month{10};
    int day{1};
    bool isCurrentMonth{true};
};

std::array<DayCellEntry, 42> BuildCalendarGrid(int viewYear, int viewMonth, bool firstDayMonday) noexcept {
    std::array<DayCellEntry, 42> cells{};
    const int clampedMonth = std::clamp(viewMonth, 1, 12);
    const int firstDow = DatePicker::DayOfWeek(viewYear, clampedMonth, 1); // 0 = Sun .. 6 = Sat
    const int leading = firstDayMonday ? ((firstDow + 6) % 7) : firstDow;
    const int daysInCurr = DatePicker::DaysInMonth(viewYear, clampedMonth);

    int prevYear = viewYear;
    int prevMonth = clampedMonth;
    StepMonth(prevYear, prevMonth, -1);
    const int daysInPrev = DatePicker::DaysInMonth(prevYear, prevMonth);

    int nextYear = viewYear;
    int nextMonth = clampedMonth;
    StepMonth(nextYear, nextMonth, +1);

    for (int i = 0; i < 42; ++i) {
        if (i < leading) {
            cells[static_cast<size_t>(i)] = DayCellEntry{
                .year = prevYear,
                .month = prevMonth,
                .day = daysInPrev - leading + 1 + i,
                .isCurrentMonth = false
            };
        } else if (i < leading + daysInCurr) {
            cells[static_cast<size_t>(i)] = DayCellEntry{
                .year = viewYear,
                .month = clampedMonth,
                .day = i - leading + 1,
                .isCurrentMonth = true
            };
        } else {
            cells[static_cast<size_t>(i)] = DayCellEntry{
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
    const float bodyY = r.y + r.h * 0.14f;
    const float bodyH = r.h * 0.82f;
    const Rect bodyRect(r.x + r.w * 0.08f, bodyY, r.w * 0.84f, bodyH);
    canvas.DrawRoundRect(bodyRect, 2.5f, Color::Transparent(), col, 1.4f);

    // Top rings
    const float ringTop = r.y + r.h * 0.04f;
    const float ringBot = bodyY + r.h * 0.10f;
    canvas.DrawLine({r.x + r.w * 0.30f, ringTop}, {r.x + r.w * 0.30f, ringBot}, col, 1.4f);
    canvas.DrawLine({r.x + r.w * 0.70f, ringTop}, {r.x + r.w * 0.70f, ringBot}, col, 1.4f);

    // Header divider line
    const float sepY = bodyY + bodyH * 0.32f;
    canvas.DrawLine({bodyRect.x + 1.0f, sepY}, {bodyRect.Right() - 1.0f, sepY}, col, 1.2f);

    // Grid dots
    const float dotSz = std::max(1.6f, r.w * 0.11f);
    const float row1Y = sepY + (bodyRect.Bottom() - sepY) * 0.30f - dotSz * 0.5f;
    const float row2Y = sepY + (bodyRect.Bottom() - sepY) * 0.68f - dotSz * 0.5f;
    for (int colIdx = 0; colIdx < 3; ++colIdx) {
        const float cx = bodyRect.x + bodyRect.w * (0.25f + static_cast<float>(colIdx) * 0.25f) - dotSz * 0.5f;
        canvas.DrawRoundRect(Rect(cx, row1Y, dotSz, dotSz), 0.8f, col);
        canvas.DrawRoundRect(Rect(cx, row2Y, dotSz, dotSz), 0.8f, col);
    }
}

void DrawClockIcon(ICanvas& canvas, Rect r, Color col) {
    if (r.w <= 0.0f || r.h <= 0.0f) return;
    const float sz = std::min(r.w, r.h) * 0.86f;
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
    float cellW{0.0f};
    float cellH{0.0f};

    // Time selector bar
    Rect timeBarRect{};
    int timeUnitCount{0};
    std::array<Rect, 3> timeUnitRects{};
    std::array<Rect, 3> timeMinusRects{};
    std::array<Rect, 3> timeValueRects{};
    std::array<Rect, 3> timePlusRects{};

    // Footer bar
    Rect footerRect{};
    Rect todayBtnRect{};
    Rect doneBtnRect{};
};

DatePickerPopoverLayout ComputePopoverLayout(
    Rect boxRect,
    const DatePickerOptions& opts,
    const UiTheme& theme,
    Vec2 viewportSize
) noexcept {
    DatePickerPopoverLayout l{};
    l.hasCalendar = (opts.mode != DatePickerMode::Time);
    l.hasTime = (opts.mode == DatePickerMode::DateTime || opts.mode == DatePickerMode::Time);

    const float pad = theme.Scale(10.0f);
    const float headerH = l.hasCalendar ? theme.Scale(30.0f) : 0.0f;
    const float weekdayH = l.hasCalendar ? theme.Scale(22.0f) : 0.0f;
    const float cellH = l.hasCalendar ? theme.Scale(28.0f) : 0.0f;
    const float gridH = l.hasCalendar ? (cellH * 6.0f) : 0.0f;
    const float timeH = l.hasTime ? theme.Scale(42.0f) : 0.0f;
    const float footerH = theme.Scale(36.0f);

    const float popupW = l.hasCalendar
        ? theme.Scale(264.0f)
        : theme.Scale(opts.showSeconds ? 258.0f : 196.0f);

    float totalH = pad * 2.0f + footerH;
    if (l.hasCalendar) {
        totalH += headerH + theme.Scale(4.0f) + weekdayH + gridH + theme.Scale(6.0f);
    }
    if (l.hasTime) {
        totalH += timeH + theme.Scale(6.0f);
    }

    float popupX = boxRect.x;
    float popupY = boxRect.y + boxRect.h + 4.0f;
    if (viewportSize.y > 0.0f) {
        const float availDown = viewportSize.y - popupY - 8.0f;
        const float availUp = boxRect.y - 4.0f - 8.0f;
        if (popupY + totalH > viewportSize.y && availUp > availDown) {
            popupY = std::max(8.0f, boxRect.y - 4.0f - totalH);
        }
    }
    if (viewportSize.x > 0.0f) {
        popupX = std::clamp(popupX, 4.0f, std::max(4.0f, viewportSize.x - popupW - 4.0f));
    }

    l.popupRect = Rect(popupX, popupY, popupW, totalH);

    float curY = popupY + pad;
    const float innerX = popupX + pad;
    const float innerW = popupW - pad * 2.0f;

    if (l.hasCalendar) {
        const float navBtnSz = theme.Scale(26.0f);
        const float navY = curY + (headerH - navBtnSz) * 0.5f;
        l.prevYearRect = Rect(innerX, navY, navBtnSz, navBtnSz);
        l.prevMonthRect = Rect(innerX + navBtnSz + 2.0f, navY, navBtnSz, navBtnSz);
        l.nextYearRect = Rect(innerX + innerW - navBtnSz, navY, navBtnSz, navBtnSz);
        l.nextMonthRect = Rect(innerX + innerW - (navBtnSz * 2.0f + 2.0f), navY, navBtnSz, navBtnSz);
        const float titleX = l.prevMonthRect.Right() + 4.0f;
        l.titleRect = Rect(titleX, curY, std::max(0.0f, l.nextMonthRect.x - 4.0f - titleX), headerH);
        curY += headerH + theme.Scale(4.0f);

        l.weekdayRowRect = Rect(innerX, curY, innerW, weekdayH);
        curY += weekdayH;

        l.cellW = innerW / 7.0f;
        l.cellH = cellH;
        l.gridRect = Rect(innerX, curY, innerW, gridH);
        curY += gridH + theme.Scale(6.0f);
    }

    if (l.hasTime) {
        l.timeBarRect = Rect(innerX, curY, innerW, timeH);
        l.timeUnitCount = opts.showSeconds ? 3 : 2;
        const float unitGap = theme.Scale(8.0f);
        const float unitH = theme.Scale(28.0f);
        const float unitY = curY + (timeH - unitH) * 0.5f;
        const float totalGap = unitGap * static_cast<float>(l.timeUnitCount - 1);
        const float unitW = (innerW - totalGap) / static_cast<float>(l.timeUnitCount);
        const float stepBtnW = theme.Scale(22.0f);

        for (int u = 0; u < l.timeUnitCount; ++u) {
            const float ux = innerX + static_cast<float>(u) * (unitW + unitGap);
            l.timeUnitRects[static_cast<size_t>(u)] = Rect(ux, unitY, unitW, unitH);
            l.timeMinusRects[static_cast<size_t>(u)] = Rect(ux, unitY, stepBtnW, unitH);
            l.timePlusRects[static_cast<size_t>(u)] = Rect(ux + unitW - stepBtnW, unitY, stepBtnW, unitH);
            l.timeValueRects[static_cast<size_t>(u)] = Rect(ux + stepBtnW, unitY, std::max(0.0f, unitW - stepBtnW * 2.0f), unitH);
        }
        curY += timeH + theme.Scale(6.0f);
    }

    l.footerRect = Rect(innerX, popupY + totalH - pad - footerH, innerW, footerH);
    const float btnH = theme.Scale(26.0f);
    const float btnY = l.footerRect.y + (footerH - btnH) * 0.5f + 2.0f;
    if (opts.showTodayButton) {
        l.todayBtnRect = Rect(innerX, btnY, theme.Scale(64.0f), btnH);
    }
    const float doneW = theme.Scale(62.0f);
    l.doneBtnRect = Rect(innerX + innerW - doneW, btnY, doneW, btnH);

    return l;
}

struct DatePickerPopupRenderData {
    DatePickerPopoverLayout layout{};
    DateTime value{};
    int viewYear{2026};
    int viewMonth{10};
    DatePickerOptions opts{};
    UiSize size{UiSize::Md};
    UiTheme theme{};
    int hoveredNavBtn{-1};
    int hoveredDayCell{-1};
    int hoveredTimeBtn{-1};
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

    const float bodyFont = d.theme.ResolveFontSize(UiText{}, TextRole::LabelMedium, d.size);
    const float smallFont = d.theme.ResolveFontSize(UiText{}, TextRole::LabelSmall, d.size);

    if (d.layout.hasCalendar) {
        // Navigation buttons: «, ‹, ›, »
        const std::array<Rect, 4> navRects = {
            d.layout.prevYearRect,
            d.layout.prevMonthRect,
            d.layout.nextMonthRect,
            d.layout.nextYearRect
        };
        const std::array<std::string_view, 4> navLabels = {"\xC2\xAB", "\xE2\x80\xB9", "\xE2\x80\xBA", "\xC2\xBB"};
        for (int i = 0; i < 4; ++i) {
            const bool hov = (d.hoveredNavBtn == i);
            if (hov) {
                canvas.DrawRoundRect(navRects[static_cast<size_t>(i)], 4.0f, pal.surfaceHover);
            }
            const Color navCol = hov ? pal.textPrimary : pal.textSecondary;
            canvas.DrawTextBox(navLabels[static_cast<size_t>(i)], navRects[static_cast<size_t>(i)], bodyFont * 1.15f, navCol, 0.5f, 0.45f);
        }

        // Month & year title ("October 2026")
        const std::string titleStr = std::format("{} {:04d}", MonthName(d.viewMonth), d.viewYear);
        canvas.DrawTextBox(titleStr, d.layout.titleRect, bodyFont, pal.textPrimary, 0.5f, 0.5f);

        // Weekday column headers
        static constexpr std::array<std::string_view, 7> kWeekdaysMon = {"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"};
        static constexpr std::array<std::string_view, 7> kWeekdaysSun = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
        const auto& weekdays = d.opts.firstDayMonday ? kWeekdaysMon : kWeekdaysSun;
        for (int col = 0; col < 7; ++col) {
            const Rect wRect(
                d.layout.weekdayRowRect.x + static_cast<float>(col) * d.layout.cellW,
                d.layout.weekdayRowRect.y,
                d.layout.cellW,
                d.layout.weekdayRowRect.h
            );
            canvas.DrawTextBox(weekdays[static_cast<size_t>(col)], wRect, smallFont, pal.textMuted, 0.5f, 0.5f);
        }

        // 6x7 day grid (42 cells)
        const auto cells = BuildCalendarGrid(d.viewYear, d.viewMonth, d.opts.firstDayMonday);
        for (int idx = 0; idx < 42; ++idx) {
            const int row = idx / 7;
            const int col = idx % 7;
            const Rect cellRect(
                d.layout.gridRect.x + static_cast<float>(col) * d.layout.cellW,
                d.layout.gridRect.y + static_cast<float>(row) * d.layout.cellH,
                d.layout.cellW,
                d.layout.cellH
            );
            const Rect pillRect(
                cellRect.x + 2.0f,
                cellRect.y + 1.5f,
                std::max(0.0f, cellRect.w - 4.0f),
                std::max(0.0f, cellRect.h - 3.0f)
            );

            const DayCellEntry& cell = cells[static_cast<size_t>(idx)];
            const bool isSelected = (cell.year == d.value.year && cell.month == d.value.month && cell.day == d.value.day);
            const bool isToday = (cell.year == 2026 && cell.month == 10 && cell.day == 20);
            const bool isHovered = (d.hoveredDayCell == idx);

            if (isSelected) {
                canvas.DrawRoundRect(pillRect, 5.0f, accentCol);
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
            } else if (!cell.isCurrentMonth) {
                dayCol = pal.textMuted.WithAlphaF(0.55f);
            } else if (isToday) {
                dayCol = accentCol;
            }

            const std::string dayStr = std::format("{}", cell.day);
            canvas.DrawTextBox(dayStr, cellRect, smallFont, dayCol, 0.5f, 0.5f);
        }
    }

    if (d.layout.hasTime) {
        if (d.layout.hasCalendar) {
            const float sepY = d.layout.timeBarRect.y - d.theme.Scale(2.0f);
            canvas.DrawLine(
                {d.layout.timeBarRect.x, sepY},
                {d.layout.timeBarRect.Right(), sepY},
                pal.borderSubtle,
                1.0f
            );
        }

        const std::array<int, 3> vals = {
            std::clamp(d.value.hour, 0, 23),
            std::clamp(d.value.minute, 0, 59),
            std::clamp(d.value.second, 0, 59)
        };
        const std::array<std::string_view, 3> suffixes = {"h", "m", "s"};

        for (int u = 0; u < d.layout.timeUnitCount; ++u) {
            const size_t su = static_cast<size_t>(u);
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

            const std::string valStr = std::format("{:02d}{}", vals[su], suffixes[su]);
            canvas.DrawTextBox(valStr, valRect, smallFont, pal.textPrimary, 0.5f, 0.5f);

            if (u + 1 < d.layout.timeUnitCount) {
                const float colonX = unitRect.Right() + (d.layout.timeUnitRects[su + 1].x - unitRect.Right()) * 0.5f;
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
        const std::string_view todayLabel = (d.opts.mode == DatePickerMode::Date) ? "Today" : "Now";
        canvas.DrawTextBox(todayLabel, d.layout.todayBtnRect, smallFont, accentCol, 0.5f, 0.5f);
    }

    const Color doneBg = d.doneHovered ? accentCol.WithAlphaF(0.88f) : accentCol;
    canvas.DrawRoundRect(d.layout.doneBtnRect, 4.0f, doneBg);
    canvas.DrawTextBox("Done", d.layout.doneBtnRect, smallFont, Color(255, 255, 255, 255), 0.5f, 0.5f);
}

} // namespace

std::string DatePicker::FormatDateTime(const DateTime& dt, DatePickerMode mode, bool showSeconds) {
    const int month = std::clamp(dt.month, 1, 12);
    const int maxDay = DaysInMonth(dt.year, month);
    const int day = std::clamp(dt.day, 1, maxDay);
    const int hour = std::clamp(dt.hour, 0, 23);
    const int minute = std::clamp(dt.minute, 0, 59);
    const int second = std::clamp(dt.second, 0, 59);

    switch (mode) {
        case DatePickerMode::Date:
            return std::format("{:04d}-{:02d}-{:02d}", dt.year, month, day);
        case DatePickerMode::DateTime:
            if (showSeconds) {
                return std::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}", dt.year, month, day, hour, minute, second);
            }
            return std::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}", dt.year, month, day, hour, minute);
        case DatePickerMode::Time:
            if (showSeconds) {
                return std::format("{:02d}:{:02d}:{:02d}", hour, minute, second);
            }
            return std::format("{:02d}:{:02d}", hour, minute);
    }
    return std::format("{:04d}-{:02d}-{:02d}", dt.year, month, day);
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

    const std::string formatted = FormatDateTime(value, opts.mode, opts.showSeconds);
    const float font = theme.ResolveFontSize(UiText{}, TextRole::LabelMedium, size);
    const float textW = TextUtil::MeasureWidth(formatted, font, canvas);
    const float minContentW = m.paddingX * 2.0f + m.iconSize * 2.0f + 22.0f + textW;

    float boxW = std::max(m.minWidth, minContentW);
    if (opts.stretch != StretchMode::Fill && ctrl.width.has_value()) {
        boxW = theme.Scale(*ctrl.width);
    }
    const float boxH = ctrl.height.has_value() ? theme.Scale(*ctrl.height) : m.defaultHeight;
    return Control::Measure({boxW, boxH}, ctrl, opts.stretch, size, theme);
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
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
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
    value.month = std::clamp(value.month, 1, 12);
    value.day = std::clamp(value.day, 1, DaysInMonth(value.year, value.month));
    value.hour = std::clamp(value.hour, 0, 23);
    value.minute = std::clamp(value.minute, 0, 59);
    value.second = std::clamp(value.second, 0, 59);

    const UiTheme theme = (opts.theme != nullptr) ? *opts.theme : ctx.GetTheme();
    const SelectMetrics& m = theme.GetSelect(activeSize);
    ICanvas* canvas = ctx.GetCanvas();

    DatePickerOptions optsCopy = opts;
    Control::InheritElementSizing(ctx, optsCopy.stretch, optsCopy.width, optsCopy.height);

    const Vec2 intrinsicDims = DatePicker::Measure(value, optsCopy, activeSize, canvas, theme);
    if (optsCopy.stretch != StretchMode::Fill && !optsCopy.width.has_value()) {
        optsCopy.width = (theme.scale > 0.0f) ? (intrinsicDims.x / theme.scale) : intrinsicDims.x;
    }

    const ControlSpec ctrl = EffectiveDatePickerControl(optsCopy, activeSize, theme);
    const Rect prevBounds = ctx.GetPrevFrameBounds(id);
    const Rect boxRect = (prevBounds.w > 0.0f && prevBounds.h > 0.0f)
        ? prevBounds
        : Rect({0.0f, 0.0f}, intrinsicDims);

    struct DatePickerState {
        int viewYear{2026};
        int viewMonth{10};
        int syncedYear{0};
        int syncedMonth{0};
        bool initialized{false};
    };
    auto& state = ctx.GetOrCreateState<DatePickerState>(id);
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

    const DatePickerPopoverLayout popLayout = ComputePopoverLayout(boxRect, optsCopy, theme, ctx.GetViewportSize());
    if (isOpen) {
        ctx.OpenPopup(id, popLayout.popupRect);
        ctx.RegisterOverlayHitRegion(id, popLayout.popupRect);
    }

    int hoveredNavBtn = -1;
    int hoveredDayCell = -1;
    int hoveredTimeBtn = -1;
    bool todayHovered = false;
    bool doneHovered = false;

    if (isOpen && wasOpen && !effectiveDisabled) {
        ctx.BeginOverlayHitCheck(id);
        const Vec2 mouse = ctx.GetMousePos();
        const bool mouseClicked = ctx.IsMouseClicked();

        if (popLayout.hasCalendar) {
            const std::array<Rect, 4> navRects = {
                popLayout.prevYearRect,
                popLayout.prevMonthRect,
                popLayout.nextMonthRect,
                popLayout.nextYearRect
            };
            for (int i = 0; i < 4; ++i) {
                if (navRects[static_cast<size_t>(i)].Contains(mouse)) {
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
                const int col = std::clamp(static_cast<int>((mouse.x - popLayout.gridRect.x) / popLayout.cellW), 0, 6);
                const int row = std::clamp(static_cast<int>((mouse.y - popLayout.gridRect.y) / popLayout.cellH), 0, 5);
                hoveredDayCell = row * 7 + col;
                if (mouseClicked) {
                    const auto cells = BuildCalendarGrid(state.viewYear, state.viewMonth, optsCopy.firstDayMonday);
                    const DayCellEntry& chosen = cells[static_cast<size_t>(hoveredDayCell)];
                    if (value.year != chosen.year || value.month != chosen.month || value.day != chosen.day) {
                        value.year = chosen.year;
                        value.month = chosen.month;
                        value.day = chosen.day;
                        res.changed = true;
                    }
                    state.viewYear = chosen.year;
                    state.viewMonth = chosen.month;
                    state.syncedYear = chosen.year;
                    state.syncedMonth = chosen.month;
                    ctx.ConsumeMouseClick();
                }
            }
        }

        if (popLayout.hasTime) {
            const Vec2 wheel = ctx.GetPopupMouseWheel(id);
            for (int u = 0; u < popLayout.timeUnitCount; ++u) {
                const size_t su = static_cast<size_t>(u);
                if (popLayout.timeMinusRects[su].Contains(mouse)) {
                    hoveredTimeBtn = u * 2;
                    if (mouseClicked) {
                        if (u == 0) value.hour = (value.hour + 23) % 24;
                        else if (u == 1) value.minute = (value.minute + 59) % 60;
                        else value.second = (value.second + 59) % 60;
                        res.changed = true;
                        ctx.ConsumeMouseClick();
                    }
                } else if (popLayout.timePlusRects[su].Contains(mouse)) {
                    hoveredTimeBtn = u * 2 + 1;
                    if (mouseClicked) {
                        if (u == 0) value.hour = (value.hour + 1) % 24;
                        else if (u == 1) value.minute = (value.minute + 1) % 60;
                        else value.second = (value.second + 1) % 60;
                        res.changed = true;
                        ctx.ConsumeMouseClick();
                    }
                }

                if (wheel.y != 0.0f && popLayout.timeUnitRects[su].Contains(mouse)) {
                    const int step = (wheel.y > 0.0f) ? 1 : -1;
                    if (u == 0) value.hour = (value.hour + 24 + step) % 24;
                    else if (u == 1) value.minute = (value.minute + 60 + step) % 60;
                    else value.second = (value.second + 60 + step) % 60;
                    res.changed = true;
                    ctx.ConsumeMouseWheel();
                }
            }
        }

        if (optsCopy.showTodayButton && popLayout.todayBtnRect.Contains(mouse)) {
            todayHovered = true;
            if (mouseClicked) {
                const DateTime todayVal{2026, 10, 20, 14, 30, 0};
                if (optsCopy.mode == DatePickerMode::Date) {
                    if (value.year != todayVal.year || value.month != todayVal.month || value.day != todayVal.day) {
                        value.year = todayVal.year;
                        value.month = todayVal.month;
                        value.day = todayVal.day;
                        res.changed = true;
                    }
                } else if (optsCopy.mode == DatePickerMode::Time) {
                    if (value.hour != todayVal.hour || value.minute != todayVal.minute || value.second != todayVal.second) {
                        value.hour = todayVal.hour;
                        value.minute = todayVal.minute;
                        value.second = todayVal.second;
                        res.changed = true;
                    }
                } else {
                    if (value != todayVal) {
                        value = todayVal;
                        res.changed = true;
                    }
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

        ctx.EndOverlayHitCheck();
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
            static_cast<uint8_t>(optsCopy.mode),
            optsCopy.showSeconds,
            optsCopy.showTodayButton,
            optsCopy.firstDayMonday,
            hoveredNavBtn,
            hoveredDayCell,
            hoveredTimeBtn,
            todayHovered,
            doneHovered
        ));

        ctx.RegisterOverlay([renderData](ICanvas& overlayCanvas) {
            RenderDatePickerPopover(overlayCanvas, renderData);
        });
    }

    if (res.hovered && !isOpen && !opts.tooltip.empty()) {
        ctx.SetTooltip(opts.tooltip, prevBounds);
    }

    const float btnSize = m.iconSize + 4.0f;
    const Rect chevronRect(
        boxRect.x + boxRect.w - m.paddingX - m.iconSize - 2.0f,
        boxRect.y + (boxRect.h - btnSize) * 0.5f,
        btnSize,
        btnSize
    );
    const bool chevronHovered = res.hovered && !effectiveDisabled && chevronRect.Contains(ctx.GetMousePos());
    const bool finalFocused = isOpen || ctx.IsFocused(id);

    const std::string formattedStr = FormatDateTime(value, optsCopy.mode, optsCopy.showSeconds);

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
        static_cast<uint8_t>(optsCopy.mode),
        optsCopy.showSeconds,
        optsCopy.showTodayButton,
        optsCopy.firstDayMonday,
        static_cast<uint8_t>(optsCopy.stretch),
        static_cast<uint8_t>(optsCopy.align),
        optsCopy.width.value_or(0.0f),
        optsCopy.height.value_or(0.0f),
        optsCopy.color,
        optsCopy.background,
        optsCopy.border,
        data->formattedText,
        data->isOpen,
        data->chevronHovered
    );
    node->measureFn = [](const Node* n, float /*availableWidth*/, ICanvas* c, const UiTheme& t) noexcept -> Vec2 {
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
    node->focused = finalFocused;
    res.focused = finalFocused;

    Control::SetupNode(ctx, node, ctrl, optsCopy.stretch, optsCopy.align);
    ctx.AdaptChildToContainer(node);

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
    const ControlSpec ctrl = EffectiveDatePickerControl(opts, size, theme);
    const auto& pal = theme.palette;
    const Color accentCol = ResolveAccentColor(opts, theme, hovered, open || focused, disabled);

    Color boxBg = ctrl.HasBackground(true)
        ? ((hovered && !disabled) ? pal.surface : pal.inputBackground)
        : Color::Transparent();
    Color boxBorder;
    float strokeW = (ctrl.border.width > 0.0f) ? theme.Scale(ctrl.border.width) : m.strokeWidth;

    if ((open || focused) && !disabled) {
        boxBorder = opts.color.HasAny() ? accentCol : pal.focusRing;
        if (ctrl.border.width > 0.0f || m.strokeWidth > 0.0f) strokeW += 0.5f;
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

    if (ctrl.HasBackground(true) && ctrl.background.HasAny() && !ctrl.background.IsSurface()) {
        boxBg = ctrl.background.Resolve(hovered, open || focused, disabled, boxBg);
    }
    if (ctrl.border.color.HasAny()) {
        boxBorder = ctrl.border.color.Resolve(hovered, open || focused, disabled, boxBorder);
    }

    const float radius = ctrl.border.radius.has_value() ? theme.Scale(*ctrl.border.radius) : m.cornerRadius;
    canvas.DrawRoundRect(bounds, radius, boxBg, boxBorder, strokeW);

    // Left vector calendar / clock icon
    float leftX = bounds.x + m.paddingX;
    const Rect iconRect(leftX, bounds.y + (bounds.h - m.iconSize) * 0.5f, m.iconSize, m.iconSize);
    if (opts.mode == DatePickerMode::Time) {
        DrawClockIcon(canvas, iconRect, leadIconCol);
    } else {
        DrawCalendarIcon(canvas, iconRect, leadIconCol);
    }
    leftX += m.iconSize + 8.0f;

    // Right chevron icon
    const float rightX = bounds.x + bounds.w - m.paddingX - m.iconSize;
    const Rect chevronRect(rightX, bounds.y + (bounds.h - m.iconSize) * 0.5f, m.iconSize, m.iconSize);
    const Icon chevron = open ? Icon::ChevronUp() : Icon::ChevronDown();
    chevron.Draw(canvas, chevronRect, chevronCol);

    // Formatted date/time text
    const float availW = std::max(0.0f, (rightX - 6.0f) - leftX);
    const Rect textRect(leftX, bounds.y, availW, bounds.h);
    canvas.PushClip(textRect);

    const UiText displayTxt = d ? d->formattedText : UiText{"2026-10-20"};
    const float font = theme.ResolveFontSize(displayTxt, TextRole::LabelMedium, size);
    const Color textCol = disabled ? pal.textPrimary.WithAlphaF(0.40f) : pal.textPrimary;
    TextUtil::DrawUiText(canvas, displayTxt, textRect, font, bounds.h, textCol, 0.0f, 0.5f);

    canvas.PopClip();
}

} // namespace guikit
