#include "../demo_context.hpp"

namespace guikit::demo {

namespace {
struct InputState {
    keel::SmallString<128> text{"guikit.dev"};
    keel::SmallString<128> placeholder{"Enter domain..."};
    keel::SmallString<64> prefix{"https://"};
    keel::SmallString<64> suffix{":443"};
    guikit::CursorBlinkMode cursorMode = guikit::CursorBlinkMode::Blinking;
    bool password = false;
    bool clearable = true;
    bool readOnly = false;
    bool companionButton = false;
    keel::i32 connectCount = 0;
};
InputState s_state;
} // namespace

void RenderInputInspector(guikit::Context& ui, DemoContext&) {
    if (auto el = ui.Element({
        .label = "Multi-Control Slot (Input + Button)",
        .description = "Place InputField + Button inside a single ui.Element"
    })) {
        if (ui.Switch(s_state.companionButton).changed) {
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
            .placeholder = "e.g. https://",
            .clearable = true
        }).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Suffix Text"})) {
        if (ui.InputField(s_state.suffix, {
            .placeholder = "e.g. :443",
            .clearable = true
        }).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Clearable Button ('X')"})) {
        if (ui.Switch(s_state.clearable).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Password Mask & Reveal"})) {
        if (ui.Switch(s_state.password).changed) ui.InvalidateAll();
    }

    static constexpr keel::StaticArray<keel::StringRef, 3> cursorModes = {"Blinking", "Solid", "Hidden"};
    if (auto el = ui.Element({.label = "Cursor Blink Mode"})) {
        if (ui.Segmented(s_state.cursorMode, cursorModes).changed) {
            ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({.label = "Read Only"})) {
        if (ui.Switch(s_state.readOnly).changed) ui.InvalidateAll();
    }
}

void RenderInputStage(guikit::Context& ui, DemoContext& ctx) {
    ctx.RunElementStage(ui, [&]() {
        static constexpr keel::StaticArray<keel::StringRef, 2> kHostVariants = {{"guikit.dev", "api.guikit.dev"}};
        guikit::InputFieldOptions opts{};
        opts.placeholder = s_state.placeholder;
        opts.prefix = s_state.prefix;
        opts.suffix = s_state.suffix;
        opts.valueFormat = {kHostVariants};
        opts.cursorMode = s_state.cursorMode;
        opts.width = ctx.ResolveCtrlWidth();
        opts.height = ctx.ResolveCtrlHeight();
        opts.background = ctx.GetControlBackground(NavCategory::ElemInputField);
        opts.border = ctx.GetCtrlBorder();
        opts.password = s_state.password;
        opts.clearable = s_state.clearable;
        opts.size = ctx.controlsSize;
        opts.disabled = ctx.elemDisabled;
        opts.readOnly = s_state.readOnly;
        ui.InputField(s_state.text, opts);

        if (s_state.companionButton) {
            const keel::StringRef btnLbl = (s_state.connectCount > 0)
                ? ui.Format("Connect ({})", s_state.connectCount)
                : keel::StringRef("Connect");
            if (ui.Button({
                .text = btnLbl,
                .icon = guikit::Icon::Play(),
                .color = ctx.GetControlColor(),
                .height = ctx.ResolveCtrlHeight(),
                .disabled = ctx.elemDisabled,
                .size = ctx.controlsSize
            })) {
                ++s_state.connectCount;
                ui.InvalidateAll();
            }
        }
    });
}

keel::String GenerateInputCode(const DemoContext& ctx) {
    keel::String ctrl = keel::format(
        "ui.InputField(serverEndpoint, {{\n"
        "    .placeholder = \"{}\",\n"
        "    .prefix = \"{}\",\n"
        "    .suffix = \"{}\",\n"
        "    .valueFormat = {{{{\"guikit.dev\", \"api.guikit.dev\"}}}},\n"
        "    .cursorMode = guikit::CursorBlinkMode::{},\n"
        "    .width = {},\n"
        "    .height = {},\n"
        "    .background = {},\n"
        "    .border = {},\n"
        "    .password = {},\n"
        "    .clearable = {}{},\n"
        "    .disabled = {},\n"
        "    .readOnly = {}\n"
        "}});",
        s_state.placeholder,
        s_state.prefix,
        s_state.suffix,
        CursorModeName(s_state.cursorMode),
        ctx.FormatCtrlWidthCode(),
        ctx.FormatCtrlHeightCode(),
        ctx.FormatControlBackgroundCode(NavCategory::ElemInputField),
        ctx.FormatCtrlBorderCode(),
        s_state.password ? "true" : "false",
        s_state.clearable ? "true" : "false",
        FormatUiSizePrefix(ctx.controlsSize),
        ctx.elemDisabled ? "true" : "false",
        s_state.readOnly ? "true" : "false"
    );

    if (s_state.companionButton) {
        keel::format_into(
            ctrl,
            "\nif (ui.Button({{.text = \"Connect\", .icon = guikit::Icon::Play(), .color = {}{}}})) {{\n"
            "    // Trigger endpoint action\n"
            "}}",
            ctx.FormatControlColorCode(),
            FormatUiSizePrefix(ctx.controlsSize)
        );
    }

    return ctx.WrapInElementCode(ctrl);
}

} // namespace guikit::demo
