#include "../DemoContext.hpp"
#include <format>

namespace guikit::demo {

namespace {
struct CodeEditorState {
    bool showLabel = true;
    bool showDesc = true;
    bool lineNumbers = true;
    bool copyButton = true;
    bool monospace = true;
    bool wordWrap = false;
    bool readOnly = false;
    bool allowZoom = true;
    float zoom = 1.0f;
    size_t syntaxMode = 0; // 0 = C++20, 1 = JSON, 2 = Custom DSL, 3 = Plain
    static constexpr std::array<std::string_view, 4> syntaxLabels = {"C++20", "JSON", "Custom", "Plain"};
    static constexpr std::array<std::string_view, 5> customKeywords = {"rule", "when", "then", "emit", "alert"};
    static constexpr std::array<std::string_view, 4> customTypes = {"Sensor", "Signal", "Threshold", "Millis"};
    std::string editorBuffer =
        "#include \"guikit/GuiKit.hpp\"\n\n"
        "// Interactive Code Editor (Click, Select, Edit, Ctrl+Wheel Zoom)\n"
        "void RenderEditorDemo(guikit::Context& ui, std::string& sourceCode) {\n"
        "    const auto syntax = guikit::SyntaxConfig::Cpp();\n"
        "    ui.TextArea(sourceCode, {\n"
        "        .label = \"A text area\",\n"
        "        .description = \"This is a text area\",\n"
        "        .showLineNumbers = true,\n"
        "        .showCopyButton = true,\n"
        "        .allowZoom = true,\n"
        "        .syntax = syntax\n"
        "    });\n"
        "}\n";
};
CodeEditorState s_state;
} // namespace

void RenderCodeEditorInspector(guikit::Context& ui, DemoContext&) {
    if (ui.Switch(s_state.showLabel, {.label = "Show Element Label"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showDesc, {.label = "Show Description"}).changed) ui.InvalidateAll();
    if (ui.StepSlider(s_state.syntaxMode, CodeEditorState::syntaxLabels, {.label = "Syntax Preset", .showTicks = true, .showLabels = true}).changed) {
        if (s_state.syntaxMode == 0) {
            s_state.editorBuffer =
                "#include \"guikit/GuiKit.hpp\"\n\n"
                "// Interactive Code Editor (Click, Select, Edit, Ctrl+Wheel Zoom)\n"
                "void RenderEditorDemo(guikit::Context& ui, std::string& sourceCode) {\n"
                "    const auto syntax = guikit::SyntaxConfig::Cpp();\n"
                "    ui.TextArea(sourceCode, {\n"
                "        .label = \"A text area\",\n"
                "        .description = \"This is a text area\",\n"
                "        .showLineNumbers = true,\n"
                "        .showCopyButton = true,\n"
                "        .allowZoom = true,\n"
                "        .syntax = syntax\n"
                "    });\n"
                "}\n";
        } else if (s_state.syntaxMode == 1) {
            s_state.editorBuffer =
                "{\n"
                "    \"widget\": \"TextArea\",\n"
                "    \"monospace\": true,\n"
                "    \"zoomRange\": [0.5, 3.0],\n"
                "    \"features\": [\n"
                "        \"2D Cursor & Selection\",\n"
                "        \"Configurable Syntax\",\n"
                "        \"Ctrl+Wheel Zoom\"\n"
                "    ],\n"
                "    \"readOnly\": false\n"
                "}\n";
        } else if (s_state.syntaxMode == 2) {
            s_state.editorBuffer =
                "# Custom Rule Engine DSL (Keywords, Types & Comments)\n"
                "rule OverheatProtection {\n"
                "    when Sensor.temp > 85.5 then {\n"
                "        emit Signal(\"EMERGENCY_STOP\", 100);\n"
                "        alert Threshold(85.5);\n"
                "    }\n"
                "}\n";
        } else {
            s_state.editorBuffer =
                "Plain text mode without syntax highlighting.\n"
                "You can click anywhere to place the cursor,\n"
                "drag or Shift+Arrow to select across multiple lines,\n"
                "and hold Ctrl + Mouse Wheel to zoom in and out!\n";
        }
        ui.InvalidateAll();
    }
    if (ui.Switch(s_state.lineNumbers, {.label = "Line Numbers Gutter"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.copyButton, {.label = "Show Copy Button"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.monospace, {.label = "Monospace Font"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.wordWrap, {.label = "Word Wrap"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.readOnly, {.label = "Read-Only Mode"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.allowZoom, {.label = "Allow Ctrl + Wheel Zoom"}).changed) ui.InvalidateAll();
    if (ui.Slider(s_state.zoom, 0.5f, 2.5f, {.label = "Editor Zoom", .valueFormat = "%.2fx"}).changed) ui.InvalidateAll();
}

void RenderCodeEditorStage(guikit::Context& ui, DemoContext& ctx) {
    guikit::SyntaxConfig synCfg = guikit::SyntaxConfig::Plain();
    if (s_state.syntaxMode == 0) {
        synCfg = guikit::SyntaxConfig::Cpp();
    } else if (s_state.syntaxMode == 1) {
        synCfg = guikit::SyntaxConfig::Json();
    } else if (s_state.syntaxMode == 2) {
        synCfg = guikit::SyntaxConfig{
            .enabled = true,
            .keywords = CodeEditorState::customKeywords,
            .types = CodeEditorState::customTypes,
            .lineComment = "#",
            .blockCommentStart = "/*",
            .blockCommentEnd = "*/",
            .preprocessorPrefix = '\0',
            .highlightStrings = true,
            .highlightNumbers = true,
            .highlightFunctions = true,
            .highlightPunctuation = true
        };
    }
    ui.TextArea(s_state.editorBuffer, {
        .label = s_state.showLabel ? "A text area" : "",
        .description = s_state.showDesc ? "This is a text area" : "",
        .autoHeight = false,
        .readOnly = s_state.readOnly,
        .showLineNumbers = s_state.lineNumbers,
        .showCopyButton = s_state.copyButton,
        .monospace = s_state.monospace,
        .wordWrap = s_state.wordWrap,
        .allowZoom = s_state.allowZoom,
        .zoom = s_state.zoom,
        .syntax = synCfg,
        .heightMode = guikit::SizeMode::Stretch,
        .size = ctx.controlsSize
    });
}

std::string GenerateCodeEditorCode(const DemoContext& ctx) {
    std::string synLine = "guikit::SyntaxConfig::Plain()";
    if (s_state.syntaxMode == 0) synLine = "guikit::SyntaxConfig::Cpp()";
    else if (s_state.syntaxMode == 1) synLine = "guikit::SyntaxConfig::Json()";
    else if (s_state.syntaxMode == 2) synLine = "customSyntaxConfig";

    std::string snippet = "ui.TextArea(sourceBuffer, {\n";
    if (s_state.showLabel) snippet += "    .label = \"A text area\",\n";
    if (s_state.showDesc) snippet += "    .description = \"This is a text area\",\n";
    snippet += std::format(
        "    .size = guikit::UiSize::{},\n"
        "    .wordWrap = {},\n"
        "    .readOnly = {},\n"
        "    .showLineNumbers = {},\n"
        "    .showCopyButton = {},\n"
        "    .monospace = {},\n"
        "    .allowZoom = {},\n"
        "    .zoom = {:.2f}f,\n"
        "    .syntax = {},\n"
        "    .heightMode = guikit::SizeMode::Stretch\n"
        "}});",
        SizeName(ctx.controlsSize),
        s_state.wordWrap ? "true" : "false",
        s_state.readOnly ? "true" : "false",
        s_state.lineNumbers ? "true" : "false",
        s_state.copyButton ? "true" : "false",
        s_state.monospace ? "true" : "false",
        s_state.allowZoom ? "true" : "false",
        s_state.zoom,
        synLine
    );
    return snippet;
}

} // namespace guikit::demo
