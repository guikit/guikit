#include "../DemoContext.hpp"
#include "guikit/panels/CommandPalette.hpp"
#include <array>
#include <format>
#include <string>
#include <string_view>

namespace guikit::demo {

namespace {

struct OverlaysDemoState {
    // Modal settings & state
    bool customModalOpen = false;
    bool confirmDeleteModalOpen = false;
    bool telemetryModalOpen = false;
    float modalWidth = 460.0f;
    float backdropOpacity = 0.55f;
    bool closeOnBackdrop = true;
    bool closeOnEscape = true;
    bool showCloseButton = true;
    bool showHeaderDivider = true;
    bool confineModalToStage = false;

    // Interactive form fields inside the Modal
    std::string profileName = "Radar-Station-Alpha";
    std::string endpointUrl = "wss://telemetry.guikit.dev/v2/stream";
    size_t protocolIdx = 1;
    bool autoReconnect = true;
    bool encryptPayload = true;
    std::string lastModalStatus = "Ready (no modal closed yet)";

    // ContextMenu settings & state
    size_t triggerModeIdx = 0; // 0 = RightClick, 1 = LeftClick
    float menuMinWidth = 200.0f;
    bool itemBookmarked = true;
    bool itemLocked = false;
    bool itemGridSnap = true;
    std::string lastMenuAction = "Right-click the target zone or click 'Quick Actions'";
    int duplicateCount = 0;

    // Toast settings & state
    size_t toastPlacementIdx = 0; // BottomRight, TopRight, BottomLeft, TopLeft, TopCenter, BottomCenter
    float toastDuration = 4.0f;
    float toastWidth = 320.0f;
    int toastMaxVisible = 5;
    bool toastPauseOnHover = true;
    bool toastShowProgress = true;
    bool toastClosable = true;
    bool confineToastToStage = false;
    int toastCounter = 0;

