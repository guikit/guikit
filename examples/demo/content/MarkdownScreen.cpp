#include "../DemoContext.hpp"
#include <SDL3/SDL.h>
#include <array>
#include <format>

namespace guikit::demo {

namespace {
struct MarkdownDemoState {
    // Zoom & Interactive
    float zoom = 1.0f;
    bool enableSelection = true;
    bool interactiveTasks = true;
    std::string searchQuery{};

    // Typography
    float bodyFontSize = 13.5f;
    float headingScale = 1.0f;
    float inlineCodeFontSize = 12.5f;
    float codeFontSize = 12.5f;
    float lineHeightScale = 1.15f;

    // Page Layout & Spacing
    float documentPadding = 8.0f;
    bool constrainContentWidth = false;
    float maxContentWidth = 680.0f;
    float blockGap = 12.0f;
    float paragraphIndent = 0.0f;
    float listIndent = 18.0f;
    float blockPadding = 12.0f;
    float blockRadius = 6.0f;
    bool headingDividers = true;

    // Code Block Styling (shared with CodeEditor)
    size_t codeSchemeIdx = 3; // Default to Dracula
    static constexpr std::array<std::string_view, 8> codeSchemeLabels = {
        "Theme Default",
        "VS Code Dark+",
        "Monokai Pro",
        "Dracula",
        "One Dark Pro",
        "MATLAB Classic",
        "Solarized Dark",
        "GitHub Light"
    };
    bool codeLineNumbers = true;
    bool syntaxHighlight = true;
    bool showCopyButton = true;
    bool codeShowWhitespace = false;
    bool codeIndentGuides = true;

    // Embedded Images
    bool imageCheckerboard = true;
    float imageMaxHeight = 200.0f;
    float imageRadius = 8.0f;

    // Status feedback
    std::string lastClickedLink{};
    int lastToggledTask = -1;
    bool lastToggledChecked = false;
    int searchMatches = 0;
};
MarkdownDemoState s_state;

std::optional<guikit::SyntaxPalette> ResolveCodePalette(size_t idx) noexcept {
    switch (idx) {
        case 1: return guikit::SyntaxPalette::VsCodeDark();
        case 2: return guikit::SyntaxPalette::Monokai();
        case 3: return guikit::SyntaxPalette::Dracula();
        case 4: return guikit::SyntaxPalette::OneDark();
        case 5: return guikit::SyntaxPalette::MatlabClassic();
        case 6: return guikit::SyntaxPalette::Solarized();
        case 7: return guikit::SyntaxPalette::GitHubLight();
        default: return std::nullopt;
    }
}

std::string_view CodePaletteExpr(size_t idx) noexcept {
    switch (idx) {
        case 1: return "guikit::SyntaxPalette::VsCodeDark()";
        case 2: return "guikit::SyntaxPalette::Monokai()";
        case 3: return "guikit::SyntaxPalette::Dracula()";
        case 4: return "guikit::SyntaxPalette::OneDark()";
        case 5: return "guikit::SyntaxPalette::MatlabClassic()";
        case 6: return "guikit::SyntaxPalette::Solarized()";
        case 7: return "guikit::SyntaxPalette::GitHubLight()";
        default: return "std::nullopt";
    }
}

constexpr std::string_view kDemoMarkdownDoc =
R"(# GuiKit Rich Markdown Engine

Welcome to **GuiKit's** native vector `ui.Markdown` viewer! Everything is parsed into a lightweight cached AST and rendered at **60+ FPS** via *ThorVG* with crisp typography, **bold**, *italic*, ~~strikethrough~~, `inline code` pills, and interactive links like [https://github.com/guikit](https://github.com/guikit).

> [!TIP]
> Hold **Ctrl + MouseWheel** (or press `Ctrl+=` / `Ctrl+-` / `Ctrl+0`) to smoothly zoom, click and drag to **select text** across blocks, press `Ctrl+C` to copy, or press `Ctrl+F` to search!

## 1. Architecture Diagram (`assets/images/architecture.svg`)

![GuiKit 4-Layer Architecture: Solver -> Scene -> ThorVG](assets/images/architecture.svg)

## 2. Multi-Language Code Blocks (Shared `CodeEditor` Highlighter)

```cpp
// C++20: Autonomous 2D World Camera & Rich Markdown
if (auto scene = ui.Scene({.worldSize = {1600.0f, 1000.0f}})) {
    scene.DrawImage("assets/images/circuit.svg", {0, 0, 800, 500});
    const guikit::Rect visible = scene.GetVisibleWorldRect();
}
```

```matlab
% MATLAB: Signal envelope & spectral peak analysis
function [peakFreq, snrDb] = analyzeSpectrum(x, fs)
    N = length(x);
    X = abs(fft(x .* hann(N))) / N;
    [peakVal, idx] = max(X(1:floor(N/2)));
    peakFreq = (idx - 1) * (fs / N);
    snrDb = 20 * log10(peakVal / ( mean(X) + eps ));
end
```

```json
{
  "widget": "ui.Markdown",
  "zoomRange": [0.5, 3.0],
  "sharedHighlighter": true,
  "features": ["Selection", "Copy", "Find", "TaskLists"]
}
```

## 3. Interactive Task Checklist (Click to Toggle!)

- [x] **Centralized `KeyAction` & `Keymap`** (`Copy`, `Find`, `SelectAll`, Arrow navigation)
- [x] **Zero-Duplication Code Blocks** reusing `SyntaxConfig::ForLanguage` & `SyntaxPalette`
- [x] **Smooth Zooming & Selection** across headings, paragraphs, tables, and code
- [ ] Try clicking this checkbox or customizing typography & spacing in the Inspector!

| Component | Layer | Coordinate Space | Responsibility |
| :--- | :---: | :---: | :--- |
| `Keymap` | Core | Input Dispatch | Configurable `KeyAction` bindings |
| `ui.TextArea` | Widget | Line Virtualized | IDE Code Editor & Syntax Engine |
| `ui.Markdown` | Widget | Flow Layout | Rich AST Document Viewer |

> [!WARNING]
> Hover over any `[link](...)` to preview its target URL tooltip, or click **Copy** on any code block above!
)";
} // namespace

