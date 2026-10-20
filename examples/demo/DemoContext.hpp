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
    ElemKnob,
    ElemSpinBox,
    ElemInputField,
    ElemSelect,
    ElemSegmented,
    ElemColorPicker,
    ElemDatePicker,
    ElemSwitch,
    ElemCheckbox,
    ElemRadio,
    ElemImage,

    // 2. PANELS & NAV (Containers & Navigation)
    PanelCard,
    PanelSplitView,
    PanelTabBar,
    PanelNavigation,
    PanelToolbar,
    PanelSidebar,
    PanelTreeView,
    PanelTable,
    PanelOverlays,

    // 3. CONTENT & MEDIA (Documents, Editors, Graphics)
    ContentMarkdown,
    ContentCodeEditor,

    // 4. 2D SCENES (Interactive Vector Viewports & GIS)
    SceneSchematic2D,
    SceneOsmMap,
    SceneSignalView,
    SceneSonarView,
    SceneWaterfallView,
    SceneImageView,
    SceneAttitudeView,
    SceneCarDashboard,
    SceneBpmnEditor,

    // 5. ASSEMBLY (Compositions & Real-World Screens)
    AsmDashboard,
    AsmFormControls,
    AsmFlexRibbon,
    AsmDiagnostics,
    AsmThemeStudio
};

constexpr bool IsElementCategory(NavCategory cat) noexcept {
    return cat <= NavCategory::ElemImage;
}

constexpr bool IsSandboxCategory(NavCategory cat) noexcept {
    return cat <= NavCategory::SceneBpmnEditor;
}

