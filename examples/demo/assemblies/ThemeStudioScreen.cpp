#include "../DemoContext.hpp"

namespace guikit::demo {

void RenderThemeStudioInspector(guikit::Context& ui, DemoContext& ctx) {
    ctx.RenderAssemblyCommonInspector(ui);
}

void RenderThemeStudioScreen(guikit::Context& ui, DemoContext& ctx) {
    auto& th = ctx.previewTheme;
    if (auto card = ui.Card(ctx.MakeCard())) {
        ui.Size(ctx.headingSize).Heading({
            .label = "Theme & Palette Studio",
            .description = "Live token tweaking and contrast validation"
        });

        ui.Size(guikit::UiSize::Sm).Heading({.label = "Quick Accent Color Presets"});
        if (auto accentRow = ui.Wrap({.gap = 8.0f, .runGap = 8.0f})) {
            auto makePreset = [&](std::string_view name, guikit::Color col) {
                if (ui.Size(guikit::UiSize::Sm).Button({.text = name})) {
                    th.palette.accent = col;
                    th.GetVariant(guikit::UiVariant::Primary).bg = col;
                    th.palette.focusRing = col;
                    ui.InvalidateAll();
                }
            };
            makePreset("Cyan", guikit::Color(6, 182, 212));
            makePreset("Blue", guikit::Color(59, 130, 246));
            makePreset("Purple", guikit::Color(168, 85, 247));
            makePreset("Emerald", guikit::Color(16, 185, 129));
            makePreset("Rose", guikit::Color(244, 63, 94));
            makePreset("Amber", guikit::Color(245, 158, 11));
        }

        ui.Size(guikit::UiSize::Sm).Heading({.label = "Base Palette Tokens"});
        auto makeColorSliders = [&](std::string_view title, guikit::Color& c) {
            float r = c.r, g = c.g, b = c.b;
            bool changed = false;
            ui.Text(title, {.role = guikit::TextRole::LabelMedium});
            if (auto g_row = ui.Grid({.gap = 8.0f})) {
                if (auto c1 = g_row.Cell4()) {
                    if (auto el = ui.Element({.label = "R"})) {
                        if (ui.Slider(r, 0.0f, 255.0f, {.valueFormat = "%.0f"}).changed) changed = true;
                    }
                }
                if (auto c2 = g_row.Cell4()) {
                    if (auto el = ui.Element({.label = "G"})) {
                        if (ui.Slider(g, 0.0f, 255.0f, {.valueFormat = "%.0f"}).changed) changed = true;
                    }
                }
                if (auto c3 = g_row.Cell4()) {
                    if (auto el = ui.Element({.label = "B"})) {
                        if (ui.Slider(b, 0.0f, 255.0f, {.valueFormat = "%.0f"}).changed) changed = true;
                    }
                }
            }
            if (changed) {
                c = guikit::Color(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b), c.a);
                ui.InvalidateAll();
            }
        };

        makeColorSliders("Primary Accent Fill", th.palette.accent);
        makeColorSliders("Surface Color", th.palette.surface);
        makeColorSliders("Background Color", th.palette.background);
    }
}

} // namespace guikit::demo
