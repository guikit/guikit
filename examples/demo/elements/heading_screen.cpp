#include "../demo_context.hpp"

namespace guikit::demo {

namespace {

struct HeadingStudioState {
    bool hierarchySpecimen = false;
    bool wrapInCard = false;
    guikit::HeadingLevel level = guikit::HeadingLevel::H3;
    keel::SmallString<128> title{"Signal Processing Pipeline"};
    guikit::UiVariant titleVariant = guikit::UiVariant::Default;
    keel::SmallString<160> subtitle{"Real-time polyphase filterbank & quadrature demodulator"};
    guikit::UiVariant subtitleVariant = guikit::UiVariant::Muted;
    bool wordWrap = true;
    bool showIcon = true;
    keel::SmallString<64> badge{"v3.2"};
    guikit::UiVariant badgeVariant = guikit::UiVariant::Primary;
    guikit::HeadingLayout layout = guikit::HeadingLayout::Stacked;
    guikit::HeadingSeparator separator = guikit::HeadingSeparator::Bottom;
    guikit::UiVariant separatorVariant = guikit::UiVariant::Default;
    float separatorWidth = 1.0f;
    guikit::Align align = guikit::Align::Start;
    keel::usize widthModeIdx = 0; // 0 = Auto (Stretch), 1 = Fit, 2 = Stretch, 3 = Fixed
    float widthDp = 360.0f;
    bool disabled = false;
};

HeadingStudioState s_state;

constexpr keel::StringRef LevelCodeName(guikit::HeadingLevel lvl) noexcept {
    switch (lvl) {
        case guikit::HeadingLevel::H1: return "guikit::HeadingLevel::H1";
        case guikit::HeadingLevel::H2: return "guikit::HeadingLevel::H2";
        case guikit::HeadingLevel::H3: return "guikit::HeadingLevel::H3";
        case guikit::HeadingLevel::H4: return "guikit::HeadingLevel::H4";
        case guikit::HeadingLevel::H5: return "guikit::HeadingLevel::H5";
        case guikit::HeadingLevel::H6: return "guikit::HeadingLevel::H6";
    }
    return "guikit::HeadingLevel::H3";
}

guikit::HeadingDivider BuildDivider() noexcept {
    return guikit::HeadingDivider{
        s_state.separator,
        guikit::StateColor::FromVariant(s_state.separatorVariant),
        s_state.separatorWidth
    };
}

keel::String FormatDividerCode() {
    if (s_state.separator == guikit::HeadingSeparator::None) {
        return keel::String::from("guikit::HeadingSeparator::None");
    }
    const keel::StringRef placeStr = (s_state.separator == guikit::HeadingSeparator::Bottom)
        ? "guikit::HeadingSeparator::Bottom"
        : "guikit::HeadingSeparator::Trailing";
    const bool defaultWidth = std::fabs(s_state.separatorWidth - 1.0f) < 0.01f;
    if (s_state.separatorVariant == guikit::UiVariant::Default && defaultWidth) {
        return keel::String::from(placeStr);
    }
    if (defaultWidth) {
        return keel::format("{{{}, guikit::StateColor::{}()}}", placeStr, VariantName(s_state.separatorVariant));
    }
    return keel::format(
        "{{{}, guikit::StateColor::{}(), {:.1f}f}}",
        placeStr,
        VariantName(s_state.separatorVariant),
        s_state.separatorWidth
    );
}

guikit::AxisSize ActiveHeadingWidth() noexcept {
    return DemoContext::ResolveAxisSize(s_state.widthModeIdx, s_state.widthDp);
}

} // namespace

void RenderHeadingInspector(guikit::Context& ui, DemoContext& ctx) {
    if (auto el = ui.Element({
        .label = "H1–H6 Scale Specimen",
        .description = "Compare full typographic hierarchy vs single Heading"
    })) {
        if (ui.Switch(s_state.hierarchySpecimen).changed) {
            ui.InvalidateAll();
        }
    }

    static constexpr keel::StaticArray<keel::StringRef, 6> kLevelNames = {
        "H1", "H2", "H3", "H4", "H5", "H6"
    };
    if (auto el = ui.Element({.label = "Heading Level", .disabled = s_state.hierarchySpecimen})) {
        if (ui.StepSlider(s_state.level, kLevelNames, {
            .showTicks = true,
            .showLabels = true,
            .disabled = s_state.hierarchySpecimen
        }).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Title Text", .disabled = s_state.hierarchySpecimen})) {
        if (ui.InputField(s_state.title, {
            .placeholder = "Heading title...",
            .clearable = true,
            .disabled = s_state.hierarchySpecimen
        }).changed) {
            ui.InvalidateAll();
        }
    }

    const bool titleColorDisabled = (s_state.title.empty() && !s_state.showIcon);
    if (auto el = ui.Element({.label = "Title Color", .disabled = titleColorDisabled})) {
        if (ui.StepSlider(s_state.titleVariant, {
            .showTicks = true,
            .showLabels = true,
            .disabled = titleColorDisabled
        }).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Subtitle / Overline", .disabled = s_state.hierarchySpecimen})) {
        if (ui.InputField(s_state.subtitle, {
            .placeholder = "Optional subtitle or overline...",
            .clearable = true,
            .disabled = s_state.hierarchySpecimen
        }).changed) {
            ui.InvalidateAll();
        }
    }
    ctx.RenderDescVariantPicker(ui, s_state.subtitleVariant, !s_state.subtitle.empty());
    if (auto el = ui.Element({.label = "Word Wrap Subtitle", .disabled = s_state.subtitle.empty()})) {
        if (ui.Switch(s_state.wordWrap, {.disabled = s_state.subtitle.empty()}).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Leading Icon"})) {
        if (ui.Switch(s_state.showIcon).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Badge Text"})) {
        if (ui.InputField(s_state.badge, {
            .placeholder = "No badge (e.g. v3.2)...",
            .clearable = true
        }).changed) {
            ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({.label = "Badge Variant", .disabled = s_state.badge.empty()})) {
        if (ui.StepSlider(s_state.badgeVariant, {
            .showTicks = true,
            .showLabels = true,
            .disabled = s_state.badge.empty()
        }).changed) {
            ui.InvalidateAll();
        }
    }

    static constexpr keel::StaticArray<keel::StringRef, 3> kHeadingLayouts = {
        "Stacked", "Overline", "Inline"
    };
    keel::usize layoutIdx = static_cast<keel::usize>(s_state.layout);
    if (auto el = ui.Element({.label = "Heading Layout"})) {
        if (ui.StepSlider(layoutIdx, kHeadingLayouts, {
            .showTicks = true,
            .showLabels = true
        }).changed) {
            s_state.layout = static_cast<guikit::HeadingLayout>(layoutIdx);
            ui.InvalidateAll();
        }
    }

    static constexpr keel::StaticArray<keel::StringRef, 3> kSepStyles = {"None", "Bottom", "Trailing"};
    if (auto el = ui.Element({.label = "Separator"})) {
        if (ui.StepSlider(s_state.separator, kSepStyles, {
            .showTicks = true,
            .showLabels = true
        }).changed) {
            ui.InvalidateAll();
        }
    }
    const bool sepDisabled = (s_state.separator == guikit::HeadingSeparator::None);
    if (auto el = ui.Element({.label = "Separator Color", .disabled = sepDisabled})) {
        if (ui.StepSlider(s_state.separatorVariant, {
            .showTicks = true,
            .showLabels = true,
            .disabled = sepDisabled
        }).changed) {
            ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({.label = "Separator Stroke Width", .disabled = sepDisabled})) {
        if (ui.Slider(s_state.separatorWidth, 0.5f, 4.0f, {
            .valueFormat = "%.1f dp",
            .step = 0.5f,
            .disabled = sepDisabled
        }).changed) {
            ui.InvalidateAll();
        }
    }

    keel::usize alignIdx = static_cast<keel::usize>(s_state.align);
    if (auto el = ui.Element({.label = "Alignment"})) {
        if (ui.StepSlider(alignIdx, kLabelAlignNames, {
            .showTicks = true,
            .showLabels = true
        }).changed) {
            s_state.align = static_cast<guikit::Align>(alignIdx);
            ui.InvalidateAll();
        }
    }

    static constexpr keel::StaticArray<keel::StringRef, 4> kWidthModes = {"Auto", "Fit", "Stretch", "Fixed"};
    if (auto el = ui.Element({
        .label = "Heading Width",
        .badge = {"Stretch", guikit::UiVariant::Secondary}
    })) {
        if (ui.StepSlider(s_state.widthModeIdx, kWidthModes, {
            .showTicks = true,
            .showLabels = true
        }).changed) {
            ui.InvalidateAll();
        }
    }

    const bool fixedWidth = (s_state.widthModeIdx == 3);
    if (auto el = ui.Element({.label = "Heading Width (dp)", .disabled = !fixedWidth})) {
        if (ui.Slider(s_state.widthDp, 140.0f, 560.0f, {
            .valueFormat = "%.0f dp",
            .step = 10.0f,
            .disabled = !fixedWidth
        }).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Preview on Surface Card"})) {
        if (ui.Switch(s_state.wrapInCard).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Disabled"})) {
        if (ui.Switch(s_state.disabled).changed) {
            ui.InvalidateAll();
        }
    }
}

void RenderHeadingStage(guikit::Context& ui, DemoContext&) {
    auto renderContent = [&]() {
        const guikit::HeadingDivider divider = BuildDivider();
        const guikit::AxisSize headingW = ActiveHeadingWidth();

        if (s_state.hierarchySpecimen) {
            struct LevelSample {
                guikit::HeadingLevel level;
                keel::u8 num;
                keel::StringRef title;
                keel::StringRef sub;
            };
            static constexpr keel::StaticArray<LevelSample, 6> kSamples = {{
                {guikit::HeadingLevel::H1, 1, "Telemetry Command Deck",     "Top-level workspace display heading"},
                {guikit::HeadingLevel::H2, 2, "Signal Processing Pipeline", "Major subsystem section header"},
                {guikit::HeadingLevel::H3, 3, "Quadrature Demodulator",     "Card or panel primary title (default)"},
                {guikit::HeadingLevel::H4, 4, "Carrier Recovery Loop",      "Group sub-heading"},
                {guikit::HeadingLevel::H5, 5, "Loop Filter Coefficients",   "Compact inspector group header"},
                {guikit::HeadingLevel::H6, 6, "DIAGNOSTIC TELEMETRY",       "Dense overline / micro section header"}
            }};

            const auto& th = ui.GetTheme();
            const float invScale = (th.scale > 0.0f) ? (1.0f / th.scale) : 1.0f;
            if (auto col = ui.Column({.gap = 16.0f})) {
                for (const auto& item : kSamples) {
                    const float baseSp = th.GetHeading(item.level).titleFontSize * invScale;
                    ui.Heading({
                        .label = {item.title, s_state.titleVariant},
                        .description = s_state.subtitle.empty()
                            ? guikit::UiText{}
                            : guikit::UiText{
                                item.sub,
                                s_state.subtitleVariant,
                                s_state.wordWrap ? guikit::TextWrap::Word() : guikit::TextWrap::SingleLine()
                            },
                        .icon = s_state.showIcon ? guikit::Icon::Flash() : guikit::Icon{},
                        .badge = {ui.Format("H{} • {:.0f}sp", item.num, baseSp), s_state.badgeVariant},
                        .level = item.level,
                        .layout = s_state.layout,
                        .align = s_state.align,
                        .width = headingW,
                        .separator = divider,
                        .disabled = s_state.disabled
                    });
                }
            }
            return;
        }

        ui.Heading({
            .label = {s_state.title, s_state.titleVariant},
            .description = s_state.subtitle.empty()
                ? guikit::UiText{}
                : guikit::UiText{
                    s_state.subtitle,
                    s_state.subtitleVariant,
                    s_state.wordWrap ? guikit::TextWrap::Word() : guikit::TextWrap::SingleLine()
                },
            .icon = s_state.showIcon ? guikit::Icon::Flash() : guikit::Icon{},
            .badge = s_state.badge.empty()
                ? guikit::UiText{}
                : guikit::UiText{s_state.badge, s_state.badgeVariant},
            .level = s_state.level,
            .layout = s_state.layout,
            .align = s_state.align,
            .width = headingW,
            .separator = divider,
            .disabled = s_state.disabled
        });
    };

    if (s_state.wrapInCard) {
        if (auto card = ui.Card({.gap = 12.0f, .padding = guikit::Padding::All(18.0f)})) {
            renderContent();
        }
    } else {
        renderContent();
    }
}

keel::String GenerateHeadingCode(const DemoContext&) {
    const keel::String labelCode = (s_state.titleVariant == guikit::UiVariant::Default)
        ? keel::format("\"{}\"", s_state.title)
        : keel::format("{{\"{}\", guikit::StateColor::{}()}}", s_state.title, VariantName(s_state.titleVariant));

    const keel::String descCode = FormatDesc(!s_state.subtitle.empty(), s_state.subtitle, s_state.subtitleVariant);
    const keel::String badgeCode = s_state.badge.empty()
        ? keel::String::from("\"\"")
        : keel::format("{{\"{}\", guikit::StateColor::{}()}}", s_state.badge, VariantName(s_state.badgeVariant));
    const keel::FixedString<48> widthCode = DemoContext::FormatAxisSizeCode(ActiveHeadingWidth());

    keel::String headingStmt = keel::format(
        "ui.Heading({{\n"
        "    .label = {},\n"
        "    .description = {},\n"
        "    .icon = {},\n"
        "    .badge = {},\n"
        "    .level = {},\n"
        "    .layout = guikit::HeadingLayout::{},\n"
        "    .align = guikit::Align::{},\n"
        "    .width = {},\n"
        "    .separator = {},\n"
        "    .disabled = {}\n"
        "}});",
        labelCode,
        descCode,
        s_state.showIcon ? "guikit::Icon::Flash()" : "{}",
        badgeCode,
        LevelCodeName(s_state.level),
        HeadingLayoutEnumName(s_state.layout),
        LabelAlignEnumName(s_state.align),
        widthCode,
        FormatDividerCode(),
        s_state.disabled ? "true" : "false"
    );

    if (!s_state.wrapInCard) {
        return headingStmt;
    }

    keel::String out = keel::String::from(
        "if (auto card = ui.Card({.gap = 12.0f, .padding = guikit::Padding::All(18.0f)})) {\n"
    );
    keel::usize start = 0;
    const keel::StringRef ref = headingStmt.as_ref();
    while (start <= ref.size()) {
        keel::usize end = start;
        while (end < ref.size() && ref[end] != '\n') {
            ++end;
        }
        out.append("    ");
        out.append(ref.substr(start, end - start));
        out.push_back('\n');
        if (end == ref.size()) break;
        start = end + 1;
    }
    out.push_back('}');
    return out;
}

} // namespace guikit::demo
