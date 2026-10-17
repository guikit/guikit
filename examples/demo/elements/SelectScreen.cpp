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
    if (ui.InputField(s_state.placeholder, {
        .label = "Placeholder",
        .placeholder = "Select...",
        .clearable = true
    }).changed) ui.InvalidateAll();
    float maxItemsFloat = static_cast<float>(s_state.maxItems);
    if (ui.Slider(maxItemsFloat, 2.0f, 8.0f, {.label = "Max Visible Menu Items", .valueFormat = "%.0f items", .step = 1.0f}).changed) {
        s_state.maxItems = static_cast<uint32_t>(maxItemsFloat);
        ui.InvalidateAll();
    }
    if (ui.Switch(s_state.hasPrefix, {.label = "Prefix Icon"}).changed) ui.InvalidateAll();
}

void RenderSelectStage(guikit::Context& ui, DemoContext& ctx) {
    ui.Select(s_state.idx, s_state.items, {
        .label = ctx.elemLabel,
        .description = ctx.GetElementDescription(),
        .placeholder = s_state.placeholder,
        .icon = ctx.GetElementIcon(),
        .badge = ctx.GetElementBadge(),
        .prefix = s_state.hasPrefix ? guikit::Icon::Settings() : guikit::Icon{},
        .variant = ctx.btnVariant,
        .layout = ctx.elemLayout,
        .inlineOrder = ctx.elemInlineOrder,
        .elementAlign = ctx.elemAlign,
        .maxVisibleItems = s_state.maxItems,
        .size = ctx.controlsSize,
        .stretch = ctx.GetElementStretch(),
        .background = ctx.elemBackground,
        .borderWidth = ctx.elemBorderWidth,
        .borderRadius = ctx.GetElemBorderRadius(),
        .control = ctx.GetControlSpec(NavCategory::ElemSelect),
        .disabled = ctx.elemDisabled
    });
}

std::string GenerateSelectCode(const DemoContext& ctx) {
    return std::format(
        "ui.Select(presetIndex, itemsList, {{\n"
        "    .label = \"{}\",\n"
        "    .description = {},\n"
        "    .placeholder = \"{}\",\n"
        "    .icon = {},\n"
        "    .badge = {},\n"
        "    .prefix = {},\n"
        "    .variant = guikit::UiVariant::{},\n"
        "    .layout = guikit::ElementLayout::{},\n"
        "    .inlineOrder = guikit::InlineOrder::{},\n"
        "    .elementAlign = guikit::Align::{},\n"
        "    .maxVisibleItems = {},\n"
        "    .size = guikit::UiSize::{},\n"
        "    .stretch = guikit::StretchMode::{},\n"
        "    .background = {},\n"
        "    .borderWidth = {:.1f}f,\n"
        "    .borderRadius = {},\n"
        "    .control = {},\n"
        "    .disabled = {}\n"
        "}});",
        ctx.elemLabel,
        ctx.FormatElementDescCode(),
        s_state.placeholder,
        ctx.FormatElementIconCode(),
        ctx.FormatElementBadgeCode(),
        s_state.hasPrefix ? "guikit::Icon::Settings()" : "{}",
        VariantName(ctx.btnVariant),
        ElementLayoutEnumName(ctx.elemLayout),
        InlineOrderEnumName(ctx.elemInlineOrder),
        LabelAlignEnumName(ctx.elemAlign),
        s_state.maxItems,
        SizeName(ctx.controlsSize),
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.elemBackground ? "true" : "false",
        ctx.elemBorderWidth,
        ctx.FormatElemBorderRadiusCode(),
        ctx.FormatControlSpecCode(NavCategory::ElemSelect),
        ctx.elemDisabled ? "true" : "false"
    );
}

} // namespace guikit::demo
