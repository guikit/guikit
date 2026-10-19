#include "../DemoContext.hpp"
#include <algorithm>
#include <array>
#include <format>
#include <string>
#include <string_view>

namespace guikit::demo {

namespace {

struct TreeDemoState {
    int selectedIndex = 0;
    guikit::Id selectedId = guikit::Id::Null();
    std::string filterQuery;

    guikit::TreeGuides guides = guikit::TreeGuides::Connected;
    guikit::TreeSelectionStyle selectionStyle = guikit::TreeSelectionStyle::Pill;
    float indent = 18.0f;
    bool showColumns = true;
    bool gridLines = true;
    bool searchable = true;
    bool showToolbar = true;
    bool striped = false;
    bool expandOnSelect = false;
    bool showCheckboxes = true;
    bool showEmbeddedControls = true;
    bool showDescriptions = false;
    bool showBadgesAndDetails = true;

    // Checked states for tree items
    bool chkSun = true;
    bool chkFog = true;
    bool chkSonar = true;
    bool chkWaterfall = true;
    bool chkImu = false;
    bool chkShaderSonar = true;
    bool chkShaderWaterfall = true;
    bool chkThemeJson = true;

    // Embedded control states inside TreeItem rows
    bool sceneActive = true;
    float envExposure = 1.15f;
    bool sunEnabled = true;
    float fogDensity = 35.0f;
    size_t sonarModeIdx = 0;
    bool wfTracking = true;
    bool imuEnabled = true;
    int calibrateCount = 0;
};

TreeDemoState s_tree;

constexpr std::string_view GuidesEnumName(guikit::TreeGuides g) noexcept {
    switch (g) {
        case guikit::TreeGuides::None:      return "None";
        case guikit::TreeGuides::Vertical:  return "Vertical";
        case guikit::TreeGuides::Connected: return "Connected";
    }
    return "Connected";
}

} // namespace

void RenderTreeViewInspector(guikit::Context& ui, DemoContext& ctx) {
    if (auto el = ui.Element({.label = "Tree Size"})) {
        if (ui.StepSlider(ctx.controlsSize, {.showTicks = true, .showLabels = true, .includeNone = true}).changed) {
            ui.InvalidateAll();
        }
    }
    ctx.RenderVariantPicker(ui);

    static constexpr std::array<std::string_view, 2> kSelStyleNames = {"Pill", "Indicator"};
    size_t selStyleIdx = (s_tree.selectionStyle == guikit::TreeSelectionStyle::Indicator) ? 1 : 0;
    if (auto el = ui.Element({.label = "Selection Style"})) {
        if (ui.StepSlider(selStyleIdx, kSelStyleNames, {.showTicks = true, .showLabels = true}).changed) {
            s_tree.selectionStyle = (selStyleIdx == 1)
                ? guikit::TreeSelectionStyle::Indicator
                : guikit::TreeSelectionStyle::Pill;
            ui.InvalidateAll();
        }
    }

    static constexpr std::array<std::string_view, 3> kGuideNames = {"None", "Vertical", "Connected"};
    size_t guideIdx = static_cast<size_t>(s_tree.guides);
    if (auto el = ui.Element({.label = "Hierarchy Guides"})) {
        if (ui.StepSlider(guideIdx, kGuideNames, {.showTicks = true, .showLabels = true}).changed) {
            s_tree.guides = static_cast<guikit::TreeGuides>(guideIdx);
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Indent Step"})) {
        if (ui.Slider(s_tree.indent, 12.0f, 32.0f, {.valueFormat = "%.0f px", .step = 2.0f}).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Multi-Column Mode (TreeTable)"})) {
        if (ui.Switch(s_tree.showColumns).changed) ui.InvalidateAll();
    }
    if (s_tree.showColumns) {
        if (auto el = ui.Element({.label = "Column Grid Lines"})) {
            if (ui.Switch(s_tree.gridLines).changed) ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({.label = "Search Filter Bar"})) {
        if (ui.Switch(s_tree.searchable).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Expand/Collapse Toolbar"})) {
        if (ui.Switch(s_tree.showToolbar).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Striped Rows"})) {
        if (ui.Switch(s_tree.striped).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Expand Branch on Select"})) {
        if (ui.Switch(s_tree.expandOnSelect).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Show Tri-State Checkboxes"})) {
        if (ui.Switch(s_tree.showCheckboxes).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Show Embedded Controls"})) {
        if (ui.Switch(s_tree.showEmbeddedControls).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Show Row Descriptions"})) {
        if (ui.Switch(s_tree.showDescriptions).changed) ui.InvalidateAll();
    }
    if (!s_tree.showColumns) {
        if (auto el = ui.Element({.label = "Show Details & Badges"})) {
            if (ui.Switch(s_tree.showBadgesAndDetails).changed) ui.InvalidateAll();
        }
    }
}

void RenderTreeViewStage(guikit::Context& ui, DemoContext& ctx) {
    // Compute tri-state parent checkboxes from leaf states
        const int envCheckedCount = (s_tree.chkSun ? 1 : 0) + (s_tree.chkFog ? 1 : 0);
        bool chkEnvAll = (envCheckedCount == 2);
        const bool indEnv = (envCheckedCount > 0 && envCheckedCount < 2);

        const int sensorCheckedCount = (s_tree.chkSonar ? 1 : 0) + (s_tree.chkWaterfall ? 1 : 0) + (s_tree.chkImu ? 1 : 0);
        bool chkSensorsAll = (sensorCheckedCount == 3);
        const bool indSensors = (sensorCheckedCount > 0 && sensorCheckedCount < 3);

        const int sceneCheckedCount = envCheckedCount + sensorCheckedCount;
        bool chkSceneAll = (sceneCheckedCount == 5);
        const bool indScene = (sceneCheckedCount > 0 && sceneCheckedCount < 5);

        const int shaderCheckedCount = (s_tree.chkShaderSonar ? 1 : 0) + (s_tree.chkShaderWaterfall ? 1 : 0);
        bool chkShadersAll = (shaderCheckedCount == 2);
        const bool indShaders = (shaderCheckedCount > 0 && shaderCheckedCount < 2);

        const int assetsCheckedCount = shaderCheckedCount + (s_tree.chkThemeJson ? 1 : 0);
        bool chkAssetsAll = (assetsCheckedCount == 3);
        const bool indAssets = (assetsCheckedCount > 0 && assetsCheckedCount < 3);

        static constexpr std::array<guikit::TableColumn, 4> kTreeCols = {{
            {.title = "Node / Asset", .flex = 1.6f,    .minWidth = 200.0f, .align = guikit::Align::Start},
            {.title = "Spec",         .width = 104.0f, .minWidth = 72.0f,  .align = guikit::Align::Start},
            {.title = "Status",       .width = 96.0f,  .minWidth = 72.0f,  .align = guikit::Align::Center},
            {.title = "Controls",     .width = 176.0f, .minWidth = 120.0f, .align = guikit::Align::End}
        }};

        const bool useCols = s_tree.showColumns;
        const bool inlineDecor = !useCols && s_tree.showBadgesAndDetails;

        if (auto tree = ui.Size(ctx.controlsSize).TreeView({
            .title = "SCENE & ASSET HIERARCHY",
            .columns = useCols ? std::span<const guikit::TableColumn>(kTreeCols) : std::span<const guikit::TableColumn>{},
            .gridLines = useCols && s_tree.gridLines,
            .guides = s_tree.guides,
            .selectionStyle = s_tree.selectionStyle,
            .variant = ctx.btnVariant,
            .indent = s_tree.indent,
            .striped = s_tree.striped,
            .expandOnSelect = s_tree.expandOnSelect,
            .searchable = s_tree.searchable,
            .filterText = &s_tree.filterQuery,
            .showToolbar = s_tree.showToolbar,
            .selectedIndex = &s_tree.selectedIndex,
            .selectedId = &s_tree.selectedId
        })) {
            // 1. Root Branch: Scene Hierarchy
            if (auto sceneItem = ui.TreeItem({
                .label = "Scene Hierarchy",
                .description = s_tree.showDescriptions ? "Active viewport nodes & real-time subsystems" : "",
                .badge = inlineDecor ? guikit::UiText{"5 nodes", guikit::StateColor::Primary()} : guikit::UiText{},
                .icon = guikit::Icon::Folder(),
                .defaultOpen = true,
                .checked = s_tree.showCheckboxes ? &chkSceneAll : nullptr,
                .indeterminate = s_tree.showCheckboxes && indScene
            })) {
                if (sceneItem.checkChanged) {
                    s_tree.chkSun = s_tree.chkFog = s_tree.chkSonar = s_tree.chkWaterfall = s_tree.chkImu = chkSceneAll;
                }
                if (useCols) {
                    sceneItem.CellText("Viewport", {.variant = guikit::UiVariant::Secondary});
                    sceneItem.CellBadge("5 nodes", guikit::UiVariant::Primary);
                }
                if (s_tree.showEmbeddedControls) {
                    ui.Switch(s_tree.sceneActive);
                }

                // 1.1 Environment & Lighting
                if (auto envItem = ui.TreeItem({
                    .label = "Environment & Lighting",
                    .description = s_tree.showDescriptions ? "PBR skybox, directional sun, and volumetric fog" : "",
                    .icon = guikit::Icon::Eye(),
                    .defaultOpen = true,
                    .checked = s_tree.showCheckboxes ? &chkEnvAll : nullptr,
                    .indeterminate = s_tree.showCheckboxes && indEnv
                })) {
                    if (envItem.checkChanged) {
                        s_tree.chkSun = s_tree.chkFog = chkEnvAll;
                    }
                    if (useCols) {
                        envItem.CellText("PBR Group", {.variant = guikit::UiVariant::Secondary});
                        envItem.CellBadge("2 passes", guikit::UiVariant::Info);
                    }
                    if (s_tree.showEmbeddedControls) {
                        ui.Slider(s_tree.envExposure, 0.0f, 2.5f, {
                            .valueFormat = "%.2f EV",
                            .width = useCols ? std::optional<float>{} : std::optional<float>{128.0f}
                        });
                    }

                    if (auto sunItem = ui.TreeItem({
                        .label = "Directional Sun",
                        .description = s_tree.showDescriptions ? "Cascaded shadow map primary light source" : "",
                        .detail = inlineDecor ? "6500K" : "",
                        .badge = inlineDecor ? guikit::UiText{"HDR", guikit::StateColor::Warning()} : guikit::UiText{},
                        .icon = guikit::Icon::Eye(),
                        .checked = s_tree.showCheckboxes ? &s_tree.chkSun : nullptr
                    })) {
                        if (useCols) {
                            sunItem.CellText("6500K", {.variant = guikit::UiVariant::Secondary});
                            sunItem.CellBadge("HDR", guikit::UiVariant::Warning);
                        }
                        if (s_tree.showEmbeddedControls) {
                            ui.Switch(s_tree.sunEnabled);
                        }
                    }

                    if (auto fogItem = ui.TreeItem({
                        .label = "Volumetric Fog",
                        .description = s_tree.showDescriptions ? "Exponential height fog with temporal reprojection" : "",
                        .detail = inlineDecor ? "Raymarched" : "",
                        .icon = guikit::Icon::Eye(),
                        .checked = s_tree.showCheckboxes ? &s_tree.chkFog : nullptr
                    })) {
                        if (useCols) {
                            fogItem.CellText("Raymarched", {.variant = guikit::UiVariant::Secondary});
                            fogItem.CellBadge("Froxel", guikit::UiVariant::Default);
                        }
                        if (s_tree.showEmbeddedControls) {
                            ui.Slider(s_tree.fogDensity, 0.0f, 100.0f, {
                                .valueFormat = "%.0f%%",
                                .width = useCols ? std::optional<float>{} : std::optional<float>{110.0f}
                            });
                        }
                    }
                }

                // 1.2 Sensors & Telemetry
                if (auto sensorsItem = ui.TreeItem({
                    .label = "Sensors & Telemetry",
                    .description = s_tree.showDescriptions ? "Acoustic, RF spectrum, and inertial streams" : "",
                    .badge = inlineDecor ? guikit::UiText{"3 ch", guikit::StateColor::Info()} : guikit::UiText{},
                    .icon = guikit::Icon::Search(),
                    .defaultOpen = true,
                    .checked = s_tree.showCheckboxes ? &chkSensorsAll : nullptr,
                    .indeterminate = s_tree.showCheckboxes && indSensors
                })) {
                    if (sensorsItem.checkChanged) {
                        s_tree.chkSonar = s_tree.chkWaterfall = s_tree.chkImu = chkSensorsAll;
                    }
                    if (useCols) {
                        sensorsItem.CellText("DSP Bus", {.variant = guikit::UiVariant::Secondary});
                        sensorsItem.CellBadge("3 ch", guikit::UiVariant::Info);
                    }

                    static constexpr std::array<std::string_view, 3> kSonarModes = {"Active", "Passive", "CW"};
                    if (auto sonarItem = ui.TreeItem({
                        .label = "Sonar Array PPI",
                        .description = s_tree.showDescriptions ? "128-beam polar acoustic beamforming receiver" : "",
                        .detail = inlineDecor ? "750 kHz" : "",
                        .icon = guikit::Icon::Play(),
                        .checked = s_tree.showCheckboxes ? &s_tree.chkSonar : nullptr
                    })) {
                        if (useCols) {
                            sonarItem.CellText("750 kHz", {.variant = guikit::UiVariant::Secondary});
                            sonarItem.CellBadge("128-Beam", guikit::UiVariant::Primary);
                        }
                        if (s_tree.showEmbeddedControls) {
                            ui.Select(s_tree.sonarModeIdx, kSonarModes, {
                                .width = useCols ? std::optional<float>{} : std::optional<float>{100.0f}
                            });
                        }
                    }

                    if (auto wfItem = ui.TreeItem({
                        .label = "RTSA Waterfall",
                        .description = s_tree.showDescriptions ? "Real-time FFT spectrum & DOA target tracker" : "",
                        .detail = inlineDecor ? "2.4 GHz" : "",
                        .badge = inlineDecor ? guikit::UiText{"LIVE", guikit::StateColor::Success()} : guikit::UiText{},
                        .icon = guikit::Icon::Play(),
                        .checked = s_tree.showCheckboxes ? &s_tree.chkWaterfall : nullptr
                    })) {
                        if (useCols) {
                            wfItem.CellText("2.4 GHz", {.variant = guikit::UiVariant::Secondary});
                            wfItem.CellBadge("LIVE", guikit::UiVariant::Success);
                        }
                        if (s_tree.showEmbeddedControls) {
                            // Multi-widget cell: Switch + Button grouped inside a single column cell
                            if (auto cell = wfItem.Cell()) {
                                ui.Switch(s_tree.wfTracking);
                                if (ui.Button({
                                    .text = "Calibrate",
                                    .color = guikit::StateColor::Secondary()
                                })) {
                                    ++s_tree.calibrateCount;
                                }
                            }
                        }
                    }

                    if (auto imuItem = ui.TreeItem({
                        .label = "Inertial Navigation (IMU)",
                        .description = s_tree.showDescriptions ? "6-DOF strapdown attitude & heading reference" : "",
                        .detail = inlineDecor ? "400 Hz" : "",
                        .icon = guikit::Icon::Settings(),
                        .checked = s_tree.showCheckboxes ? &s_tree.chkImu : nullptr
                    })) {
                        if (useCols) {
                            imuItem.CellText("400 Hz", {.variant = guikit::UiVariant::Secondary});
                            imuItem.CellBadge("6-DOF", guikit::UiVariant::Default);
                        }
                        if (s_tree.showEmbeddedControls) {
                            ui.Switch(s_tree.imuEnabled);
                        }
                    }
                }
            }

            // 2. Root Branch: Build & Assets
            if (auto assetsItem = ui.TreeItem({
                .label = "Build & Assets",
                .description = s_tree.showDescriptions ? "Compiled GLSL shaders and runtime theme bundles" : "",
                .detail = inlineDecor ? "14.2 MB" : "",
                .icon = guikit::Icon::Folder(),
                .defaultOpen = true,
                .checked = s_tree.showCheckboxes ? &chkAssetsAll : nullptr,
                .indeterminate = s_tree.showCheckboxes && indAssets
            })) {
                if (assetsItem.checkChanged) {
                    s_tree.chkShaderSonar = s_tree.chkShaderWaterfall = s_tree.chkThemeJson = chkAssetsAll;
                }
                if (useCols) {
                    assetsItem.CellText("14.2 MB", {.variant = guikit::UiVariant::Secondary});
                    assetsItem.CellBadge("Bundle", guikit::UiVariant::Default);
                }

                if (auto shadersItem = ui.TreeItem({
                    .label = "Shaders (GLSL 330)",
                    .description = s_tree.showDescriptions ? "Hardware-accelerated GPU underlay programs" : "",
                    .badge = inlineDecor ? guikit::UiText{"2 files", guikit::StateColor::Muted()} : guikit::UiText{},
                    .icon = guikit::Icon::Folder(),
                    .defaultOpen = true,
                    .checked = s_tree.showCheckboxes ? &chkShadersAll : nullptr,
                    .indeterminate = s_tree.showCheckboxes && indShaders
                })) {
                    if (shadersItem.checkChanged) {
                        s_tree.chkShaderSonar = s_tree.chkShaderWaterfall = chkShadersAll;
                    }
                    if (useCols) {
                        shadersItem.CellText("GLSL 330", {.variant = guikit::UiVariant::Secondary});
                        shadersItem.CellBadge("2 files", guikit::UiVariant::Secondary);
                    }

                    if (auto s1 = ui.TreeItem({
                        .label = "sonar_ppi.frag",
                        .detail = inlineDecor ? "4.8 KB" : "",
                        .badge = inlineDecor ? guikit::UiText{"OK", guikit::StateColor::Success()} : guikit::UiText{},
                        .icon = guikit::Icon::Edit(),
                        .checked = s_tree.showCheckboxes ? &s_tree.chkShaderSonar : nullptr
                    })) {
                        if (useCols) {
                            s1.CellText("4.8 KB", {.variant = guikit::UiVariant::Secondary});
                            s1.CellBadge("OK", guikit::UiVariant::Success);
                        }
                    }

                    if (auto s2 = ui.TreeItem({
                        .label = "waterfall_doa.frag",
                        .detail = inlineDecor ? "6.1 KB" : "",
                        .badge = inlineDecor ? guikit::UiText{"OK", guikit::StateColor::Success()} : guikit::UiText{},
                        .icon = guikit::Icon::Edit(),
                        .checked = s_tree.showCheckboxes ? &s_tree.chkShaderWaterfall : nullptr
                    })) {
                        if (useCols) {
                            s2.CellText("6.1 KB", {.variant = guikit::UiVariant::Secondary});
                            s2.CellBadge("OK", guikit::UiVariant::Success);
                        }
                    }
                }

                if (auto themeItem = ui.TreeItem({
                    .label = "theme_tokens.json",
                    .description = s_tree.showDescriptions ? "Design system color & typography specification" : "",
                    .detail = inlineDecor ? "2.1 KB" : "",
                    .icon = guikit::Icon::Settings(),
                    .checked = s_tree.showCheckboxes ? &s_tree.chkThemeJson : nullptr
                })) {
                    if (useCols) {
                        themeItem.CellText("2.1 KB", {.variant = guikit::UiVariant::Secondary});
                        themeItem.CellBadge("JSON", guikit::UiVariant::Info);
                    }
                }
            }
        }
}

std::string GenerateTreeViewCode(const DemoContext& ctx) {
    const std::string sizePrefix = FormatUiSizePrefix(ctx.controlsSize);
    const bool useCols = s_tree.showColumns;
    std::string out;

    if (useCols) {
        out +=
            "static constexpr std::array<guikit::TableColumn, 4> kTreeCols = {{\n"
            "    {.title = \"Node / Asset\", .flex = 1.6f,    .minWidth = 200.0f, .align = guikit::Align::Start},\n"
            "    {.title = \"Spec\",         .width = 104.0f, .minWidth = 72.0f,  .align = guikit::Align::Start},\n"
            "    {.title = \"Status\",       .width = 96.0f,  .minWidth = 72.0f,  .align = guikit::Align::Center},\n"
            "    {.title = \"Controls\",     .width = 176.0f, .minWidth = 120.0f, .align = guikit::Align::End}\n"
            "}};\n\n";
    }

    out += std::format(
        "if (auto tree = ui.{}TreeView({{\n"
        "    .title = \"SCENE & ASSET HIERARCHY\",\n",
        sizePrefix
    );
    if (useCols) {
        out += std::format(
            "    .columns = kTreeCols,\n"
            "    .gridLines = {},\n",
            s_tree.gridLines ? "true" : "false"
        );
    }
    out += std::format(
        "    .guides = guikit::TreeGuides::{},\n"
        "    .selectionStyle = guikit::TreeSelectionStyle::{},\n"
        "    .variant = guikit::UiVariant::{},\n"
        "    .indent = {:.1f}f,\n"
        "    .striped = {},\n"
        "    .expandOnSelect = {},\n"
        "    .searchable = {},\n"
        "    .filterText = &filterQuery,\n"
        "    .showToolbar = {},\n"
        "    .selectedIndex = &selectedIndex,\n"
        "    .selectedId = &selectedId\n"
        "}})) {{\n",
        GuidesEnumName(s_tree.guides),
        s_tree.selectionStyle == guikit::TreeSelectionStyle::Indicator ? "Indicator" : "Pill",
        VariantName(ctx.btnVariant),
        s_tree.indent,
        s_tree.striped ? "true" : "false",
        s_tree.expandOnSelect ? "true" : "false",
        s_tree.searchable ? "true" : "false",
        s_tree.showToolbar ? "true" : "false"
    );

    const bool inlineDecor = !useCols && s_tree.showBadgesAndDetails;

    auto appendItemOpts = [&](std::string_view indent,
                              std::string_view varName,
                              std::string_view label,
                              std::string_view desc,
                              std::string_view detail,
                              std::string_view badgeExpr,
                              std::string_view iconExpr,
                              bool defaultOpen,
                              std::string_view checkedVar,
                              std::string_view indVar) {
        out += std::format("{}if (auto {} = ui.TreeItem({{\n", indent, varName);
        out += std::format("{}    .label = \"{}\",\n", indent, label);
        if (s_tree.showDescriptions && !desc.empty()) {
            out += std::format("{}    .description = \"{}\",\n", indent, desc);
        }
        if (inlineDecor && !detail.empty()) {
            out += std::format("{}    .detail = \"{}\",\n", indent, detail);
        }
        if (inlineDecor && !badgeExpr.empty()) {
            out += std::format("{}    .badge = {},\n", indent, badgeExpr);
        }
        out += std::format("{}    .icon = {}", indent, iconExpr);
        if (defaultOpen) {
            out += std::format(",\n{}    .defaultOpen = true", indent);
        }
        if (s_tree.showCheckboxes && !checkedVar.empty()) {
            out += std::format(",\n{}    .checked = &{}", indent, checkedVar);
            if (!indVar.empty()) {
                out += std::format(",\n{}    .indeterminate = {}", indent, indVar);
            }
        }
        out += std::format("\n{}}})) {{\n", indent);
    };

    // 1. Root Branch: Scene Hierarchy
    appendItemOpts(
        "    ", "sceneItem", "Scene Hierarchy",
        "Active viewport nodes & real-time subsystems", "",
        "{\"5 nodes\", guikit::StateColor::Primary()}",
        "guikit::Icon::Folder()", true, "chkSceneAll", "indScene"
    );
    if (s_tree.showCheckboxes) {
        out += "        if (sceneItem.checkChanged) {\n"
               "            chkSun = chkFog = chkSonar = chkWaterfall = chkImu = chkSceneAll;\n"
               "        }\n";
    }
    if (useCols) {
        out += "        sceneItem.CellText(\"Viewport\", {.variant = guikit::UiVariant::Secondary});\n"
               "        sceneItem.CellBadge(\"5 nodes\", guikit::UiVariant::Primary);\n";
    }
    if (s_tree.showEmbeddedControls) {
        out += "        ui.Switch(sceneActive);\n";
    }
    out += "\n";

    // 1.1 Environment & Lighting
    appendItemOpts(
        "        ", "envItem", "Environment & Lighting",
        "PBR skybox, directional sun, and volumetric fog", "", "",
        "guikit::Icon::Eye()", true, "chkEnvAll", "indEnv"
    );
    if (s_tree.showCheckboxes) {
        out += "            if (envItem.checkChanged) {\n"
               "                chkSun = chkFog = chkEnvAll;\n"
               "            }\n";
    }
    if (useCols) {
        out += "            envItem.CellText(\"PBR Group\", {.variant = guikit::UiVariant::Secondary});\n"
               "            envItem.CellBadge(\"2 passes\", guikit::UiVariant::Info);\n";
    }
    if (s_tree.showEmbeddedControls) {
        out += useCols
            ? "            ui.Slider(envExposure, 0.0f, 2.5f, {.valueFormat = \"%.2f EV\"});\n"
            : "            ui.Slider(envExposure, 0.0f, 2.5f, {.valueFormat = \"%.2f EV\", .width = 128.0f});\n";
    }
    out += "\n";

    appendItemOpts(
        "            ", "sunItem", "Directional Sun",
        "Cascaded shadow map primary light source", "6500K",
        "{\"HDR\", guikit::StateColor::Warning()}",
        "guikit::Icon::Eye()", false, "chkSun", ""
    );
    if (useCols) {
        out += "                sunItem.CellText(\"6500K\", {.variant = guikit::UiVariant::Secondary});\n"
               "                sunItem.CellBadge(\"HDR\", guikit::UiVariant::Warning);\n";
    }
    if (s_tree.showEmbeddedControls) {
        out += "                ui.Switch(sunEnabled);\n";
    }
    out += "            }\n\n";

    appendItemOpts(
        "            ", "fogItem", "Volumetric Fog",
        "Exponential height fog with temporal reprojection", "Raymarched", "",
        "guikit::Icon::Eye()", false, "chkFog", ""
    );
    if (useCols) {
        out += "                fogItem.CellText(\"Raymarched\", {.variant = guikit::UiVariant::Secondary});\n"
               "                fogItem.CellBadge(\"Froxel\", guikit::UiVariant::Default);\n";
    }
    if (s_tree.showEmbeddedControls) {
        out += useCols
            ? "                ui.Slider(fogDensity, 0.0f, 100.0f, {.valueFormat = \"%.0f%%\"});\n"
            : "                ui.Slider(fogDensity, 0.0f, 100.0f, {.valueFormat = \"%.0f%%\", .width = 110.0f});\n";
    }
    out += "            }\n";
    out += "        }\n\n";

    // 1.2 Sensors & Telemetry
    appendItemOpts(
        "        ", "sensorsItem", "Sensors & Telemetry",
        "Acoustic, RF spectrum, and inertial streams", "",
        "{\"3 ch\", guikit::StateColor::Info()}",
        "guikit::Icon::Search()", true, "chkSensorsAll", "indSensors"
    );
    if (s_tree.showCheckboxes) {
        out += "            if (sensorsItem.checkChanged) {\n"
               "                chkSonar = chkWaterfall = chkImu = chkSensorsAll;\n"
               "            }\n";
    }
    if (useCols) {
        out += "            sensorsItem.CellText(\"DSP Bus\", {.variant = guikit::UiVariant::Secondary});\n"
               "            sensorsItem.CellBadge(\"3 ch\", guikit::UiVariant::Info);\n";
    }

    appendItemOpts(
        "            ", "sonarItem", "Sonar Array PPI",
        "128-beam polar acoustic beamforming receiver", "750 kHz", "",
        "guikit::Icon::Play()", false, "chkSonar", ""
    );
    if (useCols) {
        out += "                sonarItem.CellText(\"750 kHz\", {.variant = guikit::UiVariant::Secondary});\n"
               "                sonarItem.CellBadge(\"128-Beam\", guikit::UiVariant::Primary);\n";
    }
    if (s_tree.showEmbeddedControls) {
        out += useCols
            ? "                ui.Select(sonarModeIdx, kSonarModes);\n"
            : "                ui.Select(sonarModeIdx, kSonarModes, {.width = 100.0f});\n";
    }
    out += "            }\n\n";

    appendItemOpts(
        "            ", "wfItem", "RTSA Waterfall",
        "Real-time FFT spectrum & DOA target tracker", "2.4 GHz",
        "{\"LIVE\", guikit::StateColor::Success()}",
        "guikit::Icon::Play()", false, "chkWaterfall", ""
    );
    if (useCols) {
        out += "                wfItem.CellText(\"2.4 GHz\", {.variant = guikit::UiVariant::Secondary});\n"
               "                wfItem.CellBadge(\"LIVE\", guikit::UiVariant::Success);\n";
    }
    if (s_tree.showEmbeddedControls) {
        out += "                // Multi-widget cell: Switch + Button inside a single column cell\n"
               "                if (auto cell = wfItem.Cell()) {\n"
               "                    ui.Switch(wfTracking);\n"
               "                    if (ui.Button({.text = \"Calibrate\", .color = guikit::StateColor::Secondary()})) {\n"
               "                        ++calibrateCount;\n"
               "                    }\n"
               "                }\n";
    }
    out += "            }\n\n";

    appendItemOpts(
        "            ", "imuItem", "Inertial Navigation (IMU)",
        "6-DOF strapdown attitude & heading reference", "400 Hz", "",
        "guikit::Icon::Settings()", false, "chkImu", ""
    );
    if (useCols) {
        out += "                imuItem.CellText(\"400 Hz\", {.variant = guikit::UiVariant::Secondary});\n"
               "                imuItem.CellBadge(\"6-DOF\", guikit::UiVariant::Default);\n";
    }
    if (s_tree.showEmbeddedControls) {
        out += "                ui.Switch(imuEnabled);\n";
    }
    out += "            }\n";
    out += "        }\n";
    out += "    }\n\n";

    // 2. Root Branch: Build & Assets
    appendItemOpts(
        "    ", "assetsItem", "Build & Assets",
        "Compiled GLSL shaders and runtime theme bundles", "14.2 MB", "",
        "guikit::Icon::Folder()", true, "chkAssetsAll", "indAssets"
    );
    if (s_tree.showCheckboxes) {
        out += "        if (assetsItem.checkChanged) {\n"
               "            chkShaderSonar = chkShaderWaterfall = chkThemeJson = chkAssetsAll;\n"
               "        }\n";
    }
    if (useCols) {
        out += "        assetsItem.CellText(\"14.2 MB\", {.variant = guikit::UiVariant::Secondary});\n"
               "        assetsItem.CellBadge(\"Bundle\", guikit::UiVariant::Default);\n";
    }

    appendItemOpts(
        "        ", "shadersItem", "Shaders (GLSL 330)",
        "Hardware-accelerated GPU underlay programs", "",
        "{\"2 files\", guikit::StateColor::Muted()}",
        "guikit::Icon::Folder()", true, "chkShadersAll", "indShaders"
    );
    if (s_tree.showCheckboxes) {
        out += "            if (shadersItem.checkChanged) {\n"
               "                chkShaderSonar = chkShaderWaterfall = chkShadersAll;\n"
               "            }\n";
    }
    if (useCols) {
        out += "            shadersItem.CellText(\"GLSL 330\", {.variant = guikit::UiVariant::Secondary});\n"
               "            shadersItem.CellBadge(\"2 files\", guikit::UiVariant::Secondary);\n";
    }

    appendItemOpts(
        "            ", "s1", "sonar_ppi.frag",
        "", "4.8 KB", "{\"OK\", guikit::StateColor::Success()}",
        "guikit::Icon::Edit()", false, "chkShaderSonar", ""
    );
    if (useCols) {
        out += "                s1.CellText(\"4.8 KB\", {.variant = guikit::UiVariant::Secondary});\n"
               "                s1.CellBadge(\"OK\", guikit::UiVariant::Success);\n";
    }
    out += "            }\n\n";

    appendItemOpts(
        "            ", "s2", "waterfall_doa.frag",
        "", "6.1 KB", "{\"OK\", guikit::StateColor::Success()}",
        "guikit::Icon::Edit()", false, "chkShaderWaterfall", ""
    );
    if (useCols) {
        out += "                s2.CellText(\"6.1 KB\", {.variant = guikit::UiVariant::Secondary});\n"
               "                s2.CellBadge(\"OK\", guikit::UiVariant::Success);\n";
    }
    out += "            }\n";
    out += "        }\n\n";

    appendItemOpts(
        "        ", "themeItem", "theme_tokens.json",
        "Design system color & typography specification", "2.1 KB", "",
        "guikit::Icon::Settings()", false, "chkThemeJson", ""
    );
    if (useCols) {
        out += "            themeItem.CellText(\"2.1 KB\", {.variant = guikit::UiVariant::Secondary});\n"
               "            themeItem.CellBadge(\"JSON\", guikit::UiVariant::Info);\n";
    }
    out += "        }\n";
    out += "    }\n";
    out += "}";

    return out;
}

} // namespace guikit::demo
