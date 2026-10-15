#pragma once

#include <array>
#include <string>
#include <string_view>
#include "guikit/GuiKit.hpp"

namespace guikit::demo {

enum class NavCategory : uint8_t {
    // 1. ELEMENTS (Atomic Controls)
    ElemHeading = 0,
    ElemButton,
    ElemSlider,
    ElemInputField,
    ElemSelect,
    ElemSwitch,
    ElemCheckbox,

    // 2. PANELS & NAV (Containers & Navigation)
    PanelCard,
    PanelTabBar,
    PanelSidebar,

    // 3. CONTENT & MEDIA (Documents, Editors, Graphics)
    ContentImage,
    ContentMarkdown,
    ContentCodeEditor,

    // 4. 2D SCENES (Interactive Vector Viewports & GIS)
    SceneSchematic2D,
    SceneOsmMap,

    // 5. ASSEMBLY (Compositions & Real-World Screens)
    AsmDashboard,
    AsmFormControls,
    AsmFlexRibbon,
    AsmDiagnostics,
    AsmThemeStudio
};

constexpr bool IsElementCategory(NavCategory cat) noexcept {
    return cat <= NavCategory::ElemCheckbox;
}

constexpr bool IsSandboxCategory(NavCategory cat) noexcept {
    return cat <= NavCategory::SceneOsmMap;
}

constexpr bool IsFullStageCategory(NavCategory cat) noexcept {
    return cat == NavCategory::PanelSidebar ||
           cat == NavCategory::ContentMarkdown ||
           cat == NavCategory::ContentCodeEditor ||
           cat == NavCategory::SceneSchematic2D ||
           cat == NavCategory::SceneOsmMap;
}

inline constexpr std::array<std::string_view, 3> kStretchModeNames = {"Compact", "Space", "Fill"};
inline constexpr std::array<std::string_view, 4> kElementLayoutNames = {"Stacked", "Top", "Bottom", "Inline"};
inline constexpr std::array<std::string_view, 2> kInlineOrderNames = {"LabelFirst", "ControlFirst"};
inline constexpr std::array<std::string_view, 3> kLabelAlignNames = {"Start (Left)", "Center", "End (Right)"};

constexpr std::string_view StretchModeEnumName(guikit::StretchMode m) noexcept {
    switch (m) {
        case guikit::StretchMode::Compact: return "Compact";
        case guikit::StretchMode::Space:   return "Space";
        default:                           return "Fill";
    }
}

constexpr std::string_view ElementLayoutEnumName(guikit::ElementLayout l) noexcept {
    switch (l) {
        case guikit::ElementLayout::Stacked: return "Stacked";
        case guikit::ElementLayout::Top:     return "Top";
        case guikit::ElementLayout::Bottom:  return "Bottom";
        case guikit::ElementLayout::Inline:  return "Inline";
    }
    return "Stacked";
}

constexpr std::string_view InlineOrderEnumName(guikit::InlineOrder o) noexcept {
    switch (o) {
        case guikit::InlineOrder::LabelFirst:   return "LabelFirst";
        case guikit::InlineOrder::ControlFirst: return "ControlFirst";
    }
    return "LabelFirst";
}

constexpr std::string_view LabelAlignEnumName(guikit::Align a) noexcept {
    switch (a) {
        case guikit::Align::Start:  return "Start";
        case guikit::Align::Center: return "Center";
        case guikit::Align::End:    return "End";
    }
    return "Start";
}

constexpr std::string_view AlignName(guikit::Align a) noexcept {
    switch (a) {
        case guikit::Align::Start:  return "Start";
        case guikit::Align::Center: return "Center";
        case guikit::Align::End:    return "End";
    }
    return "Center";
}

constexpr std::string_view VariantName(guikit::UiVariant v) noexcept {
    switch (v) {
        case guikit::UiVariant::Primary:   return "Primary";
        case guikit::UiVariant::Secondary: return "Secondary";
        case guikit::UiVariant::Success:   return "Success";
        case guikit::UiVariant::Warning:   return "Warning";
        case guikit::UiVariant::Danger:    return "Danger";
        case guikit::UiVariant::Info:      return "Info";
        case guikit::UiVariant::Ghost:     return "Ghost";
        case guikit::UiVariant::Muted:     return "Muted";
        default:                           return "Default";
    }
}

constexpr std::string_view SizeName(guikit::UiSize s) noexcept {
    switch (s) {
        case guikit::UiSize::Xs: return "Xs";
        case guikit::UiSize::Sm: return "Sm";
        case guikit::UiSize::Md: return "Md";
        case guikit::UiSize::Lg: return "Lg";
        case guikit::UiSize::Xl: return "Xl";
        default:                 return "Md";
    }
}

constexpr std::string_view PlacementName(guikit::Placement p) noexcept {
    switch (p) {
        case guikit::Placement::Leading:  return "Leading";
        case guikit::Placement::Trailing: return "Trailing";
        case guikit::Placement::Top:      return "Top";
        case guikit::Placement::Bottom:   return "Bottom";
        case guikit::Placement::Thumb:    return "Thumb";
    }
    return "Trailing";
}

constexpr std::string_view CursorModeName(guikit::CursorBlinkMode m) noexcept {
    switch (m) {
        case guikit::CursorBlinkMode::Blinking: return "Blinking";
        case guikit::CursorBlinkMode::Solid:    return "Solid";
        case guikit::CursorBlinkMode::Hidden:   return "Hidden";
    }
    return "Blinking";
}

inline std::string FormatDesc(bool show, std::string_view text, guikit::UiVariant v) {
    if (!show) return "\"\"";
    if (v == guikit::UiVariant::Muted) {
        return std::string("\"") + std::string(text) + "\"";
    }
    return std::string("{\"") + std::string(text) + "\", guikit::UiVariant::" + std::string(VariantName(v)) + "}";
}

struct DemoContext {
    bool shellLightMode = false;
    bool previewDarkMode = true;
    bool previewLightMode = false;
    bool previewBounds = false;
    bool previewInspect = false;

