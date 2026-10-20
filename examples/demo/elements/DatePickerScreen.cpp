#include "../DemoContext.hpp"
#include "guikit/widgets/DatePicker.hpp"
#include <format>
#include <string>
#include <string_view>

namespace guikit::demo {

namespace {

template <typename T>
constexpr T ResolveDatePickerNavCat() noexcept {
    if constexpr (requires { T::ElemDatePicker; }) {
        return T::ElemDatePicker;
    } else {
        return T::ElemSelect;
    }
}

struct DatePickerDemoState {
    guikit::DateTime value{2026, 10, 20, 14, 30, 0};
    int modeIdx = 0; // 0 = Date, 1 = DateTime, 2 = Time
    int colorIdx = 0; // 0 = Default, 1 = Emerald, 2 = Amber, 3 = Rose
    bool showSeconds = false;
    bool showTodayButton = true;
    bool firstDayMonday = true;
};
DatePickerDemoState s_state;

guikit::DatePickerMode ActiveMode() noexcept {
    switch (s_state.modeIdx) {
        case 0:  return guikit::DatePickerMode::Date;
        case 1:  return guikit::DatePickerMode::DateTime;
        case 2:  return guikit::DatePickerMode::Time;
        default: return guikit::DatePickerMode::Date;
    }
}

std::string_view ModeEnumName(guikit::DatePickerMode mode) noexcept {
    switch (mode) {
        case guikit::DatePickerMode::Date:     return "Date";
        case guikit::DatePickerMode::DateTime: return "DateTime";
        case guikit::DatePickerMode::Time:     return "Time";
    }
    return "Date";
}

} // namespace

void RenderDatePickerInspector(guikit::Context& ui, DemoContext&) {
    if (auto el = ui.Element({.label = "Picker Mode"})) {
        if (ui.Segmented(s_state.modeIdx, {"Date", "DateTime", "Time"}, {
            .stretch = guikit::StretchMode::Fill
        }).changed) {
            ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({.label = "Show Seconds"})) {
        if (ui.Switch(s_state.showSeconds).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Show Today / Now Button"})) {
        if (ui.Switch(s_state.showTodayButton).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Start Week on Monday"})) {
        if (ui.Switch(s_state.firstDayMonday).changed) ui.InvalidateAll();
    }
}

void RenderDatePickerStage(guikit::Context& ui, DemoContext& ctx) {
    const NavCategory navCat = NavCategory::ElemDatePicker;
    if (auto el = ui.Size(ctx.elemSize).Element({
        .label = ctx.GetElementLabel(),
        .description = ctx.GetElementDescription(),
        .icon = ctx.GetElementIcon(),
        .badge = ctx.GetElementBadge(),
        .layout = ctx.elemLayout,
        .inlineOrder = ctx.elemInlineOrder,
        .align = ctx.elemAlign,
        .stretch = ctx.GetElementStretch(),
        .background = ctx.elemBackground,
        .border = ctx.GetElemBorder(),
        .disabled = ctx.elemDisabled
    })) {
        guikit::DatePickerOptions opts{
            .mode = ActiveMode(),
            .showSeconds = s_state.showSeconds,
            .showTodayButton = s_state.showTodayButton,
            .firstDayMonday = s_state.firstDayMonday,
            .color = ctx.GetControlColor(),
            .stretch = ctx.GetElementStretch(),
            .width = ctx.GetElementWidth(),
            .height = ctx.GetElementHeight(),
            .background = ctx.GetControlBackground(navCat),
            .border = ctx.GetCtrlBorder(),
            .disabled = ctx.elemDisabled
        };

        ui.Size(ctx.controlsSize).DatePicker(s_state.value, opts);
    }
}

std::string GenerateDatePickerCode(const DemoContext& ctx) {
    const NavCategory navCat = NavCategory::ElemDatePicker;
    const guikit::DatePickerMode mode = ActiveMode();
    return std::format(
        "if (auto el = ui.{}Element({{\n"
        "    .label = {},\n"
        "    .description = {},\n"
        "    .icon = {},\n"
        "    .badge = {},\n"
        "    .layout = guikit::ElementLayout::{},\n"
        "    .inlineOrder = guikit::InlineOrder::{},\n"
        "    .align = guikit::Align::{},\n"
        "    .stretch = guikit::StretchMode::{},\n"
        "    .background = {},\n"
        "    .border = {},\n"
        "    .disabled = {}\n"
        "}})) {{\n"
        "    ui.{}DatePicker(dateTimeValue, {{\n"
        "        .mode = guikit::DatePickerMode::{},\n"
        "        .showSeconds = {},\n"
        "        .showTodayButton = {},\n"
        "        .firstDayMonday = {},\n"
        "        .color = {},\n"
        "        .stretch = guikit::StretchMode::{},\n"
        "        .width = {},\n"
        "        .height = {},\n"
        "        .background = {},\n"
        "        .border = {},\n"
        "        .disabled = {}\n"
        "    }});\n"
        "}}",
        FormatUiSizePrefix(ctx.elemSize),
        ctx.FormatElementLabelCode(),
        ctx.FormatElementDescCode(),
        ctx.FormatElementIconCode(),
        ctx.FormatElementBadgeCode(),
        ElementLayoutEnumName(ctx.elemLayout),
        InlineOrderEnumName(ctx.elemInlineOrder),
        LabelAlignEnumName(ctx.elemAlign),
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.FormatElemBackgroundCode(),
        ctx.FormatElemBorderCode(),
        ctx.elemDisabled ? "true" : "false",
        FormatUiSizePrefix(ctx.controlsSize),
        ModeEnumName(mode),
        s_state.showSeconds ? "true" : "false",
        s_state.showTodayButton ? "true" : "false",
        s_state.firstDayMonday ? "true" : "false",
        ctx.FormatControlColorCode(),
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.FormatElementWidthCode(),
        ctx.FormatElementHeightCode(),
        ctx.FormatControlBackgroundCode(navCat),
        ctx.FormatCtrlBorderCode(),
        ctx.elemDisabled ? "true" : "false"
    );
}

} // namespace guikit::demo
