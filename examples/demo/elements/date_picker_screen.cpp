#include "../demo_context.hpp"
#include "guikit/widgets/date_picker.hpp"

namespace guikit::demo {

namespace {

struct DatePickerDemoState {
    guikit::DateTime value{2026, 10, 20, 14, 30, 0};
    keel::i32 modeIdx = 0; // 0 = Date, 1 = DateTime, 2 = Time
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

keel::StringRef ModeEnumName(guikit::DatePickerMode mode) noexcept {
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
        if (ui.Segmented(s_state.modeIdx, {"Date", "DateTime", "Time"}).changed) {
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
    ctx.RunElementStage(ui, [&]() {
        guikit::DatePickerOptions opts{};
        opts.mode = ActiveMode();
        opts.showSeconds = s_state.showSeconds;
        opts.showTodayButton = s_state.showTodayButton;
        opts.firstDayMonday = s_state.firstDayMonday;
        opts.disabled = ctx.elemDisabled;
        opts.size = ctx.controlsSize;
        opts.width = ctx.ResolveCtrlWidth();
        opts.height = ctx.ResolveCtrlHeight();
        opts.color = ctx.GetControlColor();
        opts.background = ctx.GetControlBackground(navCat);
        opts.border = ctx.GetCtrlBorder();
        (void)ui.DatePicker(s_state.value, opts);
    });
}

keel::String GenerateDatePickerCode(const DemoContext& ctx) {
    const NavCategory navCat = NavCategory::ElemDatePicker;
    const guikit::DatePickerMode mode = ActiveMode();
    const bool hasCustomSize = (ctx.controlsSize != guikit::UiSize::None && ctx.controlsSize != guikit::UiSize::Md);
    const keel::FixedString<64> sizeLine = hasCustomSize
        ? keel::format_fixed<64>("    .size = guikit::UiSize::{},\n", SizeName(ctx.controlsSize))
        : keel::FixedString<64>{};
    const keel::String ctrl = keel::format(
        "ui.DatePicker(dateTimeValue, {{\n"
        "    .mode = guikit::DatePickerMode::{},\n"
        "    .showSeconds = {},\n"
        "    .showTodayButton = {},\n"
        "    .firstDayMonday = {},\n"
        "    .disabled = {},\n"
        "{}"
        "    .width = {},\n"
        "    .height = {},\n"
        "    .color = {},\n"
        "    .background = {},\n"
        "    .border = {}\n"
        "}});",
        ModeEnumName(mode),
        s_state.showSeconds ? "true" : "false",
        s_state.showTodayButton ? "true" : "false",
        s_state.firstDayMonday ? "true" : "false",
        ctx.elemDisabled ? "true" : "false",
        sizeLine,
        ctx.FormatCtrlWidthCode(),
        ctx.FormatCtrlHeightCode(),
        ctx.FormatControlColorCode(),
        ctx.FormatControlBackgroundCode(navCat),
        ctx.FormatCtrlBorderCode()
    );
    return ctx.WrapInElementCode(ctrl);
}

} // namespace guikit::demo