    guikit::UiTheme shellTheme = guikit::UiTheme::Dark(guikit::Density::Standard);
    guikit::UiTheme previewTheme = guikit::UiTheme::Dark(guikit::Density::Standard);

    // Shared Element tokens (synchronized across all 7 ELEMENTS screens)
    guikit::UiSize controlsSize = guikit::UiSize::Md;
    guikit::UiVariant btnVariant = guikit::UiVariant::Primary;
    std::string elemLabel = "Element Label";
    std::string elemDesc = "Secondary description text";
    guikit::UiVariant elemDescVariant = guikit::UiVariant::Muted;
    bool elemWordWrap = true;
    bool elemShowIcon = false;
    std::string elemBadge = "";
    guikit::UiVariant elemBadgeVariant = guikit::UiVariant::Info;
    guikit::ElementLayout elemLayout = guikit::ElementLayout::Stacked;
    guikit::InlineOrder elemInlineOrder = guikit::InlineOrder::LabelFirst;
    guikit::Align elemAlign = guikit::Align::Start;
    size_t elemStretchIdx = 2; // 0 = Compact, 1 = Space, 2 = Fill
    float elemWidth = 0.0f;    // 0 = Auto (px in Compact/Space)
    float elemFillPct = 50.0f; // % in Fill mode
    bool elemDivider = false;
    guikit::ElementDivider elemDividerStyle = guikit::ElementDivider::Bottom;
    bool elemDisabled = false;

    // Shared assembly & container layout tokens
    float cardRadius = 8.0f;
    float cardPadding = 16.0f;
    float cardGap = 8.0f;
    float cardBorderWidth = 1.0f;
    float gridColGap = 12.0f;
    float gridRowGap = 10.0f;
    float colGap = 12.0f;
    float rowGap = 8.0f;
    guikit::UiSize headingSize = guikit::UiSize::Md;
    bool headingWordWrap = true;

    bool HasElementExternal() const noexcept {
        return !elemLabel.empty() || !elemDesc.empty() || elemShowIcon || !elemBadge.empty();
    }

    guikit::UiText GetElementDescription() const noexcept {
        if (elemDesc.empty()) return {};
        return guikit::UiText{
            elemDesc,
            elemDescVariant,
            elemWordWrap ? guikit::TextWrap::Word : guikit::TextWrap::SingleLine
        };
    }

    guikit::Icon GetElementIcon() const noexcept {
        return elemShowIcon ? guikit::Icon::Edit() : guikit::Icon{};
    }

    guikit::UiText GetElementBadge() const noexcept {
        if (elemBadge.empty()) return {};
        return guikit::UiText{elemBadge, elemBadgeVariant};
    }

    guikit::StretchMode GetElementStretch() const noexcept {
        return static_cast<guikit::StretchMode>(elemStretchIdx);
    }

    std::optional<float> GetElementWidth() const noexcept {
        if (elemStretchIdx == 2) return elemFillPct;
        return (elemWidth > 0.0f) ? std::optional<float>(elemWidth) : std::nullopt;
    }

    std::string FormatElementDescCode() const {
        return FormatDesc(!elemDesc.empty(), elemDesc, elemDescVariant);
    }

