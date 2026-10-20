#include "../DemoContext.hpp"
#include <format>
#include <vector>

namespace guikit::demo {

namespace {
struct SelectState {
    int mode = 1; // 0 = Select, 1 = ComboBox (Searchable), 2 = Autocomplete (Editable), 3 = MultiSelect
    size_t idx = 0;
    std::string customText = "Option Alpha";
    std::vector<size_t> multiSelected = {0, 2, 5};
    std::vector<std::string_view> plainItems = {
        "Option Alpha",
        "Option Beta",
        "Option Gamma",
        "Option Delta",
        "Option Epsilon",
        "Signal Harmonic",
        "Sonar Beamformer",
        "Waterfall Spectrum"
    };
    std::string placeholder = "Select or search...";
    uint32_t maxItems = 5;
    bool clearable = true;
    bool richItems = true;
    bool hasPrefix = false;
};
SelectState s_state;

std::vector<guikit::SelectItem> BuildRichSelectItems() {
    return {
        {.text = "Option Alpha",       .prefix = guikit::Icon::Play(),     .suffix = "100 Hz"},
        {.text = "Option Beta",        .prefix = guikit::Icon::Settings(), .suffix = "250 Hz"},
        {.text = "Option Gamma",       .prefix = guikit::Icon::Flash(),    .suffix = "500 Hz"},
        {.text = "Option Delta",       .prefix = guikit::Icon::Search(),   .suffix = "1.0 kHz"},
        {.text = "Option Epsilon",     .prefix = guikit::Icon::Eye(),      .suffix = "2.4 kHz"},
        {.text = "Signal Harmonic",    .prefix = guikit::Icon::Refresh(),  .suffix = "DSP"},
        {.text = "Sonar Beamformer",   .prefix = guikit::Icon::Folder(),   .suffix = "DDS"},
        {.text = "Waterfall Spectrum", .prefix = guikit::Icon::Check(),    .suffix = "RTSA"}
    };
}
} // namespace

void RenderSelectInspector(guikit::Context& ui, DemoContext&) {
    if (auto el = ui.Element({.label = "Control Mode"})) {
        if (ui.Segmented(s_state.mode, {"Select", "ComboBox", "Autocomplete", "MultiSelect"}, {
            .stretch = guikit::StretchMode::Fill
        }).changed) {
            if (s_state.mode == 2 && s_state.customText.empty() && s_state.idx < s_state.plainItems.size()) {
                s_state.customText = std::string(s_state.plainItems[s_state.idx]);
            }
            ui.InvalidateAll();
        }
    }
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
    if (auto el = ui.Element({.label = "Clearable (X Button)"})) {
        if (ui.Switch(s_state.clearable).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Rich Items (Icons & Units)"})) {
        if (ui.Switch(s_state.richItems).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Default Prefix Icon"})) {
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
        guikit::SelectOptions opts{
            .placeholder = s_state.placeholder,
            .prefix = s_state.hasPrefix ? guikit::Icon::Search() : guikit::Icon{},
            .maxVisibleItems = s_state.maxItems,
            .stretch = ctx.GetElementStretch(),
            .width = ctx.GetElementWidth(),
            .height = ctx.GetElementHeight(),
            .background = ctx.GetControlBackground(NavCategory::ElemSelect),
            .border = ctx.GetCtrlBorder(),
            .clearable = s_state.clearable,
            .disabled = ctx.elemDisabled
        };

        const auto richList = BuildRichSelectItems();
        if (s_state.mode == 0) {
            if (s_state.richItems) {
                ui.Size(ctx.controlsSize).Select(s_state.idx, richList, opts);
            } else {
                ui.Size(ctx.controlsSize).Select(s_state.idx, s_state.plainItems, opts);
            }
        } else if (s_state.mode == 1) {
            if (s_state.richItems) {
                ui.Size(ctx.controlsSize).ComboBox(s_state.idx, richList, opts);
            } else {
                ui.Size(ctx.controlsSize).ComboBox(s_state.idx, s_state.plainItems, opts);
            }
        } else if (s_state.mode == 2) {
            if (s_state.richItems) {
                ui.Size(ctx.controlsSize).Autocomplete(s_state.customText, richList, opts);
            } else {
                ui.Size(ctx.controlsSize).Autocomplete(s_state.customText, s_state.plainItems, opts);
            }
        } else {
            opts.searchable = true;
            if (s_state.richItems) {
                ui.Size(ctx.controlsSize).MultiSelect(s_state.multiSelected, richList, opts);
            } else {
                ui.Size(ctx.controlsSize).MultiSelect(s_state.multiSelected, s_state.plainItems, opts);
            }
        }
    }
}

std::string GenerateSelectCode(const DemoContext& ctx) {
    const std::string_view widgetCall = (s_state.mode == 0)
        ? "Select(presetIndex"
        : ((s_state.mode == 1)
            ? "ComboBox(presetIndex"
            : ((s_state.mode == 2) ? "Autocomplete(customText" : "MultiSelect(selectedIndices"));
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
        "    ui.{}{}, itemsList, {{\n"
        "        .placeholder = \"{}\",\n"
        "        .prefix = {},\n"
        "        .maxVisibleItems = {},\n"
        "        .stretch = guikit::StretchMode::{},\n"
        "        .width = {},\n"
        "        .height = {},\n"
        "        .background = {},\n"
        "        .border = {},\n"
        "        .clearable = {},\n"
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
        widgetCall,
        s_state.placeholder,
        s_state.hasPrefix ? "guikit::Icon::Search()" : "{}",
        s_state.maxItems,
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.FormatElementWidthCode(),
        ctx.FormatElementHeightCode(),
        ctx.FormatControlBackgroundCode(NavCategory::ElemSelect),
        ctx.FormatCtrlBorderCode(),
        s_state.clearable ? "true" : "false",
        ctx.elemDisabled ? "true" : "false"
    );
}

} // namespace guikit::demo
