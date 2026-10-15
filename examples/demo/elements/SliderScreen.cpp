#include "../DemoContext.hpp"
#include <format>

namespace guikit::demo {

namespace {
struct SliderState {
    float val = 65.0f;
    size_t stepIdx = 2;
    bool continuous = true;
    bool showTicks = true;
    bool showLabels = true;
    bool showValue = true;
    guikit::Placement valuePlacement = guikit::Placement::Trailing;
    std::string textLeft = "";
    std::string textRight = "";
};
SliderState s_state;
} // namespace

void RenderSliderInspector(guikit::Context& ui, DemoContext&) {
    if (ui.Switch(s_state.continuous, {.label = "Continuous Mode", .description = "Continuous vs Quantized StepSlider"}).changed) ui.InvalidateAll();
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
    if (ui.InputField(s_state.textLeft, {
        .label = "Text Left (Min)",
        .placeholder = "e.g. 0% / Min",
        .clearable = true
    }).changed) ui.InvalidateAll();
    if (ui.InputField(s_state.textRight, {
        .label = "Text Right (Max)",
        .placeholder = "e.g. 100% / Max",
        .clearable = true
    }).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showTicks, {.label = "Show Step Ticks", .disabled = s_state.continuous}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showLabels, {.label = "Show Step Labels", .disabled = s_state.continuous}).changed) ui.InvalidateAll();
}

void RenderSliderStage(guikit::Context& ui, DemoContext& ctx) {
    if (s_state.continuous) {
        ui.Slider(s_state.val, 0.0f, 100.0f, {
            .label = ctx.elemLabel,
            .textLeft = s_state.textLeft,
            .textRight = s_state.textRight,
            .description = ctx.GetElementDescription(),
            .icon = ctx.GetElementIcon(),
            .badge = ctx.GetElementBadge(),
            .valueFormat = "%.1f %%",
            .layout = ctx.elemLayout,
            .inlineOrder = ctx.elemInlineOrder,
            .elementAlign = ctx.elemAlign,
            .showValue = s_state.showValue,
            .valuePlacement = s_state.valuePlacement,
            .variant = ctx.btnVariant,
            .size = ctx.controlsSize,
            .stretch = ctx.GetElementStretch(),
            .width = ctx.GetElementWidth(),
            .divider = ctx.elemDivider,
            .dividerStyle = ctx.elemDividerStyle,
            .disabled = ctx.elemDisabled
        });
    } else {
        static constexpr std::array<std::string_view, 5> stepItems = {"Off", "Low", "Mid", "High", "Max"};
        ui.StepSlider(s_state.stepIdx, stepItems, {
            .label = ctx.elemLabel,
            .textLeft = s_state.textLeft,
            .textRight = s_state.textRight,
            .description = ctx.GetElementDescription(),
            .icon = ctx.GetElementIcon(),
            .badge = ctx.GetElementBadge(),
            .layout = ctx.elemLayout,
            .inlineOrder = ctx.elemInlineOrder,
            .elementAlign = ctx.elemAlign,
            .showTicks = s_state.showTicks,
            .showLabels = s_state.showLabels,
            .showValue = s_state.showValue,
            .valuePlacement = s_state.valuePlacement,
            .variant = ctx.btnVariant,
            .size = ctx.controlsSize,
            .stretch = ctx.GetElementStretch(),
            .width = ctx.GetElementWidth(),
            .divider = ctx.elemDivider,
            .dividerStyle = ctx.elemDividerStyle,
            .disabled = ctx.elemDisabled
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
            "    .icon = {},\n"
            "    .badge = {},\n"
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
            "    .divider = {},\n"
            "    .dividerStyle = guikit::ElementDivider::{},\n"
            "    .disabled = {}\n"
            "}});",
            ctx.elemLabel,
            s_state.textLeft,
            s_state.textRight,
            ctx.FormatElementDescCode(),
            ctx.FormatElementIconCode(),
            ctx.FormatElementBadgeCode(),
            ElementLayoutEnumName(ctx.elemLayout),
            InlineOrderEnumName(ctx.elemInlineOrder),
            LabelAlignEnumName(ctx.elemAlign),
            s_state.showValue ? "true" : "false",
            PlacementName(s_state.valuePlacement),
            VariantName(ctx.btnVariant),
            SizeName(ctx.controlsSize),
            StretchModeEnumName(ctx.GetElementStretch()),
            ctx.FormatElementWidthCode(),
            ctx.elemDivider ? "true" : "false",
            ctx.FormatDividerStyleCode(),
            ctx.elemDisabled ? "true" : "false"
        );
    }
    return std::format(
        "static constexpr std::array<std::string_view, 5> stepItems = {{\"Off\", \"Low\", \"Mid\", \"High\", \"Max\"}};\n"
        "ui.StepSlider(presetIndex, stepItems, {{\n"
        "    .label = \"{}\",\n"
        "    .textLeft = \"{}\",\n"
        "    .textRight = \"{}\",\n"
        "    .description = {},\n"
        "    .icon = {},\n"
        "    .badge = {},\n"
        "    .layout = guikit::ElementLayout::{},\n"
        "    .inlineOrder = guikit::InlineOrder::{},\n"
        "    .elementAlign = guikit::Align::{},\n"
        "    .showTicks = {},\n"
        "    .showLabels = {},\n"
        "    .showValue = {},\n"
        "    .valuePlacement = guikit::Placement::{},\n"
        "    .variant = guikit::UiVariant::{},\n"
        "    .size = guikit::UiSize::{},\n"
        "    .stretch = guikit::StretchMode::{},\n"
        "    .width = {},\n"
        "    .divider = {},\n"
        "    .dividerStyle = guikit::ElementDivider::{},\n"
        "    .disabled = {}\n"
        "}});",
        ctx.elemLabel,
        s_state.textLeft,
        s_state.textRight,
        ctx.FormatElementDescCode(),
        ctx.FormatElementIconCode(),
        ctx.FormatElementBadgeCode(),
        ElementLayoutEnumName(ctx.elemLayout),
        InlineOrderEnumName(ctx.elemInlineOrder),
        LabelAlignEnumName(ctx.elemAlign),
        s_state.showTicks ? "true" : "false",
        s_state.showLabels ? "true" : "false",
        s_state.showValue ? "true" : "false",
        PlacementName(s_state.valuePlacement),
        VariantName(ctx.btnVariant),
        SizeName(ctx.controlsSize),
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.FormatElementWidthCode(),
        ctx.elemDivider ? "true" : "false",
        ctx.FormatDividerStyleCode(),
        ctx.elemDisabled ? "true" : "false"
    );
}

} // namespace guikit::demo
