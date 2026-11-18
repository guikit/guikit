#include "../demo_context.hpp"

namespace guikit::demo {

namespace {

enum class ElementSlotPreset : keel::u8 {
    Switch = 0,
    Slider,
    SpinBox,
    InputField,
    Button,
    CompositeRow,
    Standalone
};

struct ElementStudioState {
    bool allLayoutsSpecimen = false;
    bool wrapInCard = true;
    ElementSlotPreset slotPreset = ElementSlotPreset::Switch;
    guikit::UiSize elemSize = guikit::UiSize::Md;
    guikit::UiSize slotSize = guikit::UiSize::Md;
    keel::SmallString<128> label{"Adaptive Low-Pass Filter"};
    guikit::UiVariant labelVariant = guikit::UiVariant::Default;
    keel::SmallString<160> description{"Attenuates high-frequency carrier harmonics above cutoff (-24 dB/oct)"};
    guikit::UiVariant descVariant = guikit::UiVariant::Muted;
    bool wordWrap = true;
    bool showIcon = true;
    keel::SmallString<64> badge{"DSP"};
    guikit::UiVariant badgeVariant = guikit::UiVariant::Primary;
    keel::usize layoutIdx = 0; // 0 = Auto, 1 = Stacked, 2 = Top, 3 = Bottom, 4 = Leading, 5 = Trailing
    guikit::Align align = guikit::Align::Start;
    keel::usize widthModeIdx = 0; // 0 = Auto, 1 = Fit, 2 = Stretch, 3 = Fixed
    float widthDp = 380.0f;
    bool disabled = false;

