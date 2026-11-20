#include "../demo_context.hpp"

namespace guikit::demo {

namespace {
inline constexpr keel::StaticArray<keel::StringRef, 8> kPlainItems = {
    "Option Alpha",
    "Option Beta",
    "Option Gamma",
    "Option Delta",
    "Option Epsilon",
    "Signal Harmonic",
    "Sonar Beamformer",
    "Waterfall Spectrum"
};

struct SelectState {
    keel::i32 mode = 1; // 0 = Select, 1 = ComboBox (Searchable), 2 = Autocomplete (Editable), 3 = MultiSelect
    keel::usize idx = 0;
    keel::SmallString<128> customText{"Option Alpha"};
    keel::SmallArray<keel::usize, 8> multiSelected = keel::SmallArray<keel::usize, 8>::from({0, 2, 5});
    keel::SmallString<128> placeholder{"Select or search..."};
    keel::u32 maxItems = 5;
    bool clearable = true;
    bool richItems = true;
    bool hasPrefix = false;
};
SelectState s_state;

keel::StaticArray<guikit::SelectItem, 8> BuildRichSelectItems() {
    return {{
        {.text = "Option Alpha",       .prefix = guikit::Icon::Play(),     .suffix = "100 Hz"},
        {.text = "Option Beta",        .prefix = guikit::Icon::Settings(), .suffix = "250 Hz"},
        {.text = "Option Gamma",       .prefix = guikit::Icon::Flash(),    .suffix = "500 Hz"},
        {.text = "Option Delta",       .prefix = guikit::Icon::Search(),   .suffix = "1.0 kHz"},
        {.text = "Option Epsilon",     .prefix = guikit::Icon::Eye(),      .suffix = "2.4 kHz"},
        {.text = "Signal Harmonic",    .prefix = guikit::Icon::Refresh(),  .suffix = "DSP"},
        {.text = "Sonar Beamformer",   .prefix = guikit::Icon::Folder(),   .suffix = "DDS"},
        {.text = "Waterfall Spectrum", .prefix = guikit::Icon::Check(),    .suffix = "RTSA"}
    }};
}
} // namespace

void RenderSelectInspector(guikit::Context& ui, DemoContext&) {
    static constexpr keel::StaticArray<keel::StringRef, 4> kControlModes = {
        "Select", "ComboBox", "Autocomplete", "MultiSelect"
    };
    if (auto el = ui.Element({.label = "Control Mode"})) {
        if (ui.Select(s_state.mode, kControlModes).changed) {
            if (s_state.mode == 2 && s_state.customText.empty() && s_state.idx < kPlainItems.size()) {
                s_state.customText.assign(kPlainItems[s_state.idx]);
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
            s_state.maxItems = static_cast<keel::u32>(maxItemsFloat);
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
    ctx.RunElementStage(ui, [&]() {
        guikit::SelectOptions opts{};
        opts.placeholder = s_state.placeholder;
        opts.prefix = s_state.hasPrefix ? guikit::Icon::Search() : guikit::Icon{};
        opts.maxVisibleItems = s_state.maxItems;
        opts.align = ctx.GetCtrlAlign(NavCategory::ElemSelect);
        opts.clearable = s_state.clearable;
        opts.disabled = ctx.elemDisabled;
        opts.size = ctx.controlsSize;
        opts.width = ctx.ResolveCtrlWidth();
        opts.height = ctx.ResolveCtrlHeight();
        opts.background = ctx.GetControlBackground(NavCategory::ElemSelect);
        opts.border = ctx.GetCtrlBorder();

        const auto richList = BuildRichSelectItems();
        if (s_state.mode == 0) {
            if (s_state.richItems) {
                ui.Select(s_state.idx, richList, opts);
            } else {
                ui.Select(s_state.idx, kPlainItems, opts);
            }
        } else if (s_state.mode == 1) {
            if (s_state.richItems) {
                ui.ComboBox(s_state.idx, richList, opts);
            } else {
                ui.ComboBox(s_state.idx, kPlainItems, opts);
            }
        } else if (s_state.mode == 2) {
            if (s_state.richItems) {
                ui.Autocomplete(s_state.customText, richList, opts);
            } else {
                ui.Autocomplete(s_state.customText, kPlainItems, opts);
            }
        } else {
            opts.searchable = true;
            if (s_state.richItems) {
                ui.MultiSelect(s_state.multiSelected, richList, opts);
            } else {
                ui.MultiSelect(s_state.multiSelected, kPlainItems, opts);
            }
        }
    });
}

keel::String GenerateSelectCode(const DemoContext& ctx) {
    const keel::StringRef widgetCall = (s_state.mode == 0)
        ? "Select(presetIndex"
        : ((s_state.mode == 1)
            ? "ComboBox(presetIndex"
            : ((s_state.mode == 2) ? "Autocomplete(customText" : "MultiSelect(selectedIndices"));
    const guikit::Align ctrlAlign = ctx.GetCtrlAlign(NavCategory::ElemSelect);
    const bool hasCustomSize = (ctx.controlsSize != guikit::UiSize::None && ctx.controlsSize != guikit::UiSize::Md);
    const keel::FixedString<64> sizeLine = hasCustomSize
        ? keel::format_fixed<64>("    .size = guikit::UiSize::{},\n", SizeName(ctx.controlsSize))
        : keel::FixedString<64>{};

    const keel::String ctrl = keel::format(
        "ui.{}, itemsList, {{\n"
        "    .placeholder = \"{}\",\n"
        "    .prefix = {},\n"
        "    .maxVisibleItems = {},\n"
        "    .align = guikit::Align::{},\n"
        "    .clearable = {},\n"
        "    .disabled = {},\n"
        "{}"
        "    .width = {},\n"
        "    .height = {},\n"
        "    .background = {},\n"
        "    .border = {}\n"
        "}});",
        widgetCall,
        s_state.placeholder,
        s_state.hasPrefix ? "guikit::Icon::Search()" : "{}",
        s_state.maxItems,
        AlignName(ctrlAlign),
        s_state.clearable ? "true" : "false",
        ctx.elemDisabled ? "true" : "false",
        sizeLine,
        ctx.FormatCtrlWidthCode(),
        ctx.FormatCtrlHeightCode(),
        ctx.FormatControlBackgroundCode(NavCategory::ElemSelect),
        ctx.FormatCtrlBorderCode()
    );
    return ctx.WrapInElementCode(ctrl);
}

} // namespace guikit::demo
