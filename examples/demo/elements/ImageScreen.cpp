#include "../DemoContext.hpp"
#include <algorithm>
#include <cmath>
#include <format>

namespace guikit::demo {

namespace {

struct ImageState {
    size_t sourceIdx = 0;
    static constexpr std::array<std::string_view, 3> sources = {
        "assets/images/architecture.svg",
        "assets/images/circuit.svg",
        "assets/images/missing_preview.svg"
    };
    static constexpr std::array<std::string_view, 3> sourceNames = {
        "Architecture Diagram (SVG)",
        "Circuit Schematic (SVG)",
        "Missing Asset (Alt Fallback)"
    };
    std::string altText = "Fallback: asset unavailable";
    guikit::ObjectFit fit = guikit::ObjectFit::Contain;
    static constexpr std::array<std::string_view, 5> fitModeNames = {
        "Contain", "Cover", "Fill", "ScaleDown", "Original"
    };
    guikit::Align alignX = guikit::Align::Center;
    guikit::Align alignY = guikit::Align::Center;
    float opacityPct = 100.0f;
    bool checkerboard = true;
};

ImageState s_state;

} // namespace

void RenderImageInspector(guikit::Context& ui, DemoContext&) {
    if (ui.Select(s_state.sourceIdx, ImageState::sourceNames, {.label = "Image Source"}).changed) {
        ui.InvalidateAll();
    }

    if (ui.InputField(s_state.altText, {
        .label = "Alt Fallback Text",
        .placeholder = "Fallback text when missing...",
        .clearable = true
    }).changed) {
        ui.InvalidateAll();
    }

    size_t fitIdx = static_cast<size_t>(s_state.fit);
    if (ui.StepSlider(fitIdx, ImageState::fitModeNames, {
        .label = "ObjectFit Policy",
        .showTicks = true,
        .showLabels = true
    }).changed) {
        s_state.fit = static_cast<guikit::ObjectFit>(fitIdx);
        ui.InvalidateAll();
    }

    static constexpr std::array<std::string_view, 3> alignNames = {"Start", "Center", "End"};
    size_t axIdx = static_cast<size_t>(s_state.alignX);
    if (ui.StepSlider(axIdx, alignNames, {
        .label = "Horizontal Fit Align",
        .showTicks = true,
        .showLabels = true
    }).changed) {
        s_state.alignX = static_cast<guikit::Align>(axIdx);
        ui.InvalidateAll();
    }

    size_t ayIdx = static_cast<size_t>(s_state.alignY);
    if (ui.StepSlider(ayIdx, alignNames, {
        .label = "Vertical Fit Align",
        .showTicks = true,
        .showLabels = true
    }).changed) {
        s_state.alignY = static_cast<guikit::Align>(ayIdx);
        ui.InvalidateAll();
    }

    if (ui.Slider(s_state.opacityPct, 10.0f, 100.0f, {
        .label = "Opacity",
        .valueFormat = "%.0f %%",
        .step = 5.0f
    }).changed) {
        ui.InvalidateAll();
    }

    if (ui.Switch(s_state.checkerboard, {.label = "Checkerboard Backdrop"}).changed) {
        ui.InvalidateAll();
    }
}

void RenderImageStage(guikit::Context& ui, DemoContext& ctx) {
    const uint8_t alpha = static_cast<uint8_t>(std::clamp(std::lround(s_state.opacityPct * 2.55f), 0l, 255l));
    ui.Image(ImageState::sources[s_state.sourceIdx % ImageState::sources.size()], {
        .label = ctx.elemLabel,
        .description = ctx.GetElementDescription(),
        .icon = ctx.GetElementIcon(),
        .badge = ctx.GetElementBadge(),
        .variant = ctx.btnVariant,
        .alt = s_state.altText,
        .fit = s_state.fit,
        .layout = ctx.elemLayout,
        .inlineOrder = ctx.elemInlineOrder,
        .elementAlign = ctx.elemAlign,
        .alignX = s_state.alignX,
        .alignY = s_state.alignY,
        .stretch = ctx.GetElementStretch(),
        .background = ctx.elemBackground,
        .borderWidth = ctx.elemBorderWidth,
        .borderRadius = ctx.GetElemBorderRadius(),
        .control = ctx.GetControlSpec(NavCategory::ElemImage),
        .checkerboard = s_state.checkerboard,
        .opacity = alpha,
        .size = ctx.controlsSize,
        .disabled = ctx.elemDisabled
    });
}

std::string GenerateImageCode(const DemoContext& ctx) {
    const int alpha = static_cast<int>(std::clamp(std::lround(s_state.opacityPct * 2.55f), 0l, 255l));

    return std::format(
        "ui.Image(\"{}\", {{\n"
        "    .label = \"{}\",\n"
        "    .description = {},\n"
        "    .icon = {},\n"
        "    .badge = {},\n"
        "    .variant = guikit::UiVariant::{},\n"
        "    .alt = \"{}\",\n"
        "    .fit = guikit::ObjectFit::{},\n"
        "    .layout = guikit::ElementLayout::{},\n"
        "    .inlineOrder = guikit::InlineOrder::{},\n"
        "    .elementAlign = guikit::Align::{},\n"
        "    .alignX = guikit::Align::{},\n"
        "    .alignY = guikit::Align::{},\n"
        "    .stretch = guikit::StretchMode::{},\n"
        "    .background = {},\n"
        "    .borderWidth = {:.1f}f,\n"
        "    .borderRadius = {},\n"
        "    .control = {},\n"
        "    .checkerboard = {},\n"
        "    .opacity = {},\n"
        "    .size = guikit::UiSize::{},\n"
        "    .disabled = {}\n"
        "}});",
        ImageState::sources[s_state.sourceIdx % ImageState::sources.size()],
        ctx.elemLabel,
        ctx.FormatElementDescCode(),
        ctx.FormatElementIconCode(),
        ctx.FormatElementBadgeCode(),
        VariantName(ctx.btnVariant),
        s_state.altText,
        ImageState::fitModeNames[static_cast<size_t>(s_state.fit)],
        ElementLayoutEnumName(ctx.elemLayout),
        InlineOrderEnumName(ctx.elemInlineOrder),
        LabelAlignEnumName(ctx.elemAlign),
        AlignName(s_state.alignX),
        AlignName(s_state.alignY),
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.elemBackground ? "true" : "false",
        ctx.elemBorderWidth,
        ctx.FormatElemBorderRadiusCode(),
        ctx.FormatControlSpecCode(NavCategory::ElemImage),
        s_state.checkerboard ? "true" : "false",
        alpha,
        SizeName(ctx.controlsSize),
        ctx.elemDisabled ? "true" : "false"
    );
}

} // namespace guikit::demo
