#include "../DemoContext.hpp"
#include "guikit/widgets/TagInput.hpp"
#include <array>
#include <format>
#include <string>
#include <vector>

namespace guikit::demo {

namespace {

struct TagInputDemoState {
    std::vector<std::string> tags = {"C++20", "Vulkan", "Immediate-Mode", "Zero-Alloc"};
    std::string placeholder = "Add tag...";
    std::string prefix = "";
    guikit::UiVariant tagVariant = guikit::UiVariant::Primary;
    int maxTags = 8;
    bool allowDuplicates = false;
    bool splitOnComma = true;
    bool splitOnSpace = false;
    bool clearable = true;
    bool showCounter = true;
    bool readOnly = false;
    size_t sampleCounter = 0;
};

TagInputDemoState s_state;

constexpr std::array<std::string_view, 8> kTagVariantLabels = {
    "Pri", "Sec", "Suc", "Wrn", "Dgr", "Inf", "Gho", "Mut"
};

constexpr std::array<guikit::UiVariant, 8> kTagVariants = {
    guikit::UiVariant::Primary,
    guikit::UiVariant::Secondary,
    guikit::UiVariant::Success,
    guikit::UiVariant::Warning,
    guikit::UiVariant::Danger,
    guikit::UiVariant::Info,
    guikit::UiVariant::Ghost,
    guikit::UiVariant::Muted
};

constexpr std::array<std::string_view, 8> kSampleTags = {
    "SIMD", "WebGPU", "SDF-Fonts", "Flexbox", "Reactive", "Telemetry", "Hot-Reload", "Low-Latency"
};

} // namespace

void RenderTagInputInspector(guikit::Context& ui, DemoContext&) {
    size_t varIdx = 0;
    for (size_t i = 0; i < kTagVariants.size(); ++i) {
        if (kTagVariants[i] == s_state.tagVariant) {
            varIdx = i;
            break;
        }
    }
    if (auto el = ui.Element({.label = "Tag Variant"})) {
        if (ui.StepSlider(varIdx, kTagVariantLabels, {
            .showTicks = true,
            .showLabels = true,
            .color = guikit::StateColor::FromVariant(s_state.tagVariant)
        }).changed) {
            s_state.tagVariant = kTagVariants[varIdx];
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Placeholder"})) {
        if (ui.InputField(s_state.placeholder, {
            .placeholder = "No placeholder...",
            .clearable = true
        }).changed) ui.InvalidateAll();
    }

    if (auto el = ui.Element({.label = "Prefix Text"})) {
        if (ui.InputField(s_state.prefix, {
            .placeholder = "e.g. Tags:",
            .clearable = true
        }).changed) ui.InvalidateAll();
    }

    if (auto el = ui.Element({.label = "Max Tags (0 = Unlimited)"})) {
        if (ui.Slider(s_state.maxTags, 0, 12, {
            .valueFormat = (s_state.maxTags == 0)
                ? guikit::UiText{"Unlimited", {"Unlimited", "%d tags"}}
                : guikit::UiText{"%d tags", {"Unlimited", "%d tags"}}
        }).changed) ui.InvalidateAll();
    }

    if (auto el = ui.Element({.label = "Split on Comma / Semicolon"})) {
        if (ui.Switch(s_state.splitOnComma).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Split on Space"})) {
        if (ui.Switch(s_state.splitOnSpace).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Allow Duplicates"})) {
        if (ui.Switch(s_state.allowDuplicates).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Clearable Button ('X')"})) {
        if (ui.Switch(s_state.clearable).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Show Counter Badge"})) {
        if (ui.Switch(s_state.showCounter).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Read Only"})) {
        if (ui.Switch(s_state.readOnly).changed) ui.InvalidateAll();
    }

    if (auto row = ui.Row({.gap = 8.0f})) {
        if (ui.Button({
            .text = "Add Sample Tag",
            .icon = guikit::Icon::Plus(),
            .stretch = guikit::StretchMode::Fill,
            .background = guikit::StateColor::Secondary()
        }).clicked) {
            const std::string_view candidate = kSampleTags[s_state.sampleCounter % kSampleTags.size()];
            ++s_state.sampleCounter;
            if (s_state.maxTags == 0 || s_state.tags.size() < static_cast<size_t>(s_state.maxTags)) {
                s_state.tags.emplace_back(candidate);
                ui.InvalidateAll();
            }
        }
        if (ui.Button({
            .text = "Reset Default Tags",
            .icon = guikit::Icon::Refresh(),
            .stretch = guikit::StretchMode::Fill,
            .background = guikit::StateColor::Ghost()
        }).clicked) {
            s_state.tags = {"C++20", "Vulkan", "Immediate-Mode", "Zero-Alloc"};
            ui.InvalidateAll();
        }
    }
}

void RenderTagInputStage(guikit::Context& ui, DemoContext& ctx) {
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
        if (auto sz = ui.Size(ctx.controlsSize)) {
            (void)guikit::TagInput::Run(ui, s_state.tags, {
                .placeholder = s_state.placeholder,
                .prefix = s_state.prefix,
                .tagVariant = s_state.tagVariant,
                .maxTags = static_cast<uint32_t>(s_state.maxTags),
                .allowDuplicates = s_state.allowDuplicates,
                .splitOnComma = s_state.splitOnComma,
                .splitOnSpace = s_state.splitOnSpace,
                .clearable = s_state.clearable,
                .showCounter = s_state.showCounter,
                .readOnly = s_state.readOnly,
                .stretch = ctx.GetElementStretch(),
                .width = ctx.GetElementWidth(),
                .height = ctx.GetElementHeight(),
                .background = ctx.GetControlBackground(NavCategory::ElemTagInput),
                .border = ctx.GetCtrlBorder(),
                .disabled = ctx.elemDisabled
            });
        }
    }
}

std::string GenerateTagInputCode(const DemoContext& ctx) {
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
        "    ui.{}TagInput(tags, {{\n"
        "        .placeholder = \"{}\",\n"
        "        .prefix = \"{}\",\n"
        "        .tagVariant = guikit::UiVariant::{},\n"
        "        .maxTags = {},\n"
        "        .allowDuplicates = {},\n"
        "        .splitOnComma = {},\n"
        "        .splitOnSpace = {},\n"
        "        .clearable = {},\n"
        "        .showCounter = {},\n"
        "        .readOnly = {},\n"
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
        s_state.prefix,
        VariantName(s_state.tagVariant),
        s_state.maxTags,
        s_state.allowDuplicates ? "true" : "false",
        s_state.splitOnComma ? "true" : "false",
        s_state.splitOnSpace ? "true" : "false",
        s_state.clearable ? "true" : "false",
        s_state.showCounter ? "true" : "false",
        s_state.readOnly ? "true" : "false",
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.FormatElementWidthCode(),
        ctx.FormatElementHeightCode(),
        ctx.FormatControlBackgroundCode(NavCategory::ElemTagInput),
        ctx.FormatCtrlBorderCode(),
        ctx.elemDisabled ? "true" : "false"
    );
}

} // namespace guikit::demo
