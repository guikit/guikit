#pragma once

#include "guikit/guikit.hpp"
#include "keel/keel.h"

namespace guikit::demo {

enum class NavCategory : keel::u8 {
    // 1. ELEMENTS (Atomic Controls)
    ElemHeading = 0,
    ElemElement,
    ElemButton,
    ElemSlider,
    ElemLevelMeter,
    ElemKnob,
    ElemSpinBox,
    ElemInputField,
    ElemTagInput,
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
    PanelBreadcrumb,
    PanelPagination,
    PanelToolbar,
    PanelSidebar,
    PanelTreeView,
    PanelTable,
    PanelOverlays,

    // 3. CONTENT & MEDIA (Documents, Editors, Graphics)
    ContentMarkdown,
    ContentCodeEditor,

    // 4. 2D SCENES (Interactive Vector Viewports)
    SceneSchematic2D,
    SceneImageView
};

constexpr bool IsControlCategory(NavCategory cat) noexcept {
    return cat >= NavCategory::ElemButton && cat <= NavCategory::ElemImage;
}

constexpr bool IsSandboxCategory(NavCategory cat) noexcept {
    return cat <= NavCategory::SceneImageView;
}

constexpr bool IsFullStageCategory(NavCategory cat) noexcept {
    return cat == NavCategory::PanelSplitView ||
           cat == NavCategory::PanelSidebar ||
           cat == NavCategory::PanelTreeView ||
           cat == NavCategory::PanelTable ||
           cat == NavCategory::ContentMarkdown ||
           cat == NavCategory::ContentCodeEditor ||
           cat == NavCategory::SceneSchematic2D ||
           cat == NavCategory::SceneImageView;
}

enum class PreviewFormFactor : keel::u8 {
    Window = 0, ///< Desktop OS Window with titlebar (traffic lights, title, W x H badge)
    Tablet,     ///< Tablet / Touch Panel bezel (camera dot + Home Indicator)
    Phone,      ///< Smartphone / Handheld bezel (Dynamic Island + 9:41 status + Home Indicator)
    Full        ///< Clean edge-to-edge artboard on the recessed backdrop
};

inline constexpr keel::StaticArray<keel::StringRef, 4> kPreviewFormFactorNames = {
    "Window",
    "Tablet",
    "Phone",
    "Full"
};

enum class PreviewZoomMode : keel::u8 {
    Fit = 0,     ///< Auto-fit device to available canvas (or 100% in Window/Full)
    Real1To1,    ///< 1:1 physical millimeter match using monitor EDID / DPI
    Zoom75,      ///< 75%
    Zoom100,     ///< 100%
    Zoom125,     ///< 125%
    Zoom150,     ///< 150%
    Zoom175,     ///< 175%
    Zoom200,     ///< 200%
    Custom       ///< Custom zoom factor driven by Ctrl + MouseWheel
};

inline constexpr keel::StaticArray<keel::StringRef, 8> kPreviewZoomPresetNames = {
    "Fit",
    "1:1",
    "75%",
    "100%",
    "125%",
    "150%",
    "175%",
    "200%"
};

constexpr keel::StringRef CategoryDisplayTitle(NavCategory cat) noexcept {
    switch (cat) {
        case NavCategory::ElemHeading:       return "Heading — Section & Title Typography";
        case NavCategory::ElemElement:       return "Element — Form-Field Wrapper & Slot Anatomy";
        case NavCategory::ElemButton:        return "Button — Interactive Push Control";
        case NavCategory::ElemSlider:        return "Slider & StepSlider — Continuous & Discrete";
        case NavCategory::ElemLevelMeter:    return "LevelMeter — Segmented Audio/DSP VU Meter";
        case NavCategory::ElemKnob:          return "Knob — Rotary Parameter Control";
        case NavCategory::ElemSpinBox:       return "SpinBox — Precision Numeric Stepper";
        case NavCategory::ElemInputField:    return "InputField — Text & Validated Input";
        case NavCategory::ElemTagInput:      return "TagInput — Editable Multi-Tag Chips";
        case NavCategory::ElemSelect:        return "Select & MultiSelect — Dropdown & ComboBox";
        case NavCategory::ElemSegmented:     return "Segmented — Mutually Exclusive Pill Bar";
        case NavCategory::ElemColorPicker:   return "ColorPicker — 2D HSV Canvas & HEX/RGBA";
        case NavCategory::ElemDatePicker:    return "DatePicker — Calendar Grid & Time Stepper";
        case NavCategory::ElemSwitch:        return "Switch — Toggle & Symmetric Dual-Label";
        case NavCategory::ElemCheckbox:      return "Checkbox — Tri-State Boolean Control";
        case NavCategory::ElemRadio:         return "Radio — Exclusive Option Group";
        case NavCategory::ElemImage:         return "Image — Vector SVG & Raster Display";
        case NavCategory::PanelCard:         return "Card — Surface Container & Elevation";
        case NavCategory::PanelSplitView:    return "SplitView — Resizable Dual-Pane Sash";
        case NavCategory::PanelTabBar:       return "TabBar — Scrollable & Closable Tabs";
        case NavCategory::PanelBreadcrumb:   return "Breadcrumb — Hierarchical Path Trail";
        case NavCategory::PanelPagination:   return "Pagination — Page Navigation & Ellipsis";
        case NavCategory::PanelToolbar:      return "Toolbar — Command Strip & Tool Palettes";
        case NavCategory::PanelSidebar:      return "Sidebar — Application Workbench Shell";
        case NavCategory::PanelTreeView:     return "TreeView — Hierarchical Node Explorer";
        case NavCategory::PanelTable:        return "Table — 100k Virtualized Data Grid";
        case NavCategory::PanelOverlays:     return "Overlays — Modal, Toast & CommandPalette";
        case NavCategory::ContentMarkdown:   return "Markdown — Rich Document Renderer";
        case NavCategory::ContentCodeEditor: return "Code Editor — Syntax Highlighting & Minimap";
        case NavCategory::SceneSchematic2D:  return "Schematic 2D — Node Graph Canvas";
        case NavCategory::SceneImageView:    return "Image View — 16-bit Scientific Inspector";
    }
    return "GuiKit Preview";
}

constexpr keel::StringRef CategoryControlName(NavCategory cat) noexcept {
    switch (cat) {
        case NavCategory::ElemHeading:       return "Heading";
        case NavCategory::ElemElement:       return "Element";
        case NavCategory::ElemButton:        return "Button";
        case NavCategory::ElemSlider:        return "Slider";
        case NavCategory::ElemLevelMeter:    return "LevelMeter";
        case NavCategory::ElemKnob:          return "Knob";
        case NavCategory::ElemSpinBox:       return "SpinBox";
        case NavCategory::ElemInputField:    return "InputField";
        case NavCategory::ElemTagInput:      return "TagInput";
        case NavCategory::ElemSelect:        return "Select";
        case NavCategory::ElemSegmented:     return "Segmented";
        case NavCategory::ElemColorPicker:   return "ColorPicker";
        case NavCategory::ElemDatePicker:    return "DatePicker";
        case NavCategory::ElemSwitch:        return "Switch";
        case NavCategory::ElemCheckbox:      return "Checkbox";
        case NavCategory::ElemRadio:         return "Radio";
        case NavCategory::ElemImage:         return "Image";
        default:                             return "Control";
    }
}

inline constexpr keel::StaticArray<keel::StringRef, 3> kLabelAlignNames = {"Start", "Center", "End"};

inline constexpr keel::StaticArray<keel::StringRef, 8> kUiVariantNames = {
    "Default", "Primary", "Secondary", "Success", "Warning", "Danger", "Info", "Ghost"
};

inline constexpr keel::StaticArray<keel::StringRef, 4> kAxisModeNames = {
    "Auto", "Fit", "Stretch", "Fixed"
};

inline constexpr keel::StaticArray<keel::StringRef, 6> kElemLayoutNames = {
    "Auto", "Stacked", "Top", "Bottom", "Leading", "Trailing"
};

constexpr keel::StringRef SizeModeEnumName(guikit::SizeMode m) noexcept {
    switch (m) {
        case guikit::SizeMode::Fit:     return "Fit";
        case guikit::SizeMode::Fixed:   return "Fixed";
        case guikit::SizeMode::Stretch: return "Stretch";
    }
    return "Stretch";
}

constexpr keel::StringRef ElementLayoutEnumName(guikit::ElementLayout l) noexcept {
    switch (l) {
        case guikit::ElementLayout::Stacked:  return "Stacked";
        case guikit::ElementLayout::Top:      return "Top";
        case guikit::ElementLayout::Bottom:   return "Bottom";
        case guikit::ElementLayout::Leading:  return "Leading";
        case guikit::ElementLayout::Trailing: return "Trailing";
    }
    return "Stacked";
}

constexpr keel::StringRef HeadingLayoutEnumName(guikit::HeadingLayout l) noexcept {
    switch (l) {
        case guikit::HeadingLayout::Stacked:  return "Stacked";
        case guikit::HeadingLayout::Overline: return "Overline";
        case guikit::HeadingLayout::Inline:   return "Inline";
    }
    return "Stacked";
}

constexpr keel::StringRef LabelAlignEnumName(guikit::Align a) noexcept {
    switch (a) {
        case guikit::Align::Start:   return "Start";
        case guikit::Align::Center:  return "Center";
        case guikit::Align::End:     return "End";
        case guikit::Align::Stretch: return "Stretch";
    }
    return "Start";
}

constexpr keel::StringRef AlignName(guikit::Align a) noexcept {
    switch (a) {
        case guikit::Align::Start:   return "Start";
        case guikit::Align::Center:  return "Center";
        case guikit::Align::End:     return "End";
        case guikit::Align::Stretch: return "Stretch";
    }
    return "Center";
}

constexpr keel::StringRef VariantName(guikit::UiVariant v) noexcept {
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

constexpr keel::StringRef SizeName(guikit::UiSize s) noexcept {
    switch (s.GetPreset()) {
        case guikit::UiSize::Preset::Xs: return "Xs";
        case guikit::UiSize::Preset::Sm: return "Sm";
        case guikit::UiSize::Preset::Md: return "Md";
        case guikit::UiSize::Preset::Lg: return "Lg";
        case guikit::UiSize::Preset::Xl: return "Xl";
        default:                         return "Md";
    }
}

inline keel::FixedString<48> FormatUiSizePrefix(guikit::UiSize s) noexcept {
    if (s == guikit::UiSize::None || s == guikit::UiSize::Md) {
        return {};
    }
    return keel::format_fixed<48>(",\n    .size = guikit::UiSize::{}", SizeName(s));
}

constexpr keel::StringRef PlacementName(guikit::Placement p) noexcept {
    switch (p) {
        case guikit::Placement::Leading:  return "Leading";
        case guikit::Placement::Trailing: return "Trailing";
        case guikit::Placement::Top:      return "Top";
        case guikit::Placement::Bottom:   return "Bottom";
        case guikit::Placement::Thumb:    return "Thumb";
    }
    return "Trailing";
}

constexpr keel::StringRef CursorModeName(guikit::CursorBlinkMode m) noexcept {
    switch (m) {
        case guikit::CursorBlinkMode::Blinking: return "Blinking";
        case guikit::CursorBlinkMode::Solid:    return "Solid";
        case guikit::CursorBlinkMode::Hidden:   return "Hidden";
    }
    return "Blinking";
}

inline keel::String FormatDesc(bool show, keel::StringRef text, guikit::UiVariant v) {
    if (!show) return keel::String::from("\"\"");
    if (v == guikit::UiVariant::Muted) {
        return keel::format("\"{}\"", text);
    }
    return keel::format("{{\"{}\", guikit::StateColor::{}()}}", text, VariantName(v));
}

inline void AppendIndentedLines(keel::String& out, keel::StringRef block, keel::StringRef indent) {
    keel::usize start = 0;
    while (start <= block.size()) {
        keel::usize end = start;
        while (end < block.size() && block[end] != '\n') {
            ++end;
        }
        const keel::StringRef line = block.substr(start, end - start);
        if (!line.empty()) {
            out.append(indent);
            out.append(line);
        }
        out.push_back('\n');
        if (end == block.size()) {
            break;
        }
        start = end + 1;
    }
}

inline constexpr keel::StaticArray<keel::StringRef, 9> kBgPresetNames = {
    "None / Off", "Surface", "Primary", "Secondary", "Success", "Warning", "Danger", "Info", "Ghost"
};

constexpr guikit::StateColor BgPresetToStateColor(
    keel::usize idx,
    bool defaultEnabled,
    float tint = 1.0f,
    float alpha = 1.0f
) noexcept {
    guikit::StateColor res{};
    switch (idx) {
        case 0:  return defaultEnabled ? guikit::StateColor::None() : guikit::StateColor::Default();
        case 1:  res = guikit::StateColor::Surface(); break;
        case 2:  res = guikit::StateColor::Primary(); break;
        case 3:  res = guikit::StateColor::Secondary(); break;
        case 4:  res = guikit::StateColor::Success(); break;
        case 5:  res = guikit::StateColor::Warning(); break;
        case 6:  res = guikit::StateColor::Danger(); break;
        case 7:  res = guikit::StateColor::Info(); break;
        case 8:  res = guikit::StateColor::Ghost(); break;
        default: return defaultEnabled ? guikit::StateColor::Surface() : guikit::StateColor::Default();
    }
    if (idx >= 2 && idx <= 7 && tint < 0.995f) {
        res = res.Tinted(tint);
    }
    if (alpha < 0.995f) {
        res = res.WithAlphaF(alpha);
    }
    return res;
}

constexpr guikit::StateColor BgPresetToTrackColor(keel::usize idx) noexcept {
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

inline keel::String BgPresetToCode(
    keel::usize idx,
    bool defaultEnabled,
    float tint = 1.0f,
    float alpha = 1.0f
) {
    keel::String code;
    switch (idx) {
        case 0:  return keel::String::from(defaultEnabled ? "guikit::StateColor::None()" : "guikit::StateColor::Default()");
        case 1:  code = keel::String::from("guikit::StateColor::Surface()"); break;
        case 2:  code = keel::String::from("guikit::StateColor::Primary()"); break;
        case 3:  code = keel::String::from("guikit::StateColor::Secondary()"); break;
        case 4:  code = keel::String::from("guikit::StateColor::Success()"); break;
        case 5:  code = keel::String::from("guikit::StateColor::Warning()"); break;
        case 6:  code = keel::String::from("guikit::StateColor::Danger()"); break;
        case 7:  code = keel::String::from("guikit::StateColor::Info()"); break;
        case 8:  code = keel::String::from("guikit::StateColor::Ghost()"); break;
        default: return keel::String::from(defaultEnabled ? "guikit::StateColor::Surface()" : "guikit::StateColor::Default()");
    }
    if (idx >= 2 && idx <= 7 && tint < 0.995f) {
        keel::format_into(code, ".Tinted({:.2f}f)", tint);
    }
    if (alpha < 0.995f) {
        keel::format_into(code, ".WithAlphaF({:.2f}f)", alpha);
    }
    return code;
}

struct DemoContext {
    bool shellLightMode = false;
    bool shellFloating = false;
    bool previewDarkMode = true;
    bool previewLightMode = false;
    bool previewBounds = false;
    bool previewInspect = false;
    bool previewShowCode = true;
    keel::usize previewFormFactorIdx = 0; // 0 = Window, 1 = Tablet, 2 = Phone, 3 = Full
    bool previewLandscape = false;        // Orientation state for Tablet / Phone
    keel::usize previewZoomModeIdx = 3;   // 0=Fit, 1=Real 1:1, 2=75%, 3=100%, 4=125%, 5=150%, 6=175%, 7=200%, 8=Custom
    float previewCustomZoom = 1.0f;       // Custom zoom factor set via Ctrl + MouseWheel
    float previewEffectiveZoom = 1.0f;    // Resolved effective zoom factor for current frame
    float displayPxPerMm = 96.0f / 25.4f; // Physical monitor density (px/mm) for Real 1:1 mode
    bool previewWindowCustomSize = false; // false = Maximized (Stretch), true = user-resized WxH (dp)
    float previewWindowW = 760.0f;        // User-resized window width in logical dp
    float previewWindowH = 500.0f;        // User-resized window height in logical dp
    keel::u8 previewWindowResizeEdge = 0; // Active resize edge bitmask (1=Left, 2=Right, 4=Bottom)
    keel::u8 previewWindowHoverEdge = 0;  // Hovered resize edge bitmask (1=Left, 2=Right, 4=Bottom)
    guikit::Vec2 previewWindowDragStartMouse{0.0f, 0.0f};
    float previewWindowDragStartW = 0.0f;
    float previewWindowDragStartH = 0.0f;

    guikit::UiTheme shellTheme = guikit::UiTheme::Dark();
    guikit::UiTheme previewTheme = guikit::UiTheme::Dark();
    float previewDensity = 1.0f;

    // Semantic Element form-field wrapper state (shared across ELEMENTS screens, excluding ElemHeading)
    guikit::UiSize elemSize = guikit::UiSize::Md;
    keel::SmallString<128> elemLabel{"Element Label"};
    guikit::UiVariant elemLabelVariant = guikit::UiVariant::Default;
    keel::SmallString<128> elemDesc{"Secondary description text"};
    guikit::UiVariant elemDescVariant = guikit::UiVariant::Muted;
    bool elemWordWrap = true;
    bool elemShowIcon = false;
    keel::SmallString<128> elemBadge{""};
    guikit::UiVariant elemBadgeVariant = guikit::UiVariant::Info;
    keel::usize elemLayoutIdx = 0;      // 0 = Auto (kNullOpt), 1 = Stacked, 2 = Top, 3 = Bottom, 4 = Leading, 5 = Trailing
    guikit::Align elemAlign = guikit::Align::Start;
    keel::usize elemWidthModeIdx = 0;   // 0 = Auto, 1 = Fit, 2 = Stretch, 3 = Fixed
    float elemWidthDp = 280.0f;         // 120..520 dp
    bool elemDisabled = false;

    // Shared Control state
    bool ctrlLayoutOpen = false;
    guikit::UiSize controlsSize = guikit::UiSize::Md;
    guikit::UiVariant btnVariant = guikit::UiVariant::Primary;
    keel::usize ctrlWidthModeIdx = 0;   // 0 = Auto, 1 = Fit, 2 = Stretch, 3 = Fixed
    float ctrlWidthDp = 220.0f;         // 80..480 dp
    keel::usize ctrlHeightModeIdx = 0;  // 0 = Auto, 1 = Fit, 2 = Stretch, 3 = Fixed
    float ctrlHeightDp = 44.0f;         // 24..320 dp
    bool knobCustomDiameter = false;    // Optional explicit diameter override for Knob
    float knobDiameterDp = 64.0f;       // 28..140 dp
    guikit::Align ctrlAlign = guikit::Align::Center;      // Capsule content alignment for Button
    guikit::Align ctrlSelectAlign = guikit::Align::Start; // Content alignment for Select
    keel::StaticArray<keel::usize, 17> ctrlBgIdxByCat = {0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0};
    float ctrlBgTint = 1.0f;
    float ctrlBgAlpha = 1.0f;
    float ctrlBorderWidth = 0.0f;
    guikit::UiVariant ctrlBorderVariant = guikit::UiVariant::Default;
    float ctrlBorderRadius = 6.0f;

    // Shared assembly & container layout tokens
    float cardRadius = 8.0f;
    float cardPadding = 16.0f;
    float cardGap = 8.0f;
    float cardBorderWidth = 1.0f;
    float gridColGap = 12.0f;
    float gridRowGap = 10.0f;
    float colGap = 12.0f;
    float rowGap = 8.0f;

    static constexpr guikit::SizeMode DefaultCtrlWidthMode(NavCategory cat) noexcept {
        switch (cat) {
            case NavCategory::ElemButton:
            case NavCategory::ElemKnob:
            case NavCategory::ElemSwitch:
            case NavCategory::ElemCheckbox:
            case NavCategory::ElemRadio:
            case NavCategory::ElemImage:
                return guikit::SizeMode::Fit;
            default:
                return guikit::SizeMode::Stretch;
        }
    }

    static constexpr bool HasCtrlLayoutCategory(NavCategory cat) noexcept {
        return cat != NavCategory::ElemSwitch &&
               cat != NavCategory::ElemCheckbox &&
               cat != NavCategory::ElemRadio;
    }

    static constexpr bool IsCtrlBoxedCategory(NavCategory cat) noexcept {
        return cat == NavCategory::ElemButton ||
               cat == NavCategory::ElemSpinBox ||
               cat == NavCategory::ElemInputField ||
               cat == NavCategory::ElemTagInput ||
               cat == NavCategory::ElemSelect ||
               cat == NavCategory::ElemSegmented ||
               cat == NavCategory::ElemColorPicker ||
               cat == NavCategory::ElemDatePicker ||
               cat == NavCategory::ElemImage;
    }

    static constexpr bool IsCtrlBgDefaultEnabled(NavCategory cat) noexcept {
        return cat == NavCategory::ElemButton ||
               cat == NavCategory::ElemSpinBox ||
               cat == NavCategory::ElemInputField ||
               cat == NavCategory::ElemTagInput ||
               cat == NavCategory::ElemSelect ||
               cat == NavCategory::ElemSegmented ||
               cat == NavCategory::ElemColorPicker ||
               cat == NavCategory::ElemDatePicker;
    }

    static constexpr guikit::ElementLayout DefaultElemLayout(NavCategory cat) noexcept {
        switch (cat) {
            case NavCategory::ElemSwitch:
            case NavCategory::ElemCheckbox:
            case NavCategory::ElemRadio:
                return guikit::ElementLayout::Trailing;
            case NavCategory::ElemKnob:
                return guikit::ElementLayout::Leading;
            default:
                return guikit::ElementLayout::Stacked;
        }
    }

    bool HasElementExternal() const noexcept {
        return !elemLabel.empty() || !elemDesc.empty() || elemShowIcon || !elemBadge.empty();
    }

    constexpr guikit::StateColor ResolveControlColor() const noexcept {
        return guikit::StateColor::FromVariant(btnVariant);
    }

    constexpr guikit::StateColor GetControlColor() const noexcept {
        return guikit::StateColor::FromVariant(btnVariant);
    }

    constexpr guikit::Align GetCtrlAlign(NavCategory cat) const noexcept {
        return (cat == NavCategory::ElemSelect) ? ctrlSelectAlign : ctrlAlign;
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

    constexpr keel::Optional<guikit::ElementLayout> ResolveElemLayout() const noexcept {
        switch (elemLayoutIdx) {
            case 1:  return guikit::ElementLayout::Stacked;
            case 2:  return guikit::ElementLayout::Top;
            case 3:  return guikit::ElementLayout::Bottom;
            case 4:  return guikit::ElementLayout::Leading;
            case 5:  return guikit::ElementLayout::Trailing;
            default: return keel::kNullOpt;
        }
    }

    static constexpr guikit::AxisSize ResolveAxisSize(keel::usize modeIdx, float dp) noexcept {
        switch (modeIdx) {
            case 1:  return guikit::AxisSize::Fit();
            case 2:  return guikit::AxisSize::Stretch();
            case 3:  return guikit::AxisSize::Fixed(dp);
            default: return guikit::AxisSize::Auto();
        }
    }

    static keel::FixedString<48> FormatAxisSizeCode(guikit::AxisSize sz) noexcept {
        if (sz.IsFit()) {
            return "guikit::SizeMode::Fit";
        }
        if (sz.IsStretch()) {
            if (sz.HasCustomStretchWeight()) {
                return keel::format_fixed<48>("guikit::AxisSize::Stretch({:.1f}f)", sz.StretchWeight());
            }
            return "guikit::SizeMode::Stretch";
        }
        if (sz.IsFixed()) {
            return keel::format_fixed<48>("{:.1f}f", sz.Dp());
        }
        return "guikit::AxisSize::Auto()";
    }

    constexpr guikit::AxisSize ResolveElemWidth() const noexcept {
        return ResolveAxisSize(elemWidthModeIdx, elemWidthDp);
    }

    constexpr guikit::AxisSize ResolveCtrlWidth() const noexcept {
        return ResolveAxisSize(ctrlWidthModeIdx, ctrlWidthDp);
    }

    constexpr guikit::AxisSize ResolveCtrlHeight() const noexcept {
        return ResolveAxisSize(ctrlHeightModeIdx, ctrlHeightDp);
    }

    constexpr keel::Optional<float> ResolveKnobDiameter() const noexcept {
        return knobCustomDiameter ? keel::Optional<float>(knobDiameterDp) : keel::kNullOpt;
    }

    keel::FixedString<48> FormatKnobDiameterCode() const noexcept {
        if (knobCustomDiameter) {
            return keel::format_fixed<48>("{:.1f}f", knobDiameterDp);
        }
        return "keel::kNullOpt";
    }

    guikit::StateColor GetControlBackground(NavCategory cat) const noexcept {
        const keel::usize idx = static_cast<keel::usize>(cat);
        const keel::usize bgIdx = (idx < ctrlBgIdxByCat.size()) ? ctrlBgIdxByCat[idx] : 0;
        return BgPresetToStateColor(bgIdx, IsCtrlBgDefaultEnabled(cat), ctrlBgTint, ctrlBgAlpha);
    }

    keel::String FormatControlBackgroundCode(NavCategory cat) const {
        const keel::usize idx = static_cast<keel::usize>(cat);
        const keel::usize bgIdx = (idx < ctrlBgIdxByCat.size()) ? ctrlBgIdxByCat[idx] : 0;
        return BgPresetToCode(bgIdx, IsCtrlBgDefaultEnabled(cat), ctrlBgTint, ctrlBgAlpha);
    }

    guikit::StateColor GetCtrlBorderColor() const noexcept {
        return guikit::StateColor::FromVariant(ctrlBorderVariant);
    }

    guikit::Border GetCtrlBorder() const noexcept {
        return guikit::Border{
            .width = ctrlBorderWidth,
            .radius = ctrlBorderRadius,
            .color = GetCtrlBorderColor()
        };
    }

    guikit::ElementOptions MakeElementOptions(guikit::UiText customLabel) const noexcept {
        return guikit::ElementOptions{
            .label = customLabel,
            .description = GetElementDescription(),
            .icon = GetElementIcon(),
            .badge = GetElementBadge(),
            .layout = ResolveElemLayout(),
            .align = elemAlign,
            .width = ResolveElemWidth(),
            .size = elemSize,
            .disabled = elemDisabled
        };
    }

    guikit::ElementOptions MakeElementOptions() const noexcept {
        return MakeElementOptions(GetElementLabel());
    }

    template <typename Fn>
    void RunElementStage(guikit::Context& ui, Fn&& renderControls) const {
        if (auto el = ui.Element(MakeElementOptions())) {
            renderControls();
        }
    }

    keel::String FormatElementLabelCode(keel::Optional<keel::StringRef> customLabelOverride = keel::kNullOpt) const {
        const keel::StringRef lbl = customLabelOverride.has_value() ? *customLabelOverride : elemLabel.as_ref();
        if (lbl.empty()) return keel::String::from("\"\"");
        if (elemLabelVariant == guikit::UiVariant::Default) {
            return keel::format("\"{}\"", lbl);
        }
        return keel::format("{{\"{}\", guikit::StateColor::{}()}}", lbl, VariantName(elemLabelVariant));
    }

    keel::String FormatElementDescCode() const {
        return FormatDesc(!elemDesc.empty(), elemDesc, elemDescVariant);
    }

    keel::FixedString<48> FormatCtrlWidthCode() const noexcept {
        return FormatAxisSizeCode(ResolveCtrlWidth());
    }

    keel::FixedString<48> FormatControlWidthCode() const noexcept {
        return FormatCtrlWidthCode();
    }

    keel::FixedString<48> FormatCtrlHeightCode() const noexcept {
        return FormatAxisSizeCode(ResolveCtrlHeight());
    }

    keel::FixedString<48> FormatControlHeightCode() const noexcept {
        return FormatCtrlHeightCode();
    }

    keel::FixedString<64> FormatControlColorCode() const noexcept {
        return keel::format_fixed<64>("guikit::StateColor::{}()", VariantName(btnVariant));
    }

    keel::FixedString<64> FormatCtrlBorderColorCode() const noexcept {
        return keel::format_fixed<64>("guikit::StateColor::{}()", VariantName(ctrlBorderVariant));
    }

    keel::String FormatCtrlBorderCode() const {
        return keel::format(
            "{{.width = {:.1f}f, .radius = {:.1f}f, .color = {}}}",
            ctrlBorderWidth,
            ctrlBorderRadius,
            FormatCtrlBorderColorCode()
        );
    }

    keel::String FormatElementOpenCode(keel::Optional<keel::StringRef> customLabelOverride = keel::kNullOpt) const {
        keel::String out = keel::format(
            "if (auto el = ui.Element({{\n"
            "    .label = {}",
            FormatElementLabelCode(customLabelOverride)
        );
        if (!elemDesc.empty()) {
            keel::format_into(out, ",\n    .description = {}", FormatElementDescCode());
        }
        if (elemShowIcon) {
            out.append(",\n    .icon = guikit::Icon::Edit()");
        }
        if (!elemBadge.empty()) {
            keel::format_into(
                out,
                ",\n    .badge = {{\"{}\", guikit::StateColor::{}()}}",
                elemBadge,
                VariantName(elemBadgeVariant)
            );
        }
        if (const auto layoutOpt = ResolveElemLayout(); layoutOpt.has_value()) {
            keel::format_into(out, ",\n    .layout = guikit::ElementLayout::{}", ElementLayoutEnumName(*layoutOpt));
        }
        if (elemAlign != guikit::Align::Start) {
            keel::format_into(out, ",\n    .align = guikit::Align::{}", LabelAlignEnumName(elemAlign));
        }
        if (const guikit::AxisSize elemW = ResolveElemWidth(); !elemW.IsAuto()) {
            keel::format_into(out, ",\n    .width = {}", FormatAxisSizeCode(elemW));
        }
        if (elemSize != guikit::UiSize::None && elemSize != guikit::UiSize::Md) {
            keel::format_into(out, ",\n    .size = guikit::UiSize::{}", SizeName(elemSize));
        }
        if (elemDisabled) {
            out.append(",\n    .disabled = true");
        }
        out.append("\n})) {");
        return out;
    }

    keel::String WrapInElementCode(
        keel::StringRef controlCode,
        keel::Optional<keel::StringRef> customLabelOverride = keel::kNullOpt
    ) const {
        keel::String out = FormatElementOpenCode(customLabelOverride);
        out.push_back('\n');

        keel::usize start = 0;
        while (start <= controlCode.size()) {
            keel::usize end = start;
            while (end < controlCode.size() && controlCode[end] != '\n') {
                ++end;
            }
            const keel::StringRef line = controlCode.substr(start, end - start);
            if (!line.empty()) {
                out.append("    ");
                out.append(line);
            }
            out.push_back('\n');
            if (end == controlCode.size()) {
                break;
            }
            start = end + 1;
        }
        out.push_back('}');
        return out;
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
            if (ui.Select(btnVariant, kUiVariantNames).changed) {
                ui.InvalidateAll();
            }
        }
    }

    void RenderDescVariantPicker(guikit::Context& ui, guikit::UiVariant& descVar, bool enabled) {
        static constexpr keel::StaticArray<keel::StringRef, 7> descVarNames = {
            "Muted", "Default", "Primary", "Success", "Warning", "Danger", "Info"
        };
        static constexpr keel::StaticArray<guikit::UiVariant, 7> descVars = {
            guikit::UiVariant::Muted, guikit::UiVariant::Default, guikit::UiVariant::Primary,
            guikit::UiVariant::Success, guikit::UiVariant::Warning, guikit::UiVariant::Danger, guikit::UiVariant::Info
        };
        keel::usize descVarIdx = 0;
        for (keel::usize i = 0; i < descVars.size(); ++i) {
            if (descVars[i] == descVar) {
                descVarIdx = i;
                break;
            }
        }
        if (auto el = ui.Element({.label = "Description Color", .disabled = !enabled})) {
            if (ui.Select(descVarIdx, descVarNames, {
                .disabled = !enabled
            }).changed) {
                descVar = descVars[descVarIdx];
                ui.InvalidateAll();
            }
        }
    }

    void RenderElementSection(guikit::Context& ui) {
        if (auto el = ui.Element({.label = "Size"})) {
            if (ui.StepSlider(elemSize, {.showTicks = true, .showLabels = true}).changed) {
                ui.InvalidateAll();
            }
        }

        if (auto el = ui.Element({.label = "Label"})) {
            if (ui.InputField(elemLabel, {
                .placeholder = "No label...",
                .clearable = true
            }).changed) {
                ui.InvalidateAll();
            }
        }
        const bool labelDisabled = (elemLabel.empty() && !elemShowIcon);
        if (auto el = ui.Element({.label = "Label Color", .disabled = labelDisabled})) {
            if (ui.Select(elemLabelVariant, kUiVariantNames, {
                .disabled = labelDisabled
            }).changed) {
                ui.InvalidateAll();
            }
        }

        if (auto el = ui.Element({.label = "Description"})) {
            if (ui.InputField(elemDesc, {
                .placeholder = "No description...",
                .clearable = true
            }).changed) {
                ui.InvalidateAll();
            }
        }
        RenderDescVariantPicker(ui, elemDescVariant, !elemDesc.empty());
        if (auto el = ui.Element({.label = "Word Wrap", .disabled = elemDesc.empty()})) {
            if (ui.Switch(elemWordWrap, {.disabled = elemDesc.empty()}).changed) {
                ui.InvalidateAll();
            }
        }

        if (auto el = ui.Element({.label = "Leading Icon"})) {
            if (ui.Switch(elemShowIcon).changed) {
                ui.InvalidateAll();
            }
        }

        if (auto el = ui.Element({.label = "Badge"})) {
            if (ui.InputField(elemBadge, {
                .placeholder = "No badge (e.g. dB, Hz)...",
                .clearable = true
            }).changed) {
                ui.InvalidateAll();
            }
        }
        if (auto el = ui.Element({.label = "Badge Variant", .disabled = elemBadge.empty()})) {
            if (ui.Select(elemBadgeVariant, kUiVariantNames, {
                .disabled = elemBadge.empty()
            }).changed) {
                ui.InvalidateAll();
            }
        }

        if (auto el = ui.Element({.label = "Disabled"})) {
            if (ui.Switch(elemDisabled).changed) {
                ui.InvalidateAll();
            }
        }
    }

    void RenderElementLayoutSection(guikit::Context& ui, NavCategory cat) {
        const bool hasExternal = HasElementExternal();
        const auto explicitLayout = ResolveElemLayout();
        const guikit::ElementLayout effLayout = explicitLayout.value_or(DefaultElemLayout(cat));
        if (auto el = ui.Element({
            .label = "Layout",
            .badge = {
                ElementLayoutEnumName(effLayout),
                explicitLayout.has_value() ? guikit::UiVariant::Primary : guikit::UiVariant::Secondary
            },
            .disabled = !hasExternal
        })) {
            if (ui.Select(elemLayoutIdx, kElemLayoutNames, {
                .disabled = !hasExternal
            }).changed) {
                ui.InvalidateAll();
            }
        }

        keel::usize elemAlignIdx = static_cast<keel::usize>(elemAlign);
        if (auto el = ui.Element({.label = "Alignment"})) {
            if (ui.Segmented(elemAlignIdx, kLabelAlignNames).changed) {
                elemAlign = static_cast<guikit::Align>(elemAlignIdx);
                ui.InvalidateAll();
            }
        }

        if (auto el = ui.Element({.label = "Width"})) {
            if (ui.Segmented(elemWidthModeIdx, kAxisModeNames).changed) {
                ui.InvalidateAll();
            }
        }

        const bool elemFixedWidth = (elemWidthModeIdx == 3);
        if (auto el = ui.Element({.label = "Width (dp)", .disabled = !elemFixedWidth})) {
            if (ui.Slider(elemWidthDp, 120.0f, 520.0f, {
                .valueFormat = "%.0f dp",
                .step = 10.0f,
                .disabled = !elemFixedWidth
            }).changed) {
                ui.InvalidateAll();
            }
        }
    }

    void RenderControlLayoutSection(guikit::Context& ui, NavCategory cat) {
        if (!HasCtrlLayoutCategory(cat)) {
            return;
        }

        if (cat == NavCategory::ElemKnob) {
            if (auto el = ui.Element({
                .label = "Custom Diameter",
                .badge = {
                    knobCustomDiameter ? "Fixed" : "Auto",
                    knobCustomDiameter ? guikit::UiVariant::Primary : guikit::UiVariant::Secondary
                }
            })) {
                if (ui.Switch(knobCustomDiameter).changed) {
                    ui.InvalidateAll();
                }
            }
            if (auto el = ui.Element({.label = "Diameter (dp)", .disabled = !knobCustomDiameter})) {
                if (ui.Slider(knobDiameterDp, 28.0f, 140.0f, {
                    .valueFormat = "%.0f dp",
                    .step = 2.0f,
                    .disabled = !knobCustomDiameter
                }).changed) {
                    ui.InvalidateAll();
                }
            }
            return;
        }

        const auto ctrlWidth = ResolveCtrlWidth();
        const keel::StringRef ctrlWidthBadge = ctrlWidth.IsAuto()
            ? SizeModeEnumName(DefaultCtrlWidthMode(cat))
            : (ctrlWidth.IsFixed() ? "Fixed" : SizeModeEnumName(ctrlWidth.ResolveMode(guikit::SizeMode::Fit)));
        if (auto el = ui.Element({
            .label = "Width",
            .badge = {
                ctrlWidthBadge,
                ctrlWidth.IsAuto() ? guikit::UiVariant::Secondary : guikit::UiVariant::Primary
            }
        })) {
            if (ui.Segmented(ctrlWidthModeIdx, kAxisModeNames).changed) {
                ui.InvalidateAll();
            }
        }

        const bool ctrlFixedWidth = (ctrlWidthModeIdx == 3);
        if (auto el = ui.Element({.label = "Width (dp)", .disabled = !ctrlFixedWidth})) {
            if (ui.Slider(ctrlWidthDp, 80.0f, 480.0f, {
                .valueFormat = "%.0f dp",
                .step = 10.0f,
                .disabled = !ctrlFixedWidth
            }).changed) {
                ui.InvalidateAll();
            }
        }

        const auto ctrlHeight = ResolveCtrlHeight();
        const keel::StringRef ctrlHeightBadge = ctrlHeight.IsAuto()
            ? "Fit"
            : (ctrlHeight.IsFixed() ? "Fixed" : SizeModeEnumName(ctrlHeight.ResolveMode(guikit::SizeMode::Fit)));
        if (auto el = ui.Element({
            .label = "Height",
            .badge = {
                ctrlHeightBadge,
                ctrlHeight.IsAuto() ? guikit::UiVariant::Secondary : guikit::UiVariant::Primary
            }
        })) {
            if (ui.Segmented(ctrlHeightModeIdx, kAxisModeNames).changed) {
                ui.InvalidateAll();
            }
        }

        const bool ctrlFixedHeight = (ctrlHeightModeIdx == 3);
        if (auto el = ui.Element({.label = "Height (dp)", .disabled = !ctrlFixedHeight})) {
            if (ui.Slider(ctrlHeightDp, 24.0f, 320.0f, {
                .valueFormat = "%.0f dp",
                .step = 4.0f,
                .disabled = !ctrlFixedHeight
            }).changed) {
                ui.InvalidateAll();
            }
        }

        if (cat == NavCategory::ElemButton || cat == NavCategory::ElemSelect) {
            guikit::Align& targetAlign = (cat == NavCategory::ElemSelect) ? ctrlSelectAlign : ctrlAlign;
            keel::usize alignIdx = (targetAlign == guikit::Align::Start)
                ? 0
                : ((targetAlign == guikit::Align::End) ? 2 : 1);
            if (auto el = ui.Element({.label = "Alignment"})) {
                if (ui.Segmented(alignIdx, kLabelAlignNames).changed) {
                    targetAlign = (alignIdx == 0)
                        ? guikit::Align::Start
                        : ((alignIdx == 2) ? guikit::Align::End : guikit::Align::Center);
                    ui.InvalidateAll();
                }
            }
        }
    }

    void RenderControlStyleSection(guikit::Context& ui, NavCategory cat) {
        if (auto el = ui.Element({.label = "Size"})) {
            if (ui.StepSlider(controlsSize, {.showTicks = true, .showLabels = true}).changed) {
                ui.InvalidateAll();
            }
        }

        const bool hasControlVariant = (
            cat == NavCategory::ElemButton ||
            cat == NavCategory::ElemSlider ||
            cat == NavCategory::ElemLevelMeter ||
            cat == NavCategory::ElemKnob ||
            cat == NavCategory::ElemSpinBox ||
            cat == NavCategory::ElemSegmented ||
            cat == NavCategory::ElemDatePicker ||
            cat == NavCategory::ElemSwitch ||
            cat == NavCategory::ElemCheckbox ||
            cat == NavCategory::ElemRadio
        );
        if (hasControlVariant) {
            if (auto el = ui.Element({.label = "Color"})) {
                if (ui.Select(btnVariant, kUiVariantNames).changed) {
                    ui.InvalidateAll();
                }
            }
        }

        if (IsCtrlBoxedCategory(cat)) {
            keel::usize& ctrlBgIdx = ctrlBgIdxByCat[static_cast<keel::usize>(cat)];
            if (auto el = ui.Element({.label = "Background"})) {
                if (ui.Select(ctrlBgIdx, kBgPresetNames).changed) {
                    ui.InvalidateAll();
                }
            }
            const bool hasCtrlBgTint = (ctrlBgIdx >= 2 && ctrlBgIdx <= 7);
            if (auto el = ui.Element({.label = "Background Tint", .disabled = !hasCtrlBgTint})) {
                if (ui.Slider(ctrlBgTint, 0.0f, 1.0f, {
                    .valueFormat = "%.2f",
                    .step = 0.05f,
                    .disabled = !hasCtrlBgTint
                }).changed) {
                    ui.InvalidateAll();
                }
            }
            const bool hasCtrlBgAlpha = (ctrlBgIdx >= 1);
            if (auto el = ui.Element({.label = "Background Opacity", .disabled = !hasCtrlBgAlpha})) {
                if (ui.Slider(ctrlBgAlpha, 0.0f, 1.0f, {
                    .valueFormat = "%.2f",
                    .step = 0.05f,
                    .disabled = !hasCtrlBgAlpha
                }).changed) {
                    ui.InvalidateAll();
                }
            }

            const bool hasIntrinsicBorder = (cat != NavCategory::ElemImage && cat != NavCategory::ElemTagInput);
            if (auto el = ui.Element({.label = "Border Width"})) {
                static constexpr keel::StaticArray<keel::StringRef, 2> kBorderVariants = {{"Default", "%.1f dp"}};
                if (ui.Slider(ctrlBorderWidth, 0.0f, 4.0f, {
                    .valueFormat = (hasIntrinsicBorder && ctrlBorderWidth <= 0.0f)
                        ? guikit::UiText{"Default", kBorderVariants}
                        : guikit::UiText{"%.1f dp", kBorderVariants},
                    .step = 0.5f
                }).changed) {
                    ui.InvalidateAll();
                }
            }

            const bool hasVisibleCtrlBorder =
                (ctrlBorderWidth > 0.0f) ||
                (cat == NavCategory::ElemSpinBox) ||
                (cat == NavCategory::ElemInputField) ||
                (cat == NavCategory::ElemTagInput) ||
                (cat == NavCategory::ElemSelect) ||
                (cat == NavCategory::ElemSegmented) ||
                (cat == NavCategory::ElemColorPicker) ||
                (cat == NavCategory::ElemDatePicker) ||
                (cat == NavCategory::ElemButton &&
                    (btnVariant == guikit::UiVariant::Default || btnVariant == guikit::UiVariant::Secondary));
            if (auto el = ui.Element({.label = "Border Color", .disabled = !hasVisibleCtrlBorder})) {
                if (ui.Select(ctrlBorderVariant, kUiVariantNames, {
                    .disabled = !hasVisibleCtrlBorder
                }).changed) {
                    ui.InvalidateAll();
                }
            }

            if (auto el = ui.Element({.label = "Corner Radius"})) {
                if (ui.Slider(ctrlBorderRadius, 0.0f, 24.0f, {
                    .valueFormat = "%.0f dp",
                    .step = 1.0f
                }).changed) {
                    ui.InvalidateAll();
                }
            }
        }
    }

    void RenderAssemblyCommonInspector(guikit::Context& ui) {
        if (auto el = ui.Element({
            .label = "Preview Dark Mode",
            .description = "Switch preview between Dark and Light palette"
        })) {
            if (ui.Switch(previewDarkMode).changed) {
                previewLightMode = !previewDarkMode;
                const auto defTheme = previewDarkMode
                    ? guikit::UiTheme::Dark()
                    : guikit::UiTheme::Light();
                previewTheme.palette = defTheme.palette;
                previewTheme.variants = defTheme.variants;
                previewTheme.syntax = defTheme.syntax;
                previewTheme.mode = previewDarkMode ? guikit::ThemeMode::Dark : guikit::ThemeMode::Light;
                ui.InvalidateAll();
            }
        }

        if (auto el = ui.Element({.label = "Layout Density"})) {
            if (ui.Slider(previewDensity, 0.5f, 2.0f, {
                .valueFormat = "%.2fx",
                .step = 0.05
            }).changed) {
                ui.InvalidateAll();
            }
        }

        if (auto el = ui.Element({.label = "Horizontal Gap (Columns)"})) {
            if (ui.Slider(gridColGap, 0.0f, 32.0f, {.valueFormat = "%.0f dp"}).changed) ui.InvalidateAll();
        }
        if (auto el = ui.Element({.label = "Vertical Gap (Rows)"})) {
            if (ui.Slider(gridRowGap, 0.0f, 32.0f, {.valueFormat = "%.0f dp"}).changed) ui.InvalidateAll();
        }

        if (auto el = ui.Element({.label = "Card Corner Radius"})) {
            if (ui.Slider(cardRadius, 0.0f, 24.0f, {.valueFormat = "%.0f dp"}).changed) ui.InvalidateAll();
        }
        if (auto el = ui.Element({.label = "Card Padding"})) {
            if (ui.Slider(cardPadding, 4.0f, 32.0f, {.valueFormat = "%.0f dp"}).changed) ui.InvalidateAll();
        }
        if (auto el = ui.Element({.label = "Card Border Width"})) {
            if (ui.Slider(cardBorderWidth, 0.0f, 4.0f, {.valueFormat = "%.1f dp"}).changed) ui.InvalidateAll();
        }
    }
};

// ============================================================================
// 1. ELEMENTS (Atomic Controls)
// ============================================================================
void RenderHeadingInspector(guikit::Context& ui, DemoContext& ctx);
void RenderHeadingStage(guikit::Context& ui, DemoContext& ctx);
keel::String GenerateHeadingCode(const DemoContext& ctx);

void RenderElementInspector(guikit::Context& ui, DemoContext& ctx);
void RenderElementStage(guikit::Context& ui, DemoContext& ctx);
keel::String GenerateElementCode(const DemoContext& ctx);

void RenderButtonInspector(guikit::Context& ui, DemoContext& ctx);
void RenderButtonStage(guikit::Context& ui, DemoContext& ctx);
keel::String GenerateButtonCode(const DemoContext& ctx);

void RenderSliderInspector(guikit::Context& ui, DemoContext& ctx);
void RenderSliderStage(guikit::Context& ui, DemoContext& ctx);
keel::String GenerateSliderCode(const DemoContext& ctx);

void RenderLevelMeterInspector(guikit::Context& ui, DemoContext& ctx);
void RenderLevelMeterStage(guikit::Context& ui, DemoContext& ctx);
keel::String GenerateLevelMeterCode(const DemoContext& ctx);

void RenderKnobInspector(guikit::Context& ui, DemoContext& ctx);
void RenderKnobStage(guikit::Context& ui, DemoContext& ctx);
keel::String GenerateKnobCode(const DemoContext& ctx);

void RenderSpinBoxInspector(guikit::Context& ui, DemoContext& ctx);
void RenderSpinBoxStage(guikit::Context& ui, DemoContext& ctx);
keel::String GenerateSpinBoxCode(const DemoContext& ctx);

void RenderInputInspector(guikit::Context& ui, DemoContext& ctx);
void RenderInputStage(guikit::Context& ui, DemoContext& ctx);
keel::String GenerateInputCode(const DemoContext& ctx);

void RenderTagInputInspector(guikit::Context& ui, DemoContext& ctx);
void RenderTagInputStage(guikit::Context& ui, DemoContext& ctx);
keel::String GenerateTagInputCode(const DemoContext& ctx);

void RenderSelectInspector(guikit::Context& ui, DemoContext& ctx);
void RenderSelectStage(guikit::Context& ui, DemoContext& ctx);
keel::String GenerateSelectCode(const DemoContext& ctx);

void RenderSegmentedInspector(guikit::Context& ui, DemoContext& ctx);
void RenderSegmentedStage(guikit::Context& ui, DemoContext& ctx);
keel::String GenerateSegmentedCode(const DemoContext& ctx);

void RenderColorPickerInspector(guikit::Context& ui, DemoContext& ctx);
void RenderColorPickerStage(guikit::Context& ui, DemoContext& ctx);
keel::String GenerateColorPickerCode(const DemoContext& ctx);

void RenderDatePickerInspector(guikit::Context& ui, DemoContext& ctx);
void RenderDatePickerStage(guikit::Context& ui, DemoContext& ctx);
keel::String GenerateDatePickerCode(const DemoContext& ctx);

void RenderSwitchInspector(guikit::Context& ui, DemoContext& ctx);
void RenderSwitchStage(guikit::Context& ui, DemoContext& ctx);
keel::String GenerateSwitchCode(const DemoContext& ctx);

void RenderCheckboxInspector(guikit::Context& ui, DemoContext& ctx);
void RenderCheckboxStage(guikit::Context& ui, DemoContext& ctx);
keel::String GenerateCheckboxCode(const DemoContext& ctx);

void RenderRadioInspector(guikit::Context& ui, DemoContext& ctx);
void RenderRadioStage(guikit::Context& ui, DemoContext& ctx);
keel::String GenerateRadioCode(const DemoContext& ctx);

// ============================================================================
// 2. PANELS & NAV (Containers & Navigation)
// ============================================================================
void RenderCardInspector(guikit::Context& ui, DemoContext& ctx);
void RenderCardStage(guikit::Context& ui, DemoContext& ctx);
keel::String GenerateCardCode(const DemoContext& ctx);

void RenderSplitViewInspector(guikit::Context& ui, DemoContext& ctx);
void RenderSplitViewStage(guikit::Context& ui, DemoContext& ctx);
keel::String GenerateSplitViewCode(const DemoContext& ctx);

void RenderTabBarInspector(guikit::Context& ui, DemoContext& ctx);
void RenderTabBarStage(guikit::Context& ui, DemoContext& ctx);
keel::String GenerateTabBarCode(const DemoContext& ctx);

void RenderBreadcrumbInspector(guikit::Context& ui, DemoContext& ctx);
void RenderBreadcrumbStage(guikit::Context& ui, DemoContext& ctx);
keel::String GenerateBreadcrumbCode(const DemoContext& ctx);

void RenderPaginationInspector(guikit::Context& ui, DemoContext& ctx);
void RenderPaginationStage(guikit::Context& ui, DemoContext& ctx);
keel::String GeneratePaginationCode(const DemoContext& ctx);

void RenderSidebarInspector(guikit::Context& ui, DemoContext& ctx);
void RenderSidebarStage(guikit::Context& ui, DemoContext& ctx);
keel::String GenerateSidebarCode(const DemoContext& ctx);

void RenderTreeViewInspector(guikit::Context& ui, DemoContext& ctx);
void RenderTreeViewStage(guikit::Context& ui, DemoContext& ctx);
keel::String GenerateTreeViewCode(const DemoContext& ctx);

void RenderTableInspector(guikit::Context& ui, DemoContext& ctx);
void RenderTableStage(guikit::Context& ui, DemoContext& ctx);
keel::String GenerateTableCode(const DemoContext& ctx);

void RenderOverlaysInspector(guikit::Context& ui, DemoContext& ctx);
void RenderOverlaysStage(guikit::Context& ui, DemoContext& ctx);
keel::String GenerateOverlaysCode(const DemoContext& ctx);

void RenderToolbarInspector(guikit::Context& ui, DemoContext& ctx);
void RenderToolbarStage(guikit::Context& ui, DemoContext& ctx);
keel::String GenerateToolbarCode(const DemoContext& ctx);

// ============================================================================
// 3. CONTENT & MEDIA (Documents, Editors, Graphics)
// ============================================================================
void RenderImageInspector(guikit::Context& ui, DemoContext& ctx);
void RenderImageStage(guikit::Context& ui, DemoContext& ctx);
keel::String GenerateImageCode(const DemoContext& ctx);

void RenderMarkdownInspector(guikit::Context& ui, DemoContext& ctx);
void RenderMarkdownStage(guikit::Context& ui, DemoContext& ctx);
keel::String GenerateMarkdownCode(const DemoContext& ctx);

void RenderCodeEditorInspector(guikit::Context& ui, DemoContext& ctx);
void RenderCodeEditorStage(guikit::Context& ui, DemoContext& ctx);
keel::String GenerateCodeEditorCode(const DemoContext& ctx);

// ============================================================================
// 4. 2D SCENES (Interactive Vector Viewports)
// ============================================================================
void RenderSchematicInspector(guikit::Context& ui, DemoContext& ctx);
void RenderSchematicStage(guikit::Context& ui, DemoContext& ctx);
keel::String GenerateSchematicCode(const DemoContext& ctx);

void RenderImageViewInspector(guikit::Context& ui, DemoContext& ctx);
void RenderImageViewStage(guikit::Context& ui, DemoContext& ctx);
keel::String GenerateImageViewCode(const DemoContext& ctx);

void ReleaseImageViewDemoBuffers() noexcept;

} // namespace guikit::demo
