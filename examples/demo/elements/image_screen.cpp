#include "../demo_context.hpp"

namespace guikit::demo {

namespace {

struct ImageState {
    keel::usize sourceIdx = 0;
    static constexpr keel::StaticArray<keel::StringRef, 2> sources = {
        "assets/images/lena.png",
        "assets/images/missing_preview.png"
    };
    static constexpr keel::StaticArray<keel::StringRef, 2> sourceNames = {
        "Lena (assets/images/lena.png)",
        "Missing Asset (Alt Fallback)"
    };
    keel::SmallString<128> altText{"Fallback: asset unavailable"};
    guikit::ObjectFit fit = guikit::ObjectFit::Contain;
    static constexpr keel::StaticArray<keel::StringRef, 5> fitModeNames = {
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

    if (auto el = ui.Element({.label = "ObjectFit Policy"})) {
        if (ui.Select(s_state.fit, ImageState::fitModeNames).changed) {
            ui.InvalidateAll();
        }
    }

    keel::usize axIdx = static_cast<keel::usize>(s_state.alignX);
    if (auto el = ui.Element({.label = "Horizontal Fit Align"})) {
        if (ui.Segmented(axIdx, kLabelAlignNames).changed) {
            s_state.alignX = static_cast<guikit::Align>(axIdx);
            ui.InvalidateAll();
        }
    }

    keel::usize ayIdx = static_cast<keel::usize>(s_state.alignY);
    if (auto el = ui.Element({.label = "Vertical Fit Align"})) {
        if (ui.Segmented(ayIdx, kLabelAlignNames).changed) {
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
    const keel::u8 alpha = static_cast<keel::u8>(keel::clamp(keel::round_i32(s_state.opacityPct * 2.55f), 0, 255));
    ctx.RunElementStage(ui, [&]() {
        guikit::ImageOptions opts{};
        opts.alt = s_state.altText;
        opts.fit = s_state.fit;
        opts.alignX = s_state.alignX;
        opts.alignY = s_state.alignY;
        opts.checkerboard = s_state.checkerboard;
        opts.opacity = alpha;
        opts.disabled = ctx.elemDisabled;
        opts.size = ctx.controlsSize;
        opts.width = ctx.ResolveCtrlWidth();
        opts.height = ctx.ResolveCtrlHeight();
        opts.background = ctx.GetControlBackground(NavCategory::ElemImage);
        opts.border = ctx.GetCtrlBorder();
        ui.Image(ImageState::sources[s_state.sourceIdx % ImageState::sources.size()], opts);
    });
}

keel::String GenerateImageCode(const DemoContext& ctx) {
    const keel::i32 alpha = keel::clamp(keel::round_i32(s_state.opacityPct * 2.55f), 0, 255);
    const bool hasCustomSize = (ctx.controlsSize != guikit::UiSize::None && ctx.controlsSize != guikit::UiSize::Md);
    const keel::FixedString<64> sizeLine = hasCustomSize
        ? keel::format_fixed<64>("    .size = guikit::UiSize::{},\n", SizeName(ctx.controlsSize))
        : keel::FixedString<64>{};
    const keel::String ctrl = keel::format(
        "ui.Image(\"{}\", {{\n"
        "    .alt = \"{}\",\n"
        "    .fit = guikit::ObjectFit::{},\n"
        "    .alignX = guikit::Align::{},\n"
        "    .alignY = guikit::Align::{},\n"
        "    .checkerboard = {},\n"
        "    .opacity = {},\n"
        "    .disabled = {},\n"
        "{}"
        "    .width = {},\n"
        "    .height = {},\n"
        "    .background = {},\n"
        "    .border = {}\n"
        "}});",
        ImageState::sources[s_state.sourceIdx % ImageState::sources.size()],
        s_state.altText,
        ImageState::fitModeNames[static_cast<keel::usize>(s_state.fit)],
        AlignName(s_state.alignX),
        AlignName(s_state.alignY),
        s_state.checkerboard ? "true" : "false",
        alpha,
        ctx.elemDisabled ? "true" : "false",
        sizeLine,
        ctx.FormatCtrlWidthCode(),
        ctx.FormatCtrlHeightCode(),
        ctx.FormatControlBackgroundCode(NavCategory::ElemImage),
        ctx.FormatCtrlBorderCode()
    );
    return ctx.WrapInElementCode(ctrl);
}

} // namespace guikit::demo
