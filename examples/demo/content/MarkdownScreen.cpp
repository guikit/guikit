#include "../DemoContext.hpp"
#include <SDL3/SDL.h>
#include <format>

namespace guikit::demo {

namespace {
struct MarkdownState {
    bool codeLineNumbers = true;
    bool syntaxHighlight = true;
    bool showCopyButton = true;
    bool imageCheckerboard = true;
    float imageMaxHeight = 200.0f;
    float imageRadius = 8.0f;
    float blockGap = 12.0f;
    std::string lastClickedLink{};
};
MarkdownState s_state;

constexpr std::string_view kDemoMarkdownDoc =
R"(# GuiKit Rich Markdown Engine

Welcome to **GuiKit's** native vector `ui.Markdown` viewer! Everything is parsed into a lightweight cached AST and rendered at **60+ FPS** via *ThorVG* with crisp typography, **bold**, *italic*, ~~strikethrough~~, `inline code` pills, and interactive links like [https://github.com/guikit](https://github.com/guikit).

> [!TIP]
> You can embed vector **SVG**, **PNG**, or **JPG** diagrams directly inside Markdown, copy code snippets with one click, and inspect every layout box!

## 1. Architecture Diagram (`assets/images/architecture.svg`)

![GuiKit 4-Layer Architecture: Solver -> Scene -> ThorVG](assets/images/architecture.svg)

## 2. Interactive C++20 Code Block

```cpp
// Autonomous 2D World Camera & Rich Markdown
if (auto scene = ui.Scene({.worldSize = {1600.0f, 1000.0f}})) {
    scene.DrawImage("assets/images/circuit.svg", {0, 0, 800, 500});
    const guikit::Rect visible = scene.GetVisibleWorldRect();
}
```

## 3. Key Features & Task Checklist

- [x] **Pure BoxFit & Solver** with `aspectRatio`, `minHeight`, and 2D `ScrollArea`
- [x] **Stateless `ui.Image`** with `Contain`, `Cover`, `Fill`, `ScaleDown`, `Original`
- [x] **Stateful `ui.Scene`** with cursor-centric zoom, drag pan, and OSM tile culling
- [ ] Connect live HTTP tile loader for OpenStreetMap `[z/x/y]` tiles

| Component | Layer | Coordinate Space | Responsibility |
| :--- | :---: | :---: | :--- |
| `ComputeBoxFit` | Core | Pure Math | Aspect-ratio fitting |
| `ui.Image` | Widget | Screen Space | Stateless SVG/PNG/JPG leaf |
| `ui.Scene` | Panel | World <-> Screen | 2D Camera, Pan/Zoom & HUD |
| `ui.Markdown` | Widget | Flow Layout | Rich AST Document Viewer |

> [!WARNING]
> Hover over any `[link](...)` to preview its target URL tooltip, or click **Copy** on the code block above!
)";
} // namespace

void RenderMarkdownInspector(guikit::Context& ui, DemoContext&) {
    if (ui.Switch(s_state.syntaxHighlight, {.label = "C++ Syntax Highlighting"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.codeLineNumbers, {.label = "Code Line Numbers Gutter"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showCopyButton, {.label = "Interactive Copy Button"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.imageCheckerboard, {.label = "Embedded Image Checkerboard"}).changed) ui.InvalidateAll();
    if (ui.Slider(s_state.imageMaxHeight, 100.0f, 320.0f, {.label = "Image Max Height", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
    if (ui.Slider(s_state.imageRadius, 0.0f, 20.0f, {.label = "Image Corner Radius", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
    if (ui.Slider(s_state.blockGap, 4.0f, 24.0f, {.label = "Paragraph / Block Gap", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
    if (!s_state.lastClickedLink.empty()) {
        ui.Text(ui.Format("Clicked: {}", s_state.lastClickedLink), {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Info});
    }
}

void RenderMarkdownStage(guikit::Context& ui, DemoContext& ctx) {
    if (auto mdScroll = ui.ScrollArea({
        .padding = guikit::Padding::All(8.0f)
    })) {
        const auto mdRes = ui.Markdown(kDemoMarkdownDoc, {
            .codeLineNumbers = s_state.codeLineNumbers,
            .syntaxHighlight = s_state.syntaxHighlight,
            .showCopyButton = s_state.showCopyButton,
            .imageCheckerboard = s_state.imageCheckerboard,
            .imageMaxHeight = s_state.imageMaxHeight,
            .imageRadius = s_state.imageRadius,
            .blockGap = s_state.blockGap,
            .size = ctx.controlsSize
        });
        if (!mdRes.clickedLink.empty()) {
            s_state.lastClickedLink = std::string(mdRes.clickedLink);
            SDL_OpenURL(s_state.lastClickedLink.c_str());
        }
    }
}

std::string GenerateMarkdownCode(const DemoContext& ctx) {
    return std::format(
        "if (auto scroll = ui.ScrollArea({{.padding = guikit::Padding::All(8.0f)}})) {{\n"
        "    auto res = ui.Markdown(markdownSource, {{\n"
        "        .codeLineNumbers = {},\n"
        "        .syntaxHighlight = {},\n"
        "        .showCopyButton = {},\n"
        "        .imageCheckerboard = {},\n"
        "        .imageMaxHeight = {:.1f}f,\n"
        "        .imageRadius = {:.1f}f,\n"
        "        .blockGap = {:.1f}f,\n"
        "        .size = guikit::UiSize::{}\n"
        "    }});\n"
        "    if (!res.clickedLink.empty()) SDL_OpenURL(std::string(res.clickedLink).c_str());\n"
        "}}",
        s_state.codeLineNumbers ? "true" : "false",
        s_state.syntaxHighlight ? "true" : "false",
        s_state.showCopyButton ? "true" : "false",
        s_state.imageCheckerboard ? "true" : "false",
        s_state.imageMaxHeight,
        s_state.imageRadius,
        s_state.blockGap,
        SizeName(ctx.controlsSize)
    );
}

} // namespace guikit::demo
