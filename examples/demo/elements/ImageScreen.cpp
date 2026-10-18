#include "../DemoContext.hpp"
#include <algorithm>
#include <cmath>
#include <format>

namespace guikit::demo {

namespace {

struct ImageState {
    size_t sourceIdx = 0;
    static constexpr std::array<std::string_view, 2> sources = {
        "assets/images/lena.png",
        "assets/images/missing_preview.png"
    };
    static constexpr std::array<std::string_view, 2> sourceNames = {
        "Lena (assets/images/lena.png)",
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
    if (auto el = ui.Element({.label = "Image Source"})) {
        if (ui.Select(s_state.sourceIdx, ImageState::sourceNames).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Alt Fallback Text"})) {
        if (ui.InputField(s_state.altText, {
            .placeholder = "Fallback text when missing...",
            .clearable = true
        }).changed) {
            ui.InvalidateAll();
        }
    }

    size_t fitIdx = static_cast<size_t>(s_state.fit);
    if (auto el = ui.Element({.label = "ObjectFit Policy"})) {
        if (ui.StepSlider(fitIdx, ImageState::fitModeNames, {
            .showTicks = true,
            .showLabels = true
        }).changed) {
            s_state.fit = static_cast<guikit::ObjectFit>(fitIdx);
            ui.InvalidateAll();
        }
    }

    static constexpr std::array<std::string_view, 3> alignNames = {"Start", "Center", "End"};
    size_t axIdx = static_cast<size_t>(s_state.alignX);
    if (auto el = ui.Element({.label = "Horizontal Fit Align"})) {
        if (ui.StepSlider(axIdx, alignNames, {
            .showTicks = true,
            .showLabels = true
        }).changed) {
            s_state.alignX = static_cast<guikit::Align>(axIdx);
            ui.InvalidateAll();
        }
    }

    size_t ayIdx = static_cast<size_t>(s_state.alignY);
    if (auto el = ui.Element({.label = "Vertical Fit Align"})) {
        if (ui.StepSlider(ayIdx, alignNames, {
            .showTicks = true,
            .showLabels = true
        }).changed) {
            s_state.alignY = static_cast<guikit::Align>(ayIdx);
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Opacity"})) {
        if (ui.Slider(s_state.opacityPct, 10.0f, 100.0f, {
            .valueFormat = "%.0f %%",
            .step = 5.0f
        }).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Checkerboard Backdrop"})) {
        if (ui.Switch(s_state.checkerboard).changed) {
            ui.InvalidateAll();
        }
    }
}

void RenderImageStage(guikit::Context& ui, DemoContext& ctx) {
    const uint8_t alpha = static_cast<uint8_t>(std::clamp(std::lround(s_state.opacityPct * 2.55f), 0l, 255l));
    if (auto el = ui.Element({
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
        .size = ctx.controlsSize,
        .disabled = ctx.elemDisabled
    })) {
        ui.Image(ImageState::sources[s_state.sourceIdx % ImageState::sources.size()], {
            .alt = s_state.altText,
            .fit = s_state.fit,
            .alignX = s_state.alignX,
            .alignY = s_state.alignY,
            .stretch = ctx.GetElementStretch(),
            .width = ctx.GetElementWidth(),
            .height = ctx.GetElementHeight(),
            .background = ctx.GetControlBackground(NavCategory::ElemImage),
            .border = ctx.GetCtrlBorder(),
            .checkerboard = s_state.checkerboard,
            .opacity = alpha,
            .size = ctx.controlsSize,
            .disabled = ctx.elemDisabled
        });
    }
}

std::string GenerateImageCode(const DemoContext& ctx) {
    const int alpha = static_cast<int>(std::clamp(std::lround(s_state.opacityPct * 2.55f), 0l, 255l));

    return std::format(
        "if (auto el = ui.Element({{\n"
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
        "    .size = guikit::UiSize::{},\n"
        "    .disabled = {}\n"
        "}})) {{\n"
        "    ui.Image(\"{}\", {{\n"
        "        .alt = \"{}\",\n"
        "        .fit = guikit::ObjectFit::{},\n"
        "        .alignX = guikit::Align::{},\n"
        "        .alignY = guikit::Align::{},\n"
        "        .stretch = guikit::StretchMode::{},\n"
        "        .width = {},\n"
        "        .height = {},\n"
        "        .background = {},\n"
        "        .border = {},\n"
        "        .checkerboard = {},\n"
        "        .opacity = {},\n"
        "        .size = guikit::UiSize::{},\n"
        "        .disabled = {}\n"
        "    }});\n"
        "}}",
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
        SizeName(ctx.controlsSize),
        ctx.elemDisabled ? "true" : "false",
        ImageState::sources[s_state.sourceIdx % ImageState::sources.size()],
        s_state.altText,
        ImageState::fitModeNames[static_cast<size_t>(s_state.fit)],
        AlignName(s_state.alignX),
        AlignName(s_state.alignY),
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.FormatElementWidthCode(),
        ctx.FormatElementHeightCode(),
        ctx.FormatControlBackgroundCode(NavCategory::ElemImage),
        ctx.FormatCtrlBorderCode(),
        s_state.checkerboard ? "true" : "false",
        alpha,
        SizeName(ctx.controlsSize),
        ctx.elemDisabled ? "true" : "false"
    );
}

} // namespace guikit::demo
