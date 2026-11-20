#include "../demo_context.hpp"

namespace guikit::demo {

namespace {

enum class ElementSlotPreset : keel::u8 {
    InputField = 0,
    Switch,
    CompositeRow,
    Standalone
};

struct ElementStudioState {
    ElementSlotPreset slotPreset = ElementSlotPreset::InputField;
    keel::usize layoutIdx = 0; // 0 = Auto, 1 = Stacked, 2 = Top, 3 = Bottom, 4 = Leading, 5 = Trailing
    guikit::UiSize elemSize = guikit::UiSize::Md;
    guikit::Align align = guikit::Align::Start;
    keel::usize widthModeIdx = 0; // 0 = Auto, 1 = Fit, 2 = Stretch, 3 = Fixed
    float widthDp = 380.0f;
    bool showDescription = true;
    bool showIconAndBadge = true;
    bool wrapInCard = true;
    bool disabled = false;

    // Interactive slot state
    keel::SmallString<128> endpointHost{"192.168.10.42:5005"};
    bool switchEnabled = true;
    keel::SmallString<128> searchQuery{"fft_window_kaiser"};
    keel::u32 triggerCount = 0;
};

ElementStudioState s_state;

constexpr keel::StringRef kSampleLabel = "Signal Processing Stage";
constexpr keel::StringRef kSampleDesc =
    "Attenuates high-frequency carrier harmonics above cutoff (-24 dB/oct)";
constexpr keel::StringRef kSampleBadge = "DSP";

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

guikit::ElementOptions BuildElementOptions() noexcept {
    return guikit::ElementOptions{
        .label = kSampleLabel,
        .description = s_state.showDescription
            ? guikit::UiText{kSampleDesc, guikit::UiVariant::Muted, guikit::TextWrap::Word()}
            : guikit::UiText{},
        .icon = s_state.showIconAndBadge ? guikit::Icon::Flash() : guikit::Icon{},
        .badge = s_state.showIconAndBadge
            ? guikit::UiText{kSampleBadge, guikit::UiVariant::Primary}
            : guikit::UiText{},
        .layout = ResolveExplicitLayout(),
        .align = s_state.align,
        .width = ActiveElementWidth(),
        .size = s_state.elemSize,
        .disabled = s_state.disabled
    };
}

void RenderSlotControl(guikit::Context& ui, ElementSlotPreset preset) {
    switch (preset) {
        case ElementSlotPreset::InputField: {
            ui.InputField(s_state.endpointHost, {
                .placeholder = "host:port",
                .clearable = true
            });
            break;
        }
        case ElementSlotPreset::Switch: {
            ui.Switch(s_state.switchEnabled);
            break;
        }
        case ElementSlotPreset::CompositeRow: {
            if (auto row = ui.Row({.gap = 8.0f, .align = guikit::Alignment::Center})) {
                ui.InputField(s_state.searchQuery, {
                    .placeholder = "Filter symbol...",
                    .prefix = {guikit::Icon::Search()},
                    .clearable = true
                });
                if (ui.Button({
                    .text = "Apply",
                    .color = guikit::StateColor::Primary()
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
    switch (preset) {
        case ElementSlotPreset::InputField:
            return keel::String::from(
                "ui.InputField(endpointHost, {.placeholder = \"host:port\", .clearable = true});"
            );
        case ElementSlotPreset::Switch:
            return keel::String::from("ui.Switch(enabled);");
        case ElementSlotPreset::CompositeRow:
            return keel::String::from(
                "if (auto row = ui.Row({.gap = 8.0f, .align = guikit::Alignment::Center})) {\n"
                "    ui.InputField(searchQuery, {.placeholder = \"Filter symbol...\", .prefix = {guikit::Icon::Search()}, .clearable = true});\n"
                "    ui.Button({.text = \"Apply\", .color = guikit::StateColor::Primary()});\n"
                "}"
            );
        case ElementSlotPreset::Standalone:
            return {};
    }
    return {};
}

} // namespace

void RenderElementInspector(guikit::Context& ui, DemoContext&) {
    static constexpr keel::StaticArray<keel::StringRef, 4> kSlotNames = {
        "InputField (Stretch)",
        "Switch (Compact)",
        "Composite ui.Row",
        "Standalone (No Slot)"
    };
    keel::usize presetIdx = static_cast<keel::usize>(s_state.slotPreset);
    if (auto el = ui.Element({.label = "Control Slot"})) {
        if (ui.Select(presetIdx, kSlotNames).changed) {
            s_state.slotPreset = static_cast<ElementSlotPreset>(presetIdx);
            ui.InvalidateAll();
        }
    }

    const auto explicitLayout = ResolveExplicitLayout();
    const guikit::ElementLayout effLayout = explicitLayout.value_or(DefaultSlotLayout(s_state.slotPreset));
    if (auto el = ui.Element({
        .label = "Layout",
        .badge = {
            ElementLayoutEnumName(effLayout),
            explicitLayout.has_value() ? guikit::UiVariant::Primary : guikit::UiVariant::Secondary
        }
    })) {
        if (ui.Select(s_state.layoutIdx, kElemLayoutNames).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Size"})) {
        if (ui.StepSlider(s_state.elemSize, {.showTicks = true, .showLabels = true}).changed) {
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

    if (auto el = ui.Element({.label = "Show Description"})) {
        if (ui.Switch(s_state.showDescription).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Icon & Badge"})) {
        if (ui.Switch(s_state.showIconAndBadge).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Preview on Surface Card"})) {
        if (ui.Switch(s_state.wrapInCard).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Disabled"})) {
        if (ui.Switch(s_state.disabled).changed) {
            ui.InvalidateAll();
        }
    }
}

void RenderElementStage(guikit::Context& ui, DemoContext&) {
    auto renderContent = [&]() {
        const guikit::ElementOptions opts = BuildElementOptions();
        if (s_state.slotPreset == ElementSlotPreset::Standalone) {
            ui.Element(opts);
        } else if (auto el = ui.Element(opts)) {
            RenderSlotControl(ui, s_state.slotPreset);
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
    keel::String elemOpen = keel::format(
        "{}ui.Element({{\n"
        "    .label = \"{}\"",
        (s_state.slotPreset == ElementSlotPreset::Standalone) ? "" : "if (auto el = ",
        kSampleLabel
    );

    if (s_state.showDescription) {
        keel::format_into(elemOpen, ",\n    .description = \"{}\"", kSampleDesc);
    }
    if (s_state.showIconAndBadge) {
        elemOpen.append(",\n    .icon = guikit::Icon::Flash()");
        keel::format_into(
            elemOpen,
            ",\n    .badge = {{\"{}\", guikit::StateColor::Primary()}}",
            kSampleBadge
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