    // Interactive slot values
    bool singleToggle = true;
    keel::StaticArray<bool, 5> specimenToggles = {true, false, true, true, false};
    float sliderGainDb = -6.0f;
    keel::StaticArray<float, 5> specimenGains = {-3.0f, -6.0f, -9.0f, -4.5f, -12.0f};
    float cutoffHz = 2400.0f;
    keel::SmallString<128> endpointHost{"192.168.10.42:5005"};
    keel::SmallString<128> searchQuery{"fft_window_kaiser"};
    keel::u32 triggerCount = 0;
};

ElementStudioState s_state;

constexpr guikit::ElementLayout DefaultSlotLayout(ElementSlotPreset preset) noexcept {
    if (preset == ElementSlotPreset::Switch) {
        return guikit::ElementLayout::Trailing;
    }
    return guikit::ElementLayout::Stacked;
}

keel::Optional<guikit::ElementLayout> ResolveExplicitLayout() noexcept {
    switch (s_state.layoutIdx) {
        case 1:  return guikit::ElementLayout::Stacked;
        case 2:  return guikit::ElementLayout::Top;
        case 3:  return guikit::ElementLayout::Bottom;
        case 4:  return guikit::ElementLayout::Leading;
        case 5:  return guikit::ElementLayout::Trailing;
        default: return keel::kNullOpt;
    }
}

guikit::AxisSize ActiveElementWidth() noexcept {
    return DemoContext::ResolveAxisSize(s_state.widthModeIdx, s_state.widthDp);
}

guikit::ElementOptions BuildElementOptions(
    keel::StringRef customLabel,
    keel::StringRef customDesc,
    keel::StringRef customBadge,
    keel::Optional<guikit::ElementLayout> layoutOverride
) noexcept {
    return guikit::ElementOptions{
        .label = customLabel.empty()
            ? guikit::UiText{}
            : guikit::UiText{customLabel, s_state.labelVariant},
        .description = customDesc.empty()
            ? guikit::UiText{}
            : guikit::UiText{
                customDesc,
                s_state.descVariant,
                s_state.wordWrap ? guikit::TextWrap::Word() : guikit::TextWrap::SingleLine()
            },
        .icon = s_state.showIcon ? guikit::Icon::Flash() : guikit::Icon{},
        .badge = customBadge.empty()
            ? guikit::UiText{}
            : guikit::UiText{customBadge, s_state.badgeVariant},
        .layout = layoutOverride,
        .align = s_state.align,
        .width = ActiveElementWidth(),
        .size = s_state.elemSize,
        .disabled = s_state.disabled
    };
}

void RenderSlotControl(guikit::Context& ui, ElementSlotPreset preset, keel::usize specimenIdx) {
    switch (preset) {
        case ElementSlotPreset::Switch: {
            bool& flag = (specimenIdx < s_state.specimenToggles.size())
                ? s_state.specimenToggles[specimenIdx]
                : s_state.singleToggle;
            ui.Switch(flag, {.size = s_state.slotSize});
            break;
        }
        case ElementSlotPreset::Slider: {
            float& val = (specimenIdx < s_state.specimenGains.size())
                ? s_state.specimenGains[specimenIdx]
                : s_state.sliderGainDb;
            ui.Slider(val, -48.0f, 12.0f, {.valueFormat = "%.1f dB", .size = s_state.slotSize});
            break;
        }
        case ElementSlotPreset::SpinBox: {
            ui.SpinBox(s_state.cutoffHz, 20.0f, 20000.0f, 50.0f, {
                .suffix = {"Hz"},
                .valueFormat = "%.0f",
                .size = s_state.slotSize
            });
            break;
        }
        case ElementSlotPreset::InputField: {
            ui.InputField(s_state.endpointHost, {
                .placeholder = "host:port",
                .clearable = true,
                .size = s_state.slotSize
            });
            break;
        }
        case ElementSlotPreset::Button: {
            if (ui.Button({
                .text = "Calibrate Stage",
                .icon = guikit::Icon::Play(),
                .color = guikit::StateColor::Primary(),
                .size = s_state.slotSize
            })) {
                ++s_state.triggerCount;
            }
            break;
        }
        case ElementSlotPreset::CompositeRow: {
            if (auto row = ui.Row({.gap = 8.0f, .align = guikit::Alignment::Center})) {
                ui.InputField(s_state.searchQuery, {
                    .placeholder = "Filter symbol...",
                    .prefix = {guikit::Icon::Search()},
                    .clearable = true,
                    .size = s_state.slotSize
                });
                if (ui.Button({
                    .text = "Apply",
                    .color = guikit::StateColor::Primary(),
                    .size = s_state.slotSize
                })) {
                    ++s_state.triggerCount;
                }
            }
            break;
        }
        case ElementSlotPreset::Standalone:
            break;
    }
}

keel::String FormatSlotControlCode(ElementSlotPreset preset) {
    const bool hasSlotSize = (s_state.slotSize != guikit::UiSize::None && s_state.slotSize != guikit::UiSize::Md);
    const keel::FixedString<48> szInline = hasSlotSize
        ? keel::format_fixed<48>(", .size = guikit::UiSize::{}", SizeName(s_state.slotSize))
        : keel::FixedString<48>{};
    switch (preset) {
        case ElementSlotPreset::Switch:
            return hasSlotSize
                ? keel::format("ui.Switch(enabled, {{.size = guikit::UiSize::{}}});", SizeName(s_state.slotSize))
                : keel::String::from("ui.Switch(enabled);");
        case ElementSlotPreset::Slider:
            return keel::format("ui.Slider(gainDb, -48.0f, 12.0f, {{.valueFormat = \"%.1f dB\"{}}});", szInline);
        case ElementSlotPreset::SpinBox:
            return keel::format(
                "ui.SpinBox(cutoffHz, 20.0f, 20000.0f, 50.0f, {{.suffix = {{\"Hz\"}}, .valueFormat = \"%.0f\"{}}});",
                szInline
            );
        case ElementSlotPreset::InputField:
            return keel::format(
                "ui.InputField(endpointHost, {{.placeholder = \"host:port\", .clearable = true{}}});",
                szInline
            );
        case ElementSlotPreset::Button:
            return keel::format(
                "ui.Button({{.text = \"Calibrate Stage\", .icon = guikit::Icon::Play(), .color = guikit::StateColor::Primary(){}}});",
                szInline
            );
        case ElementSlotPreset::CompositeRow:
            return keel::format(
                "if (auto row = ui.Row({{.gap = 8.0f, .align = guikit::Alignment::Center}})) {{\n"
                "    ui.InputField(searchQuery, {{.placeholder = \"Filter symbol...\", .prefix = {{guikit::Icon::Search()}}, .clearable = true{}}});\n"
                "    ui.Button({{.text = \"Apply\", .color = guikit::StateColor::Primary(){}}});\n"
                "}}",
                szInline,
                szInline
            );
        case ElementSlotPreset::Standalone:
            return {};
    }
    return {};
}

void AppendIndentedLines(keel::String& out, keel::StringRef block, keel::StringRef indent) {
    keel::usize start = 0;
    while (start <= block.size()) {
        keel::usize end = start;
        while (end < block.size() && block[end] != '\n') {
            ++end;
        }
        const keel::StringRef line = block.substr(start, end - start);
        if (!line.empty()) {
            out.append(indent);
            out.append(line);
        }
        out.push_back('\n');
        if (end == block.size()) {
            break;
        }
        start = end + 1;
    }
}

} // namespace

void RenderElementInspector(guikit::Context& ui, DemoContext& ctx) {
    if (auto el = ui.Element({
        .label = "All 5 Layouts Specimen",
        .description = "Compare Stacked, Top, Bottom, Leading & Trailing side-by-side"
    })) {
        if (ui.Switch(s_state.allLayoutsSpecimen).changed) {
            ui.InvalidateAll();
        }
    }

    static constexpr keel::StaticArray<keel::StringRef, 7> kSlotNames = {
        "Switch (Trailing default)",
        "Slider (Stacked default)",
        "SpinBox (Numeric Stepper)",
        "InputField (Text Input)",
        "Button (Compact Fit)",
        "Composite ui.Row (Input + Button)",
        "Standalone (No Control Slot)"
    };
    keel::usize presetIdx = static_cast<keel::usize>(s_state.slotPreset);
    if (auto el = ui.Element({.label = "Control Slot Content"})) {
        if (ui.Select(presetIdx, kSlotNames).changed) {
            s_state.slotPreset = static_cast<ElementSlotPreset>(presetIdx);
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Preview on Surface Card"})) {
        if (ui.Switch(s_state.wrapInCard).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto elemAcc = ui.Accordion({
        .label = "ELEMENT",
        .defaultOpen = true,
        .size = guikit::UiSize::Sm
    })) {
        if (auto el = ui.Element({.label = "Size"})) {
            if (ui.StepSlider(s_state.elemSize, {.showTicks = true, .showLabels = true}).changed) {
                ui.InvalidateAll();
            }
        }

        if (auto el = ui.Element({.label = "Label", .disabled = s_state.allLayoutsSpecimen})) {
            if (ui.InputField(s_state.label, {
                .placeholder = "No label...",
                .clearable = true,
                .disabled = s_state.allLayoutsSpecimen
            }).changed) {
                ui.InvalidateAll();
            }
        }

        const bool labelColorDisabled = (!s_state.allLayoutsSpecimen && s_state.label.empty() && !s_state.showIcon);
        if (auto el = ui.Element({.label = "Label Color", .disabled = labelColorDisabled})) {
            if (ui.Select(s_state.labelVariant, kUiVariantNames, {
                .disabled = labelColorDisabled
            }).changed) {
                ui.InvalidateAll();
            }
        }

        if (auto el = ui.Element({.label = "Description", .disabled = s_state.allLayoutsSpecimen})) {
            if (ui.InputField(s_state.description, {
                .placeholder = "Optional helper description...",
                .clearable = true,
                .disabled = s_state.allLayoutsSpecimen
            }).changed) {
                ui.InvalidateAll();
            }
        }
        ctx.RenderDescVariantPicker(ui, s_state.descVariant, s_state.allLayoutsSpecimen || !s_state.description.empty());
        if (auto el = ui.Element({
            .label = "Word Wrap",
            .disabled = (!s_state.allLayoutsSpecimen && s_state.description.empty())
        })) {
            if (ui.Switch(s_state.wordWrap, {
                .disabled = (!s_state.allLayoutsSpecimen && s_state.description.empty())
            }).changed) {
                ui.InvalidateAll();
            }
        }

        if (auto el = ui.Element({.label = "Leading Icon"})) {
            if (ui.Switch(s_state.showIcon).changed) {
                ui.InvalidateAll();
            }
        }

        if (auto el = ui.Element({.label = "Badge", .disabled = s_state.allLayoutsSpecimen})) {
            if (ui.InputField(s_state.badge, {
                .placeholder = "No badge (e.g. DSP, dB)...",
                .clearable = true,
                .disabled = s_state.allLayoutsSpecimen
            }).changed) {
                ui.InvalidateAll();
            }
        }
        if (auto el = ui.Element({
            .label = "Badge Variant",
            .disabled = (!s_state.allLayoutsSpecimen && s_state.badge.empty())
        })) {
            if (ui.Select(s_state.badgeVariant, kUiVariantNames, {
                .disabled = (!s_state.allLayoutsSpecimen && s_state.badge.empty())
            }).changed) {
                ui.InvalidateAll();
            }
        }

        if (auto el = ui.Element({.label = "Disabled"})) {
            if (ui.Switch(s_state.disabled).changed) {
                ui.InvalidateAll();
            }
        }
    }

    if (auto elemLayoutAcc = ui.Accordion({
        .label = "ELEMENT LAYOUT",
        .defaultOpen = true,
        .size = guikit::UiSize::Sm
    })) {
        const auto explicitLayout = ResolveExplicitLayout();
        const guikit::ElementLayout effLayout = explicitLayout.value_or(DefaultSlotLayout(s_state.slotPreset));
        if (auto el = ui.Element({
            .label = "Layout",
            .badge = {
                ElementLayoutEnumName(effLayout),
                explicitLayout.has_value() ? guikit::UiVariant::Primary : guikit::UiVariant::Secondary
            },
            .disabled = s_state.allLayoutsSpecimen
        })) {
            if (ui.Select(s_state.layoutIdx, kElemLayoutNames, {
                .disabled = s_state.allLayoutsSpecimen
            }).changed) {
                ui.InvalidateAll();
            }
        }

        keel::usize alignIdx = static_cast<keel::usize>(s_state.align);
        if (auto el = ui.Element({.label = "Alignment"})) {
            if (ui.Segmented(alignIdx, kLabelAlignNames).changed) {
                s_state.align = static_cast<guikit::Align>(alignIdx);
                ui.InvalidateAll();
            }
        }

        if (auto el = ui.Element({.label = "Width"})) {
            if (ui.Segmented(s_state.widthModeIdx, kAxisModeNames).changed) {
                ui.InvalidateAll();
            }
        }

        const bool fixedWidth = (s_state.widthModeIdx == 3);
        if (auto el = ui.Element({.label = "Width (dp)", .disabled = !fixedWidth})) {
            if (ui.Slider(s_state.widthDp, 160.0f, 560.0f, {
                .valueFormat = "%.0f dp",
                .step = 10.0f,
                .disabled = !fixedWidth
            }).changed) {
                ui.InvalidateAll();
            }
        }
    }

    const bool hasControl = (s_state.slotPreset != ElementSlotPreset::Standalone);
    if (auto slotStyleAcc = ui.Accordion({
        .label = "CONTROL STYLE",
        .defaultOpen = false,
        .size = guikit::UiSize::Sm,
        .disabled = !hasControl
    })) {
        if (auto el = ui.Element({.label = "Size", .disabled = !hasControl})) {
            if (ui.StepSlider(s_state.slotSize, {
                .showTicks = true,
                .showLabels = true,
                .disabled = !hasControl
            }).changed) {
                ui.InvalidateAll();
            }
        }
    }
}

void RenderElementStage(guikit::Context& ui, DemoContext&) {
    auto renderContent = [&]() {
        if (s_state.allLayoutsSpecimen) {
            struct LayoutSpecimen {
                guikit::ElementLayout layout;
                keel::StringRef badge;
                keel::StringRef title;
                keel::StringRef desc;
            };
            static constexpr keel::StaticArray<LayoutSpecimen, 5> kSpecimens = {{
                {
                    guikit::ElementLayout::Stacked,
                    "Stacked",
                    "Label Above, Description Below",
                    "Classic vertical form-field sandwich: Label -> [Slot] -> Description"
                },
                {
                    guikit::ElementLayout::Top,
                    "Top",
                    "Header Block Above Slot",
                    "Both Label and Description grouped together above the control slot"
                },
                {
                    guikit::ElementLayout::Bottom,
                    "Bottom",
                    "Caption Block Below Slot",
                    "Control slot placed first with Label and Description underneath"
                },
                {
                    guikit::ElementLayout::Leading,
                    "Leading",
                    "Leading Text + Trailing Slot",
                    "Settings row: Label & Description on the left, control anchored right"
                },
                {
                    guikit::ElementLayout::Trailing,
                    "Trailing",
                    "Leading Slot + Trailing Text",
                    "Toggle / Checkbox idiom: control on the left, Label & Description on the right"
                }
            }};

            if (auto col = ui.Column({.gap = 16.0f})) {
                for (keel::usize i = 0; i < kSpecimens.size(); ++i) {
                    const auto& item = kSpecimens[i];
                    const guikit::ElementOptions opts = BuildElementOptions(
                        item.title,
                        item.desc,
                        item.badge,
                        item.layout
                    );
                    if (s_state.slotPreset == ElementSlotPreset::Standalone) {
                        ui.Element(opts);
                    } else if (auto el = ui.Element(opts)) {
                        RenderSlotControl(ui, s_state.slotPreset, i);
                    }
                }
            }
            return;
        }

        const guikit::ElementOptions opts = BuildElementOptions(
            s_state.label.as_ref(),
            s_state.description.as_ref(),
            s_state.badge.as_ref(),
            ResolveExplicitLayout()
        );
        if (s_state.slotPreset == ElementSlotPreset::Standalone) {
            ui.Element(opts);
        } else if (auto el = ui.Element(opts)) {
            RenderSlotControl(ui, s_state.slotPreset, 99);
        }
    };

    if (s_state.wrapInCard) {
        if (auto card = ui.Card({
            .minWidth = 340.0f,
            .maxWidth = 560.0f,
            .gap = 14.0f,
            .padding = guikit::Padding::All(18.0f)
        })) {
            renderContent();
        }
    } else {
        renderContent();
    }
}

keel::String GenerateElementCode(const DemoContext&) {
    const keel::String labelCode = s_state.label.empty()
        ? keel::String::from("\"\"")
        : ((s_state.labelVariant == guikit::UiVariant::Default)
            ? keel::format("\"{}\"", s_state.label)
            : keel::format("{{\"{}\", guikit::StateColor::{}()}}", s_state.label, VariantName(s_state.labelVariant)));

    keel::String elemOpen = keel::format(
        "{}ui.Element({{\n"
        "    .label = {}",
        (s_state.slotPreset == ElementSlotPreset::Standalone) ? "" : "if (auto el = ",
        labelCode
    );

    if (!s_state.description.empty()) {
        keel::format_into(
            elemOpen,
            ",\n    .description = {}",
            FormatDesc(true, s_state.description, s_state.descVariant)
        );
    }
    if (s_state.showIcon) {
        elemOpen.append(",\n    .icon = guikit::Icon::Flash()");
    }
    if (!s_state.badge.empty()) {
        keel::format_into(
            elemOpen,
            ",\n    .badge = {{\"{}\", guikit::StateColor::{}()}}",
            s_state.badge,
            VariantName(s_state.badgeVariant)
        );
    }
    if (const auto layoutOpt = ResolveExplicitLayout(); layoutOpt.has_value()) {
        keel::format_into(
            elemOpen,
            ",\n    .layout = guikit::ElementLayout::{}",
            ElementLayoutEnumName(*layoutOpt)
        );
    }
    if (s_state.align != guikit::Align::Start) {
        keel::format_into(elemOpen, ",\n    .align = guikit::Align::{}", LabelAlignEnumName(s_state.align));
    }
    if (const guikit::AxisSize elemW = ActiveElementWidth(); !elemW.IsAuto()) {
        keel::format_into(elemOpen, ",\n    .width = {}", DemoContext::FormatAxisSizeCode(elemW));
    }
    if (s_state.elemSize != guikit::UiSize::None && s_state.elemSize != guikit::UiSize::Md) {
        keel::format_into(elemOpen, ",\n    .size = guikit::UiSize::{}", SizeName(s_state.elemSize));
    }
    if (s_state.disabled) {
        elemOpen.append(",\n    .disabled = true");
    }

    keel::String stmt;
    if (s_state.slotPreset == ElementSlotPreset::Standalone) {
        elemOpen.append("\n});");
        stmt = static_cast<keel::String&&>(elemOpen);
    } else {
        elemOpen.append("\n})) {\n");
        const keel::String slotCode = FormatSlotControlCode(s_state.slotPreset);
        AppendIndentedLines(elemOpen, slotCode.as_ref(), "    ");
        elemOpen.push_back('}');
        stmt = static_cast<keel::String&&>(elemOpen);
    }

    if (!s_state.wrapInCard) {
        return stmt;
    }

    keel::String out = keel::String::from(
        "if (auto card = ui.Card({.minWidth = 340.0f, .maxWidth = 560.0f, .gap = 14.0f, .padding = guikit::Padding::All(18.0f)})) {\n"
    );
    AppendIndentedLines(out, stmt.as_ref(), "    ");
    out.push_back('}');
    return out;
}

} // namespace guikit::demo
