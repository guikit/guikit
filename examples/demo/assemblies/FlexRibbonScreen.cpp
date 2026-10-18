#include "../DemoContext.hpp"

namespace guikit::demo {

namespace {
float s_carouselGap = 8.0f;
} // namespace

void RenderFlexRibbonInspector(guikit::Context& ui, DemoContext& ctx) {
    ctx.RenderAssemblyCommonInspector(ui);
    if (ui.Slider(s_carouselGap, 0.0f, 24.0f, {.label = "Ribbon Items Gap", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
}

void RenderFlexRibbonScreen(guikit::Context& ui, DemoContext& ctx) {
    if (auto card = ui.Card(ctx.MakeCard())) {
        ui.Heading({
            .title = "Dynamic 3-Way Equal Stretch Layout",
            .subtitle = "Each button automatically receives exactly 33.3% available width via stretch = StretchMode::Fill.",
            .size = ctx.headingSize,
            .wordWrap = ctx.headingWordWrap
        });

        if (auto stretchRow = ui.Row({.gap = ctx.rowGap})) {
            ui.Button({.label = "Col 1 (Stretch)", .stretch = guikit::StretchMode::Fill});
            ui.Button({.label = "Col 2 (Stretch)", .stretch = guikit::StretchMode::Fill});
            ui.Button({.label = "Col 3 (Stretch)", .stretch = guikit::StretchMode::Fill});
        }

        ui.Heading({
            .title = "Horizontal Carousel Ribbon",
            .subtitle = "Autonomous scroll container with navigation arrows and mouse wheel support.",
            .size = ctx.headingSize,
            .wordWrap = ctx.headingWordWrap
        });
        if (auto ribbon = ui.Carousel({.gap = s_carouselGap})) {
            for (int k = 1; k <= 10; ++k) {
                ui.Button({
                    .label = ui.Format("Module #{:02d}", k),
                    .color = (k % 3 == 0) ? guikit::StateColor::Primary() : guikit::StateColor::Default()
                });
            }
        }
    }
}

} // namespace guikit::demo