    std::string FormatElementIconCode() const {
        return elemShowIcon ? "guikit::Icon::Edit()" : "{}";
    }

    std::string FormatElementBadgeCode() const {
        if (elemBadge.empty()) return "\"\"";
        return std::string("{\"") + elemBadge + "\", guikit::UiVariant::" + std::string(VariantName(elemBadgeVariant)) + "}";
    }

    std::string_view FormatDividerStyleCode() const noexcept {
        return (elemDividerStyle == guikit::ElementDivider::Trailing) ? "Trailing" : "Bottom";
    }

    std::string FormatElementWidthCode() const {
        if (elemStretchIdx == 2) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.1ff", elemFillPct);
            return buf;
        }
        if (elemWidth > 0.0f) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.1ff", elemWidth);
            return buf;
        }
        return "std::nullopt";
    }

    guikit::CardOptions MakeCard() const noexcept {
        return guikit::CardOptions{
            .gap = cardGap,
            .padding = guikit::Padding(cardPadding),
            .borderRadius = cardRadius
        };
    }

    guikit::GridOptions MakeGrid() const noexcept {
        return guikit::GridOptions{
            .gap = gridColGap,
            .runGap = gridRowGap
        };
    }

    void RenderVariantPicker(guikit::Context& ui) {
        if (ui.StepSlider(btnVariant, {
            .label = "Semantic Variant",
            .showTicks = true,
            .showLabels = true,
            .showValue = true
        }).changed) {
            ui.InvalidateAll();
        }
    }

    void RenderDescVariantPicker(guikit::Context& ui, guikit::UiVariant& descVar, bool enabled) {
        static constexpr std::array<std::string_view, 7> descVarShort = {"Mut", "Def", "Pri", "Suc", "Wrn", "Dgr", "Inf"};
        static constexpr std::array<std::string_view, 7> descVarFull = {"Muted", "Default", "Primary", "Success", "Warning", "Danger", "Info"};
        static constexpr std::array<guikit::UiVariant, 7> descVars = {
            guikit::UiVariant::Muted, guikit::UiVariant::Default, guikit::UiVariant::Primary,
            guikit::UiVariant::Success, guikit::UiVariant::Warning, guikit::UiVariant::Danger, guikit::UiVariant::Info
        };
        size_t descVarIdx = 0;
        for (size_t i = 0; i < descVars.size(); ++i) {
            if (descVars[i] == descVar) {
                descVarIdx = i;
                break;
            }
        }
        if (ui.StepSlider(descVarIdx, descVarShort, {
            .label = "Description Variant",
            .showTicks = true,
            .showLabels = true,
            .showValue = true,
            .valueText = {VariantName(descVar), (descVar == guikit::UiVariant::Muted) ? guikit::UiVariant::Secondary : descVar, descVarFull},
            .variant = (descVar == guikit::UiVariant::Muted) ? guikit::UiVariant::Secondary : descVar,
            .disabled = !enabled
        }).changed) {
            descVar = descVars[descVarIdx];
            ui.InvalidateAll();
        }
    }

    void RenderElementLayoutControls(
        guikit::Context& ui,
        bool hasExternal,
        guikit::ElementLayout& layout,
        guikit::InlineOrder& inlineOrder,
        guikit::Align& elementAlign,
        size_t& stretchIdx
    ) {
        size_t layoutIdx = static_cast<size_t>(layout);
        if (ui.StepSlider(layoutIdx, kElementLayoutNames, {.label = "Element Layout", .showTicks = true, .showLabels = true, .disabled = !hasExternal}).changed) {
            layout = static_cast<guikit::ElementLayout>(layoutIdx);
            ui.InvalidateAll();
        }

        size_t orderIdx = static_cast<size_t>(inlineOrder);
        if (ui.StepSlider(orderIdx, kInlineOrderNames, {.label = "Inline Order", .showTicks = true, .showLabels = true, .disabled = (!hasExternal || layout != guikit::ElementLayout::Inline)}).changed) {
            inlineOrder = static_cast<guikit::InlineOrder>(orderIdx);
            ui.InvalidateAll();
        }

        size_t alignIdx = static_cast<size_t>(elementAlign);
        if (ui.StepSlider(alignIdx, kLabelAlignNames, {.label = "Element Alignment", .showTicks = true, .showLabels = true, .disabled = (!hasExternal && stretchIdx == 0)}).changed) {
            elementAlign = static_cast<guikit::Align>(alignIdx);
            ui.InvalidateAll();
        }

        if (ui.StepSlider(stretchIdx, kStretchModeNames, {.label = "Stretch Mode", .showTicks = true, .showLabels = true}).changed) {
            ui.InvalidateAll();
        }
    }

    void RenderElementInspector(guikit::Context& ui) {
        ui.Heading({
            .title = "ELEMENT",
            .size = guikit::UiSize::Xs,
            .divider = true
        });
        if (ui.StepSlider(controlsSize, {.label = "Element Size", .showTicks = true, .showLabels = true, .showValue = true}).changed) {
            ui.InvalidateAll();
        }

        RenderVariantPicker(ui);

        if (ui.InputField(elemLabel, {
            .label = "Label Text",
            .placeholder = "No label...",
            .clearable = true
        }).changed) {
            ui.InvalidateAll();
        }

        if (ui.InputField(elemDesc, {
            .label = "Description Text",
            .placeholder = "No description...",
            .clearable = true
        }).changed) {
            ui.InvalidateAll();
        }

        RenderDescVariantPicker(ui, elemDescVariant, !elemDesc.empty());
        if (ui.Switch(elemWordWrap, {.label = "Description Word Wrap", .disabled = elemDesc.empty()}).changed) {
            ui.InvalidateAll();
        }

        RenderElementLayoutControls(ui, HasElementExternal(), elemLayout, elemInlineOrder, elemAlign, elemStretchIdx);
        if (elemStretchIdx == 2) {
            if (ui.Slider(elemFillPct, 10.0f, 100.0f, {.label = "Explicit Width", .valueFormat = "%.0f %%", .step = 5.0f}).changed) ui.InvalidateAll();
        } else {
            if (ui.Slider(elemWidth, 0.0f, 400.0f, {.label = "Explicit Width", .valueFormat = {(elemWidth > 0.0f) ? "%.0f px" : "Auto", {"Auto", "%.0f px"}}, .step = 10.0f}).changed) ui.InvalidateAll();
        }

        if (ui.Switch(elemShowIcon, {.label = "Show Leading Icon"}).changed) ui.InvalidateAll();

        if (ui.InputField(elemBadge, {
            .label = "Badge Text",
            .placeholder = "No badge (e.g. v2.4)...",
            .clearable = true
        }).changed) {
            ui.InvalidateAll();
        }
        if (ui.StepSlider(elemBadgeVariant, {
            .label = "Badge Variant",
            .showTicks = true,
            .showLabels = true,
            .showValue = true,
            .disabled = elemBadge.empty()
        }).changed) {
            ui.InvalidateAll();
        }

        if (ui.Switch(elemDivider, {.label = "Show Divider Line"}).changed) ui.InvalidateAll();
        static constexpr std::array<std::string_view, 2> divStyles = {"Bottom", "Trailing"};
        size_t divStyleIdx = (elemDividerStyle == guikit::ElementDivider::Trailing) ? 1 : 0;
        if (ui.StepSlider(divStyleIdx, divStyles, {
            .label = "Divider Placement",
            .showTicks = true,
            .showLabels = true,
            .disabled = !elemDivider
        }).changed) {
            elemDividerStyle = (divStyleIdx == 1) ? guikit::ElementDivider::Trailing : guikit::ElementDivider::Bottom;
            ui.InvalidateAll();
        }

        if (ui.Switch(elemDisabled, {.label = "Disabled State"}).changed) ui.InvalidateAll();
    }

    void RenderAssemblyCommonInspector(guikit::Context& ui) {
        if (ui.Switch(previewDarkMode, {
            .label = "Preview Dark Mode",
            .description = "Switch preview between Dark and Light palette"
        }).changed) {
            previewLightMode = !previewDarkMode;
            const auto defTheme = previewDarkMode ? guikit::UiTheme::Dark(previewTheme.density) : guikit::UiTheme::Light(previewTheme.density);
            previewTheme.palette = defTheme.palette;
            previewTheme.variants = defTheme.variants;
            previewTheme.syntax = defTheme.syntax;
            previewTheme.mode = previewDarkMode ? guikit::ThemeMode::Dark : guikit::ThemeMode::Light;
            ui.InvalidateAll();
        }

        static constexpr std::array<std::string_view, 3> asmDensityLabels = {"Compact", "Standard", "Touch"};
        size_t dIdx = (previewTheme.density == guikit::Density::Compact) ? 0 : ((previewTheme.density == guikit::Density::Touch) ? 2 : 1);
        if (ui.StepSlider(dIdx, asmDensityLabels, {.label = "Layout Density", .showTicks = true, .showLabels = true}).changed) {
            previewTheme.density = (dIdx == 0) ? guikit::Density::Compact : ((dIdx == 2) ? guikit::Density::Touch : guikit::Density::Standard);
            previewTheme.SetDensity(previewTheme.density);
            ui.InvalidateAll();
        }

        if (ui.Slider(gridColGap, 0.0f, 32.0f, {.label = "Horizontal Gap (Columns)", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
        if (ui.Slider(gridRowGap, 0.0f, 32.0f, {.label = "Vertical Gap (Rows)", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();

        if (ui.Slider(cardRadius, 0.0f, 24.0f, {.label = "Card Corner Radius", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
        if (ui.Slider(cardPadding, 4.0f, 32.0f, {.label = "Card Padding", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
        if (ui.Slider(cardBorderWidth, 0.0f, 4.0f, {.label = "Card Border Width", .valueFormat = "%.1f px"}).changed) ui.InvalidateAll();
    }
};

// ============================================================================
// 1. ELEMENTS (Atomic Controls)
// ============================================================================
void RenderHeadingInspector(guikit::Context& ui, DemoContext& ctx);
void RenderHeadingStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateHeadingCode(const DemoContext& ctx);

void RenderButtonInspector(guikit::Context& ui, DemoContext& ctx);
void RenderButtonStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateButtonCode(const DemoContext& ctx);

void RenderSliderInspector(guikit::Context& ui, DemoContext& ctx);
void RenderSliderStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateSliderCode(const DemoContext& ctx);

void RenderInputInspector(guikit::Context& ui, DemoContext& ctx);
void RenderInputStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateInputCode(const DemoContext& ctx);

void RenderSelectInspector(guikit::Context& ui, DemoContext& ctx);
void RenderSelectStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateSelectCode(const DemoContext& ctx);

void RenderSwitchInspector(guikit::Context& ui, DemoContext& ctx);
void RenderSwitchStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateSwitchCode(const DemoContext& ctx);

void RenderCheckboxInspector(guikit::Context& ui, DemoContext& ctx);
void RenderCheckboxStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateCheckboxCode(const DemoContext& ctx);

// ============================================================================
// 2. PANELS & NAV (Containers & Navigation)
// ============================================================================
void RenderCardInspector(guikit::Context& ui, DemoContext& ctx);
void RenderCardStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateCardCode(const DemoContext& ctx);

void RenderTabBarInspector(guikit::Context& ui, DemoContext& ctx);
void RenderTabBarStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateTabBarCode(const DemoContext& ctx);

void RenderSidebarInspector(guikit::Context& ui, DemoContext& ctx);
void RenderSidebarStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateSidebarCode(const DemoContext& ctx);

// ============================================================================
// 3. CONTENT & MEDIA (Documents, Editors, Graphics)
// ============================================================================
void RenderImageInspector(guikit::Context& ui, DemoContext& ctx);
void RenderImageStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateImageCode(const DemoContext& ctx);

void RenderMarkdownInspector(guikit::Context& ui, DemoContext& ctx);
void RenderMarkdownStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateMarkdownCode(const DemoContext& ctx);

void RenderCodeEditorInspector(guikit::Context& ui, DemoContext& ctx);
void RenderCodeEditorStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateCodeEditorCode(const DemoContext& ctx);

// ============================================================================
// 4. 2D SCENES (Interactive Vector Viewports & GIS)
// ============================================================================
void RenderSchematicInspector(guikit::Context& ui, DemoContext& ctx);
void RenderSchematicStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateSchematicCode(const DemoContext& ctx);

void RenderOsmMapInspector(guikit::Context& ui, DemoContext& ctx);
void RenderOsmMapStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateOsmMapCode(const DemoContext& ctx);

// ============================================================================
// 5. ASSEMBLY (Compositions & Real-World Screens)
// ============================================================================
void RenderDashboardInspector(guikit::Context& ui, DemoContext& ctx);
void RenderDashboardScreen(guikit::Context& ui, DemoContext& ctx);

void RenderFormControlsInspector(guikit::Context& ui, DemoContext& ctx);
void RenderFormControlsScreen(guikit::Context& ui, DemoContext& ctx);

void RenderFlexRibbonInspector(guikit::Context& ui, DemoContext& ctx);
void RenderFlexRibbonScreen(guikit::Context& ui, DemoContext& ctx);

void RenderDiagnosticsInspector(guikit::Context& ui, DemoContext& ctx);
void RenderDiagnosticsScreen(guikit::Context& ui, DemoContext& ctx);

void RenderThemeStudioInspector(guikit::Context& ui, DemoContext& ctx);
void RenderThemeStudioScreen(guikit::Context& ui, DemoContext& ctx);

} // namespace guikit::demo
