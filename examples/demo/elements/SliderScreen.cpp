#include "../DemoContext.hpp"
#include <format>

namespace guikit::demo {

namespace {
struct SliderState {
    float val = 65.0f;
    size_t stepIdx = 2;
    bool continuous = true;
    size_t stretchIdx = 2; // Fill
    float width = 0.0f;
    float fillPct = 50.0f;
    bool showLabel = true;
    bool showDesc = true;
    guikit::UiVariant descVariant = guikit::UiVariant::Muted;
    bool showTicks = true;
    bool showLabels = true;
    bool showValue = true;
    guikit::Placement valuePlacement = guikit::Placement::Trailing;
    bool showRangeText = false;
    bool disabled = false;
    guikit::ElementLayout layout = guikit::ElementLayout::Stacked;
    guikit::InlineOrder inlineOrder = guikit::InlineOrder::LabelFirst;
    guikit::Align elementAlign = guikit::Align::Start;
};
SliderState s_state;
} // namespace

void RenderSliderInspector(guikit::Context& ui, DemoContext& ctx) {
    if (ui.Switch(s_state.continuous, {.label = "Continuous Mode", .description = "Continuous vs Quantized StepSlider"}).changed) ui.InvalidateAll();
    if (ui.StepSlider(ctx.controlsSize, {.label = "Controls Size", .showTicks = true, .showLabels = true, .showValue = true}).changed) ui.InvalidateAll();

    ctx.RenderVariantPicker(ui);

    if (ui.Switch(s_state.showLabel, {.label = "Show Label"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showDesc, {.label = "Show Description"}).changed) ui.InvalidateAll();
    ctx.RenderDescVariantPicker(ui, s_state.descVariant, s_state.showDesc);

    ctx.RenderElementLayoutControls(ui, s_state.showLabel || s_state.showDesc, s_state.layout, s_state.inlineOrder, s_state.elementAlign, s_state.stretchIdx);
    if (s_state.stretchIdx == 2) {
        if (ui.Slider(s_state.fillPct, 10.0f, 100.0f, {.label = "Explicit Width", .valueFormat = "%.0f %%", .step = 5.0f}).changed) ui.InvalidateAll();
    } else {
        if (ui.Slider(s_state.width, 0.0f, 400.0f, {.label = "Explicit Width", .valueFormat = {(s_state.width > 0.0f) ? "%.0f px" : "Default", {"Default", "%.0f px"}}, .step = 10.0f}).changed) ui.InvalidateAll();
    }
    if (ui.Switch(s_state.showValue, {.label = "Show Value Tag"}).changed) ui.InvalidateAll();
    static constexpr std::array<std::string_view, 5> valPlaceNames = {"Lead", "Trail", "Top", "Btm", "Thumb"};
    size_t valPlaceIdx = static_cast<size_t>(s_state.valuePlacement);
    if (ui.StepSlider(valPlaceIdx, valPlaceNames, {
        .label = "Value Placement",
        .showTicks = true,
        .showLabels = true,
        .disabled = !s_state.showValue
    }).changed) {
        s_state.valuePlacement = static_cast<guikit::Placement>(valPlaceIdx);
        ui.InvalidateAll();
    }
    if (ui.Switch(s_state.showRangeText, {.label = "Show Range Text (Min / Max)"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showTicks, {.label = "Show Step Ticks", .disabled = s_state.continuous}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showLabels, {.label = "Show Step Labels", .disabled = s_state.continuous}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.disabled, {.label = "Disabled State"}).changed) ui.InvalidateAll();
}

void RenderSliderStage(guikit::Context& ui, DemoContext& ctx) {
    if (s_state.continuous) {
        ui.Slider(s_state.val, 0.0f, 100.0f, {
            .label = s_state.showLabel ? "A slider" : "",
            .textLeft = s_state.showRangeText ? "0%" : "",
            .textRight = s_state.showRangeText ? "100%" : "",
            .description = {s_state.showDesc ? "This is a slider" : "", s_state.descVariant},
            .valueFormat = "%.1f %%",
            .layout = s_state.layout,
            .inlineOrder = s_state.inlineOrder,
            .elementAlign = s_state.elementAlign,
            .showValue = s_state.showValue,
            .valuePlacement = s_state.valuePlacement,
            .variant = ctx.btnVariant,
            .size = ctx.controlsSize,
            .stretch = static_cast<guikit::StretchMode>(s_state.stretchIdx),
            .width = (s_state.stretchIdx == 2) ? std::optional<float>(s_state.fillPct) : ((s_state.width > 0.0f) ? std::optional<float>(s_state.width) : std::nullopt),
            .disabled = s_state.disabled
        });
    } else {
        static constexpr std::array<std::string_view, 5> stepItems = {"Off", "Low", "Mid", "High", "Max"};
        ui.StepSlider(s_state.stepIdx, stepItems, {
            .label = s_state.showLabel ? "A step slider" : "",
            .textLeft = s_state.showRangeText ? "Min" : "",
            .textRight = s_state.showRangeText ? "Max" : "",
            .description = {s_state.showDesc ? "This is a step slider" : "", s_state.descVariant},
            .layout = s_state.layout,
            .inlineOrder = s_state.inlineOrder,
            .elementAlign = s_state.elementAlign,
            .showTicks = s_state.showTicks,
            .showLabels = s_state.showLabels,
            .showValue = s_state.showValue,
            .valuePlacement = s_state.valuePlacement,
            .variant = ctx.btnVariant,
            .size = ctx.controlsSize,
            .stretch = static_cast<guikit::StretchMode>(s_state.stretchIdx),
            .width = (s_state.stretchIdx == 2) ? std::optional<float>(s_state.fillPct) : ((s_state.width > 0.0f) ? std::optional<float>(s_state.width) : std::nullopt),
            .disabled = s_state.disabled
        });
    }
}

std::string GenerateSliderCode(const DemoContext& ctx) {
    if (s_state.continuous) {
        return std::format(
            "ui.Slider(gainValue, 0.0f, 100.0f, {{\n"
            "    .label = \"{}\",\n"
            "    .textLeft = \"{}\",\n"
            "    .textRight = \"{}\",\n"
            "    .description = {},\n"
            "    .valueFormat = \"%.1f %%\",\n"
            "    .step = std::nullopt,\n"
            "    .layout = guikit::ElementLayout::{},\n"
            "    .inlineOrder = guikit::InlineOrder::{},\n"
            "    .elementAlign = guikit::Align::{},\n"
            "    .showValue = {},\n"
            "    .valuePlacement = guikit::Placement::{},\n"
            "    .variant = guikit::UiVariant::{},\n"
            "    .size = guikit::UiSize::{},\n"
            "    .stretch = guikit::StretchMode::{},\n"
            "    .width = {},\n"
            "    .scale = std::nullopt,\n"
            "    .theme = nullptr,\n"
            "    .disabled = {}\n"
            "}});",
            s_state.showLabel ? "A slider" : "",
            s_state.showRangeText ? "0%" : "",
            s_state.showRangeText ? "100%" : "",
            FormatDesc(s_state.showDesc, "This is a slider", s_state.descVariant),
            ElementLayoutEnumName(s_state.layout),
            InlineOrderEnumName(s_state.inlineOrder),
            LabelAlignEnumName(s_state.elementAlign),
            s_state.showValue ? "true" : "false",
            PlacementName(s_state.valuePlacement),
            VariantName(ctx.btnVariant),
            SizeName(ctx.controlsSize),
            StretchModeEnumName(static_cast<guikit::StretchMode>(s_state.stretchIdx)),
            (s_state.stretchIdx == 2) ? std::format("{:.1f}f", s_state.fillPct) : ((s_state.width > 0.0f) ? std::format("{:.1f}f", s_state.width) : "std::nullopt"),
            s_state.disabled ? "true" : "false"
        );
    }
    return std::format(
        "static constexpr std::array<std::string_view, 5> stepItems = {{\"Off\", \"Low\", \"Mid\", \"High\", \"Max\"}};\n"
        "ui.StepSlider(presetIndex, stepItems, {{\n"
        "    .label = \"{}\",\n"
        "    .textLeft = \"{}\",\n"
        "    .textRight = \"{}\",\n"
        "    .description = {},\n"
        "    .stepLabels = {{}},\n"
        "    .layout = guikit::ElementLayout::{},\n"
        "    .inlineOrder = guikit::InlineOrder::{},\n"
        "    .elementAlign = guikit::Align::{},\n"
        "    .showTicks = {},\n"
        "    .showLabels = {},\n"
        "    .showValue = {},\n"
        "    .valuePlacement = guikit::Placement::{},\n"
        "    .valueText = \"\",\n"
        "    .includeNone = false,\n"
        "    .variant = guikit::UiVariant::{},\n"
        "    .size = guikit::UiSize::{},\n"
        "    .stretch = guikit::StretchMode::{},\n"
        "    .width = {},\n"
        "    .scale = std::nullopt,\n"
        "    .theme = nullptr,\n"
        "    .disabled = {}\n"
        "}});",
        s_state.showLabel ? "A step slider" : "",
        s_state.showRangeText ? "Min" : "",
        s_state.showRangeText ? "Max" : "",
        FormatDesc(s_state.showDesc, "This is a step slider", s_state.descVariant),
        ElementLayoutEnumName(s_state.layout),
        InlineOrderEnumName(s_state.inlineOrder),
        LabelAlignEnumName(s_state.elementAlign),
        s_state.showTicks ? "true" : "false",
        s_state.showLabels ? "true" : "false",
        s_state.showValue ? "true" : "false",
        PlacementName(s_state.valuePlacement),
        VariantName(ctx.btnVariant),
        SizeName(ctx.controlsSize),
        StretchModeEnumName(static_cast<guikit::StretchMode>(s_state.stretchIdx)),
        (s_state.stretchIdx == 2) ? std::format("{:.1f}f", s_state.fillPct) : ((s_state.width > 0.0f) ? std::format("{:.1f}f", s_state.width) : "std::nullopt"),
        s_state.disabled ? "true" : "false"
    );
}

} // namespace guikit::demo
