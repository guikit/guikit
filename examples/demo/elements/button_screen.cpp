#include "../demo_context.hpp"

namespace guikit::demo {

namespace {
struct ButtonState {
    keel::SmallString<128> text{"Click me"};
    bool hasIcon = true;
    guikit::Placement iconPlacement = guikit::Placement::Leading;
    bool square = false;
    bool loading = false;
    keel::i32 clicks = 0;
};
ButtonState s_state;
} // namespace

void RenderButtonInspector(guikit::Context& ui, DemoContext&) {
    if (auto el = ui.Element({.label = "Capsule Text"})) {
        if (ui.InputField(s_state.text, {
            .placeholder = "Icon-only...",
            .clearable = true
        }).changed) {
            s_state.clicks = 0;
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Show Capsule Icon"})) {
        if (ui.Switch(s_state.hasIcon).changed) ui.InvalidateAll();
    }

    static constexpr keel::StaticArray<keel::StringRef, 4> placeNames = {"Leading", "Trailing", "Top", "Bottom"};
    keel::usize placeIdx = (s_state.iconPlacement == guikit::Placement::Trailing)
        ? 1
        : ((s_state.iconPlacement == guikit::Placement::Top)
            ? 2
            : ((s_state.iconPlacement == guikit::Placement::Bottom) ? 3 : 0));
    const bool placeDisabled = !s_state.hasIcon || s_state.square;
    if (auto el = ui.Element({.label = "Icon Placement", .disabled = placeDisabled})) {
        if (ui.StepSlider(placeIdx, placeNames, {
            .showTicks = true,
            .showLabels = true,
            .disabled = placeDisabled
        }).changed) {
            s_state.iconPlacement = (placeIdx == 1)
                ? guikit::Placement::Trailing
                : ((placeIdx == 2)
                    ? guikit::Placement::Top
                    : ((placeIdx == 3) ? guikit::Placement::Bottom : guikit::Placement::Leading));
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Loading Spinner"})) {
        if (ui.Switch(s_state.loading).changed) ui.InvalidateAll();
    }

    if (auto el = ui.Element({.label = "Square ToolButton"})) {
        if (ui.Switch(s_state.square).changed) ui.InvalidateAll();
    }
}

void RenderButtonStage(guikit::Context& ui, DemoContext& ctx) {
    const keel::StringRef btnText = (s_state.clicks > 0 && !s_state.text.empty())
        ? ui.Format("{} ({}x)", s_state.text, s_state.clicks)
        : s_state.text.as_ref();

    ctx.RunElementStage(ui, [&]() {
        guikit::ButtonOptions opts{};
        opts.text = btnText;
        opts.icon = s_state.hasIcon ? guikit::Icon::Play() : guikit::Icon{};
        opts.color = ctx.GetControlColor();
        opts.iconPlacement = s_state.iconPlacement;
        opts.align = ctx.GetCtrlAlign(NavCategory::ElemButton);
        opts.width = ctx.ResolveCtrlWidth();
        opts.height = ctx.ResolveCtrlHeight();
        opts.background = ctx.GetControlBackground(NavCategory::ElemButton);
        opts.border = ctx.GetCtrlBorder();
        opts.disabled = ctx.elemDisabled;
        opts.loading = s_state.loading;
        opts.square = s_state.square;

        if (ui.Size(ctx.controlsSize).Button(opts)) {
            ++s_state.clicks;
            ui.InvalidateAll();
        }
    });
}

keel::String GenerateButtonCode(const DemoContext& ctx) {
    const guikit::Align ctrlAlign = ctx.GetCtrlAlign(NavCategory::ElemButton);
    const keel::String ctrlCode = keel::format(
        "if (ui.{}Button({{\n"
        "    .text = \"{}\",\n"
        "    .icon = {},\n"
        "    .color = {},\n"
        "    .iconPlacement = guikit::Placement::{},\n"
        "    .align = guikit::Align::{},\n"
        "    .width = {},\n"
        "    .height = {},\n"
        "    .background = {},\n"
        "    .border = {},\n"
        "    .disabled = {},\n"
        "    .loading = {},\n"
        "    .square = {}\n"
        "}})) {{\n"
        "    // On click handler\n"
        "}}",
        FormatUiSizePrefix(ctx.controlsSize),
        s_state.text,
        s_state.hasIcon ? "guikit::Icon::Play()" : "{}",
        ctx.FormatControlColorCode(),
        PlacementName(s_state.iconPlacement),
        AlignName(ctrlAlign),
        ctx.FormatControlWidthCode(),
        ctx.FormatControlHeightCode(),
        ctx.FormatControlBackgroundCode(NavCategory::ElemButton),
        ctx.FormatCtrlBorderCode(),
        ctx.elemDisabled ? "true" : "false",
        s_state.loading ? "true" : "false",
        s_state.square ? "true" : "false"
    );
    return ctx.WrapInElementCode(ctrlCode);
}

} // namespace guikit::demo
