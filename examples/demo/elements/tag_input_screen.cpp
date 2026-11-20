#include "../demo_context.hpp"
#include "guikit/widgets/tag_input.hpp"

namespace guikit::demo {

namespace {

struct TagInputDemoState {
    keel::SmallArray<keel::FixedString<64>, 16> tags =
        keel::SmallArray<keel::FixedString<64>, 16>::from({"C++20", "Vulkan", "Immediate-Mode", "Zero-Alloc"});
    keel::SmallString<128> placeholder{"Add tag..."};
    keel::SmallString<64> prefix{""};
    guikit::UiVariant tagVariant = guikit::UiVariant::Primary;
    keel::i32 maxTags = 8;
    bool allowDuplicates = false;
    bool splitOnComma = true;
    bool splitOnSpace = false;
    bool clearable = true;
    bool showCounter = true;
    bool readOnly = false;
    keel::usize sampleCounter = 0;
};

TagInputDemoState s_state;

constexpr keel::StaticArray<keel::StringRef, 8> kTagVariantLabels = {
    "Primary", "Secondary", "Success", "Warning", "Danger", "Info", "Ghost", "Muted"
};

constexpr keel::StaticArray<guikit::UiVariant, 8> kTagVariants = {
    guikit::UiVariant::Primary,
    guikit::UiVariant::Secondary,
    guikit::UiVariant::Success,
    guikit::UiVariant::Warning,
    guikit::UiVariant::Danger,
    guikit::UiVariant::Info,
    guikit::UiVariant::Ghost,
    guikit::UiVariant::Muted
};

constexpr keel::StaticArray<keel::StringRef, 8> kSampleTags = {
    "SIMD", "WebGPU", "SDF-Fonts", "Flexbox", "Reactive", "Telemetry", "Hot-Reload", "Low-Latency"
};

} // namespace

void RenderTagInputInspector(guikit::Context& ui, DemoContext&) {
    keel::usize varIdx = 0;
    for (keel::usize i = 0; i < kTagVariants.size(); ++i) {
        if (kTagVariants[i] == s_state.tagVariant) {
            varIdx = i;
            break;
        }
    }
    if (auto el = ui.Element({.label = "Tag Variant"})) {
        if (ui.Select(varIdx, kTagVariantLabels).changed) {
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
        static constexpr keel::StaticArray<keel::StringRef, 2> kMaxTagVariants = {{"Unlimited", "%d tags"}};
        if (ui.Slider(s_state.maxTags, 0, 12, {
            .valueFormat = (s_state.maxTags == 0)
                ? guikit::UiText{"Unlimited", kMaxTagVariants}
                : guikit::UiText{"%d tags", kMaxTagVariants}
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
            .width = guikit::SizeMode::Stretch,
            .background = guikit::StateColor::Secondary()
        }).clicked) {
            const keel::StringRef candidate = kSampleTags[s_state.sampleCounter % kSampleTags.size()];
            ++s_state.sampleCounter;
            if (s_state.maxTags == 0 || s_state.tags.size() < static_cast<keel::usize>(s_state.maxTags)) {
                s_state.tags.emplace_back(candidate);
                ui.InvalidateAll();
            }
        }
        if (ui.Button({
            .text = "Reset Default Tags",
            .icon = guikit::Icon::Refresh(),
            .width = guikit::SizeMode::Stretch,
            .background = guikit::StateColor::Ghost()
        }).clicked) {
            s_state.tags.assign({"C++20", "Vulkan", "Immediate-Mode", "Zero-Alloc"});
            ui.InvalidateAll();
        }
    }
}

void RenderTagInputStage(guikit::Context& ui, DemoContext& ctx) {
    ctx.RunElementStage(ui, [&]() {
        guikit::TagInputOptions opts{};
        opts.placeholder = s_state.placeholder;
        opts.prefix = s_state.prefix;
        opts.tagVariant = s_state.tagVariant;
        opts.maxTags = static_cast<keel::u32>(s_state.maxTags);
        opts.allowDuplicates = s_state.allowDuplicates;
        opts.splitOnComma = s_state.splitOnComma;
        opts.splitOnSpace = s_state.splitOnSpace;
        opts.clearable = s_state.clearable;
        opts.showCounter = s_state.showCounter;
        opts.readOnly = s_state.readOnly;
        opts.disabled = ctx.elemDisabled;
        opts.size = ctx.controlsSize;
        opts.width = ctx.ResolveCtrlWidth();
        opts.height = ctx.ResolveCtrlHeight();
        opts.background = ctx.GetControlBackground(NavCategory::ElemTagInput);
        opts.border = ctx.GetCtrlBorder();
        ui.TagInput(s_state.tags, opts);
    });
}

keel::String GenerateTagInputCode(const DemoContext& ctx) {
    const bool hasCustomSize = (ctx.controlsSize != guikit::UiSize::None && ctx.controlsSize != guikit::UiSize::Md);
    const keel::FixedString<64> sizeLine = hasCustomSize
        ? keel::format_fixed<64>("    .size = guikit::UiSize::{},\n", SizeName(ctx.controlsSize))
        : keel::FixedString<64>{};
    const keel::String ctrl = keel::format(
        "ui.TagInput(tags, {{\n"
        "    .placeholder = \"{}\",\n"
        "    .prefix = \"{}\",\n"
        "    .tagVariant = guikit::UiVariant::{},\n"
        "    .maxTags = {},\n"
        "    .allowDuplicates = {},\n"
        "    .splitOnComma = {},\n"
        "    .splitOnSpace = {},\n"
        "    .clearable = {},\n"
        "    .showCounter = {},\n"
        "    .readOnly = {},\n"
        "    .disabled = {},\n"
        "{}"
        "    .width = {},\n"
        "    .height = {},\n"
        "    .background = {},\n"
        "    .border = {}\n"
        "}});",
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
        ctx.elemDisabled ? "true" : "false",
        sizeLine,
        ctx.FormatCtrlWidthCode(),
        ctx.FormatCtrlHeightCode(),
        ctx.FormatControlBackgroundCode(NavCategory::ElemTagInput),
        ctx.FormatCtrlBorderCode()
    );
    return ctx.WrapInElementCode(ctrl);
}

} // namespace guikit::demo
