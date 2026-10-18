#include "../DemoContext.hpp"

namespace guikit::demo {

namespace {
float s_carouselGap = 8.0f;
} // namespace

void RenderFlexRibbonInspector(guikit::Context& ui, DemoContext& ctx) {
    ctx.RenderAssemblyCommonInspector(ui);
    if (auto el = ui.Element({.label = "Ribbon Items Gap"})) {
        if (ui.Slider(s_carouselGap, 0.0f, 24.0f, {.valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
    }
}

void RenderFlexRibbonScreen(guikit::Context& ui, DemoContext& ctx) {
    if (auto card = ui.Card(ctx.MakeCard())) {
        ui.Size(ctx.headingSize).Heading({
            .label = "Dynamic 3-Way Equal Stretch Layout",
            .description = "Each button automatically receives exactly 33.3% available width via stretch = StretchMode::Fill."
        });

        if (auto stretchRow = ui.Row({.gap = ctx.rowGap})) {
            ui.Button({.text = "Col 1 (Stretch)", .stretch = guikit::StretchMode::Fill});
            ui.Button({.text = "Col 2 (Stretch)", .stretch = guikit::StretchMode::Fill});
            ui.Button({.text = "Col 3 (Stretch)", .stretch = guikit::StretchMode::Fill});
        }

        ui.Size(ctx.headingSize).Heading({
            .label = "Horizontal Carousel Ribbon",
            .description = "Autonomous scroll container with navigation arrows and mouse wheel support."
        });
        if (auto ribbon = ui.Carousel({.gap = s_carouselGap})) {
            for (int k = 1; k <= 10; ++k) {
                ui.Button({
                    .text = ui.Format("Module #{:02d}", k),
                    .color = (k % 3 == 0) ? guikit::StateColor::Primary() : guikit::StateColor::Default()
                });
            }
        }
    }
}

} // namespace guikit::demo
