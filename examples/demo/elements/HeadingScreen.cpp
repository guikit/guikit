#include "../DemoContext.hpp"
#include <format>

namespace guikit::demo {

namespace {
struct HeadingState {
    guikit::UiVariant titleVariant = guikit::UiVariant::Default;
    bool showSubtitle = true;
    guikit::UiVariant subtitleVariant = guikit::UiVariant::Secondary;
    guikit::ElementLayout layout = guikit::ElementLayout::Stacked;
    guikit::InlineOrder inlineOrder = guikit::InlineOrder::ControlFirst;
    guikit::Align elementAlign = guikit::Align::Start;
    size_t stretchIdx = 2; // Fill
    float width = 0.0f;
    float fillPct = 100.0f;
    bool showIcon = false;
    bool showBadge = false;
    guikit::UiVariant badgeVariant = guikit::UiVariant::Info;
    bool divider = true;
    guikit::HeadingDivider dividerStyle = guikit::HeadingDivider::Bottom;
};
HeadingState s_state;
} // namespace

void RenderHeadingInspector(guikit::Context& ui, DemoContext& ctx) {
    if (ui.StepSlider(ctx.headingSize, {.label = "Heading Size", .showTicks = true, .showLabels = true, .showValue = true}).changed) ui.InvalidateAll();
    if (ui.StepSlider(s_state.titleVariant, {
        .label = "Title (Label) Variant",
        .showTicks = true,
        .showLabels = true,
        .showValue = true
    }).changed) {
        ui.InvalidateAll();
    }
    if (ui.Switch(s_state.showSubtitle, {.label = "Show Subtitle (Description)"}).changed) ui.InvalidateAll();
    ctx.RenderDescVariantPicker(ui, s_state.subtitleVariant, s_state.showSubtitle);

    ctx.RenderElementLayoutControls(ui, true, s_state.layout, s_state.inlineOrder, s_state.elementAlign, s_state.stretchIdx);
    if (s_state.stretchIdx == 2) {
        if (ui.Slider(s_state.fillPct, 20.0f, 100.0f, {.label = "Explicit Width", .valueFormat = "%.0f %%", .step = 5.0f}).changed) ui.InvalidateAll();
    } else {
        if (ui.Slider(s_state.width, 0.0f, 500.0f, {.label = "Explicit Width", .valueFormat = {(s_state.width > 0.0f) ? "%.0f px" : "Auto", {"Auto", "%.0f px"}}, .step = 10.0f}).changed) ui.InvalidateAll();
    }

    if (ui.Switch(ctx.headingWordWrap, {.label = "Subtitle Word Wrap", .disabled = !s_state.showSubtitle}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showIcon, {.label = "Show Leading Icon"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showBadge, {.label = "Show Trailing Badge"}).changed) ui.InvalidateAll();
    if (ui.StepSlider(s_state.badgeVariant, {
        .label = "Badge Variant",
        .showTicks = true,
        .showLabels = true,
        .showValue = true,
        .disabled = !s_state.showBadge
    }).changed) {
        ui.InvalidateAll();
    }
    if (ui.Switch(s_state.divider, {.label = "Show Divider Line"}).changed) ui.InvalidateAll();
    static constexpr std::array<std::string_view, 2> divStyles = {"Bottom", "Trailing"};
    size_t divStyleIdx = (s_state.dividerStyle == guikit::HeadingDivider::Trailing) ? 1 : 0;
    if (ui.StepSlider(divStyleIdx, divStyles, {
        .label = "Divider Placement",
        .showTicks = true,
        .showLabels = true,
        .disabled = !s_state.divider
    }).changed) {
        s_state.dividerStyle = (divStyleIdx == 1) ? guikit::HeadingDivider::Trailing : guikit::HeadingDivider::Bottom;
        ui.InvalidateAll();
    }
}

void RenderHeadingStage(guikit::Context& ui, DemoContext& ctx) {
    ui.Heading({
        .title = {"A heading", s_state.titleVariant},
        .subtitle = s_state.showSubtitle
            ? guikit::UiText{"This is a heading", s_state.subtitleVariant}
            : guikit::UiText{},
        .icon = s_state.showIcon ? guikit::Icon::Edit() : guikit::Icon{},
        .badge = s_state.showBadge
            ? guikit::UiText{"v2.4", s_state.badgeVariant}
            : guikit::UiText{},
        .variant = s_state.titleVariant,
        .size = ctx.headingSize,
        .layout = s_state.layout,
        .inlineOrder = s_state.inlineOrder,
        .elementAlign = s_state.elementAlign,
        .stretch = static_cast<guikit::StretchMode>(s_state.stretchIdx),
        .width = (s_state.stretchIdx == 2) ? std::optional<float>(s_state.fillPct) : ((s_state.width > 0.0f) ? std::optional<float>(s_state.width) : std::nullopt),
        .divider = s_state.divider,
        .dividerStyle = s_state.dividerStyle,
        .wordWrap = ctx.headingWordWrap
    });
}

std::string GenerateHeadingCode(const DemoContext& ctx) {
    const std::string titleExpr = (s_state.titleVariant == guikit::UiVariant::Default)
        ? "\"A heading\""
        : std::format("{{\"A heading\", guikit::UiVariant::{}}}", VariantName(s_state.titleVariant));
    const std::string subExpr = !s_state.showSubtitle
        ? "\"\""
        : ((s_state.subtitleVariant == guikit::UiVariant::Secondary)
            ? "\"This is a heading\""
            : std::format("{{\"This is a heading\", guikit::UiVariant::{}}}", VariantName(s_state.subtitleVariant)));
    const std::string badgeExpr = !s_state.showBadge
        ? "\"\""
        : std::format("{{\"v2.4\", guikit::UiVariant::{}}}", VariantName(s_state.badgeVariant));
    return std::format(
        "ui.Heading({{\n"
        "    .title = {},\n"
        "    .subtitle = {},\n"
        "    .icon = {},\n"
        "    .badge = {},\n"
        "    .variant = guikit::UiVariant::{},\n"
        "    .size = guikit::UiSize::{},\n"
        "    .layout = guikit::ElementLayout::{},\n"
        "    .inlineOrder = guikit::InlineOrder::{},\n"
        "    .elementAlign = guikit::Align::{},\n"
        "    .stretch = guikit::StretchMode::{},\n"
        "    .width = {},\n"
        "    .divider = {},\n"
        "    .dividerStyle = guikit::HeadingDivider::{},\n"
        "    .wordWrap = {}\n"
        "}});",
        titleExpr,
        subExpr,
        s_state.showIcon ? "guikit::Icon::Edit()" : "{}",
        badgeExpr,
        VariantName(s_state.titleVariant),
        SizeName(ctx.headingSize),
        ElementLayoutEnumName(s_state.layout),
        InlineOrderEnumName(s_state.inlineOrder),
        LabelAlignEnumName(s_state.elementAlign),
        StretchModeEnumName(static_cast<guikit::StretchMode>(s_state.stretchIdx)),
        (s_state.stretchIdx == 2) ? std::format("{:.1f}f", s_state.fillPct) : ((s_state.width > 0.0f) ? std::format("{:.1f}f", s_state.width) : "std::nullopt"),
        s_state.divider ? "true" : "false",
        s_state.dividerStyle == guikit::HeadingDivider::Trailing ? "Trailing" : "Bottom",
        ctx.headingWordWrap ? "true" : "false"
    );
}

} // namespace guikit::demo
