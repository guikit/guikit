#include "../DemoContext.hpp"
#include <format>

namespace guikit::demo {

void RenderHeadingInspector(guikit::Context&, DemoContext&) {
}

void RenderHeadingStage(guikit::Context& ui, DemoContext& ctx) {
    ui.Heading({
        .title = !ctx.elemLabel.empty()
            ? guikit::UiText{ctx.elemLabel, ctx.btnVariant}
            : guikit::UiText{},
        .subtitle = ctx.GetElementDescription(),
        .icon = ctx.GetElementIcon(),
        .badge = ctx.GetElementBadge(),
        .variant = ctx.btnVariant,
        .size = ctx.controlsSize,
        .layout = ctx.elemLayout,
        .inlineOrder = ctx.elemInlineOrder,
        .elementAlign = ctx.elemAlign,
        .stretch = ctx.GetElementStretch(),
        .width = ctx.GetElementWidth(),
        .divider = ctx.elemDivider,
        .dividerStyle = ctx.elemDividerStyle,
        .wordWrap = ctx.elemWordWrap,
        .disabled = ctx.elemDisabled
    });
}

std::string GenerateHeadingCode(const DemoContext& ctx) {
    const std::string titleExpr = ctx.elemLabel.empty()
        ? "\"\""
        : ((ctx.btnVariant == guikit::UiVariant::Default)
            ? std::format("\"{}\"", ctx.elemLabel)
            : std::format("{{\"{}\", guikit::UiVariant::{}}}", ctx.elemLabel, VariantName(ctx.btnVariant)));
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
        "    .dividerStyle = guikit::ElementDivider::{},\n"
        "    .wordWrap = {},\n"
        "    .disabled = {}\n"
        "}});",
        titleExpr,
        ctx.FormatElementDescCode(),
        ctx.FormatElementIconCode(),
        ctx.FormatElementBadgeCode(),
        VariantName(ctx.btnVariant),
        SizeName(ctx.controlsSize),
        ElementLayoutEnumName(ctx.elemLayout),
        InlineOrderEnumName(ctx.elemInlineOrder),
        LabelAlignEnumName(ctx.elemAlign),
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.FormatElementWidthCode(),
        ctx.elemDivider ? "true" : "false",
        ctx.FormatDividerStyleCode(),
        ctx.elemWordWrap ? "true" : "false",
        ctx.elemDisabled ? "true" : "false"
    );
}

} // namespace guikit::demo
