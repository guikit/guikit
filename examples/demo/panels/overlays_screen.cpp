#include "../demo_context.hpp"
#include "guikit/panels/command_palette.hpp"

namespace guikit::demo {

namespace {

struct OverlaysDemoState {
    keel::usize activeOverlay = 0; // 0 = Modal, 1 = ContextMenu, 2 = Toast, 3 = CommandPalette

    // Modal settings & state
    bool customModalOpen = false;
    bool confirmDeleteModalOpen = false;
    float modalWidth = 440.0f;
    float backdropOpacity = 0.55f;
    bool closeOnBackdrop = true;
    bool closeOnEscape = true;
    bool showCloseButton = true;
    bool showHeaderDivider = true;
    bool confineModalToStage = false;
    keel::FixedString<128> profileName = "Station-Alpha";
    bool autoReconnect = true;
    keel::FixedString<128> lastModalStatus = "Ready";

    // ContextMenu settings & state
    keel::usize triggerModeIdx = 0; // 0 = RightClick, 1 = LeftClick
    float menuMinWidth = 200.0f;
    bool itemBookmarked = true;
    bool itemLocked = false;
    bool itemGridSnap = true;
    keel::FixedString<128> lastMenuAction = "No item clicked yet";
    keel::i32 duplicateCount = 0;

    // Toast settings & state
    keel::usize toastPlacementIdx = 0; // BottomRight, TopRight, BottomLeft, TopLeft, TopCenter, BottomCenter
    float toastDuration = 4.0f;
    float toastWidth = 320.0f;
    keel::i32 toastMaxVisible = 5;
    bool toastPauseOnHover = true;
    bool toastShowProgress = true;
    bool toastClosable = true;
    bool confineToastToStage = false;
    keel::i32 toastCounter = 0;