void RenderMarkdownInspector(guikit::Context& ui, DemoContext&) {
    ui.Text("Zoom, Selection & Search", {.role = guikit::TextRole::LabelMedium, .variant = guikit::UiVariant::Primary});
    if (ui.Slider(s_state.zoom, 0.5f, 2.5f, {.label = "Document Zoom (Ctrl+Wheel)", .valueFormat = "%.2fx"}).changed) ui.InvalidateAll();
    if (ui.InputField(s_state.searchQuery, {.label = "Highlight Search Query (Ctrl+F)", .placeholder = "Type to highlight matches..."}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.enableSelection, {.label = "Text Selection & Cursor (Ctrl+C)"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.interactiveTasks, {.label = "Interactive Task Checkboxes"}).changed) ui.InvalidateAll();

    ui.Text("Typography & Font Sizing", {.role = guikit::TextRole::LabelMedium, .variant = guikit::UiVariant::Primary});
    if (ui.Slider(s_state.bodyFontSize, 10.0f, 22.0f, {.label = "Body Font Size", .valueFormat = "%.1f pt"}).changed) ui.InvalidateAll();
    if (ui.Slider(s_state.headingScale, 0.7f, 1.6f, {.label = "Heading Scale (H1..H6)", .valueFormat = "%.2fx"}).changed) ui.InvalidateAll();
    if (ui.Slider(s_state.inlineCodeFontSize, 9.0f, 20.0f, {.label = "Inline Code Font Size (`code`)", .valueFormat = "%.1f pt"}).changed) ui.InvalidateAll();
    if (ui.Slider(s_state.codeFontSize, 9.0f, 20.0f, {.label = "Display Code Font Size (```)", .valueFormat = "%.1f pt"}).changed) ui.InvalidateAll();
    if (ui.Slider(s_state.lineHeightScale, 0.9f, 1.8f, {.label = "Line Height Multiplier", .valueFormat = "%.2fx"}).changed) ui.InvalidateAll();

    ui.Text("Page Layout, Indents & Spacing", {.role = guikit::TextRole::LabelMedium, .variant = guikit::UiVariant::Primary});
    if (ui.Slider(s_state.documentPadding, 0.0f, 40.0f, {.label = "Document Padding", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.constrainContentWidth, {.label = "Constrain Reading Column Width"}).changed) ui.InvalidateAll();
    if (s_state.constrainContentWidth) {
        if (ui.Slider(s_state.maxContentWidth, 420.0f, 960.0f, {.label = "Max Content Width", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
    }
    if (ui.Slider(s_state.blockGap, 4.0f, 28.0f, {.label = "Block Vertical Gap", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
    if (ui.Slider(s_state.paragraphIndent, 0.0f, 36.0f, {.label = "Paragraph First-Line Indent", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
    if (ui.Slider(s_state.listIndent, 10.0f, 36.0f, {.label = "List Level Indent", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
    if (ui.Slider(s_state.blockPadding, 6.0f, 24.0f, {.label = "Callout / Code Inner Padding", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
    if (ui.Slider(s_state.blockRadius, 0.0f, 16.0f, {.label = "Block Corner Radius", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.headingDividers, {.label = "H1 / H2 Divider Lines"}).changed) ui.InvalidateAll();

    ui.Text("Code Blocks (Shared CodeEditor Engine)", {.role = guikit::TextRole::LabelMedium, .variant = guikit::UiVariant::Primary});
    if (ui.Select(s_state.codeSchemeIdx, MarkdownDemoState::codeSchemeLabels, {.label = "Code Color Scheme"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.syntaxHighlight, {.label = "Multi-Language Syntax Highlight"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.codeLineNumbers, {.label = "Line Numbers Gutter"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.codeShowWhitespace, {.label = "Show Hidden Whitespace (Space/Tab)"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.codeIndentGuides, {.label = "Show Indentation Guides"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showCopyButton, {.label = "Interactive Copy Button"}).changed) ui.InvalidateAll();

    ui.Text("Embedded Images", {.role = guikit::TextRole::LabelMedium, .variant = guikit::UiVariant::Primary});
    if (ui.Switch(s_state.imageCheckerboard, {.label = "Embedded Image Checkerboard"}).changed) ui.InvalidateAll();
    if (ui.Slider(s_state.imageMaxHeight, 100.0f, 320.0f, {.label = "Image Max Height", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
    if (ui.Slider(s_state.imageRadius, 0.0f, 20.0f, {.label = "Image Corner Radius", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();

    if (!s_state.lastClickedLink.empty()) {
        ui.Text(ui.Format("Clicked link: {}", s_state.lastClickedLink), {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Info});
    }
    if (s_state.lastToggledTask >= 0) {
        ui.Text(
            ui.Format("Task #{} -> {}", s_state.lastToggledTask + 1, s_state.lastToggledChecked ? "Checked" : "Unchecked"),
            {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Success}
        );
    }
}

void RenderMarkdownStage(guikit::Context& ui, DemoContext& ctx) {
    if (auto mdScroll = ui.ScrollArea({
        .padding = guikit::Padding::All(8.0f)
    })) {
        const auto mdRes = ui.Markdown(kDemoMarkdownDoc, {
            .allowZoom = true,
            .zoom = &s_state.zoom,
            .bodyFontSize = s_state.bodyFontSize,
            .headingScale = s_state.headingScale,
            .inlineCodeFontSize = s_state.inlineCodeFontSize,
            .codeFontSize = s_state.codeFontSize,
            .lineHeightScale = s_state.lineHeightScale,
            .documentPadding = s_state.documentPadding,
            .maxContentWidth = s_state.constrainContentWidth ? std::optional<float>(s_state.maxContentWidth) : std::nullopt,
            .blockGap = s_state.blockGap,
            .paragraphIndent = s_state.paragraphIndent,
            .listIndent = s_state.listIndent,
            .blockPadding = s_state.blockPadding,
            .blockRadius = s_state.blockRadius,
            .headingDividers = s_state.headingDividers,
            .codeLineNumbers = s_state.codeLineNumbers,
            .syntaxHighlight = s_state.syntaxHighlight,
            .showCopyButton = s_state.showCopyButton,
            .codeShowWhitespace = s_state.codeShowWhitespace,
            .codeIndentGuides = s_state.codeIndentGuides,
            .codePalette = ResolveCodePalette(s_state.codeSchemeIdx),
            .imageCheckerboard = s_state.imageCheckerboard,
            .imageMaxHeight = s_state.imageMaxHeight,
            .imageRadius = s_state.imageRadius,
            .enableSelection = s_state.enableSelection,
            .enableSearch = true,
            .interactiveTasks = s_state.interactiveTasks,
            .searchQuery = s_state.searchQuery,
            .size = ctx.controlsSize
        });
        s_state.searchMatches = mdRes.searchMatchCount;
        if (!mdRes.clickedLink.empty()) {
            s_state.lastClickedLink = std::string(mdRes.clickedLink);
            SDL_OpenURL(s_state.lastClickedLink.c_str());
        }
        if (mdRes.toggledTaskIndex >= 0) {
            s_state.lastToggledTask = mdRes.toggledTaskIndex;
            s_state.lastToggledChecked = mdRes.toggledTaskChecked;
        }
    }
}

std::string GenerateMarkdownCode(const DemoContext& ctx) {
    const std::string maxColStr = s_state.constrainContentWidth
        ? std::format("{:.0f}f", s_state.maxContentWidth)
        : "std::nullopt";

    return std::format(
        "if (auto scroll = ui.ScrollArea({{.padding = guikit::Padding::All(8.0f)}})) {{\n"
        "    auto res = ui.Markdown(markdownSource, {{\n"
        "        .allowZoom = true,\n"
        "        .zoom = &zoomFactor, // {:.2f}x\n"
        "        .bodyFontSize = {:.1f}f,\n"
        "        .headingScale = {:.2f}f,\n"
        "        .inlineCodeFontSize = {:.1f}f,\n"
        "        .codeFontSize = {:.1f}f,\n"
        "        .lineHeightScale = {:.2f}f,\n"
        "        .documentPadding = {:.0f}f,\n"
        "        .maxContentWidth = {},\n"
        "        .blockGap = {:.0f}f,\n"
        "        .paragraphIndent = {:.0f}f,\n"
        "        .listIndent = {:.0f}f,\n"
        "        .blockPadding = {:.0f}f,\n"
        "        .blockRadius = {:.0f}f,\n"
        "        .headingDividers = {},\n"
        "        .codeLineNumbers = {},\n"
        "        .syntaxHighlight = {},\n"
        "        .showCopyButton = {},\n"
        "        .codeShowWhitespace = {},\n"
        "        .codeIndentGuides = {},\n"
        "        .codePalette = {},\n"
        "        .imageCheckerboard = {},\n"
        "        .imageMaxHeight = {:.0f}f,\n"
        "        .imageRadius = {:.0f}f,\n"
        "        .enableSelection = {},\n"
        "        .enableSearch = true,\n"
        "        .interactiveTasks = {},\n"
        "        .searchQuery = \"{}\",\n"
        "        .size = guikit::UiSize::{}\n"
        "    }});\n"
        "    if (!res.clickedLink.empty()) SDL_OpenURL(std::string(res.clickedLink).c_str());\n"
        "}}",
        s_state.zoom,
        s_state.bodyFontSize,
        s_state.headingScale,
        s_state.inlineCodeFontSize,
        s_state.codeFontSize,
        s_state.lineHeightScale,
        s_state.documentPadding,
        maxColStr,
        s_state.blockGap,
        s_state.paragraphIndent,
        s_state.listIndent,
        s_state.blockPadding,
        s_state.blockRadius,
        s_state.headingDividers ? "true" : "false",
        s_state.codeLineNumbers ? "true" : "false",
        s_state.syntaxHighlight ? "true" : "false",
        s_state.showCopyButton ? "true" : "false",
        s_state.codeShowWhitespace ? "true" : "false",
        s_state.codeIndentGuides ? "true" : "false",
        CodePaletteExpr(s_state.codeSchemeIdx),
        s_state.imageCheckerboard ? "true" : "false",
        s_state.imageMaxHeight,
        s_state.imageRadius,
        s_state.enableSelection ? "true" : "false",
        s_state.interactiveTasks ? "true" : "false",
        s_state.searchQuery,
        SizeName(ctx.controlsSize)
    );
}

} // namespace guikit::demo
