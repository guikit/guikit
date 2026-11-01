#include "../DemoContext.hpp"
#include <array>
#include <format>

namespace guikit::demo {

namespace {

struct CardDemoState {
    bool fullWidth = false;
    float minWidth = 280.0f;
    size_t bgIdx = 0;     // 0=Surface, 1=None, 2=Primary, 3=Secondary, 4=Success, 5=Warning, 6=Danger, 7=Info
    size_t borderIdx = 0; // 0=Default, 1=Primary, 2=Secondary, 3=Success, 4=Warning, 5=Danger, 6=Info
    bool showHeader = true;
    bool showDivider = true;
};

CardDemoState s_card;

constexpr std::array<std::string_view, 8> kBgShort = {
    "Surf", "None", "Pri", "Sec", "Suc", "Wrn", "Dgr", "Inf"
};

constexpr std::array<std::string_view, 7> kBorderShort = {
    "Def", "Pri", "Sec", "Suc", "Wrn", "Dgr", "Inf"
};

guikit::StateColor ResolveCardBg() noexcept {
    switch (s_card.bgIdx) {
        case 1: return guikit::StateColor::None();
        case 2: return guikit::StateColor::Primary();
        case 3: return guikit::StateColor::Secondary();
        case 4: return guikit::StateColor::Success();
        case 5: return guikit::StateColor::Warning();
        case 6: return guikit::StateColor::Danger();
        case 7: return guikit::StateColor::Info();
        default: return guikit::StateColor::Surface();
    }
}

std::string_view CardBgCodeName() noexcept {
    switch (s_card.bgIdx) {
        case 1: return "guikit::StateColor::None()";
        case 2: return "guikit::StateColor::Primary()";
        case 3: return "guikit::StateColor::Secondary()";
        case 4: return "guikit::StateColor::Success()";
        case 5: return "guikit::StateColor::Warning()";
        case 6: return "guikit::StateColor::Danger()";
        case 7: return "guikit::StateColor::Info()";
        default: return "guikit::StateColor::Surface()";
    }
}

guikit::StateColor ResolveCardBorderColor() noexcept {
    switch (s_card.borderIdx) {
        case 1: return guikit::StateColor::Primary();
        case 2: return guikit::StateColor::Secondary();
        case 3: return guikit::StateColor::Success();
        case 4: return guikit::StateColor::Warning();
        case 5: return guikit::StateColor::Danger();
        case 6: return guikit::StateColor::Info();
        default: return guikit::StateColor{};
    }
}

std::string_view CardBorderCodeName() noexcept {
    switch (s_card.borderIdx) {
        case 1: return "guikit::StateColor::Primary()";
        case 2: return "guikit::StateColor::Secondary()";
        case 3: return "guikit::StateColor::Success()";
        case 4: return "guikit::StateColor::Warning()";
        case 5: return "guikit::StateColor::Danger()";
        case 6: return "guikit::StateColor::Info()";
        default: return "guikit::StateColor{}";
    }
}

} // namespace

void RenderCardInspector(guikit::Context& ui, DemoContext& ctx) {
    if (auto el = ui.Element({.label = "Card Size"})) {
        if (ui.StepSlider(ctx.controlsSize, {.showTicks = true, .showLabels = true, .includeNone = true}).changed) {
            ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({.label = "Corner Radius"})) {
        if (ui.Slider(ctx.cardRadius, 0.0f, 24.0f, {.valueFormat = "%.0f px"}).changed) {
            ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({.label = "Internal Padding"})) {
        if (ui.Slider(ctx.cardPadding, 0.0f, 32.0f, {.valueFormat = "%.0f px"}).changed) {
            ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({.label = "Content Spacing (Gap)"})) {
        if (ui.Slider(ctx.cardGap, 0.0f, 24.0f, {.valueFormat = "%.0f px"}).changed) {
            ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({.label = "Background"})) {
        if (ui.StepSlider(s_card.bgIdx, kBgShort, {.showTicks = true, .showLabels = true}).changed) {
            ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({.label = "Border Width"})) {
        if (ui.Slider(ctx.cardBorderWidth, 0.0f, 4.0f, {.valueFormat = "%.1f px", .step = 0.5f}).changed) {
            ui.InvalidateAll();
        }
    }
    const bool hasBorder = (ctx.cardBorderWidth > 0.0f);
    if (auto el = ui.Element({.label = "Border Color", .disabled = !hasBorder})) {
        if (ui.StepSlider(s_card.borderIdx, kBorderShort, {.showTicks = true, .showLabels = true, .disabled = !hasBorder}).changed) {
            ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({.label = "Stretch Full Width"})) {
        if (ui.Switch(s_card.fullWidth).changed) {
            ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({.label = "Minimum Width", .disabled = s_card.fullWidth})) {
        if (ui.Slider(s_card.minWidth, 200.0f, 560.0f, {.valueFormat = "%.0f px", .step = 10.0f, .disabled = s_card.fullWidth}).changed) {
            ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({.label = "Show Header"})) {
        if (ui.Switch(s_card.showHeader).changed) {
            ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({.label = "Header Divider", .disabled = !s_card.showHeader})) {
        if (ui.Switch(s_card.showDivider, {.disabled = !s_card.showHeader}).changed) {
            ui.InvalidateAll();
        }
    }
}

void RenderCardStage(guikit::Context& ui, DemoContext& ctx) {
    if (auto card = ui.Size(ctx.controlsSize).Card({
        .minWidth = s_card.fullWidth ? 0.0f : s_card.minWidth,
        .gap = ctx.cardGap,
        .padding = guikit::Padding::All(ctx.cardPadding),
        .background = ResolveCardBg(),
        .border = {
            .width = ctx.cardBorderWidth,
            .radius = ctx.cardRadius,
            .color = ResolveCardBorderColor()
        },
        .widthMode = s_card.fullWidth ? guikit::SizeMode::Stretch : guikit::SizeMode::Fit
    })) {
        if (s_card.showHeader) {
            ui.Heading({
                .label = "Card Container",
                .description = "Elevated surface with padding, gap, and border",
                .icon = guikit::Icon::Folder(),
                .badge = {"Surface", guikit::StateColor::Primary()},
                .divider = s_card.showDivider
            });
        }

        ui.Text("First content row inside the card container.", {
            .role = guikit::TextRole::BodyMedium
        });

        ui.Text("Second content row spaced by the card's gap property.", {
            .role = guikit::TextRole::BodySmall,
            .variant = guikit::UiVariant::Secondary
        });
    }
}

std::string GenerateCardCode(const DemoContext& ctx) {
    return std::format(
        "if (auto card = ui.{}Card({{\n"
        "    .minWidth = {:.1f}f,\n"
        "    .gap = {:.1f}f,\n"
        "    .padding = guikit::Padding::All({:.1f}f),\n"
        "    .background = {},\n"
        "    .border = {{.width = {:.1f}f, .radius = {:.1f}f, .color = {}}},\n"
        "    .widthMode = guikit::SizeMode::{}\n"
        "}})) {{\n"
        "    ui.Heading({{\n"
        "        .label = \"Card Container\",\n"
        "        .description = \"Elevated surface with padding, gap, and border\",\n"
        "        .divider = {}\n"
        "    }});\n"
        "    ui.Text(\"First content row inside the card container.\");\n"
        "    ui.Text(\"Second content row spaced by the card's gap property.\");\n"
        "}}",
        FormatUiSizePrefix(ctx.controlsSize),
        s_card.fullWidth ? 0.0f : s_card.minWidth,
        ctx.cardGap,
        ctx.cardPadding,
        CardBgCodeName(),
        ctx.cardBorderWidth,
        ctx.cardRadius,
        CardBorderCodeName(),
        s_card.fullWidth ? "Stretch" : "Fit",
        s_card.showDivider ? "true" : "false"
    );
}

} // namespace guikit::demo
