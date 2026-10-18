#include "../DemoContext.hpp"
#include <format>
#include <vector>

namespace guikit::demo {

namespace {
struct SelectState {
    size_t idx = 0;
    std::vector<std::string_view> items = {"Option Alpha", "Option Beta", "Option Gamma", "Option Delta", "Option Epsilon"};
    std::string placeholder = "Select...";
    uint32_t maxItems = 4;
    bool hasPrefix = false;
};
SelectState s_state;
} // namespace

void RenderSelectInspector(guikit::Context& ui, DemoContext&) {
    if (auto el = ui.Element({.label = "Placeholder"})) {
        if (ui.InputField(s_state.placeholder, {
            .placeholder = "Select...",
            .clearable = true
        }).changed) ui.InvalidateAll();
    }
    float maxItemsFloat = static_cast<float>(s_state.maxItems);
    if (auto el = ui.Element({.label = "Max Visible Menu Items"})) {
        if (ui.Slider(maxItemsFloat, 2.0f, 8.0f, {.valueFormat = "%.0f items", .step = 1.0f}).changed) {
            s_state.maxItems = static_cast<uint32_t>(maxItemsFloat);
            ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({.label = "Prefix Icon"})) {
        if (ui.Switch(s_state.hasPrefix).changed) ui.InvalidateAll();
    }
}

void RenderSelectStage(guikit::Context& ui, DemoContext& ctx) {
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
        ui.Size(ctx.controlsSize).Select(s_state.idx, s_state.items, {
            .placeholder = s_state.placeholder,
            .prefix = s_state.hasPrefix ? guikit::Icon::Settings() : guikit::Icon{},
            .maxVisibleItems = s_state.maxItems,
            .stretch = ctx.GetElementStretch(),
            .width = ctx.GetElementWidth(),
            .height = ctx.GetElementHeight(),
            .background = ctx.GetControlBackground(NavCategory::ElemSelect),
            .border = ctx.GetCtrlBorder(),
            .disabled = ctx.elemDisabled
        });
    }
}

std::string GenerateSelectCode(const DemoContext& ctx) {
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
        "    ui.{}Select(presetIndex, itemsList, {{\n"
        "        .placeholder = \"{}\",\n"
        "        .prefix = {},\n"
        "        .maxVisibleItems = {},\n"
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
        s_state.placeholder,
        s_state.hasPrefix ? "guikit::Icon::Settings()" : "{}",
        s_state.maxItems,
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.FormatElementWidthCode(),
        ctx.FormatElementHeightCode(),
        ctx.FormatControlBackgroundCode(NavCategory::ElemSelect),
        ctx.FormatCtrlBorderCode(),
        ctx.elemDisabled ? "true" : "false"
    );
}

} // namespace guikit::demo