    // CommandPalette settings & state
    bool commandPaletteOpen = false;
    float paletteWidth = 520.0f;
    int paletteMaxVisible = 7;
    bool paletteShowFooter = true;
    std::string lastPaletteCommand = "None (press Ctrl+K or click 'Open Command Palette...')";
};

OverlaysDemoState s_ov;

constexpr std::array<std::string_view, 6> kToastPlacementNames = {
    "BottomRight", "TopRight", "BottomLeft", "TopLeft", "TopCenter", "BottomCenter"
};

constexpr std::array<std::string_view, 6> kToastPlacementShort = {
    "BR", "TR", "BL", "TL", "TC", "BC"
};

} // namespace

void RenderOverlaysInspector(guikit::Context& ui, DemoContext& ctx) {
    ui.Heading({
        .label = "Modal Dialog",
        .divider = true
    });

    if (auto el = ui.Element({.label = "Dialog Size"})) {
        if (ui.StepSlider(ctx.controlsSize, {.showTicks = true, .showLabels = true, .includeNone = true}).changed) {
            ui.InvalidateAll();
        }
    }
    ctx.RenderVariantPicker(ui);

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

    ui.Heading({
        .label = "ContextMenu Popup",
        .divider = true
    });

    static constexpr std::array<std::string_view, 2> kTriggerLabels = {"RightClick", "LeftClick"};
    if (auto el = ui.Element({.label = "Canvas Trigger"})) {
        if (ui.Segmented(s_ov.triggerModeIdx, kTriggerLabels).changed) {
            ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({.label = "Menu Min Width"})) {
        if (ui.Slider(s_ov.menuMinWidth, 150.0f, 280.0f, {.valueFormat = "%.0f px", .step = 10.0f}).changed) {
            ui.InvalidateAll();
        }
    }

    ui.Heading({
        .label = "Toast Notifications",
        .divider = true
    });

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

    ui.Heading({
        .label = "Command Palette",
        .divider = true
    });

    if (auto el = ui.Element({.label = "Palette Width"})) {
        if (ui.Slider(s_ov.paletteWidth, 360.0f, 680.0f, {.valueFormat = "%.0f px", .step = 10.0f}).changed) {
            ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({.label = "Max Visible Rows"})) {
        if (ui.Slider(s_ov.paletteMaxVisible, 3, 12, {.valueFormat = "%d rows", .step = 1}).changed) {
            ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({.label = "Show Footer Hints"})) {
        if (ui.Switch(s_ov.paletteShowFooter).changed) ui.InvalidateAll();
    }
}

void RenderOverlaysStage(guikit::Context& ui, DemoContext& ctx) {
    if (auto stageScroll = ui.ScrollArea({
        .gap = 14.0f
    })) {
        // =====================================================================
        // 1. MODAL DIALOG SHOWCASE CARD
        // =====================================================================
        if (auto card = ui.Card({
            .gap = 12.0f,
            .padding = guikit::Padding::All(16.0f),
            .widthMode = guikit::SizeMode::Stretch
        })) {
            ui.Heading({
                .label = "1. Modal Dialogs (Out-of-Flow Container Scope)",
                .description = "Centered floating dialog with backdrop dimming, input isolation, Escape/backdrop dismissal, and arbitrary child widgets.",
                .icon = guikit::Icon::Folder(),
                .badge = {"ModalScope", guikit::StateColor::Primary()},
                .divider = true
            });

            if (auto btnRow = ui.Wrap({.gap = 10.0f})) {
                if (ui.Size(ctx.controlsSize).Button({
                    .text = "Open Inspector Modal...",
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

                if (ui.Size(ctx.controlsSize).Button({
                    .text = "System Telemetry Modal...",
                    .icon = guikit::Icon::Info(),
                    .color = guikit::StateColor::Info()
                }).clicked) {
                    s_ov.telemetryModalOpen = true;
                }
            }

            if (auto statusRow = ui.Row({.gap = 8.0f, .align = guikit::Alignment::Center})) {
                ui.Text("Last Modal Event:", {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Muted});
                ui.Text(s_ov.lastModalStatus, {.role = guikit::TextRole::LabelMedium, .variant = guikit::UiVariant::Info});
            }
        }

        // =====================================================================
        // 2. CONTEXT MENU & DROPDOWN ACTION MENU CARD
        // =====================================================================
        if (auto card = ui.Card({
            .gap = 12.0f,
            .padding = guikit::Padding::All(16.0f),
            .widthMode = guikit::SizeMode::Stretch
        })) {
            ui.Heading({
                .label = "2. ContextMenu & Action Popups",
                .description = "Floating popup menus triggered by RightClick on a region, LeftClick on an action button, or manual state.",
                .icon = guikit::Icon::Copy(),
                .badge = {"Keyboard Nav", guikit::StateColor::Info()},
                .divider = true
            });

            const std::array<guikit::ContextMenuItem, 8> menuItems = {{
                {.label = "Edit Node Properties", .shortcut = "Enter",  .icon = guikit::Icon::Edit()},
                {.label = "Copy Configuration",   .shortcut = "Ctrl+C", .icon = guikit::Icon::Copy()},
                {.label = "Duplicate Channel",    .shortcut = "Ctrl+D", .icon = guikit::Icon::Plus(), .separatorAfter = true},
                {.label = "Bookmark Target",      .shortcut = "Ctrl+B", .checked = s_ov.itemBookmarked, .hasCheck = true},
                {.label = "Snap to Grid",         .shortcut = "Ctrl+G", .checked = s_ov.itemGridSnap,   .hasCheck = true},
                {.label = "Lock Layer Transform", .shortcut = "Ctrl+L", .separatorAfter = true, .checked = s_ov.itemLocked, .hasCheck = true},
                {.label = "Export Diagnostics...",                      .icon = guikit::Icon::Save(),  .disabled = s_ov.itemLocked},
                {.label = "Delete Node",          .shortcut = "Del",    .icon = guikit::Icon::Trash(), .variant = guikit::UiVariant::Danger}
            }};

            auto handleMenuResult = [&](const guikit::ContextMenuResponse& menuRes, std::string_view sourceName) {
                if (!menuRes.changed || menuRes.clickedIndex < 0) return;
                switch (menuRes.clickedIndex) {
                    case 0:
                        s_ov.lastMenuAction = std::format("[{}] Edit Node Properties", sourceName);
                        s_ov.customModalOpen = true;
                        break;
                    case 1:
                        s_ov.lastMenuAction = std::format("[{}] Copied configuration to clipboard", sourceName);
                        ui.ShowToast({
                            .title = "Copied to Clipboard",
                            .message = "Node configuration JSON copied.",
                            .variant = guikit::UiVariant::Info,
                            .duration = s_ov.toastDuration,
                            .showProgress = s_ov.toastShowProgress
                        });
                        break;
                    case 2:
                        ++s_ov.duplicateCount;
                        s_ov.lastMenuAction = std::format("[{}] Duplicated channel (#{})", sourceName, s_ov.duplicateCount);
                        ui.ShowToast({
                            .title = "Channel Duplicated",
                            .message = ui.Format("Created channel replica #{}.", s_ov.duplicateCount).View(),
                            .variant = guikit::UiVariant::Success,
                            .duration = s_ov.toastDuration,
                            .showProgress = s_ov.toastShowProgress
                        });
                        break;
                    case 3:
                        s_ov.itemBookmarked = !s_ov.itemBookmarked;
                        s_ov.lastMenuAction = std::format("[{}] Bookmark = {}", sourceName, s_ov.itemBookmarked ? "ON" : "OFF");
                        break;
                    case 4:
                        s_ov.itemGridSnap = !s_ov.itemGridSnap;
                        s_ov.lastMenuAction = std::format("[{}] Snap to Grid = {}", sourceName, s_ov.itemGridSnap ? "ON" : "OFF");
                        break;
                    case 5:
                        s_ov.itemLocked = !s_ov.itemLocked;
                        s_ov.lastMenuAction = std::format("[{}] Lock Layer = {}", sourceName, s_ov.itemLocked ? "LOCKED" : "UNLOCKED");
                        break;
                    case 6:
                        s_ov.lastMenuAction = std::format("[{}] Exported diagnostics", sourceName);
                        break;
                    case 7:
                        s_ov.lastMenuAction = std::format("[{}] Requested Delete Node", sourceName);
                        s_ov.confirmDeleteModalOpen = true;
                        break;
                }
            };

            if (auto row = ui.Row({.gap = 12.0f, .align = guikit::Alignment::Stretch})) {
                // Interactive ContextMenu target surface
                if (auto zone = ui.Box({
                    .gap = 6.0f,
                    .widthMode = guikit::SizeMode::Stretch,
                    .padding = guikit::Padding::Symmetric(16.0f, 14.0f),
                    .background = {ui.GetTheme().palette.surfaceHover.WithAlphaF(0.45f)},
                    .border = {.width = 1.0f, .radius = 8.0f, .color = {ui.GetTheme().palette.borderHover}}
                })) {
                    const char* trigText = (s_ov.triggerModeIdx == 0)
                        ? "Right-Click anywhere inside this interactive zone to open ContextMenu at cursor"
                        : "Left-Click anywhere inside this interactive zone to open ContextMenu";
                    ui.Text(trigText, {.role = guikit::TextRole::LabelMedium});
                    ui.Text(
                        ui.Format("State: Bookmarked={} | GridSnap={} | Locked={} | Duplicates={}",
                                  s_ov.itemBookmarked ? "Yes" : "No",
                                  s_ov.itemGridSnap ? "Yes" : "No",
                                  s_ov.itemLocked ? "Yes" : "No",
                                  s_ov.duplicateCount).View(),
                        {.role = guikit::TextRole::BodySmall, .variant = guikit::UiVariant::Muted}
                    );

                    auto zoneMenu = ui.Size(ctx.controlsSize).ContextMenu(menuItems, {
                        .trigger = (s_ov.triggerModeIdx == 0)
                            ? guikit::ContextMenuTrigger::RightClick
                            : guikit::ContextMenuTrigger::LeftClick,
                        .minWidth = s_ov.menuMinWidth
                    });
                    handleMenuResult(zoneMenu, "ZoneMenu");
                }

                // Button with LeftClick dropdown ContextMenu attached to its Response
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
                handleMenuResult(btnMenu, "ButtonDropdown");
            }

            if (auto statusRow = ui.Row({.gap = 8.0f, .align = guikit::Alignment::Center})) {
                ui.Text("Last Menu Action:", {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Muted});
                ui.Text(s_ov.lastMenuAction, {.role = guikit::TextRole::LabelMedium, .variant = guikit::UiVariant::Success});
            }
        }

        // =====================================================================
        // 3. TOAST NOTIFICATION STACK CARD
        // =====================================================================
        if (auto card = ui.Card({
            .gap = 12.0f,
            .padding = guikit::Padding::All(16.0f),
            .widthMode = guikit::SizeMode::Stretch
        })) {
            ui.Heading({
                .label = "3. Toast Notification Stack (ui.ShowToast / ui.ToastHost)",
                .description = "Non-blocking floating notifications with auto-dismiss countdown bar, hover pause, and 6 anchor placements.",
                .icon = guikit::Icon::Flash(),
                .badge = {ui.Format("{} active", ui.GetActiveToastCount()).View(), guikit::StateColor::Success()},
                .divider = true
            });

            if (auto btnWrap = ui.Wrap({.gap = 10.0f})) {
                if (ui.Size(ctx.controlsSize).Button({
                    .text = "Spawn Info",
                    .icon = guikit::Icon::Info(),
                    .color = guikit::StateColor::Info()
                }).clicked) {
                    ++s_ov.toastCounter;
                    ui.ShowToast({
                        .title = ui.Format("Telemetry Sync #{}", s_ov.toastCounter).View(),
                        .message = "Connected to primary DSP stream at 48.0 kHz.",
                        .variant = guikit::UiVariant::Info,
                        .duration = s_ov.toastDuration,
                        .showProgress = s_ov.toastShowProgress,
                        .closable = s_ov.toastClosable
                    });
                }

                if (ui.Size(ctx.controlsSize).Button({
                    .text = "Spawn Success",
                    .icon = guikit::Icon::Check(),
                    .color = guikit::StateColor::Success()
                }).clicked) {
                    ++s_ov.toastCounter;
                    ui.ShowToast({
                        .title = "Configuration Saved",
                        .message = ui.Format("Profile '{}' committed to flash.", s_ov.profileName).View(),
                        .variant = guikit::UiVariant::Success,
                        .duration = s_ov.toastDuration,
                        .showProgress = s_ov.toastShowProgress,
                        .closable = s_ov.toastClosable
                    });
                }

                if (ui.Size(ctx.controlsSize).Button({
                    .text = "Spawn Warning",
                    .icon = guikit::Icon::Warning(),
                    .color = guikit::StateColor::Warning()
                }).clicked) {
                    ++s_ov.toastCounter;
                    ui.ShowToast({
                        .title = "High Buffer Latency",
                        .message = "Jitter buffer exceeded 85% threshold on Channel B.",
                        .variant = guikit::UiVariant::Warning,
                        .duration = s_ov.toastDuration,
                        .showProgress = s_ov.toastShowProgress,
                        .closable = s_ov.toastClosable
                    });
                }

                if (ui.Size(ctx.controlsSize).Button({
                    .text = "Spawn Danger",
                    .icon = guikit::Icon::Error(),
                    .color = guikit::StateColor::Danger()
                }).clicked) {
                    ++s_ov.toastCounter;
                    ui.ShowToast({
                        .title = "Carrier Lock Lost",
                        .message = "Uplink transducer timeout; retrying in 2.0s...",
                        .variant = guikit::UiVariant::Danger,
                        .duration = s_ov.toastDuration,
                        .showProgress = s_ov.toastShowProgress,
                        .closable = s_ov.toastClosable
                    });
                }

                if (ui.Size(ctx.controlsSize).Button({
                    .text = "Sticky Toast",
                    .icon = guikit::Icon::Flash(),
                    .color = guikit::StateColor::Primary()
                }).clicked) {
                    ++s_ov.toastCounter;
                    ui.ShowToast({
                        .title = "Persistent Operator Notice",
                        .message = "This toast has duration = 0.0f and stays until [X] is clicked.",
                        .variant = guikit::UiVariant::Primary,
                        .duration = 0.0f,
                        .showProgress = false,
                        .closable = true
                    });
                }

                if (ui.Size(ctx.controlsSize).Button({
                    .text = "Clear All Toasts",
                    .icon = guikit::Icon::Trash(),
                    .color = guikit::StateColor::Secondary(),
                    .disabled = (ui.GetActiveToastCount() == 0)
                }).clicked) {
                    ui.ClearToasts();
                }
            }
        }

        // =====================================================================
        // 4. COMMAND PALETTE (CTRL+K) CARD
        // =====================================================================
        if (ui.ConsumeShortcut(guikit::Key::K, true)) {
            s_ov.commandPaletteOpen = !s_ov.commandPaletteOpen;
            ui.InvalidateAll();
        }

        if (auto card = ui.Card({
            .gap = 12.0f,
            .padding = guikit::Padding::All(16.0f),
            .widthMode = guikit::SizeMode::Stretch
        })) {
            ui.Heading({
                .label = "4. Command Palette (Ctrl+K)",
                .description = "Floating quick-search command runner with substring highlight, category badges, shortcuts, and keyboard navigation.",
                .icon = guikit::Icon::Search(),
                .badge = {"Ctrl+K", guikit::StateColor::Primary()},
                .divider = true
            });

            if (auto btnRow = ui.Wrap({.gap = 10.0f})) {
                if (ui.Size(ctx.controlsSize).Button({
                    .text = "Open Command Palette...",
                    .icon = guikit::Icon::Search(),
                    .color = guikit::StateColor::FromVariant(ctx.btnVariant)
                }).clicked) {
                    s_ov.commandPaletteOpen = true;
                }
            }

            if (auto statusRow = ui.Row({.gap = 8.0f, .align = guikit::Alignment::Center})) {
                ui.Text("Last Executed Command:", {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Muted});
                ui.Text(s_ov.lastPaletteCommand, {.role = guikit::TextRole::LabelMedium, .variant = guikit::UiVariant::Primary});
            }
        }

        static const std::array<guikit::CommandPaletteItem, 10> kPaletteCommands = {{
            {.label = "Configure Telemetry Endpoint", .category = "Dialog",     .description = "Open station profile & security dialog", .shortcut = "Ctrl+E", .icon = guikit::Icon::Settings()},
            {.label = "Inspect Subsystem Health",     .category = "Diagnostics",.description = "View live DSP & GPU pipeline telemetry", .shortcut = "F9",     .icon = guikit::Icon::Info()},
            {.label = "Spawn Info Notification",      .category = "Toast",      .description = "Emit a streaming telemetry info toast",  .shortcut = "Alt+I",  .icon = guikit::Icon::Flash()},
            {.label = "Duplicate Active Channel",     .category = "DSP",        .description = "Clone current radar channel pipeline",   .shortcut = "Ctrl+D", .icon = guikit::Icon::Plus()},
            {.label = "Toggle Bookmark Target",       .category = "Workspace",  .description = "Pin or unpin the active target node",    .shortcut = "Ctrl+B", .icon = guikit::Icon::Check()},
            {.label = "Toggle Grid Snap",             .category = "Canvas",     .description = "Snap scene nodes to alignment grid",     .shortcut = "Ctrl+G", .icon = guikit::Icon::Edit()},
            {.label = "Copy Station Configuration",   .category = "Clipboard",  .description = "Serialize JSON profile to clipboard",    .shortcut = "Ctrl+C", .icon = guikit::Icon::Copy()},
            {.label = "Export Calibration Archive",   .category = "Storage",    .description = "Save spectrogram calibration preset",    .shortcut = "Ctrl+S", .icon = guikit::Icon::Save()},
            {.label = "Clear Active Toasts",          .category = "Toast",      .description = "Dismiss all floating notifications",     .shortcut = "Alt+X",  .icon = guikit::Icon::Close()},
            {.label = "Delete Radar Node...",         .category = "Danger",     .description = "Permanently purge active station node",  .shortcut = "Del",    .icon = guikit::Icon::Trash()}
        }};

        auto cpRes = guikit::CommandPalette::Run(ui, s_ov.commandPaletteOpen, kPaletteCommands, {
            .placeholder = "Type a command or search (e.g. 'telemetry', 'dsp', 'toast')...",
            .width = s_ov.paletteWidth,
            .maxVisibleItems = static_cast<uint32_t>(std::max(1, s_ov.paletteMaxVisible)),
            .backdropOpacity = s_ov.backdropOpacity,
            .closeOnBackdropClick = s_ov.closeOnBackdrop,
            .closeOnEscape = s_ov.closeOnEscape,
            .showFooterHints = s_ov.paletteShowFooter,
            .confineToParent = s_ov.confineModalToStage
        });

        if (cpRes.changed && cpRes.selectedIndex >= 0 && static_cast<size_t>(cpRes.selectedIndex) < kPaletteCommands.size()) {
            const auto& cmd = kPaletteCommands[static_cast<size_t>(cpRes.selectedIndex)];
            s_ov.lastPaletteCommand = std::format("[{}] {}", cmd.category.View(), cmd.label.View());
            switch (cpRes.selectedIndex) {
                case 0:
                    s_ov.customModalOpen = true;
                    break;
                case 1:
                    s_ov.telemetryModalOpen = true;
                    break;
                case 2:
                    ++s_ov.toastCounter;
                    ui.ShowToast({
                        .title = ui.Format("Palette Toast #{}", s_ov.toastCounter).View(),
                        .message = "Triggered directly from the Command Palette.",
                        .variant = guikit::UiVariant::Info,
                        .duration = s_ov.toastDuration,
                        .showProgress = s_ov.toastShowProgress
                    });
                    break;
                case 3:
                    ++s_ov.duplicateCount;
                    ui.ShowToast({
                        .title = "Channel Duplicated",
                        .message = ui.Format("Created channel replica #{}.", s_ov.duplicateCount).View(),
                        .variant = guikit::UiVariant::Success,
                        .duration = s_ov.toastDuration,
                        .showProgress = s_ov.toastShowProgress
                    });
                    break;
                case 4:
                    s_ov.itemBookmarked = !s_ov.itemBookmarked;
                    break;
                case 5:
                    s_ov.itemGridSnap = !s_ov.itemGridSnap;
                    break;
                case 8:
                    ui.ClearToasts();
                    break;
                case 9:
                    s_ov.confirmDeleteModalOpen = true;
                    break;
                default:
                    ui.ShowToast({
                        .title = cmd.label.View(),
                        .message = cmd.description.View(),
                        .variant = guikit::UiVariant::Primary,
                        .duration = s_ov.toastDuration,
                        .showProgress = s_ov.toastShowProgress
                    });
                    break;
            }
        }

        // =====================================================================
        // ACTIVE MODAL INSTANCES
        // =====================================================================
        // 1. Inspector-configured Modal with interactive form controls
        if (auto modal = ui.Size(ctx.controlsSize).Modal(s_ov.customModalOpen, {
            .title = "Configure Telemetry Endpoint",
            .subtitle = "Edit connection parameters and security flags for the active station.",
            .variant = ctx.btnVariant,
            .width = s_ov.modalWidth,
            .backdropOpacity = s_ov.backdropOpacity,
            .closeOnBackdrop = s_ov.closeOnBackdrop,
            .closeOnEscape = s_ov.closeOnEscape,
            .showCloseButton = s_ov.showCloseButton,
            .showHeaderDivider = s_ov.showHeaderDivider,
            .confineToParent = s_ov.confineModalToStage
        })) {
            if (auto el = ui.Element({.label = "Station Profile Name", .layout = guikit::ElementLayout::Top})) {
                ui.InputField(s_ov.profileName, {.placeholder = "Enter station name...", .clearable = true});
            }

            if (auto el = ui.Element({.label = "Stream Endpoint URL", .layout = guikit::ElementLayout::Top})) {
                ui.InputField(s_ov.endpointUrl, {.placeholder = "wss://...", .clearable = true});
            }

            static constexpr std::array<std::string_view, 3> kProtocols = {
                "UDP Multicast (Low Latency)",
                "WebSocket TLS 1.3 (Secure)",
                "gRPC Bidirectional Stream"
            };
            if (auto el = ui.Element({.label = "Transport Protocol", .layout = guikit::ElementLayout::Top})) {
                ui.Select(s_ov.protocolIdx, kProtocols);
            }

            if (auto el = ui.Element({
                .label = "Auto-Reconnect on Drop",
                .description = "Automatically retry connection with exponential backoff"
            })) {
                ui.Switch(s_ov.autoReconnect);
            }

            if (auto el = ui.Element({
                .label = "AES-256-GCM Payload Encryption",
                .description = "Encrypt all outgoing telemetry packets"
            })) {
                ui.Checkbox(s_ov.encryptPayload);
            }

            if (auto footer = ui.Row({
                .gap = 10.0f,
                .align = guikit::Alignment::Center,
                .justify = guikit::Justify::End
            })) {
                if (ui.Button({
                    .text = "Cancel",
                    .color = guikit::StateColor::Secondary()
                }).clicked) {
                    s_ov.lastModalStatus = "Cancelled 'Configure Telemetry Endpoint'";
                    modal.Close();
                }

                if (ui.Button({
                    .text = "Save Changes",
                    .icon = guikit::Icon::Check(),
                    .color = guikit::StateColor::FromVariant(ctx.btnVariant)
                }).clicked) {
                    s_ov.lastModalStatus = std::format("Saved profile '{}' ({})", s_ov.profileName, kProtocols[s_ov.protocolIdx]);
                    ui.ShowToast({
                        .title = "Endpoint Updated",
                        .message = ui.Format("Saved profile '{}'.", s_ov.profileName).View(),
                        .variant = guikit::UiVariant::Success,
                        .duration = s_ov.toastDuration,
                        .showProgress = s_ov.toastShowProgress
                    });
                    modal.Close();
                }
            }
        } else if (modal.closedByUser) {
            s_ov.lastModalStatus = "Dismissed 'Configure Telemetry Endpoint' via [X] / Backdrop / Esc";
        }

        // 2. Danger Confirmation Modal
        if (auto modal = ui.Size(ctx.controlsSize).Modal(s_ov.confirmDeleteModalOpen, {
            .title = "Delete Radar Node?",
            .subtitle = "This action permanently removes the node and its recorded spectrograms.",
            .variant = guikit::UiVariant::Danger,
            .width = 400.0f,
            .backdropOpacity = s_ov.backdropOpacity,
            .closeOnBackdrop = s_ov.closeOnBackdrop,
            .closeOnEscape = s_ov.closeOnEscape,
            .showCloseButton = s_ov.showCloseButton,
            .confineToParent = s_ov.confineModalToStage
        })) {
            ui.Text(
                "Are you sure you want to delete 'Radar-Station-Alpha'? Associated calibration presets cannot be recovered once purged.",
                {.role = guikit::TextRole::BodyMedium, .variant = guikit::UiVariant::Muted}
            );

            if (auto footer = ui.Row({
                .gap = 10.0f,
                .align = guikit::Alignment::Center,
                .justify = guikit::Justify::End
            })) {
                if (ui.Button({
                    .text = "Keep Node",
                    .color = guikit::StateColor::Secondary()
                }).clicked) {
                    s_ov.lastModalStatus = "Aborted node deletion";
                    modal.Close();
                }
                if (ui.Button({
                    .text = "Delete Permanently",
                    .icon = guikit::Icon::Trash(),
                    .color = guikit::StateColor::Danger()
                }).clicked) {
                    s_ov.lastModalStatus = "Confirmed permanent deletion";
                    ui.ShowToast({
                        .title = "Node Deleted",
                        .message = "Removed 'Radar-Station-Alpha' from workspace.",
                        .variant = guikit::UiVariant::Danger,
                        .duration = s_ov.toastDuration,
                        .showProgress = s_ov.toastShowProgress
                    });
                    modal.Close();
                }
            }
        } else if (modal.closedByUser) {
            s_ov.lastModalStatus = "Dismissed 'Delete Radar Node' dialog";
        }

        // 3. System Telemetry Modal
        if (auto modal = ui.Size(ctx.controlsSize).Modal(s_ov.telemetryModalOpen, {
            .title = "Live Subsystem Health",
            .subtitle = "Real-time diagnostics for DSP & GPU pipelines.",
            .variant = guikit::UiVariant::Info,
            .width = 440.0f,
            .backdropOpacity = s_ov.backdropOpacity,
            .closeOnBackdrop = s_ov.closeOnBackdrop,
            .closeOnEscape = s_ov.closeOnEscape,
            .showCloseButton = s_ov.showCloseButton,
            .confineToParent = s_ov.confineModalToStage
        })) {
            if (auto el = ui.Element({.label = "DSP Ring Buffer Load", .layout = guikit::ElementLayout::Top})) {
                ui.ProgressBar(68.0, {.showValue = true, .color = guikit::StateColor::Info()});
            }
            if (auto el = ui.Element({.label = "GPU Rasterizer Frame Budget", .layout = guikit::ElementLayout::Top})) {
                ui.ProgressBar(34.0, {.showValue = true, .color = guikit::StateColor::Success()});
            }
            if (auto footer = ui.Row({
                .gap = 10.0f,
                .justify = guikit::Justify::End
            })) {
                if (ui.Button({
                    .text = "Done",
                    .icon = guikit::Icon::Check(),
                    .color = guikit::StateColor::Primary()
                }).clicked) {
                    s_ov.lastModalStatus = "Closed 'Live Subsystem Health'";
                    modal.Close();
                }
            }
        }

        // Configure and run the ToastHost for this stage
        ui.ToastHost({
            .placement = static_cast<guikit::ToastPlacement>(s_ov.toastPlacementIdx),
            .width = s_ov.toastWidth,
            .maxVisible = static_cast<size_t>(std::max(1, s_ov.toastMaxVisible)),
            .pauseOnHover = s_ov.toastPauseOnHover,
            .confineToParent = s_ov.confineToastToStage
        });
    }
}

std::string GenerateOverlaysCode(const DemoContext& ctx) {
    const std::string szPrefix = FormatUiSizePrefix(ctx.controlsSize);
    const std::string_view trigName = (s_ov.triggerModeIdx == 0) ? "RightClick" : "LeftClick";
    const std::string_view placementName = kToastPlacementNames[s_ov.toastPlacementIdx];

    return std::format(
        "// 1. Out-of-flow Modal Dialog Container\n"
        "if (auto modal = ui.{}Modal(isModalOpen, {{\n"
        "    .title = \"Configure Telemetry Endpoint\",\n"
        "    .subtitle = \"Edit connection parameters and security flags.\",\n"
        "    .variant = guikit::UiVariant::{},\n"
        "    .width = {:.1f}f,\n"
        "    .backdropOpacity = {:.2f}f,\n"
        "    .closeOnBackdrop = {},\n"
        "    .closeOnEscape = {},\n"
        "    .showCloseButton = {}\n"
        "}})) {{\n"
        "    if (auto el = ui.Element({{.label = \"Station Profile Name\", .layout = guikit::ElementLayout::Top}})) {{\n"
        "        ui.InputField(profileName);\n"
        "    }}\n"
        "    if (ui.Button({{.text = \"Save Changes\", .variant = guikit::UiVariant::{}}}).clicked) {{\n"
        "        modal.Close();\n"
        "    }}\n"
        "}}\n\n"
        "// 2. ContextMenu Popup\n"
        "auto menuRes = ui.{}ContextMenu(targetZone, {{\n"
        "    {{.label = \"Edit Properties\", .shortcut = \"Enter\",  .icon = guikit::Icon::Edit()}},\n"
        "    {{.label = \"Copy Config\",     .shortcut = \"Ctrl+C\", .icon = guikit::Icon::Copy(), .separatorAfter = true}},\n"
        "    {{.label = \"Bookmark Target\", .shortcut = \"Ctrl+B\", .checked = isBookmarked, .hasCheck = true}},\n"
        "    {{.label = \"Delete Node\",     .shortcut = \"Del\",    .icon = guikit::Icon::Trash(), .variant = guikit::UiVariant::Danger}}\n"
        "}}, {{\n"
        "    .trigger = guikit::ContextMenuTrigger::{},\n"
        "    .minWidth = {:.1f}f\n"
        "}});\n\n"
        "// 3. Toast Notification Stack\n"
        "ui.ShowToast({{\n"
        "    .title = \"Configuration Saved\",\n"
        "    .message = \"Profile committed to flash.\",\n"
        "    .variant = guikit::UiVariant::Success,\n"
        "    .duration = {:.1f}f,\n"
        "    .showProgress = {}\n"
        "}});\n"
        "ui.ToastHost({{\n"
        "    .placement = guikit::ToastPlacement::{},\n"
        "    .width = {:.1f}f,\n"
        "    .maxVisible = {},\n"
        "    .pauseOnHover = {}\n"
        "}});\n\n"
        "// 4. Command Palette (Ctrl+K)\n"
        "auto cpRes = ui.CommandPalette(isPaletteOpen, commands, {{\n"
        "    .width = {:.1f}f,\n"
        "    .maxVisibleItems = {},\n"
        "    .showFooterHints = {}\n"
        "}});",
        szPrefix,
        VariantName(ctx.btnVariant),
        s_ov.modalWidth,
        s_ov.backdropOpacity,
        s_ov.closeOnBackdrop ? "true" : "false",
        s_ov.closeOnEscape ? "true" : "false",
        s_ov.showCloseButton ? "true" : "false",
        VariantName(ctx.btnVariant),
        szPrefix,
        trigName,
        s_ov.menuMinWidth,
        s_ov.toastDuration,
        s_ov.toastShowProgress ? "true" : "false",
        placementName,
        s_ov.toastWidth,
        s_ov.toastMaxVisible,
        s_ov.toastPauseOnHover ? "true" : "false",
        s_ov.paletteWidth,
        s_ov.paletteMaxVisible,
        s_ov.paletteShowFooter ? "true" : "false"
    );
}

} // namespace guikit::demo
