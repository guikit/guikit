#include "guikit/fields/date_picker.hpp"
#include "keel/math.h"
#include "keel/static_array.h"
#include "keel/time.h"
#include "guikit/actions/button.hpp"
#include "guikit/context.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"
#include "guikit/display/separator.hpp"
#include "guikit/display/text.hpp"
#include "guikit/fields/spin_box.hpp"
#include "guikit/graphics/icon.hpp"
#include "guikit/layout/row.hpp"
#include "guikit/layout/spacer.hpp"
#include "guikit/overlays/popover.hpp"
#include "guikit/style/field_frame.hpp"
#include "guikit/style/style_painter.hpp"
#include "guikit/surfaces/card.hpp"

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

Color ResolveAccentColor(const DatePickerOptions& opts, const UiTheme& theme, bool /*hovered*/ = false, bool /*active*/ = false, bool disabled = false) noexcept {
    return FieldFrame::ResolveAccent(theme, opts.status, disabled);
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
    const keel::i32 firstDow = detail::DatePicker::DayOfWeek(viewYear, clampedMonth, 1); // 0 = Sun .. 6 = Sat
    const keel::i32 leading = firstDayMonday ? ((firstDow + 6) % 7) : firstDow;
    const keel::i32 daysInCurr = detail::DatePicker::DaysInMonth(viewYear, clampedMonth);

    keel::i32 prevYear = viewYear;
    keel::i32 prevMonth = clampedMonth;
    StepMonth(prevYear, prevMonth, -1);
    const keel::i32 daysInPrev = detail::DatePicker::DaysInMonth(prevYear, prevMonth);

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

struct CalendarGridData {
    DateTime value{};
    keel::i32 viewYear{2026};
    keel::i32 viewMonth{10};
    DatePickerOptions opts{};
    UiSize size{UiSize::Md};
    keel::i32 hoveredDayCell{-1};
    keel::f32 weekdayH{22.0f};
    keel::f32 cellH{28.0f};
    keel::f32 minWidth{238.0f};
};

[[nodiscard]] UiSize ResolvePopupControlSize(UiSize pickerSize) noexcept {
    switch (pickerSize.TableIndex()) {
        case 0:  return UiSize::Xs;
        case 1:  return UiSize::Sm;
        case 2:  return UiSize::Sm;
        case 3:  return UiSize::Md;
        default: return UiSize::Lg;
    }
}

void RenderCalendarGridNode(const Node* node, ICanvas& canvas, const UiTheme& theme) noexcept {
    if (!node) return;
    const Rect bounds = node->computedRect;
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    const auto* d = node->As<CalendarGridData>();
    if (!d) return;

    const SelectMetrics& m = theme.GetSelect(d->size);
    const auto& pal = theme.palette;
    const Color accentCol = ResolveAccentColor(d->opts, theme);
    const keel::f32 cellRadius = keel::min(5.0f, m.cornerRadius);
    const keel::f32 smallFont = theme.ResolveFontSize(UiText{}, TextRole::LabelSmall, d->size);

    const keel::f32 cellW = bounds.w / 7.0f;
    const keel::f32 weekdayH = keel::min(d->weekdayH, bounds.h);
    const keel::f32 gridH = keel::max(0.0f, bounds.h - weekdayH);
    const keel::f32 cellH = gridH / 6.0f;
    const Rect gridRect(bounds.x, bounds.y + weekdayH, bounds.w, gridH);

    // Weekday column headers (indexed 0 = Sunday .. 6 = Saturday)
    static constexpr keel::StaticArray<keel::StringRef, 7> kDefaultWeekdaysSun = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
    for (keel::i32 col = 0; col < 7; ++col) {
        const keel::i32 dow = (col + (d->opts.firstDayMonday ? 1 : 0)) % 7;
        const keel::StringRef wLabel = (d->opts.labels.weekdayNames.size() >= 7 && !d->opts.labels.weekdayNames[keel::as<keel::usize>(dow)].empty())
            ? d->opts.labels.weekdayNames[keel::as<keel::usize>(dow)]
            : kDefaultWeekdaysSun[keel::as<keel::usize>(dow)];
        const Rect wRect(bounds.x + keel::as<keel::f32>(col) * cellW, bounds.y, cellW, weekdayH);
        canvas.DrawTextBox(wLabel, wRect, smallFont, pal.textMuted, 0.5f, 0.5f);
    }

    // 6x7 day grid (42 cells)
    const DateTime todayRef = d->opts.todayOverride.value_or(DateTime::Today());
    const auto cells = BuildCalendarGrid(d->viewYear, d->viewMonth, d->opts.firstDayMonday);
    for (keel::i32 idx = 0; idx < 42; ++idx) {
        const keel::i32 row = idx / 7;
        const keel::i32 col = idx % 7;
        const Rect cellRect(
            gridRect.x + keel::as<keel::f32>(col) * cellW,
            gridRect.y + keel::as<keel::f32>(row) * cellH,
            cellW,
            cellH
        );
        const Rect pillRect(
            cellRect.x + 2.0f,
            cellRect.y + 1.5f,
            keel::max(0.0f, cellRect.w - 4.0f),
            keel::max(0.0f, cellRect.h - 3.0f)
        );

        const DayCellEntry& cell = cells[keel::as<keel::usize>(idx)];
        const bool cellDisabled = IsDayCellDisabled(cell.year, cell.month, cell.day, d->opts);
        const bool isSelected = (cell.year == d->value.year && cell.month == d->value.month && cell.day == d->value.day);
        const bool isToday = (cell.year == todayRef.year && cell.month == todayRef.month && cell.day == todayRef.day);
        const bool isHovered = !cellDisabled && (d->hoveredDayCell == idx);

        if (isSelected) {
            const Color selBg = cellDisabled ? accentCol.WithAlphaF(theme.disabledOpacity) : accentCol;
            canvas.DrawRoundRect(pillRect, cellRadius, selBg);
        } else if (isHovered) {
            canvas.DrawRoundRect(pillRect, cellRadius, pal.surfaceHover);
            if (isToday) {
                canvas.DrawRoundRect(pillRect, cellRadius, Color::Transparent(), accentCol.WithAlphaF(0.65f), 1.0f);
            }
        } else if (isToday) {
            canvas.DrawRoundRect(pillRect, cellRadius, accentCol.WithAlphaF(0.10f), accentCol.WithAlphaF(0.55f), 1.0f);
        }

        Color dayCol = pal.textPrimary;
        if (isSelected) {
            dayCol = pal.accentText;
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

} // namespace

keel::FixedString<32> detail::DatePicker::FormatDateTime(const DateTime& dt, DatePickerMode mode, bool showSeconds) {
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

Vec2 detail::DatePicker::Measure(
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

Vec2 detail::DatePicker::Measure(
    const DateTime& value,
    const DatePickerOptions& opts,
    UiSize size,
    ICanvas* canvas
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(value, opts, size, canvas, defaultTheme);
}

Response detail::DatePicker::Run(
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

    const Vec2 intrinsicDims = detail::DatePicker::Measure(value, optsCopy, activeSize, canvas, theme);

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

    if (isOpen && !effectiveDisabled) {
        auto popupIdScope = ctx.IdScope(id.value);
        bool popoverOpen = true;
        const bool hasCalendar = (optsCopy.mode != DatePickerMode::Time);
        const bool hasTime = (optsCopy.mode == DatePickerMode::DateTime || optsCopy.mode == DatePickerMode::Time);
        const UiSize popupCtrlSize = ResolvePopupControlSize(activeSize);
        const ComponentMetrics& popMetrics = theme.GetMetrics(popupCtrlSize);

        if (auto pop = guikit::Popover(ctx, popoverOpen, {
            .anchorRect = boxRect,
            .placement = PopoverPlacement::Auto,
            .offset = 4.0f,
            .closeOnClickOutside = false,
            .closeOnEscape = false
        })) {
            const Id portalId = pop.response.id;
            ctx.OpenPopup(id, pop.response.rect, boxRect, portalId);

            if (auto card = guikit::Card(ctx, {
                .width = SizeMode::Fit,
                .height = SizeMode::Fit,
                .gap = 6.0f,
                .padding = Padding::All(10.0f),
                .surface = SurfaceRole::Popup,
                .clipChildren = false
            })) {
                if (hasCalendar) {
                    // Calendar navigation header row: « ‹ Month Year › »
                    if (auto hdr = guikit::Row(ctx, {
                        .gap = 2.0f,
                        .align = Alignment::Center,
                        .width = SizeMode::Stretch
                    })) {
                        if (guikit::Button(ctx, {
                            .text = UiText{"\xC2\xAB"},
                            .square = true,
                            .size = popupCtrlSize,
                            .style = ButtonStyle::Ghost
                        }).clicked) {
                            state.viewYear -= 1;
                        }
                        if (guikit::Button(ctx, {
                            .text = UiText{"\xE2\x80\xB9"},
                            .square = true,
                            .size = popupCtrlSize,
                            .style = ButtonStyle::Ghost
                        }).clicked) {
                            StepMonth(state.viewYear, state.viewMonth, -1);
                        }

                        guikit::Spacer(ctx);
                        char titleBuf[96];
                        const keel::StringRef mName = MonthName(state.viewMonth, optsCopy.labels.monthNames);
                        const auto titleStr = keel::format_buf(titleBuf, "%s %04d", mName, state.viewYear);
                        guikit::Text(ctx, UiText{ctx.AllocString(titleStr)}, {.role = TextRole::LabelMedium});
                        guikit::Spacer(ctx);

                        if (guikit::Button(ctx, {
                            .text = UiText{"\xE2\x80\xBA"},
                            .square = true,
                            .size = popupCtrlSize,
                            .style = ButtonStyle::Ghost
                        }).clicked) {
                            StepMonth(state.viewYear, state.viewMonth, +1);
                        }
                        if (guikit::Button(ctx, {
                            .text = UiText{"\xC2\xBB"},
                            .square = true,
                            .size = popupCtrlSize,
                            .style = ButtonStyle::Ghost
                        }).clicked) {
                            state.viewYear += 1;
                        }
                    }

                    // Compact 6x7 day grid leaf node scaled with popupCtrlSize
                    const keel::f32 cellH = popMetrics.defaultHeight;
                    const keel::f32 weekdayH = keel::round(cellH * 0.78f);
                    const keel::f32 calendarMinW = keel::round(cellH * 7.0f + 42.0f);
                    const Id calId = ctx.MakeChildId(loc);
                    const Rect prevCalRect = ctx.GetPrevFrameBounds(calId);
                    keel::i32 hoveredDayCell = -1;
                    if (wasOpen && prevCalRect.w > 0.0f && prevCalRect.h > weekdayH && ctx.IsRectHovered(calId, prevCalRect)) {
                        const Vec2 mouse = ctx.GetMousePos();
                        const Rect gridRect(prevCalRect.x, prevCalRect.y + weekdayH, prevCalRect.w, prevCalRect.h - weekdayH);
                        if (gridRect.Contains(mouse)) {
                            const keel::f32 cellW = gridRect.w / 7.0f;
                            const keel::f32 hitCellH = gridRect.h / 6.0f;
                            const keel::i32 col = keel::clamp(keel::as<keel::i32>((mouse.x - gridRect.x) / cellW), 0, 6);
                            const keel::i32 row = keel::clamp(keel::as<keel::i32>((mouse.y - gridRect.y) / hitCellH), 0, 5);
                            const keel::i32 cellIdx = row * 7 + col;
                            const auto cells = BuildCalendarGrid(state.viewYear, state.viewMonth, optsCopy.firstDayMonday);
                            const DayCellEntry& chosen = cells[keel::as<keel::usize>(cellIdx)];
                            const bool cellDisabled = IsDayCellDisabled(chosen.year, chosen.month, chosen.day, optsCopy);
                            if (!cellDisabled) {
                                hoveredDayCell = cellIdx;
                            }
                            if (ctx.IsMouseClicked()) {
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

                    Node* calNode = ctx.CreateNode(NodeType::Custom, calId);
                    auto* calData = ctx.AllocPayload<CalendarGridData>(CalendarGridData{
                        .value = value,
                        .viewYear = state.viewYear,
                        .viewMonth = state.viewMonth,
                        .opts = optsCopy,
                        .size = activeSize,
                        .hoveredDayCell = hoveredDayCell,
                        .weekdayH = weekdayH,
                        .cellH = cellH,
                        .minWidth = calendarMinW
                    });
                    calNode->SetPayload(calData);
                    calNode->widthMode = SizeMode::Stretch;
                    calNode->minWidth = calendarMinW;
                    calNode->heightMode = SizeMode::Fixed;
                    calNode->fixedHeight = weekdayH + cellH * 6.0f;
                    calNode->measureFn = [](const Node* n, keel::f32 /*availW*/, ICanvas* /*c*/, const UiTheme& /*t*/) noexcept -> Vec2 {
                        const auto* cd = n->As<CalendarGridData>();
                        return cd
                            ? Vec2{cd->minWidth, cd->weekdayH + cd->cellH * 6.0f}
                            : Vec2{238.0f, 190.0f};
                    };
                    calNode->renderFn = &RenderCalendarGridNode;
                    calNode->visualHash = detail::HashArgs(
                        value.year,
                        value.month,
                        value.day,
                        state.viewYear,
                        state.viewMonth,
                        optsCopy.firstDayMonday,
                        hoveredDayCell,
                        popupCtrlSize
                    );
                }

                if (hasTime) {
                    if (hasCalendar) {
                        guikit::Separator(ctx);
                    }
                    if (auto timeRow = guikit::Row(ctx, {
                        .gap = 4.0f,
                        .align = Alignment::Center,
                        .justify = Justify::Center,
                        .width = SizeMode::Stretch
                    })) {
                        static constexpr keel::StaticArray<keel::StringRef, 1> kTwoDigits = {"00"};
                        const keel::i32 timeUnitCount = optsCopy.showSeconds ? 3 : 2;
                        for (keel::i32 u = 0; u < timeUnitCount; ++u) {
                            auto unitScope = ctx.IdScope(keel::as<keel::u64>(u));
                            const keel::i32 maxUnit = (u == 0) ? 23 : 59;
                            keel::i32 unitVal = (u == 0) ? value.hour : ((u == 1) ? value.minute : value.second);
                            const keel::StringRef sfx = ResolveTimeSuffix(keel::as<keel::usize>(u), optsCopy);
                            const Response spinRes = guikit::SpinBox(ctx, unitVal, {
                                .suffix = UiAffix{sfx},
                                .valueFormat = UiText{"{:02d}", kTwoDigits},
                                .min = -1.0,
                                .max = keel::as<keel::f64>(maxUnit + 1),
                                .step = 1.0,
                                .stepStyle = SpinBoxStepStyle::Split,
                                .size = popupCtrlSize,
                                .width = SizeMode::Fit,
                                .style = optsCopy.style,
                                .status = optsCopy.status
                            });
                            if (spinRes.changed) {
                                if (unitVal < 0) {
                                    unitVal = maxUnit;
                                } else if (unitVal > maxUnit) {
                                    unitVal = 0;
                                }
                                DateTime candidate = value;
                                if (u == 0) candidate.hour = unitVal;
                                else if (u == 1) candidate.minute = unitVal;
                                else candidate.second = unitVal;
                                ClampDateTimeToBounds(candidate, optsCopy);
                                if (value != candidate) {
                                    value = candidate;
                                    res.changed = true;
                                }
                            }
                            if (u + 1 < timeUnitCount) {
                                guikit::Text(ctx, ":", {.role = TextRole::LabelMedium, .variant = UiVariant::Secondary});
                            }
                        }
                    }
                }

                guikit::Separator(ctx);

                // Footer action buttons: Today/Now + Done
                if (auto footerRow = guikit::Row(ctx, {
                    .gap = 8.0f,
                    .align = Alignment::Center,
                    .width = SizeMode::Stretch
                })) {
                    if (optsCopy.showTodayButton) {
                        if (guikit::Button(ctx, {
                            .text = UiText{ResolveTodayLabel(optsCopy)},
                            .size = popupCtrlSize,
                            .style = ButtonStyle::Default,
                            .variant = UiVariant::Default
                        }).clicked) {
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
                        }
                    }

                    guikit::Spacer(ctx);

                    if (guikit::Button(ctx, {
                        .text = UiText{ResolveDoneLabel(optsCopy)},
                        .size = popupCtrlSize,
                        .style = ButtonStyle::Default,
                        .variant = UiVariant::Primary
                    }).clicked) {
                        res.submitted = true;
                        pop.Close();
                        ctx.ClosePopup();
                        isOpen = false;
                        ctx.RequestFocus(id);
                    }
                }
            }

            if (isOpen && wasOpen) {
                if (ctx.ConsumeKey(Key::Escape)) {
                    pop.Close();
                    ctx.ClosePopup();
                    isOpen = false;
                    ctx.RequestFocus(id);
                } else if (ctx.ConsumeKey(Key::Enter)) {
                    res.submitted = true;
                    pop.Close();
                    ctx.ClosePopup();
                    isOpen = false;
                    ctx.RequestFocus(id);
                }
            }
        }
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
        keel::as<keel::u8>(optsCopy.style),
        keel::as<keel::u8>(optsCopy.status),
        optsCopy.background,
        optsCopy.border,
        data->formattedText,
        data->isOpen,
        data->chevronHovered
    );
    node->measureFn = [](const Node* n, keel::f32 /*availableWidth*/, ICanvas* c, const UiTheme& t) noexcept -> Vec2 {
        const auto* d = n->As<DatePickerData>();
        return d
            ? detail::DatePicker::Measure(d->value, d->opts, n->uiSize, c, t)
            : detail::DatePicker::Measure(DateTime{}, DatePickerOptions{}, n->uiSize, c, t);
    };
    node->renderFn = &detail::DatePicker::Render;
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->active = isOpen;
    node->focused = isOpen || ctx.IsFocusVisible(id);
    res.focused = finalFocused;

    Control::SetupNode(ctx, node, ctrl);

    return res;
}

void detail::DatePicker::Render(const Node* node, ICanvas& canvas, const UiTheme& theme) {
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

    Color chevronCol = open ? accentCol : (((d && d->chevronHovered) && !disabled) ? pal.textPrimary : pal.textSecondary);
    Color leadIconCol = open ? accentCol : (disabled ? pal.textSecondary.WithAlphaF(theme.disabledOpacity) : accentCol);
    if (disabled) {
        chevronCol = chevronCol.WithAlphaF(theme.disabledOpacity);
    }

    FieldFrame::Draw(canvas, theme, FieldFrameParams{
        .rect = bounds,
        .style = opts.style,
        .status = opts.status,
        .background = opts.background,
        .border = opts.border,
        .defaultRadius = m.cornerRadius,
        .defaultStrokeWidth = m.strokeWidth,
        .hovered = hovered,
        .focused = focused,
        .active = open,
        .disabled = disabled
    });

    // Left vector calendar / clock icon
    keel::f32 leftX = bounds.x + m.paddingX;
    const Rect iconRect(leftX, bounds.y + (bounds.h - m.iconSize) * 0.5f, m.iconSize, m.iconSize);
    if (opts.mode == DatePickerMode::Time) {
        DrawClockIcon(canvas, iconRect, leadIconCol);
    } else {
        DrawCalendarIcon(canvas, iconRect, leadIconCol);
    }
    leftX += m.iconSize + 8.0f;

    // Right chevron trigger via FieldFrame::DrawTrigger
    const Icon chevron = open ? Icon::ChevronUp() : Icon::ChevronDown();
    const keel::f32 rightEdgeX = FieldFrame::DrawTrigger(
        canvas, theme, bounds, chevron,
        m.paddingX, m.iconSize, m.cornerRadius, opts.border.radius.has_value(),
        (d && d->chevronHovered), false, open, disabled, false, chevronCol
    );

    // Formatted date/time text
    const keel::f32 availW = keel::max(0.0f, rightEdgeX - leftX);
    const Rect textRect(leftX, bounds.y, availW, bounds.h);
    canvas.PushClip(textRect);

    const UiText displayTxt = d ? d->formattedText : UiText{"2026-10-20"};
    const keel::f32 font = theme.ResolveFontSize(displayTxt, TextRole::LabelMedium, size);
    const Color textCol = disabled ? pal.textPrimary.WithAlphaF(theme.disabledOpacity) : pal.textPrimary;
    TextUtil::DrawUiText(canvas, displayTxt, textRect, font, bounds.h, textCol, 0.0f, 0.5f, &theme);

    canvas.PopClip();
}

} // namespace guikit
