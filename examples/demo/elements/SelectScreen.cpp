#include "../DemoContext.hpp"
#include <format>
#include <vector>

namespace guikit::demo {

namespace {
struct SelectState {
    size_t idx = 0;
    std::vector<std::string_view> items = {"Option Alpha", "Option Beta", "Option Gamma", "Option Delta", "Option Epsilon"};
    bool disabled = false;
    size_t stretchIdx = 2; // Fill
    float width = 0.0f;
    float fillPct = 50.0f;
    uint32_t maxItems = 4;
    float cornerRadius = 6.0f;
    bool showLabel = true;
    bool showDesc = true;
    guikit::UiVariant descVariant = guikit::UiVariant::Muted;
    guikit::ElementLayout layout = guikit::ElementLayout::Stacked;
    guikit::InlineOrder inlineOrder = guikit::InlineOrder::LabelFirst;
    guikit::Align elementAlign = guikit::Align::Start;
    bool hasPrefix = false;
};
SelectState s_state;
} // namespace

void RenderSelectInspector(guikit::Context& ui, DemoContext& ctx) {
    if (ui.StepSlider(ctx.controlsSize, {.label = "Controls Size", .showTicks = true, .showLabels = true, .showValue = true}).changed) ui.InvalidateAll();
    if (ui.Slider(s_state.cornerRadius, 0.0f, 24.0f, {.label = "Corner Radius", .valueFormat = "%.0f px"}).changed) {
        ui.InvalidateAll();
    }
    float maxItemsFloat = static_cast<float>(s_state.maxItems);
    if (ui.Slider(maxItemsFloat, 2.0f, 8.0f, {.label = "Max Visible Menu Items", .valueFormat = "%.0f items", .step = 1.0f}).changed) {
        s_state.maxItems = static_cast<uint32_t>(maxItemsFloat);
        ui.InvalidateAll();
    }

    ctx.RenderVariantPicker(ui);

    if (ui.Switch(s_state.showLabel, {.label = "Show Label"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showDesc, {.label = "Show Description"}).changed) ui.InvalidateAll();
    ctx.RenderDescVariantPicker(ui, s_state.descVariant, s_state.showDesc);

    ctx.RenderElementLayoutControls(ui, s_state.showLabel || s_state.showDesc, s_state.layout, s_state.inlineOrder, s_state.elementAlign, s_state.stretchIdx);
    if (s_state.stretchIdx == 2) {
        if (ui.Slider(s_state.fillPct, 10.0f, 100.0f, {.label = "Explicit Width", .valueFormat = "%.0f %%", .step = 5.0f}).changed) ui.InvalidateAll();
    } else {
        if (ui.Slider(s_state.width, 0.0f, 400.0f, {.label = "Explicit Width", .valueFormat = {(s_state.width > 0.0f) ? "%.0f px" : "Auto", {"Auto", "%.0f px"}}, .step = 10.0f}).changed) ui.InvalidateAll();
    }

    if (ui.Switch(s_state.hasPrefix, {.label = "Prefix Icon"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.disabled, {.label = "Disabled State"}).changed) ui.InvalidateAll();
}

void RenderSelectStage(guikit::Context& ui, DemoContext& ctx) {
    ui.Select(s_state.idx, s_state.items, {
        .label = s_state.showLabel ? "A select" : "",
        .description = {s_state.showDesc ? "This is a select" : "", s_state.descVariant},
        .prefix = s_state.hasPrefix ? guikit::Icon::Settings() : guikit::Icon{},
        .variant = ctx.btnVariant,
        .layout = s_state.layout,
        .inlineOrder = s_state.inlineOrder,
        .elementAlign = s_state.elementAlign,
        .maxVisibleItems = s_state.maxItems,
        .size = ctx.controlsSize,
        .borderRadius = s_state.cornerRadius,
        .stretch = static_cast<guikit::StretchMode>(s_state.stretchIdx),
        .width = (s_state.stretchIdx == 2) ? std::optional<float>(s_state.fillPct) : ((s_state.width > 0.0f) ? std::optional<float>(s_state.width) : std::nullopt),
        .disabled = s_state.disabled
    });
}

std::string GenerateSelectCode(const DemoContext& ctx) {
    return std::format(
        "ui.Select(presetIndex, itemsList, {{\n"
        "    .label = \"{}\",\n"
        "    .description = {},\n"
        "    .placeholder = \"Select...\",\n"
        "    .prefix = {},\n"
        "    .variant = guikit::UiVariant::{},\n"
        "    .layout = guikit::ElementLayout::{},\n"
        "    .inlineOrder = guikit::InlineOrder::{},\n"
        "    .elementAlign = guikit::Align::{},\n"
        "    .maxVisibleItems = {},\n"
        "    .size = guikit::UiSize::{},\n"
        "    .borderRadius = {:.1f}f,\n"
        "    .stretch = guikit::StretchMode::{},\n"
        "    .width = {},\n"
        "    .disabled = {}\n"
        "}});",
        s_state.showLabel ? "A select" : "",
        FormatDesc(s_state.showDesc, "This is a select", s_state.descVariant),
        s_state.hasPrefix ? "guikit::Icon::Settings()" : "{}",
        VariantName(ctx.btnVariant),
        ElementLayoutEnumName(s_state.layout),
        InlineOrderEnumName(s_state.inlineOrder),
        LabelAlignEnumName(s_state.elementAlign),
        s_state.maxItems,
        SizeName(ctx.controlsSize),
        s_state.cornerRadius,
        StretchModeEnumName(static_cast<guikit::StretchMode>(s_state.stretchIdx)),
        (s_state.stretchIdx == 2) ? std::format("{:.1f}f", s_state.fillPct) : ((s_state.width > 0.0f) ? std::format("{:.1f}f", s_state.width) : "std::nullopt"),
        s_state.disabled ? "true" : "false"
    );
}

} // namespace guikit::demo