constexpr bool IsFullStageCategory(NavCategory cat) noexcept {
    return cat == NavCategory::PanelSplitView ||
           cat == NavCategory::PanelNavigation ||
           cat == NavCategory::PanelToolbar ||
           cat == NavCategory::PanelSidebar ||
           cat == NavCategory::PanelTreeView ||
           cat == NavCategory::PanelTable ||
           cat == NavCategory::PanelOverlays ||
           cat == NavCategory::ContentMarkdown ||
           cat == NavCategory::ContentCodeEditor ||
           cat == NavCategory::SceneSchematic2D ||
           cat == NavCategory::SceneOsmMap ||
           cat == NavCategory::SceneSignalView ||
           cat == NavCategory::SceneSonarView ||
           cat == NavCategory::SceneWaterfallView ||
           cat == NavCategory::SceneImageView ||
           cat == NavCategory::SceneAttitudeView ||
           cat == NavCategory::SceneCarDashboard ||
           cat == NavCategory::SceneBpmnEditor;
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

inline std::string FormatUiSizePrefix(guikit::UiSize s) {
    if (s == guikit::UiSize::None) {
        return "";
    }
    return "Size(guikit::UiSize::" + std::string(SizeName(s)) + ").";
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
    return std::string("{\"") + std::string(text) + "\", guikit::StateColor::" + std::string(VariantName(v)) + "()}";
}

inline constexpr std::array<std::string_view, 9> kBgPresetShort = {"Off", "Srf", "Pri", "Sec", "Suc", "Wrn", "Dgr", "Inf", "Gho"};

constexpr guikit::StateColor BgPresetToStateColor(size_t idx, bool defaultEnabled) noexcept {
    switch (idx) {
        case 0:  return defaultEnabled ? guikit::StateColor::None() : guikit::StateColor::Default();
        case 1:  return guikit::StateColor::Surface();
        case 2:  return guikit::StateColor::Primary();
        case 3:  return guikit::StateColor::Secondary();
        case 4:  return guikit::StateColor::Success();
        case 5:  return guikit::StateColor::Warning();
        case 6:  return guikit::StateColor::Danger();
        case 7:  return guikit::StateColor::Info();
        case 8:  return guikit::StateColor::Ghost();
        default: return defaultEnabled ? guikit::StateColor::Surface() : guikit::StateColor::Default();
    }
}

constexpr guikit::StateColor BgPresetToTrackColor(size_t idx) noexcept {
    switch (idx) {
        case 2:  return guikit::StateColor::FromTextVariant(guikit::UiVariant::Primary);
        case 3:  return guikit::StateColor::FromTextVariant(guikit::UiVariant::Secondary);
        case 4:  return guikit::StateColor::FromTextVariant(guikit::UiVariant::Success);
        case 5:  return guikit::StateColor::FromTextVariant(guikit::UiVariant::Warning);
        case 6:  return guikit::StateColor::FromTextVariant(guikit::UiVariant::Danger);
        case 7:  return guikit::StateColor::FromTextVariant(guikit::UiVariant::Info);
        case 8:  return guikit::StateColor::FromTextVariant(guikit::UiVariant::Ghost);
        default: return {};
    }
}

inline std::string BgPresetToCode(size_t idx, bool defaultEnabled) {
    switch (idx) {
        case 0:  return defaultEnabled ? "guikit::StateColor::None()" : "guikit::StateColor::Default()";
        case 1:  return "guikit::StateColor::Surface()";
        case 2:  return "guikit::StateColor::Primary()";
        case 3:  return "guikit::StateColor::Secondary()";
        case 4:  return "guikit::StateColor::Success()";
        case 5:  return "guikit::StateColor::Warning()";
        case 6:  return "guikit::StateColor::Danger()";
        case 7:  return "guikit::StateColor::Info()";
        case 8:  return "guikit::StateColor::Ghost()";
        default: return defaultEnabled ? "guikit::StateColor::Surface()" : "guikit::StateColor::Default()";
    }
}

struct DemoContext {
    bool shellLightMode = false;
    bool previewDarkMode = true;
    bool previewLightMode = false;
    bool previewBounds = false;
    bool previewInspect = false;
    bool previewShowCode = true;

    guikit::UiTheme shellTheme = guikit::UiTheme::Dark(guikit::Density::Standard);
    guikit::UiTheme previewTheme = guikit::UiTheme::Dark(guikit::Density::Standard);

    // Shared Element tokens (synchronized across all 8 ELEMENTS screens)
    guikit::UiSize elemSize = guikit::UiSize::None;
    guikit::UiSize controlsSize = guikit::UiSize::None;
    guikit::UiVariant btnVariant = guikit::UiVariant::Primary;
    std::string elemLabel = "Element Label";
    guikit::UiVariant elemLabelVariant = guikit::UiVariant::Default;
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
    float elemFillRatio = 0.5f; // 0.0..1.0 ratio in Fill mode
    float elemHeight = 0.0f;   // 0 = Auto (px)
    size_t elemBgIdx = 0;      // 0 = None (Default), 1 = Surface, 2..8 = Primary..Ghost
    guikit::StateColor elemBackground = guikit::StateColor::Default();
    float elemBorderWidth = 0.0f;
    guikit::UiVariant elemBorderVariant = guikit::UiVariant::Default;
    float elemBorderRadius = 8.0f;
    std::array<size_t, 14> ctrlBgIdxByCat = {0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0};
    float ctrlBorderWidth = 0.0f;
    guikit::UiVariant ctrlBorderVariant = guikit::UiVariant::Default;
    float ctrlBorderRadius = 6.0f;
    bool elemDivider = true;
    guikit::HeadingDivider elemDividerStyle = guikit::HeadingDivider::Bottom;
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

    guikit::UiText GetElementLabel() const noexcept {
        return guikit::UiText{elemLabel, elemLabelVariant};
    }

    guikit::UiText GetElementDescription() const noexcept {
        if (elemDesc.empty()) return {};
        return guikit::UiText{
            elemDesc,
            elemDescVariant,
            elemWordWrap ? guikit::TextWrap::Word() : guikit::TextWrap::SingleLine()
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
        if (elemStretchIdx == 2) return elemFillRatio;
        return (elemWidth > 0.0f) ? std::optional<float>(elemWidth) : std::nullopt;
    }

    std::optional<float> GetElementHeight() const noexcept {
        return (elemHeight > 0.0f) ? std::optional<float>(elemHeight) : std::nullopt;
    }

    static constexpr bool IsCtrlBgDefaultEnabled(NavCategory cat) noexcept {
        return cat == NavCategory::ElemButton ||
               cat == NavCategory::ElemSpinBox ||
               cat == NavCategory::ElemInputField ||
               cat == NavCategory::ElemSelect ||
               cat == NavCategory::ElemSegmented ||
               cat == NavCategory::ElemColorPicker ||
               cat == NavCategory::ElemDatePicker;
    }

    guikit::StateColor GetControlBackground(NavCategory cat) const noexcept {
        const size_t idx = static_cast<size_t>(cat);
        const size_t bgIdx = (idx < ctrlBgIdxByCat.size()) ? ctrlBgIdxByCat[idx] : 0;
        return BgPresetToStateColor(bgIdx, IsCtrlBgDefaultEnabled(cat));
    }

    std::string FormatElemBackgroundCode() const {
        return BgPresetToCode(elemBgIdx, false);
    }

    std::string FormatControlBackgroundCode(NavCategory cat) const {
        const size_t idx = static_cast<size_t>(cat);
        const size_t bgIdx = (idx < ctrlBgIdxByCat.size()) ? ctrlBgIdxByCat[idx] : 0;
        return BgPresetToCode(bgIdx, IsCtrlBgDefaultEnabled(cat));
    }

    std::optional<float> GetOptionalControlBorderWidth() const noexcept {
        return (ctrlBorderWidth > 0.0f) ? std::optional<float>(ctrlBorderWidth) : std::nullopt;
    }

    std::optional<float> GetElemBorderRadius() const noexcept {
        return (elemBackground.IsBackgroundEnabled(false) || elemBorderWidth > 0.0f) ? std::optional<float>(elemBorderRadius) : std::nullopt;
    }

    guikit::StateColor GetElemBorderColor() const noexcept {
        return guikit::StateColor::FromVariant(elemBorderVariant);
    }

    guikit::StateColor GetCtrlBorderColor() const noexcept {
        return guikit::StateColor::FromVariant(ctrlBorderVariant);
    }

    guikit::Border GetElemBorder() const noexcept {
        return guikit::Border{
            .width = elemBorderWidth,
            .radius = GetElemBorderRadius(),
            .color = GetElemBorderColor()
        };
    }

    guikit::Border GetCtrlBorder() const noexcept {
        return guikit::Border{
            .width = ctrlBorderWidth,
            .radius = ctrlBorderRadius,
            .color = GetCtrlBorderColor()
        };
    }

    guikit::ControlSpec GetControlSpec(NavCategory cat, guikit::Align align = guikit::Align::Center) const noexcept {
        return guikit::ControlSpec{
            .width = GetElementWidth(),
            .height = GetElementHeight(),
            .align = align,
            .background = GetControlBackground(cat),
            .border = GetCtrlBorder()
        };
    }

    std::string FormatElementLabelCode() const {
        if (elemLabel.empty()) return "\"\"";
        if (elemLabelVariant == guikit::UiVariant::Default) {
            return std::string("\"") + elemLabel + "\"";
        }
        return std::string("{\"") + elemLabel + "\", guikit::StateColor::" + std::string(VariantName(elemLabelVariant)) + "()}";
    }

    std::string FormatElementDescCode() const {
        return FormatDesc(!elemDesc.empty(), elemDesc, elemDescVariant);
    }

    std::string FormatElementIconCode() const {
        return elemShowIcon ? "guikit::Icon::Edit()" : "{}";
    }

    std::string FormatElementBadgeCode() const {
        if (elemBadge.empty()) return "\"\"";
        return std::string("{\"") + elemBadge + "\", guikit::StateColor::" + std::string(VariantName(elemBadgeVariant)) + "()}";
    }

    std::string_view FormatDividerStyleCode() const noexcept {
        return (elemDividerStyle == guikit::HeadingDivider::Trailing) ? "Trailing" : "Bottom";
    }

    std::string FormatElementWidthCode() const {
        if (elemStretchIdx == 2) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.2ff", elemFillRatio);
            return buf;
        }
        if (elemWidth > 0.0f) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.1ff", elemWidth);
            return buf;
        }
        return "std::nullopt";
    }

    std::string FormatElementHeightCode() const {
        if (elemHeight > 0.0f) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.1ff", elemHeight);
            return buf;
        }
        return "std::nullopt";
    }

    std::string FormatElemBorderRadiusCode() const {
        if (elemBackground.IsBackgroundEnabled(false) || elemBorderWidth > 0.0f) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.1ff", elemBorderRadius);
            return buf;
        }
        return "std::nullopt";
    }

    guikit::StateColor GetControlColor() const noexcept {
        return guikit::StateColor::FromVariant(btnVariant);
    }

    std::string FormatControlColorCode() const {
        return std::string("guikit::StateColor::") + std::string(VariantName(btnVariant)) + "()";
    }

    std::string FormatElemBorderColorCode() const {
        return std::string("guikit::StateColor::") + std::string(VariantName(elemBorderVariant)) + "()";
    }

    std::string FormatCtrlBorderColorCode() const {
        return std::string("guikit::StateColor::") + std::string(VariantName(ctrlBorderVariant)) + "()";
    }

    std::string FormatElemBorderCode() const {
        char bwBuf[32];
        std::snprintf(bwBuf, sizeof(bwBuf), "%.1ff", elemBorderWidth);
        return "{.width = " + std::string(bwBuf) +
               ", .radius = " + FormatElemBorderRadiusCode() +
               ", .color = " + FormatElemBorderColorCode() + "}";
    }

    std::string FormatCtrlBorderCode() const {
        char bwBuf[32];
        char brBuf[32];
        std::snprintf(bwBuf, sizeof(bwBuf), "%.1ff", ctrlBorderWidth);
        std::snprintf(brBuf, sizeof(brBuf), "%.1ff", ctrlBorderRadius);
        return "{.width = " + std::string(bwBuf) +
               ", .radius = " + std::string(brBuf) +
               ", .color = " + FormatCtrlBorderColorCode() + "}";
    }

    std::string FormatControlSpecCode(NavCategory cat, std::optional<guikit::Align> alignOpt = std::nullopt) const {
        std::string res = "{\n";
        res += "        .width = " + FormatElementWidthCode() + ",\n";
        res += "        .height = " + FormatElementHeightCode() + ",\n";
        if (alignOpt.has_value()) {
            res += "        .align = guikit::Align::" + std::string(AlignName(*alignOpt)) + ",\n";
        }
        res += "        .background = " + FormatControlBackgroundCode(cat) + ",\n";
        res += "        .border = " + FormatCtrlBorderCode() + "\n";
        res += "    }";
        return res;
    }

    guikit::CardOptions MakeCard() const noexcept {
        return guikit::CardOptions{
            .gap = cardGap,
            .padding = guikit::Padding(cardPadding),
            .border = {.width = cardBorderWidth, .radius = cardRadius}
        };
    }

    guikit::GridOptions MakeGrid() const noexcept {
        return guikit::GridOptions{
            .gap = gridColGap,
            .runGap = gridRowGap
        };
    }

    void RenderVariantPicker(guikit::Context& ui) {
        if (auto el = ui.Element({.label = "Semantic Variant"})) {
            if (ui.StepSlider(btnVariant, {
                .showTicks = true,
                .showLabels = true
            }).changed) {
                ui.InvalidateAll();
            }
        }
    }

    void RenderDescVariantPicker(guikit::Context& ui, guikit::UiVariant& descVar, bool enabled) {
        static constexpr std::array<std::string_view, 7> descVarShort = {"Mut", "Def", "Pri", "Suc", "Wrn", "Dgr", "Inf"};
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
        if (auto el = ui.Element({.label = "Description Color", .disabled = !enabled})) {
            if (ui.StepSlider(descVarIdx, descVarShort, {
                .showTicks = true,
                .showLabels = true,
                .color = guikit::StateColor::FromVariant((descVar == guikit::UiVariant::Muted) ? guikit::UiVariant::Secondary : descVar),
                .disabled = !enabled
            }).changed) {
                descVar = descVars[descVarIdx];
                ui.InvalidateAll();
            }
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
        if (auto el = ui.Element({.label = "Element Layout", .disabled = !hasExternal})) {
            if (ui.StepSlider(layoutIdx, kElementLayoutNames, {.showTicks = true, .showLabels = true, .disabled = !hasExternal}).changed) {
                layout = static_cast<guikit::ElementLayout>(layoutIdx);
                ui.InvalidateAll();
            }
        }

        size_t orderIdx = static_cast<size_t>(inlineOrder);
        if (auto el = ui.Element({.label = "Inline Order", .disabled = (!hasExternal || layout != guikit::ElementLayout::Inline)})) {
            if (ui.StepSlider(orderIdx, kInlineOrderNames, {.showTicks = true, .showLabels = true, .disabled = (!hasExternal || layout != guikit::ElementLayout::Inline)}).changed) {
                inlineOrder = static_cast<guikit::InlineOrder>(orderIdx);
                ui.InvalidateAll();
            }
        }

        size_t alignIdx = static_cast<size_t>(elementAlign);
        if (auto el = ui.Element({.label = "Element Alignment", .disabled = (!hasExternal && stretchIdx == 0)})) {
            if (ui.StepSlider(alignIdx, kLabelAlignNames, {.showTicks = true, .showLabels = true, .disabled = (!hasExternal && stretchIdx == 0)}).changed) {
                elementAlign = static_cast<guikit::Align>(alignIdx);
                ui.InvalidateAll();
            }
        }

        if (auto el = ui.Element({.label = "Stretch Mode"})) {
            if (ui.StepSlider(stretchIdx, kStretchModeNames, {.showTicks = true, .showLabels = true}).changed) {
                ui.InvalidateAll();
            }
        }
    }

    void RenderElementInspector(guikit::Context& ui, NavCategory cat = NavCategory::ElemHeading) {
        if (auto el = ui.Element({.label = "Element Size"})) {
            if (ui.StepSlider(elemSize, {.showTicks = true, .showLabels = true, .includeNone = true}).changed) {
                ui.InvalidateAll();
            }
        }

        if (auto el = ui.Element({.label = "Label Text"})) {
            if (ui.InputField(elemLabel, {
                .placeholder = "No label...",
                .clearable = true
            }).changed) {
                ui.InvalidateAll();
            }
        }
        const bool labelDisabled = (elemLabel.empty() && !elemShowIcon);
        if (auto el = ui.Element({.label = "Label Color", .disabled = labelDisabled})) {
            if (ui.StepSlider(elemLabelVariant, {
                .showTicks = true,
                .showLabels = true,
                .disabled = labelDisabled
            }).changed) {
                ui.InvalidateAll();
            }
        }

        if (auto el = ui.Element({.label = "Description Text"})) {
            if (ui.InputField(elemDesc, {
                .placeholder = "No description...",
                .clearable = true
            }).changed) {
                ui.InvalidateAll();
            }
        }
        RenderDescVariantPicker(ui, elemDescVariant, !elemDesc.empty());

        RenderElementLayoutControls(ui, HasElementExternal(), elemLayout, elemInlineOrder, elemAlign, elemStretchIdx);

        if (auto el = ui.Element({.label = "Show Leading Icon"})) {
            if (ui.Switch(elemShowIcon).changed) ui.InvalidateAll();
        }

        if (auto el = ui.Element({.label = "Badge Text"})) {
            if (ui.InputField(elemBadge, {
                .placeholder = "No badge (e.g. v2.4)...",
                .clearable = true
            }).changed) {
                ui.InvalidateAll();
            }
        }
        if (auto el = ui.Element({.label = "Badge Color", .disabled = elemBadge.empty()})) {
            if (ui.StepSlider(elemBadgeVariant, {
                .showTicks = true,
                .showLabels = true,
                .disabled = elemBadge.empty()
            }).changed) {
                ui.InvalidateAll();
            }
        }

        if (auto el = ui.Element({.label = "Element Background"})) {
            if (ui.StepSlider(elemBgIdx, kBgPresetShort, {
                .showTicks = true,
                .showLabels = true,
                .color = BgPresetToTrackColor(elemBgIdx)
            }).changed) {
                elemBackground = BgPresetToStateColor(elemBgIdx, false);
                ui.InvalidateAll();
            }
        }
        const bool hasElemFrame = elemBackground.IsBackgroundEnabled(false) || elemBorderWidth > 0.0f;
        if (auto el = ui.Element({.label = "Element Border Width"})) {
            if (ui.Slider(elemBorderWidth, 0.0f, 4.0f, {
                .valueFormat = "%.1f px",
                .step = 0.5f
            }).changed) ui.InvalidateAll();
        }
        const bool hasElemBorderOrDivider = (elemBorderWidth > 0.0f) || (cat == NavCategory::ElemHeading && elemDivider);
        if (auto el = ui.Element({.label = "Element Border Color", .disabled = !hasElemBorderOrDivider})) {
            if (ui.StepSlider(elemBorderVariant, {
                .showTicks = true,
                .showLabels = true,
                .disabled = !hasElemBorderOrDivider
            }).changed) {
                ui.InvalidateAll();
            }
        }
        if (auto el = ui.Element({.label = "Element Border Radius", .disabled = !hasElemFrame})) {
            if (ui.Slider(elemBorderRadius, 0.0f, 24.0f, {
                .valueFormat = "%.0f px",
                .step = 1.0f,
                .disabled = !hasElemFrame
            }).changed) ui.InvalidateAll();
        }

        if (auto el = ui.Element({.label = "Disabled State"})) {
            if (ui.Switch(elemDisabled).changed) ui.InvalidateAll();
        }
    }

    void RenderControlInspector(guikit::Context& ui, NavCategory cat) {
        if (auto el = ui.Element({.label = "Control Size"})) {
            if (ui.StepSlider(controlsSize, {.showTicks = true, .showLabels = true, .includeNone = true}).changed) {
                ui.InvalidateAll();
            }
        }
        const bool hasControlVariant = (
            cat == NavCategory::ElemButton ||
            cat == NavCategory::ElemSlider ||
            cat == NavCategory::ElemKnob ||
            cat == NavCategory::ElemSpinBox ||
            cat == NavCategory::ElemSegmented ||
            cat == NavCategory::ElemDatePicker ||
            cat == NavCategory::ElemSwitch ||
            cat == NavCategory::ElemCheckbox ||
            cat == NavCategory::ElemRadio
        );
        if (hasControlVariant) {
            if (auto el = ui.Element({.label = "Control Color"})) {
                if (ui.StepSlider(btnVariant, {
                    .showTicks = true,
                    .showLabels = true
                }).changed) {
                    ui.InvalidateAll();
                }
            }
        }
        if (elemStretchIdx == 2) {
            if (auto el = ui.Element({.label = "Explicit Width"})) {
                if (ui.Slider(elemFillRatio, 0.10f, 1.00f, {.valueFormat = "%.2f", .step = 0.05f}).changed) ui.InvalidateAll();
            }
        } else {
            if (auto el = ui.Element({.label = "Explicit Width"})) {
                if (ui.Slider(elemWidth, 0.0f, 400.0f, {.valueFormat = {(elemWidth > 0.0f) ? "%.0f px" : "Auto", {"Auto", "%.0f px"}}, .step = 10.0f}).changed) ui.InvalidateAll();
            }
        }
        if (auto el = ui.Element({.label = "Explicit Height"})) {
            if (ui.Slider(elemHeight, 0.0f, 320.0f, {.valueFormat = {(elemHeight > 0.0f) ? "%.0f px" : "Auto", {"Auto", "%.0f px"}}, .step = 10.0f}).changed) ui.InvalidateAll();
        }

        size_t& ctrlBgIdx = ctrlBgIdxByCat[static_cast<size_t>(cat)];
        if (auto el = ui.Element({.label = "Control Background"})) {
            if (ui.StepSlider(ctrlBgIdx, kBgPresetShort, {
                .showTicks = true,
                .showLabels = true,
                .color = BgPresetToTrackColor(ctrlBgIdx)
            }).changed) ui.InvalidateAll();
        }

        const bool hasIntrinsicBorder = (
            cat == NavCategory::ElemButton ||
            cat == NavCategory::ElemSpinBox ||
            cat == NavCategory::ElemInputField ||
            cat == NavCategory::ElemSelect ||
            cat == NavCategory::ElemSegmented ||
            cat == NavCategory::ElemColorPicker ||
            cat == NavCategory::ElemDatePicker
        );
        if (auto el = ui.Element({.label = "Control Border Width"})) {
            if (ui.Slider(ctrlBorderWidth, 0.0f, 4.0f, {
                .valueFormat = (hasIntrinsicBorder && ctrlBorderWidth <= 0.0f)
                    ? guikit::UiText{"Default", {"Default", "%.1f px"}}
                    : guikit::UiText{"%.1f px", {"Default", "%.1f px"}},
                .step = 0.5f
            }).changed) ui.InvalidateAll();
        }

        const bool hasVisibleCtrlBorder =
            (ctrlBorderWidth > 0.0f) ||
            (cat == NavCategory::ElemSpinBox) ||
            (cat == NavCategory::ElemInputField) ||
            (cat == NavCategory::ElemSelect) ||
            (cat == NavCategory::ElemSegmented) ||
            (cat == NavCategory::ElemColorPicker) ||
            (cat == NavCategory::ElemDatePicker) ||
            (cat == NavCategory::ElemButton && (btnVariant == guikit::UiVariant::Default || btnVariant == guikit::UiVariant::Secondary));
        if (auto el = ui.Element({.label = "Control Border Color", .disabled = !hasVisibleCtrlBorder})) {
            if (ui.StepSlider(ctrlBorderVariant, {
                .showTicks = true,
                .showLabels = true,
                .disabled = !hasVisibleCtrlBorder
            }).changed) {
                ui.InvalidateAll();
            }
        }

        if (auto el = ui.Element({.label = "Control Border Radius"})) {
            if (ui.Slider(ctrlBorderRadius, 0.0f, 24.0f, {
                .valueFormat = "%.0f px",
                .step = 1.0f
            }).changed) ui.InvalidateAll();
        }
    }

    void RenderAssemblyCommonInspector(guikit::Context& ui) {
        if (auto el = ui.Element({
            .label = "Preview Dark Mode",
            .description = "Switch preview between Dark and Light palette"
        })) {
            if (ui.Switch(previewDarkMode).changed) {
                previewLightMode = !previewDarkMode;
                const auto defTheme = previewDarkMode ? guikit::UiTheme::Dark(previewTheme.density) : guikit::UiTheme::Light(previewTheme.density);
                previewTheme.palette = defTheme.palette;
                previewTheme.variants = defTheme.variants;
                previewTheme.syntax = defTheme.syntax;
                previewTheme.mode = previewDarkMode ? guikit::ThemeMode::Dark : guikit::ThemeMode::Light;
                ui.InvalidateAll();
            }
        }

        static constexpr std::array<std::string_view, 3> asmDensityLabels = {"Compact", "Standard", "Touch"};
        size_t dIdx = (previewTheme.density == guikit::Density::Compact) ? 0 : ((previewTheme.density == guikit::Density::Touch) ? 2 : 1);
        if (auto el = ui.Element({.label = "Layout Density"})) {
            if (ui.StepSlider(dIdx, asmDensityLabels, {.showTicks = true, .showLabels = true}).changed) {
                previewTheme.density = (dIdx == 0) ? guikit::Density::Compact : ((dIdx == 2) ? guikit::Density::Touch : guikit::Density::Standard);
                previewTheme.SetDensity(previewTheme.density);
                ui.InvalidateAll();
            }
        }

        if (auto el = ui.Element({.label = "Horizontal Gap (Columns)"})) {
            if (ui.Slider(gridColGap, 0.0f, 32.0f, {.valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
        }
        if (auto el = ui.Element({.label = "Vertical Gap (Rows)"})) {
            if (ui.Slider(gridRowGap, 0.0f, 32.0f, {.valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
        }

        if (auto el = ui.Element({.label = "Card Corner Radius"})) {
            if (ui.Slider(cardRadius, 0.0f, 24.0f, {.valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
        }
        if (auto el = ui.Element({.label = "Card Padding"})) {
            if (ui.Slider(cardPadding, 4.0f, 32.0f, {.valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
        }
        if (auto el = ui.Element({.label = "Card Border Width"})) {
            if (ui.Slider(cardBorderWidth, 0.0f, 4.0f, {.valueFormat = "%.1f px"}).changed) ui.InvalidateAll();
        }
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

void RenderKnobInspector(guikit::Context& ui, DemoContext& ctx);
void RenderKnobStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateKnobCode(const DemoContext& ctx);

void RenderSpinBoxInspector(guikit::Context& ui, DemoContext& ctx);
void RenderSpinBoxStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateSpinBoxCode(const DemoContext& ctx);

void RenderInputInspector(guikit::Context& ui, DemoContext& ctx);
void RenderInputStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateInputCode(const DemoContext& ctx);

void RenderSelectInspector(guikit::Context& ui, DemoContext& ctx);
void RenderSelectStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateSelectCode(const DemoContext& ctx);

void RenderSegmentedInspector(guikit::Context& ui, DemoContext& ctx);
void RenderSegmentedStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateSegmentedCode(const DemoContext& ctx);

void RenderColorPickerInspector(guikit::Context& ui, DemoContext& ctx);
void RenderColorPickerStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateColorPickerCode(const DemoContext& ctx);

void RenderDatePickerInspector(guikit::Context& ui, DemoContext& ctx);
void RenderDatePickerStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateDatePickerCode(const DemoContext& ctx);

void RenderSwitchInspector(guikit::Context& ui, DemoContext& ctx);
void RenderSwitchStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateSwitchCode(const DemoContext& ctx);

void RenderCheckboxInspector(guikit::Context& ui, DemoContext& ctx);
void RenderCheckboxStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateCheckboxCode(const DemoContext& ctx);

void RenderRadioInspector(guikit::Context& ui, DemoContext& ctx);
void RenderRadioStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateRadioCode(const DemoContext& ctx);

// ============================================================================
// 2. PANELS & NAV (Containers & Navigation)
// ============================================================================
void RenderCardInspector(guikit::Context& ui, DemoContext& ctx);
void RenderCardStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateCardCode(const DemoContext& ctx);

void RenderSplitViewInspector(guikit::Context& ui, DemoContext& ctx);
void RenderSplitViewStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateSplitViewCode(const DemoContext& ctx);

void RenderTabBarInspector(guikit::Context& ui, DemoContext& ctx);
void RenderTabBarStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateTabBarCode(const DemoContext& ctx);

void RenderNavigationInspector(guikit::Context& ui, DemoContext& ctx);
void RenderNavigationStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateNavigationCode(const DemoContext& ctx);

void RenderSidebarInspector(guikit::Context& ui, DemoContext& ctx);
void RenderSidebarStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateSidebarCode(const DemoContext& ctx);

void RenderTreeViewInspector(guikit::Context& ui, DemoContext& ctx);
void RenderTreeViewStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateTreeViewCode(const DemoContext& ctx);

void RenderTableInspector(guikit::Context& ui, DemoContext& ctx);
void RenderTableStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateTableCode(const DemoContext& ctx);

void RenderOverlaysInspector(guikit::Context& ui, DemoContext& ctx);
void RenderOverlaysStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateOverlaysCode(const DemoContext& ctx);

void RenderToolbarInspector(guikit::Context& ui, DemoContext& ctx);
void RenderToolbarStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateToolbarCode(const DemoContext& ctx);

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

void RenderSignalInspector(guikit::Context& ui, DemoContext& ctx);
void RenderSignalStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateSignalCode(const DemoContext& ctx);

void RenderSonarInspector(guikit::Context& ui, DemoContext& ctx);
void RenderSonarStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateSonarCode(const DemoContext& ctx);

void RenderWaterfallInspector(guikit::Context& ui, DemoContext& ctx);
void RenderWaterfallStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateWaterfallCode(const DemoContext& ctx);

void RenderImageViewInspector(guikit::Context& ui, DemoContext& ctx);
void RenderImageViewStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateImageViewCode(const DemoContext& ctx);

void RenderAttitudeInspector(guikit::Context& ui, DemoContext& ctx);
void RenderAttitudeStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateAttitudeCode(const DemoContext& ctx);

void RenderCarDashboardInspector(guikit::Context& ui, DemoContext& ctx);
void RenderCarDashboardStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateCarDashboardCode(const DemoContext& ctx);

void RenderBpmnInspector(guikit::Context& ui, DemoContext& ctx);
void RenderBpmnStage(guikit::Context& ui, DemoContext& ctx);
std::string GenerateBpmnCode(const DemoContext& ctx);

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
