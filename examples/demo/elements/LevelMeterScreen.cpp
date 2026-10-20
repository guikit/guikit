#include "../DemoContext.hpp"
#include "guikit/widgets/LevelMeter.hpp"
#include <array>
#include <cmath>
#include <format>

namespace guikit::demo {

namespace {

struct LevelMeterDemoState {
    size_t channelModeIdx = 1; // 0 = Single-Channel (Mono dBFS), 1 = Stereo Dual-Channel (L / R)
    size_t orientationIdx = 0; // 0 = Horizontal, 1 = Vertical
    float levelDb = -8.5f;
    float stereoSpreadDb = 3.5f;
    bool simulateLive = false;
    bool segmented = true;
    uint32_t segments = 24;
    bool multiZoneColor = true;
    bool autoPeakHold = true;
    bool showClipLed = true;
    bool showScale = true;
    bool showValue = true;
    float barThickness = 12.0f;
};

LevelMeterDemoState s_state;

} // namespace

void RenderLevelMeterInspector(guikit::Context& ui, DemoContext&) {
    static constexpr std::array<std::string_view, 2> channelModes = {"Mono (1ch)", "Stereo (L/R)"};
    if (auto el = ui.Element({
        .label = "Channel Configuration",
        .description = "Single-channel dBFS vs Stereo L/R pair"
    })) {
        if (ui.StepSlider(s_state.channelModeIdx, channelModes, {
            .showTicks = true,
            .showLabels = true
        }).changed) {
            ui.InvalidateAll();
        }
    }

    static constexpr std::array<std::string_view, 2> orientNames = {"Horizontal", "Vertical"};
    if (auto el = ui.Element({.label = "Orientation"})) {
        if (ui.StepSlider(s_state.orientationIdx, orientNames, {
            .showTicks = true,
            .showLabels = true
        }).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({
        .label = "Signal Level (dBFS)",
        .description = "Drive to 0.0 dB to trigger Clip LED (click meter to clear)"
    })) {
        if (ui.Slider(s_state.levelDb, -60.0f, 0.0f, {
            .valueFormat = "%.1f dB",
            .step = 0.5f
        }).changed) {
            ui.InvalidateAll();
        }
    }

    if (s_state.channelModeIdx == 1) {
        if (auto el = ui.Element({.label = "Stereo L/R Offset"})) {
            if (ui.Slider(s_state.stereoSpreadDb, -12.0f, 12.0f, {
                .valueFormat = "%+.1f dB",
                .step = 0.5f
            }).changed) {
                ui.InvalidateAll();
            }
        }
    }

    if (auto el = ui.Element({
        .label = "Simulate Live Signal",
        .description = "Animate dynamic RF / Audio telemetry modulation"
    })) {
        if (ui.Switch(s_state.simulateLive).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({
        .label = "Discrete LED Segments",
        .description = "Segmented LED bar vs smooth continuous ribbon"
    })) {
        if (ui.Switch(s_state.segmented).changed) {
            ui.InvalidateAll();
        }
    }

    if (s_state.segmented) {
        if (auto el = ui.Element({.label = "LED Segment Count"})) {
            if (ui.Slider(s_state.segments, 8u, 40u, {
                .valueFormat = "%u LEDs",
                .step = 2.0
            }).changed) {
                ui.InvalidateAll();
            }
        }
    }

    if (auto el = ui.Element({.label = "Bar Thickness"})) {
        if (ui.Slider(s_state.barThickness, 6.0f, 24.0f, {
            .valueFormat = "%.0f px",
            .step = 1.0f
        }).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "3-Zone Colors (Green / Warn / Danger)"})) {
        if (ui.Switch(s_state.multiZoneColor).changed) ui.InvalidateAll();
    }

    if (auto el = ui.Element({.label = "Auto Peak-Hold & Decay"})) {
        if (ui.Switch(s_state.autoPeakHold).changed) ui.InvalidateAll();
    }

    if (auto el = ui.Element({.label = "Show Overload / Clip LED"})) {
        if (ui.Switch(s_state.showClipLed).changed) ui.InvalidateAll();
    }

    if (auto el = ui.Element({.label = "Show Calibrated dB Scale"})) {
        if (ui.Switch(s_state.showScale).changed) ui.InvalidateAll();
    }

    if (auto el = ui.Element({.label = "Show Numeric Readout"})) {
        if (ui.Switch(s_state.showValue).changed) ui.InvalidateAll();
    }
}

void RenderLevelMeterStage(guikit::Context& ui, DemoContext& ctx) {
    float chLeft = s_state.levelDb;
    float chRight = std::clamp(s_state.levelDb - s_state.stereoSpreadDb, -60.0f, 0.0f);

    if (s_state.simulateLive) {
        const double t = ui.GetCurrentTimeSec();
        const float envL = static_cast<float>(
            std::sin(t * 4.2) * 4.5 +
            std::sin(t * 11.3) * 2.8 +
            std::max(0.0, std::sin(t * 1.9)) * 4.2
        );
        const float envR = static_cast<float>(
            std::sin(t * 3.8 + 0.9) * 4.5 +
            std::cos(t * 9.7) * 3.0 +
            std::max(0.0, std::sin(t * 1.9 + 0.4)) * 4.0
        );
        chLeft = std::clamp(s_state.levelDb + envL, -60.0f, 0.0f);
        chRight = std::clamp(s_state.levelDb - s_state.stereoSpreadDb + envR, -60.0f, 0.0f);
        ui.RequestRepaint(2);
    }

    const guikit::Orientation orient = (s_state.orientationIdx == 0)
        ? guikit::Orientation::Horizontal
        : guikit::Orientation::Vertical;

    static constexpr std::array<std::string_view, 2> kStereoLabels = {"L", "R"};
    static constexpr std::array<std::string_view, 1> kMonoLabels = {"IN"};

    guikit::LevelMeterOptions opts{
        .orientation = orient,
        .segments = s_state.segmented ? s_state.segments : 0u,
        .multiZoneColor = s_state.multiZoneColor,
        .autoPeakHold = s_state.autoPeakHold,
        .showClipLed = s_state.showClipLed,
        .showScale = s_state.showScale,
        .showValue = s_state.showValue,
        .valueFormat = "%.1f dB",
        .channelLabels = (s_state.channelModeIdx == 1)
            ? std::span<const std::string_view>(kStereoLabels)
            : std::span<const std::string_view>(kMonoLabels),
        .barThickness = s_state.barThickness,
        .color = ctx.GetControlColor(),
        .stretch = ctx.GetElementStretch(),
        .align = ctx.elemAlign,
        .width = ctx.GetElementWidth(),
        .height = ctx.GetElementHeight(),
        .background = ctx.GetControlBackground(NavCategory::ElemLevelMeter),
        .border = ctx.GetCtrlBorder(),
        .tooltip = "Click meter to reset Peak-Hold & Clip latch",
        .disabled = ctx.elemDisabled
    };

    if (auto el = ui.Size(ctx.elemSize).Element({
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
        .disabled = ctx.elemDisabled
    })) {
        if (auto sz = ui.Size(ctx.controlsSize)) {
            if (s_state.channelModeIdx == 1) {
                const std::array<float, 2> stereoVals = {chLeft, chRight};
                guikit::LevelMeter::Run(ui, stereoVals, -60.0f, 0.0f, opts);
            } else {
                guikit::LevelMeter::Run(ui, static_cast<double>(chLeft), -60.0, 0.0, opts);
            }
        }
    }
}

std::string GenerateLevelMeterCode(const DemoContext& ctx) {
    const std::string_view orientName = (s_state.orientationIdx == 0) ? "Horizontal" : "Vertical";
    const uint32_t segVal = s_state.segmented ? s_state.segments : 0u;

    if (s_state.channelModeIdx == 1) {
        return std::format(
            "static constexpr std::array<std::string_view, 2> kChannels = {{\"L\", \"R\"}};\n"
            "const std::array<float, 2> levelsDb = {{{:.1f}f, {:.1f}f}};\n"
            "if (auto el = ui.{}Element({{\n"
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
            "    .disabled = {}\n"
            "}})) {{\n"
            "    ui.{}LevelMeter(levelsDb, -60.0f, 0.0f, {{\n"
            "        .orientation = guikit::Orientation::{},\n"
            "        .segments = {}u,\n"
            "        .multiZoneColor = {},\n"
            "        .autoPeakHold = {},\n"
            "        .showClipLed = {},\n"
            "        .showScale = {},\n"
            "        .showValue = {},\n"
            "        .valueFormat = \"%.1f dB\",\n"
            "        .channelLabels = kChannels,\n"
            "        .barThickness = {:.1f}f,\n"
            "        .color = {},\n"
            "        .stretch = guikit::StretchMode::{},\n"
            "        .width = {},\n"
            "        .height = {},\n"
            "        .background = {},\n"
            "        .border = {},\n"
            "        .disabled = {}\n"
            "    }});\n"
            "}}",
            s_state.levelDb,
            std::clamp(s_state.levelDb - s_state.stereoSpreadDb, -60.0f, 0.0f),
            FormatUiSizePrefix(ctx.elemSize),
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
            ctx.elemDisabled ? "true" : "false",
            FormatUiSizePrefix(ctx.controlsSize),
            orientName,
            segVal,
            s_state.multiZoneColor ? "true" : "false",
            s_state.autoPeakHold ? "true" : "false",
            s_state.showClipLed ? "true" : "false",
            s_state.showScale ? "true" : "false",
            s_state.showValue ? "true" : "false",
            s_state.barThickness,
            ctx.FormatControlColorCode(),
            StretchModeEnumName(ctx.GetElementStretch()),
            ctx.FormatElementWidthCode(),
            ctx.FormatElementHeightCode(),
            ctx.FormatControlBackgroundCode(NavCategory::ElemLevelMeter),
            ctx.FormatCtrlBorderCode(),
            ctx.elemDisabled ? "true" : "false"
        );
    }

    return std::format(
        "if (auto el = ui.{}Element({{\n"
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
        "    .disabled = {}\n"
        "}})) {{\n"
        "    ui.{}LevelMeter(signalDb, -60.0, 0.0, {{\n"
        "        .orientation = guikit::Orientation::{},\n"
        "        .segments = {}u,\n"
        "        .multiZoneColor = {},\n"
        "        .autoPeakHold = {},\n"
        "        .showClipLed = {},\n"
        "        .showScale = {},\n"
        "        .showValue = {},\n"
        "        .valueFormat = \"%.1f dB\",\n"
        "        .barThickness = {:.1f}f,\n"
        "        .color = {},\n"
        "        .stretch = guikit::StretchMode::{},\n"
        "        .width = {},\n"
        "        .height = {},\n"
        "        .background = {},\n"
        "        .border = {},\n"
        "        .disabled = {}\n"
        "    }});\n"
        "}}",
        FormatUiSizePrefix(ctx.elemSize),
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
        ctx.elemDisabled ? "true" : "false",
        FormatUiSizePrefix(ctx.controlsSize),
        orientName,
        segVal,
        s_state.multiZoneColor ? "true" : "false",
        s_state.autoPeakHold ? "true" : "false",
        s_state.showClipLed ? "true" : "false",
        s_state.showScale ? "true" : "false",
        s_state.showValue ? "true" : "false",
        s_state.barThickness,
        ctx.FormatControlColorCode(),
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.FormatElementWidthCode(),
        ctx.FormatElementHeightCode(),
        ctx.FormatControlBackgroundCode(NavCategory::ElemLevelMeter),
        ctx.FormatCtrlBorderCode(),
        ctx.elemDisabled ? "true" : "false"
    );
}

} // namespace guikit::demo