    // CommandPalette settings & state
    bool commandPaletteOpen = false;
    bool paletteAutoWidth = true;
    float paletteWidth = 660.0f;
    keel::i32 paletteMaxVisible = 8;
    bool paletteShowFooter = true;
    keel::FixedString<160> lastPaletteCommand = "None (press Ctrl+K or click Open)";
};

OverlaysDemoState s_ov;

constexpr keel::StaticArray<keel::StringRef, 6> kToastPlacementNames = {
    "BottomRight", "TopRight", "BottomLeft", "TopLeft", "TopCenter", "BottomCenter"
};

constexpr keel::StaticArray<keel::StringRef, 6> kToastPlacementShort = {
    "BR", "TR", "BL", "TL", "TC", "BC"
};

} // namespace

void RenderOverlaysInspector(guikit::Context& ui, DemoContext& ctx) {
    static constexpr keel::StaticArray<keel::StringRef, 4> kOverlayModes = {
        "Modal", "Menu", "Toast", "Palette"
    };
    if (auto el = ui.Element({.label = "Overlay Component", .layout = guikit::ElementLayout::Stacked})) {
        if (ui.Segmented(s_ov.activeOverlay, kOverlayModes).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Control Size"})) {
        if (ui.StepSlider(ctx.controlsSize, {.showTicks = true, .showLabels = true, .includeNone = true}).changed) {
            ui.InvalidateAll();
        }
    }
    ctx.RenderVariantPicker(ui);

    if (s_ov.activeOverlay == 0) {
        if (auto el = ui.Element({.label = "Dialog Width"})) {
            if (ui.Slider(s_ov.modalWidth, 320.0f, 640.0f, {.valueFormat = "%.0f px", .step = 10.0f}).changed) {
                ui.InvalidateAll();
            }
        }
        if (auto el = ui.Element({.label = "Backdrop Dimming"})) {
            if (ui.Slider(s_ov.backdropOpacity, 0.0f, 0.90f, {.valueFormat = "%.2f", .step = 0.05f}).changed) {
                ui.InvalidateAll();
            }
        }
        if (auto el = ui.Element({.label = "Close on Backdrop Click"})) {
            if (ui.Switch(s_ov.closeOnBackdrop).changed) ui.InvalidateAll();
        }
        if (auto el = ui.Element({.label = "Close on Escape Key"})) {
            if (ui.Switch(s_ov.closeOnEscape).changed) ui.InvalidateAll();
        }
        if (auto el = ui.Element({.label = "Show Header [X] Button"})) {
            if (ui.Switch(s_ov.showCloseButton).changed) ui.InvalidateAll();
        }
        if (auto el = ui.Element({.label = "Confine Modal to Stage"})) {
            if (ui.Switch(s_ov.confineModalToStage).changed) ui.InvalidateAll();
        }
    } else if (s_ov.activeOverlay == 1) {
        static constexpr keel::StaticArray<keel::StringRef, 2> kTriggerLabels = {"RightClick", "LeftClick"};
        if (auto el = ui.Element({.label = "Trigger Mode", .layout = guikit::ElementLayout::Stacked})) {
            if (ui.Segmented(s_ov.triggerModeIdx, kTriggerLabels).changed) {
                ui.InvalidateAll();
            }
        }
        if (auto el = ui.Element({.label = "Menu Min Width"})) {
            if (ui.Slider(s_ov.menuMinWidth, 150.0f, 280.0f, {.valueFormat = "%.0f px", .step = 10.0f}).changed) {
                ui.InvalidateAll();
            }
        }
    } else if (s_ov.activeOverlay == 2) {
        if (auto el = ui.Element({.label = "Stack Placement"})) {
            if (ui.StepSlider(s_ov.toastPlacementIdx, kToastPlacementShort, {
                .showTicks = true,
                .showLabels = true,
                .valueText = {kToastPlacementNames[s_ov.toastPlacementIdx], kToastPlacementNames}
            }).changed) {
                ui.InvalidateAll();
            }
        }
        if (auto el = ui.Element({.label = "Auto-Dismiss Timer"})) {
            if (ui.Slider(s_ov.toastDuration, 0.0f, 10.0f, {
                .valueFormat = (s_ov.toastDuration <= 0.0f)
                    ? guikit::UiText{"Sticky", {"Sticky", "%.1f s"}}
                    : guikit::UiText{"%.1f s", {"Sticky", "%.1f s"}},
                .step = 0.5f
            }).changed) {
                ui.InvalidateAll();
            }
        }
        if (auto el = ui.Element({.label = "Toast Width"})) {
            if (ui.Slider(s_ov.toastWidth, 240.0f, 420.0f, {.valueFormat = "%.0f px", .step = 10.0f}).changed) {
                ui.InvalidateAll();
            }
        }
        if (auto el = ui.Element({.label = "Max Visible Stack"})) {
            if (ui.SpinBox(s_ov.toastMaxVisible, {.min = 1.0, .max = 8.0, .step = 1.0}).changed) {
                ui.InvalidateAll();
            }
        }
        if (auto el = ui.Element({.label = "Pause Timer on Hover"})) {
            if (ui.Switch(s_ov.toastPauseOnHover).changed) ui.InvalidateAll();
        }
        if (auto el = ui.Element({.label = "Show Countdown Bar"})) {
            if (ui.Switch(s_ov.toastShowProgress).changed) ui.InvalidateAll();
        }
        if (auto el = ui.Element({.label = "Confine Toasts to Stage"})) {
            if (ui.Switch(s_ov.confineToastToStage).changed) ui.InvalidateAll();
        }
    } else {
        if (auto el = ui.Element({.label = "Auto-Fit Content Width"})) {
            if (ui.Switch(s_ov.paletteAutoWidth).changed) {
                ui.InvalidateAll();
            }
        }
        if (auto el = ui.Element({.label = "Custom Palette Width", .disabled = s_ov.paletteAutoWidth})) {
            if (ui.Slider(s_ov.paletteWidth, 380.0f, 820.0f, {.valueFormat = "%.0f px", .step = 10.0f}).changed) {
                ui.InvalidateAll();
            }
        }
        if (auto el = ui.Element({.label = "Max Visible Rows"})) {
            if (ui.Slider(s_ov.paletteMaxVisible, 3, 12, {.valueFormat = "%d rows", .step = 1}).changed) {
                ui.InvalidateAll();
            }
        }
        if (auto el = ui.Element({.label = "Backdrop Dimming"})) {
            if (ui.Slider(s_ov.backdropOpacity, 0.0f, 0.90f, {.valueFormat = "%.2f", .step = 0.05f}).changed) {
                ui.InvalidateAll();
            }
        }
        if (auto el = ui.Element({.label = "Show Footer Hints"})) {
            if (ui.Switch(s_ov.paletteShowFooter).changed) ui.InvalidateAll();
        }
        if (auto el = ui.Element({.label = "Confine Palette to Stage"})) {
            if (ui.Switch(s_ov.confineModalToStage).changed) ui.InvalidateAll();
        }
    }
}

void RenderOverlaysStage(guikit::Context& ui, DemoContext& ctx) {
    if (ui.ConsumeShortcut(guikit::Key::K, true)) {
        s_ov.commandPaletteOpen = !s_ov.commandPaletteOpen;
        ui.InvalidateAll();
    }

    if (auto stageCol = ui.Column({
        .gap = 14.0f,
        .align = guikit::Alignment::Center,
        .justify = guikit::Justify::Center
    })) {
        if (s_ov.activeOverlay == 0) {
            if (auto btnRow = ui.Row({.gap = 10.0f, .align = guikit::Alignment::Center})) {
                if (ui.Size(ctx.controlsSize).Button({
                    .text = "Open Modal Dialog...",
                    .icon = guikit::Icon::Settings(),
                    .color = guikit::StateColor::FromVariant(ctx.btnVariant)
                }).clicked) {
                    s_ov.customModalOpen = true;
                }

                if (ui.Size(ctx.controlsSize).Button({
                    .text = "Confirm Delete Modal...",
                    .icon = guikit::Icon::Trash(),
                    .color = guikit::StateColor::Danger()
                }).clicked) {
                    s_ov.confirmDeleteModalOpen = true;
                }
            }
            ui.Text(s_ov.lastModalStatus.as_ref(), {
                .role = guikit::TextRole::BodySmall,
                .variant = guikit::UiVariant::Secondary
            });
        } else if (s_ov.activeOverlay == 1) {
            const keel::StaticArray<guikit::ContextMenuItem, 6> menuItems = {{
                {.label = "Edit Properties",  .shortcut = "Enter",  .icon = guikit::Icon::Edit()},
                {.label = "Copy Config",      .shortcut = "Ctrl+C", .icon = guikit::Icon::Copy(), .separatorAfter = true},
                {.label = "Bookmark Target",  .shortcut = "Ctrl+B", .checked = s_ov.itemBookmarked, .hasCheck = true},
                {.label = "Snap to Grid",     .shortcut = "Ctrl+G", .checked = s_ov.itemGridSnap,   .hasCheck = true},
                {.label = "Lock Transform",   .shortcut = "Ctrl+L", .separatorAfter = true, .checked = s_ov.itemLocked, .hasCheck = true},
                {.label = "Delete Node",      .shortcut = "Del",    .icon = guikit::Icon::Trash(), .variant = guikit::UiVariant::Danger}
            }};

            auto handleMenuResult = [&](const guikit::ContextMenuResponse& menuRes) {
                if (!menuRes.changed || menuRes.clickedIndex < 0) return;
                switch (menuRes.clickedIndex) {
                    case 0: s_ov.lastMenuAction = "Clicked: Edit Properties"; break;
                    case 1: s_ov.lastMenuAction = "Clicked: Copy Config"; break;
                    case 2:
                        s_ov.itemBookmarked = !s_ov.itemBookmarked;
                        s_ov.lastMenuAction = keel::format_fixed<128>("Bookmark = {}", s_ov.itemBookmarked ? "ON" : "OFF");
                        break;
                    case 3:
                        s_ov.itemGridSnap = !s_ov.itemGridSnap;
                        s_ov.lastMenuAction = keel::format_fixed<128>("Snap to Grid = {}", s_ov.itemGridSnap ? "ON" : "OFF");
                        break;
                    case 4:
                        s_ov.itemLocked = !s_ov.itemLocked;
                        s_ov.lastMenuAction = keel::format_fixed<128>("Lock Transform = {}", s_ov.itemLocked ? "ON" : "OFF");
                        break;
                    case 5:
                        s_ov.lastMenuAction = "Clicked: Delete Node";
                        break;
                }
            };

            if (auto row = ui.Row({.gap = 12.0f, .align = guikit::Alignment::Center})) {
                if (auto zone = ui.Box({
                    .gap = 4.0f,
                    .padding = guikit::Padding::Symmetric(28.0f, 20.0f),
                    .background = {ui.GetTheme().palette.surfaceHover.WithAlphaF(0.45f)},
                    .border = {.width = 1.0f, .radius = 8.0f, .color = {ui.GetTheme().palette.borderHover}}
                })) {
                    ui.Text(
                        (s_ov.triggerModeIdx == 0)
                            ? "Right-Click inside this target zone"
                            : "Left-Click inside this target zone",
                        {.role = guikit::TextRole::LabelMedium}
                    );
                    auto zoneMenu = ui.Size(ctx.controlsSize).ContextMenu(menuItems, {
                        .trigger = (s_ov.triggerModeIdx == 0)
                            ? guikit::ContextMenuTrigger::RightClick
                            : guikit::ContextMenuTrigger::LeftClick,
                        .minWidth = s_ov.menuMinWidth
                    });
                    handleMenuResult(zoneMenu);
                }

                auto actionBtn = ui.Size(ctx.controlsSize).Button({
                    .text = "Quick Actions",
                    .icon = guikit::Icon::ChevronDown(),
                    .color = guikit::StateColor::Secondary(),
                    .iconPlacement = guikit::Placement::Trailing
                });
                auto btnMenu = ui.Size(ctx.controlsSize).ContextMenu(actionBtn, menuItems, {
                    .trigger = guikit::ContextMenuTrigger::LeftClick,
                    .minWidth = s_ov.menuMinWidth
                });
                handleMenuResult(btnMenu);
            }
            ui.Text(s_ov.lastMenuAction.as_ref(), {
                .role = guikit::TextRole::BodySmall,
                .variant = guikit::UiVariant::Secondary
            });
        } else if (s_ov.activeOverlay == 2) {
            if (auto btnWrap = ui.Row({.gap = 8.0f, .align = guikit::Alignment::Center})) {
                if (ui.Size(ctx.controlsSize).Button({
                    .text = "Info",
                    .icon = guikit::Icon::Info(),
                    .color = guikit::StateColor::Info()
                }).clicked) {
                    ++s_ov.toastCounter;
                    ui.ShowToast({
                        .title = ui.Format("Telemetry Sync #{}", s_ov.toastCounter),
                        .message = "Connected to primary stream at 48.0 kHz.",
                        .variant = guikit::UiVariant::Info,
                        .duration = s_ov.toastDuration,
                        .showProgress = s_ov.toastShowProgress,
                        .closable = s_ov.toastClosable
                    });
                }

                if (ui.Size(ctx.controlsSize).Button({
                    .text = "Success",
                    .icon = guikit::Icon::Check(),
                    .color = guikit::StateColor::Success()
                }).clicked) {
                    ++s_ov.toastCounter;
                    ui.ShowToast({
                        .title = "Configuration Saved",
                        .message = "Profile committed to flash.",
                        .variant = guikit::UiVariant::Success,
                        .duration = s_ov.toastDuration,
                        .showProgress = s_ov.toastShowProgress,
                        .closable = s_ov.toastClosable
                    });
                }

                if (ui.Size(ctx.controlsSize).Button({
                    .text = "Warning",
                    .icon = guikit::Icon::Warning(),
                    .color = guikit::StateColor::Warning()
                }).clicked) {
                    ++s_ov.toastCounter;
                    ui.ShowToast({
                        .title = "High Buffer Latency",
                        .message = "Jitter buffer exceeded 85% threshold.",
                        .variant = guikit::UiVariant::Warning,
                        .duration = s_ov.toastDuration,
                        .showProgress = s_ov.toastShowProgress,
                        .closable = s_ov.toastClosable
                    });
                }

                if (ui.Size(ctx.controlsSize).Button({
                    .text = "Danger",
                    .icon = guikit::Icon::Error(),
                    .color = guikit::StateColor::Danger()
                }).clicked) {
                    ++s_ov.toastCounter;
                    ui.ShowToast({
                        .title = "Carrier Lock Lost",
                        .message = "Uplink timeout; retrying in 2.0s...",
                        .variant = guikit::UiVariant::Danger,
                        .duration = s_ov.toastDuration,
                        .showProgress = s_ov.toastShowProgress,
                        .closable = s_ov.toastClosable
                    });
                }

                if (ui.Size(ctx.controlsSize).Button({
                    .text = "Clear",
                    .icon = guikit::Icon::Trash(),
                    .color = guikit::StateColor::Secondary(),
                    .disabled = (ui.GetActiveToastCount() == 0)
                }).clicked) {
                    ui.ClearToasts();
                }
            }
            ui.Text(ui.Format("Active Toasts: {}", ui.GetActiveToastCount()), {
                .role = guikit::TextRole::BodySmall,
                .variant = guikit::UiVariant::Secondary
            });
        } else {
            if (ui.Size(ctx.controlsSize).Button({
                .text = "Open Command Palette (Ctrl+K)...",
                .icon = guikit::Icon::Search(),
                .color = guikit::StateColor::FromVariant(ctx.btnVariant)
            }).clicked) {
                s_ov.commandPaletteOpen = true;
            }
            ui.Text(s_ov.lastPaletteCommand.as_ref(), {
                .role = guikit::TextRole::BodySmall,
                .variant = guikit::UiVariant::Secondary
            });
        }
    }

    // Active CommandPalette host
    static const keel::StaticArray<guikit::CommandPaletteItem, 6> kPaletteCommands = {{
        {.label = "Configure Telemetry Endpoint", .category = "Dialog",    .description = "Open station profile dialog",         .shortcut = "Ctrl+E", .icon = guikit::Icon::Settings()},
        {.label = "Spawn Info Notification",      .category = "Toast",     .description = "Emit a streaming info toast",         .shortcut = "Alt+I",  .icon = guikit::Icon::Flash()},
        {.label = "Toggle Bookmark Target",       .category = "Workspace", .description = "Pin or unpin the active target node", .shortcut = "Ctrl+B", .icon = guikit::Icon::Check()},
        {.label = "Copy Station Configuration",   .category = "Clipboard", .description = "Serialize JSON profile to clipboard", .shortcut = "Ctrl+C", .icon = guikit::Icon::Copy()},
        {.label = "Clear Active Toasts",          .category = "Toast",     .description = "Dismiss all floating notifications",  .shortcut = "Alt+X",  .icon = guikit::Icon::Close()},
        {.label = "Delete Station Node...",       .category = "Danger",    .description = "Permanently purge active node",       .shortcut = "Del",    .icon = guikit::Icon::Trash()}
    }};

    auto cpRes = ui.Size(ctx.controlsSize).CommandPalette(s_ov.commandPaletteOpen, kPaletteCommands, {
        .placeholder = "Type a command or search...",
        .width = s_ov.paletteAutoWidth ? keel::kNullOpt : keel::Optional<float>(s_ov.paletteWidth),
        .maxVisibleItems = static_cast<keel::u32>(keel::max(1, s_ov.paletteMaxVisible)),
        .backdropOpacity = s_ov.backdropOpacity,
        .closeOnBackdropClick = s_ov.closeOnBackdrop,
        .closeOnEscape = s_ov.closeOnEscape,
        .showFooterHints = s_ov.paletteShowFooter,
        .confineToParent = s_ov.confineModalToStage
    });

    if (cpRes.changed && cpRes.selectedIndex >= 0 && static_cast<keel::usize>(cpRes.selectedIndex) < kPaletteCommands.size()) {
        const auto& cmd = kPaletteCommands[static_cast<keel::usize>(cpRes.selectedIndex)];
        s_ov.lastPaletteCommand = keel::format_fixed<160>("Executed: [{}] {}", cmd.category, cmd.label);
        if (cpRes.selectedIndex == 0) s_ov.customModalOpen = true;
        else if (cpRes.selectedIndex == 4) ui.ClearToasts();
        else if (cpRes.selectedIndex == 5) s_ov.confirmDeleteModalOpen = true;
    }

    // Active Modal instances
    if (auto modal = ui.Size(ctx.controlsSize).Modal(s_ov.customModalOpen, {
        .title = "Configure Station Endpoint",
        .subtitle = "Edit connection parameters for the active station.",
        .variant = ctx.btnVariant,
        .width = s_ov.modalWidth,
        .backdropOpacity = s_ov.backdropOpacity,
        .closeOnBackdrop = s_ov.closeOnBackdrop,
        .closeOnEscape = s_ov.closeOnEscape,
        .showCloseButton = s_ov.showCloseButton,
        .showHeaderDivider = s_ov.showHeaderDivider,
        .confineToParent = s_ov.confineModalToStage
    })) {
        if (auto el = ui.Element({.label = "Profile Name", .layout = guikit::ElementLayout::Top})) {
            ui.InputField(s_ov.profileName, {.placeholder = "Enter profile name...", .clearable = true});
        }
        if (auto el = ui.Element({.label = "Auto-Reconnect on Drop"})) {
            ui.Switch(s_ov.autoReconnect);
        }
        if (auto footer = ui.Row({
            .gap = 10.0f,
            .align = guikit::Alignment::Center,
            .justify = guikit::Justify::End
        })) {
            if (ui.Button({.text = "Cancel", .color = guikit::StateColor::Secondary()}).clicked) {
                s_ov.lastModalStatus = "Cancelled Modal";
                modal.Close();
            }
            if (ui.Button({
                .text = "Save",
                .icon = guikit::Icon::Check(),
                .color = guikit::StateColor::FromVariant(ctx.btnVariant)
            }).clicked) {
                s_ov.lastModalStatus = keel::format_fixed<128>("Saved '{}'", s_ov.profileName.as_ref());
                modal.Close();
            }
        }
    } else if (modal.closedByUser) {
        s_ov.lastModalStatus = "Dismissed via [X] / Backdrop / Esc";
    }

    if (auto modal = ui.Size(ctx.controlsSize).Modal(s_ov.confirmDeleteModalOpen, {
        .title = "Delete Station Node?",
        .subtitle = "This action permanently removes the node.",
        .variant = guikit::UiVariant::Danger,
        .width = 380.0f,
        .backdropOpacity = s_ov.backdropOpacity,
        .closeOnBackdrop = s_ov.closeOnBackdrop,
        .closeOnEscape = s_ov.closeOnEscape,
        .showCloseButton = s_ov.showCloseButton,
        .confineToParent = s_ov.confineModalToStage
    })) {
        if (auto footer = ui.Row({
            .gap = 10.0f,
            .align = guikit::Alignment::Center,
            .justify = guikit::Justify::End
        })) {
            if (ui.Button({.text = "Cancel", .color = guikit::StateColor::Secondary()}).clicked) {
                s_ov.lastModalStatus = "Cancelled Delete";
                modal.Close();
            }
            if (ui.Button({
                .text = "Delete",
                .icon = guikit::Icon::Trash(),
                .color = guikit::StateColor::Danger()
            }).clicked) {
                s_ov.lastModalStatus = "Confirmed Delete";
                modal.Close();
            }
        }
    }

    ui.ToastHost({
        .placement = static_cast<guikit::ToastPlacement>(s_ov.toastPlacementIdx),
        .width = s_ov.toastWidth,
        .maxVisible = static_cast<keel::usize>(keel::max(1, s_ov.toastMaxVisible)),
        .pauseOnHover = s_ov.toastPauseOnHover,
        .confineToParent = s_ov.confineToastToStage
    });
}

keel::String GenerateOverlaysCode(const DemoContext& ctx) {
    const auto szPrefix = FormatUiSizePrefix(ctx.controlsSize);

    if (s_ov.activeOverlay == 0) {
        return keel::format(
            "if (auto modal = ui.{}Modal(isModalOpen, {{\n"
            "    .title = \"Configure Station Endpoint\",\n"
            "    .subtitle = \"Edit connection parameters for the active station.\",\n"
            "    .variant = guikit::UiVariant::{},\n"
            "    .width = {:.1f}f,\n"
            "    .backdropOpacity = {:.2f}f,\n"
            "    .closeOnBackdrop = {},\n"
            "    .closeOnEscape = {},\n"
            "    .showCloseButton = {}\n"
            "}})) {{\n"
            "    if (ui.Button({{.text = \"Save\"}}).clicked) {{\n"
            "        modal.Close();\n"
            "    }}\n"
            "}}",
            szPrefix,
            VariantName(ctx.btnVariant),
            s_ov.modalWidth,
            s_ov.backdropOpacity,
            s_ov.closeOnBackdrop ? "true" : "false",
            s_ov.closeOnEscape ? "true" : "false",
            s_ov.showCloseButton ? "true" : "false"
        );
    }

    if (s_ov.activeOverlay == 1) {
        const keel::StringRef trigName = (s_ov.triggerModeIdx == 0) ? "RightClick" : "LeftClick";
        return keel::format(
            "auto menuRes = ui.{}ContextMenu(targetResponse, {{\n"
            "    {{.label = \"Edit Properties\", .shortcut = \"Enter\",  .icon = guikit::Icon::Edit()}},\n"
            "    {{.label = \"Copy Config\",     .shortcut = \"Ctrl+C\", .icon = guikit::Icon::Copy(), .separatorAfter = true}},\n"
            "    {{.label = \"Bookmark Target\", .shortcut = \"Ctrl+B\", .checked = isBookmarked, .hasCheck = true}},\n"
            "    {{.label = \"Delete Node\",     .shortcut = \"Del\",    .icon = guikit::Icon::Trash(), .variant = guikit::UiVariant::Danger}}\n"
            "}}, {{\n"
            "    .trigger = guikit::ContextMenuTrigger::{},\n"
            "    .minWidth = {:.1f}f\n"
            "}});",
            szPrefix,
            trigName,
            s_ov.menuMinWidth
        );
    }

    if (s_ov.activeOverlay == 2) {
        const keel::StringRef placementName = kToastPlacementNames[s_ov.toastPlacementIdx];
        return keel::format(
            "ui.ShowToast({{\n"
            "    .title = \"Configuration Saved\",\n"
            "    .message = \"Profile committed to flash.\",\n"
            "    .variant = guikit::UiVariant::Success,\n"
            "    .duration = {:.1f}f,\n"
            "    .showProgress = {}\n"
            "}});\n\n"
            "ui.ToastHost({{\n"
            "    .placement = guikit::ToastPlacement::{},\n"
            "    .width = {:.1f}f,\n"
            "    .maxVisible = {},\n"
            "    .pauseOnHover = {}\n"
            "}});",
            s_ov.toastDuration,
            s_ov.toastShowProgress ? "true" : "false",
            placementName,
            s_ov.toastWidth,
            s_ov.toastMaxVisible,
            s_ov.toastPauseOnHover ? "true" : "false"
        );
    }

    return keel::format(
        "auto cpRes = ui.CommandPalette(isPaletteOpen, commands, {{\n"
        "    .placeholder = \"Type a command or search...\",\n"
        "    .width = {:.1f}f,\n"
        "    .maxVisibleItems = {},\n"
        "    .showFooterHints = {}\n"
        "}});",
        s_ov.paletteWidth,
        s_ov.paletteMaxVisible,
        s_ov.paletteShowFooter ? "true" : "false"
    );
}

} // namespace guikit::demo
