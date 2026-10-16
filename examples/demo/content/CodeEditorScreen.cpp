#include "../DemoContext.hpp"
#include <algorithm>
#include <format>
#include <sstream>
#include <vector>

namespace guikit::demo {

namespace {

struct CodeEditorState {
    bool showLabel = false;
    bool showDesc = false;
    bool lineNumbers = true;
    bool copyButton = true;
    bool monospace = true;
    bool wordWrap = false;
    bool readOnly = false;
    bool allowZoom = true;
    bool showMinimap = true;
    bool showStatusBar = true;
    bool codeFolding = true;
    bool showIndentGuides = true;
    bool showDebugToolbar = true;
    bool showInlineValues = true;
    bool showHoverDataTips = true;
    bool showDiagnostics = true;
    bool showWhitespace = false;
    float zoom = 1.0f;

    size_t syntaxMode = 0; // 0 = C++20, 1 = MATLAB, 2 = JSON, 3 = Python, 4 = Custom
    static constexpr std::array<std::string_view, 5> syntaxLabels = {
        "C++20", "MATLAB", "JSON", "Python", "Custom"
    };
    size_t colorScheme = 0; // 0 = Theme Default, 1 = VS Code Dark+, 2 = Monokai, 3 = Dracula, 4 = One Dark, 5 = MATLAB, 6 = Solarized, 7 = GitHub Light
    static constexpr std::array<std::string_view, 8> colorSchemeLabels = {
        "Theme Default",
        "VS Code Dark+",
        "Monokai Pro",
        "Dracula",
        "One Dark Pro",
        "MATLAB Classic",
        "Solarized Dark",
        "GitHub Light"
    };
    static constexpr std::array<std::string_view, 5> customKeywords = {"rule", "when", "then", "emit", "alert"};
    static constexpr std::array<std::string_view, 4> customTypes = {"Sensor", "Signal", "Threshold", "Millis"};

    std::string editorBuffer;
    std::vector<size_t> breakpoints;
    size_t executionLine = 14;
    int stepCounter = 0;
    std::string lastDebugEvent = "Paused at breakpoint";

    std::vector<std::string> ownedStrings;
    std::vector<CodeDiagnostic> diagnostics;
    std::vector<CodeInlineValue> inlineValues;
    std::vector<CodeDebugVar> debugVariables;

    CodeEditorState() {
        LoadPreset(0);
    }

    std::string_view StoreString(std::string s) {
        ownedStrings.push_back(std::move(s));
        return ownedStrings.back();
    }

    void RebuildDynamicDebugData() {
        ownedStrings.clear();
        ownedStrings.reserve(32);
        diagnostics.clear();
        inlineValues.clear();
        debugVariables.clear();

        if (syntaxMode == 0) { // C++20
            if (showDiagnostics) {
                diagnostics.push_back({
                    .line = 18,
                    .startCol = 16,
                    .endCol = 28,
                    .severity = DiagnosticSeverity::Warning,
                    .message = "narrowing conversion from 'double' to 'float' [-Wnarrowing]",
                    .code = "C4244"
                });
                diagnostics.push_back({
                    .line = 23,
                    .startCol = 8,
                    .endCol = 20,
                    .severity = DiagnosticSeverity::Info,
                    .message = "Consider constexpr evaluation for filter normalization",
                    .code = "modernize-constexpr"
                });
            }
            if (showInlineValues && executionLine > 0) {
                const float gainVal = 1.85f + 0.05f * static_cast<float>(stepCounter % 5);
                const float rmsVal = 0.707f * gainVal;
                inlineValues.push_back({11, "sampleRate = 48000.0 Hz, nyquist = 24000.0 Hz", UiVariant::Info});
                inlineValues.push_back({14, StoreString(std::format("gain = {:.2f}, cutoffHz = 2400.0f", gainVal)), UiVariant::Success});
                if (executionLine >= 16) {
                    inlineValues.push_back({16, StoreString(std::format("k = {}, omega = {:.4f} rad", stepCounter % 4, 0.3142f * ((stepCounter % 4) + 1))), UiVariant::Info});
                }
                if (executionLine >= 18) {
                    inlineValues.push_back({18, StoreString(std::format("energyAcc = {:.4f}, rms = {:.3f}", rmsVal * 4.0f, rmsVal)), UiVariant::Warning});
                }
            }
            if (showHoverDataTips) {
                const float gainVal = 1.85f + 0.05f * static_cast<float>(stepCounter % 5);
                debugVariables = {
                    {"sampleRate", "48000.0", "double"},
                    {"cutoffHz", "2400.0f", "float"},
                    {"gain", StoreString(std::format("{:.3f}f", gainVal)), "float"},
                    {"energyAcc", StoreString(std::format("{:.5f}", 2.828f * gainVal)), "double"},
                    {"coeffs", "size=4 [0.25f, 0.50f, 0.25f, 0.0f]", "std::span<float>"},
                    {"nyquist", "24000.0", "const double"}
                };
            }
        } else if (syntaxMode == 1) { // MATLAB
            if (showDiagnostics) {
                diagnostics.push_back({
                    .line = 18,
                    .startCol = 8,
                    .endCol = 15,
                    .severity = DiagnosticSeverity::Warning,
                    .message = "Preallocate 'P_dB' before loop for faster MATLAB execution",
                    .code = "MLINT-AGROW"
                });
            }
            if (showInlineValues && executionLine > 0) {
                inlineValues.push_back({9, "Fs = 10000, N = 1024, f0 = 1250", UiVariant::Info});
                inlineValues.push_back({13, "w = 1024x1 double [hann window]", UiVariant::Success});
                if (executionLine >= 15) {
                    inlineValues.push_back({15, StoreString(std::format("H = 1024x1 complex, peakBin = {}", 128 + (stepCounter % 3))), UiVariant::Info});
                }
                if (executionLine >= 20) {
                    inlineValues.push_back({20, "snr_dB = +28.42 dB, peakFreq = 1250.0 Hz", UiVariant::Success});
                }
            }
            if (showHoverDataTips) {
                debugVariables = {
                    {"Fs", "10000", "double (1x1)"},
                    {"N", "1024", "double (1x1)"},
                    {"f0", "1250", "double (1x1)"},
                    {"x", "[0.000, 0.707, 1.000, 0.707, ...]", "double (1x1024)"},
                    {"w", "hann(1024) [0.000 .. 1.000]", "double (1024x1)"},
                    {"H", "max |H| = 512.0 + 0.0i", "complex double (1024x1)"},
                    {"P_dB", "[-42.1, -38.9, ..., 0.0 dB]", "double (1024x1)"},
                    {"snr_dB", "28.4215", "double (1x1)"}
                };
            }
        } else if (syntaxMode == 2) { // JSON
            if (showDiagnostics) {
                diagnostics.push_back({
                    .line = 15,
                    .startCol = 8,
                    .endCol = 20,
                    .severity = DiagnosticSeverity::Info,
                    .message = "Schema hint: maxThreads > 16 enables NUMA pool allocation",
                    .code = "JSON-SCHEMA"
                });
            }
            if (showInlineValues && executionLine > 0) {
                inlineValues.push_back({2, "schema = \"guikit.ide.v2\"", UiVariant::Info});
                inlineValues.push_back({15, "maxThreads = 32 (active)", UiVariant::Success});
            }
            if (showHoverDataTips) {
                debugVariables = {
                    {"engine", "{ name: \"GuiKit Code Editor\", version: \"2.4.0\" }", "object"},
                    {"maxThreads", "32", "number"},
                    {"simd", "\"AVX-512\"", "string"},
                    {"languages", "[\"C++20\", \"MATLAB\", \"JSON\", \"Python\"]", "array[4]"}
                };
            }
        } else if (syntaxMode == 3) { // Python
            if (showDiagnostics) {
                diagnostics.push_back({
                    .line = 12,
                    .startCol = 12,
                    .endCol = 24,
                    .severity = DiagnosticSeverity::Warning,
                    .message = "Type hint expected 'float', got 'np.float64'",
                    .code = "mypy-type"
                });
            }
            if (showInlineValues && executionLine > 0) {
                inlineValues.push_back({6, "epochs = 12, lr = 0.0015", UiVariant::Info});
                inlineValues.push_back({11, StoreString(std::format("idx = {}, loss = {:.5f}", (stepCounter % 12), 0.42f / ((stepCounter % 6) + 1))), UiVariant::Success});
            }
            if (showHoverDataTips) {
                debugVariables = {
                    {"epochs", "12", "int"},
                    {"lr", "0.0015", "float"},
                    {"loss", "0.08412", "float"},
                    {"weights", "device='cuda:0', dtype=float32", "Tensor[64, 64]"}
                };
            }
        }
    }

    void LoadPreset(size_t mode) {
        syntaxMode = mode;
        stepCounter = 0;
        if (mode == 0) { // C++20
            editorBuffer =
                "#include <cmath>\n"
                "#include <span>\n"
                "#include <numbers>\n\n"
                "// === DSP Biquad & Spectral Energy Kernel ===\n"
                "/* Multi-line block comment:\n"
                "   Try Ctrl+F (Find), Ctrl+H (Replace), Ctrl+G (Go to Line),\n"
                "   F9 (Breakpoint), F10 (Step Over), F5 (Continue), or click gutter chevrons! */\n"
                "namespace dsp::kernel {\n\n"
                "float ComputeBandEnergy(std::span<float> coeffs, double sampleRate) {\n"
                "    const double nyquist = sampleRate * 0.5;\n"
                "    float cutoffHz = 2400.0f;\n"
                "    float gain = 1.85f;\n"
                "    double energyAcc = 0.0;\n"
                "    for (size_t k = 0; k < coeffs.size(); ++k) {\n"
                "        const double omega = (2.0 * std::numbers::pi * cutoffHz) / nyquist;\n"
                "        float weighted = coeffs[k] * gain * std::cos(omega * double(k));\n"
                "        energyAcc += weighted * weighted;\n"
                "    }\n"
                "    if (energyAcc > 1.0) {\n"
                "        gain *= 0.95f;\n"
                "        return static_cast<float>(std::sqrt(energyAcc));\n"
                "    }\n"
                "    return 0.0f;\n"
                "}\n\n"
                "} // namespace dsp::kernel\n";
            breakpoints = {14, 18, 22};
            executionLine = 14;
            lastDebugEvent = "Paused at dsp::kernel::ComputeBandEnergy (Ln 14)";
        } else if (mode == 1) { // MATLAB
            editorBuffer =
                "%% MATLAB Spectral Beamforming & Welch PSD Analysis\n"
                "%{\n"
                "   Block comment: demonstrates MATLAB transpose (x') vs strings ('periodic'),\n"
                "   section headers (%%), and foldable function / for / if ... end blocks.\n"
                "%}\n"
                "function [P_dB, snr_dB] = analyzeSpectrum(x, Fs)\n"
                "    if nargin < 2\n"
                "        Fs = 10000;\n"
                "    end\n"
                "    N = length(x);\n"
                "    f0 = 1250;\n\n"
                "    %% Windowing & Hermitian Transpose Projection\n"
                "    w = hann(N, 'periodic');\n"
                "    x_win = (x(:) .* w)' * w; % Transpose x' vs string 'periodic'\n"
                "    H = fft(x(:) .* w, N);\n"
                "    for k = 1:N\n"
                "        magSq = abs(H(k)).^2 / (Fs * sum(w.^2));\n"
                "        P_dB(k) = 10 * log10(max(magSq, eps));\n"
                "    end\n"
                "    snr_dB = max(P_dB) - median(P_dB);\n"
                "    disp(['Estimated SNR: ', num2str(snr_dB), ' dB']);\n"
                "end\n";
            breakpoints = {13, 15, 20};
            executionLine = 13;
            lastDebugEvent = "Paused in analyzeSpectrum.m (Ln 13)";
        } else if (mode == 2) { // JSON
            editorBuffer =
                "{\n"
                "    \"schema\": \"guikit.ide.v2\",\n"
                "    \"engine\": {\n"
                "        \"name\": \"GuiKit Code Editor\",\n"
                "        \"version\": \"2.4.0\",\n"
                "        \"hardwareAccelerated\": true\n"
                "    },\n"
                "    \"languages\": [\n"
                "        \"C++20\",\n"
                "        \"MATLAB\",\n"
                "        \"JSON\",\n"
                "        \"Python\"\n"
                "    ],\n"
                "    \"debugger\": {\n"
                "        \"maxThreads\": 32,\n"
                "        \"simd\": \"AVX-512\",\n"
                "        \"inlineValues\": true,\n"
                "        \"hoverDataTips\": true\n"
                "    }\n"
                "}\n";
            breakpoints = {2, 15};
            executionLine = 15;
            lastDebugEvent = "Inspecting JSON configuration node (Ln 15)";
        } else if (mode == 3) { // Python
            editorBuffer =
                "# Python Neural Spectral Filter Trainer\n"
                "import math\n\n"
                "class SpectralTrainer:\n"
                "    def __init__(self, epochs: int = 12, lr: float = 0.0015):\n"
                "        self.epochs = epochs\n"
                "        self.lr = lr\n\n"
                "    def fit_epoch(self, weights: list[float]) -> float:\n"
                "        loss = 0.0\n"
                "        for idx, w in enumerate(weights):\n"
                "            grad = math.sin(w * (idx + 1)) * self.lr\n"
                "            loss += abs(grad) * 0.25\n"
                "        return round(loss, 5)\n";
            breakpoints = {10, 12};
            executionLine = 11;
            lastDebugEvent = "Paused in SpectralTrainer.fit_epoch (Ln 11)";
        } else { // Custom DSL
            editorBuffer =
                "# Custom Rule Engine DSL (Declarative SyntaxConfig)\n"
                "rule OverheatProtection {\n"
                "    when Sensor.temp > 85.5 then {\n"
                "        emit Signal(\"EMERGENCY_STOP\", 100);\n"
                "        alert Threshold(85.5);\n"
                "    }\n"
                "}\n";
            breakpoints = {3, 4};
            executionLine = 4;
            lastDebugEvent = "Rule engine triggered on line 4";
        }
        RebuildDynamicDebugData();
    }

    size_t CountLines() const {
        size_t count = 1;
        for (char c : editorBuffer) {
            if (c == '\n') ++count;
        }
        return count;
    }

    bool IsLineExecutable(size_t line1Based) const {
        std::istringstream iss(editorBuffer);
        std::string lineText;
        size_t cur = 1;
        while (std::getline(iss, lineText)) {
            if (cur == line1Based) {
                size_t first = lineText.find_first_not_of(" \t\r");
                if (first == std::string::npos) return false;
                std::string_view trimmed(lineText.data() + first, lineText.size() - first);
                if (trimmed.starts_with("//") || trimmed.starts_with("#") || trimmed.starts_with("%") ||
                    trimmed.starts_with("/*") || trimmed.starts_with("*") || trimmed == "{" || trimmed == "}") {
                    return false;
                }
                return true;
            }
            ++cur;
        }
        return false;
    }

    void HandleDebugAction(CodeDebugAction action) {
        const size_t totalLines = CountLines();
        if (action == CodeDebugAction::Stop) {
            executionLine = 0;
            lastDebugEvent = "Debugger stopped (Press F5 or Restart to begin)";
        } else if (action == CodeDebugAction::Restart) {
            stepCounter = 0;
            executionLine = !breakpoints.empty() ? breakpoints.front() : 1;
            lastDebugEvent = std::format("Debugger restarted at Ln {}", executionLine);
        } else if (action == CodeDebugAction::StepOver || action == CodeDebugAction::StepInto) {
            ++stepCounter;
            size_t nextLine = (executionLine == 0) ? 1 : (executionLine + 1);
            for (size_t tries = 0; tries < totalLines; ++tries) {
                if (nextLine > totalLines) nextLine = 1;
                if (IsLineExecutable(nextLine)) break;
                ++nextLine;
            }
            executionLine = nextLine;
            lastDebugEvent = std::format("Stepped to Ln {} (step #{})", executionLine, stepCounter);
        } else if (action == CodeDebugAction::StepOut) {
            ++stepCounter;
            executionLine = std::min(totalLines, std::max<size_t>(1, executionLine + 3));
            lastDebugEvent = std::format("Stepped out to Ln {}", executionLine);
        } else if (action == CodeDebugAction::Continue) {
            ++stepCounter;
            if (breakpoints.empty()) {
                executionLine = (executionLine == 0) ? 1 : 0;
                lastDebugEvent = (executionLine > 0) ? "Running — paused at entry" : "Program finished";
            } else {
                size_t foundBp = breakpoints.front();
                for (size_t bp : breakpoints) {
                    if (bp > executionLine) {
                        foundBp = bp;
                        break;
                    }
                }
                executionLine = foundBp;
                lastDebugEvent = std::format("Continued to breakpoint at Ln {}", executionLine);
            }
        }
        RebuildDynamicDebugData();
    }
};

CodeEditorState s_state;

} // namespace

void RenderCodeEditorInspector(guikit::Context& ui, DemoContext&) {
    if (ui.StepSlider(s_state.syntaxMode, CodeEditorState::syntaxLabels, {
        .label = "Language Preset",
        .showTicks = true,
        .showLabels = true
    }).changed) {
        s_state.LoadPreset(s_state.syntaxMode);
        ui.InvalidateAll();
    }

    if (ui.Select(s_state.colorScheme, CodeEditorState::colorSchemeLabels, {
        .label = "Syntax Color Scheme"
    }).changed) {
        ui.InvalidateAll();
    }

    if (ui.Switch(s_state.showWhitespace, {.label = "Show Hidden Whitespace"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showMinimap, {.label = "Show Code Minimap"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.codeFolding, {.label = "Code Folding (Gutter)"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showIndentGuides, {.label = "Show Indent Guides"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showStatusBar, {.label = "Show IDE Status Bar"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showDebugToolbar, {.label = "Floating Debug Toolbar"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showInlineValues, {.label = "Inline Debug Values"}).changed) {
        s_state.RebuildDynamicDebugData();
        ui.InvalidateAll();
    }
    if (ui.Switch(s_state.showHoverDataTips, {.label = "Hover Variable DataTips"}).changed) {
        s_state.RebuildDynamicDebugData();
        ui.InvalidateAll();
    }
    if (ui.Switch(s_state.showDiagnostics, {.label = "Diagnostic Squiggles"}).changed) {
        s_state.RebuildDynamicDebugData();
        ui.InvalidateAll();
    }
    if (ui.Switch(s_state.lineNumbers, {.label = "Line Numbers Gutter"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.wordWrap, {.label = "Word Wrap"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.readOnly, {.label = "Read-Only Mode"}).changed) ui.InvalidateAll();
    if (ui.Slider(s_state.zoom, 0.5f, 2.5f, {.label = "Editor Zoom", .valueFormat = "%.2fx"}).changed) ui.InvalidateAll();
}

void RenderCodeEditorStage(guikit::Context& ui, DemoContext& ctx) {
    guikit::SyntaxConfig synCfg = guikit::SyntaxConfig::Plain();
    if (s_state.syntaxMode == 0) {
        synCfg = guikit::SyntaxConfig::Cpp();
    } else if (s_state.syntaxMode == 1) {
        synCfg = guikit::SyntaxConfig::Matlab();
    } else if (s_state.syntaxMode == 2) {
        synCfg = guikit::SyntaxConfig::Json();
    } else if (s_state.syntaxMode == 3) {
        synCfg = guikit::SyntaxConfig::Python();
    } else {
        synCfg = guikit::SyntaxConfig{
            .enabled = true,
            .languageName = "RuleDSL",
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

    std::optional<guikit::SyntaxPalette> synPalette = std::nullopt;
    switch (s_state.colorScheme) {
        case 1: synPalette = guikit::SyntaxPalette::VsCodeDark(); break;
        case 2: synPalette = guikit::SyntaxPalette::Monokai(); break;
        case 3: synPalette = guikit::SyntaxPalette::Dracula(); break;
        case 4: synPalette = guikit::SyntaxPalette::OneDark(); break;
        case 5: synPalette = guikit::SyntaxPalette::MatlabClassic(); break;
        case 6: synPalette = guikit::SyntaxPalette::Solarized(); break;
        case 7: synPalette = guikit::SyntaxPalette::GitHubLight(); break;
        default: break;
    }

    auto resp = ui.TextArea(s_state.editorBuffer, {
        .label = s_state.showLabel ? "Code Editor & Debugger" : "",
        .description = s_state.showDesc ? std::string_view(s_state.lastDebugEvent) : std::string_view{},
        .autoHeight = false,
        .readOnly = s_state.readOnly,
        .showLineNumbers = s_state.lineNumbers,
        .showCopyButton = s_state.copyButton,
        .monospace = s_state.monospace,
        .wordWrap = s_state.wordWrap,
        .allowZoom = s_state.allowZoom,
        .zoom = s_state.zoom,
        .syntax = synCfg,
        .syntaxPalette = synPalette,
        .heightMode = guikit::SizeMode::Stretch,
        .size = ctx.controlsSize,
        .showMinimap = s_state.showMinimap,
        .showStatusBar = s_state.showStatusBar,
        .showIndentGuides = s_state.showIndentGuides,
        .showWhitespace = s_state.showWhitespace,
        .codeFolding = s_state.codeFolding,
        .autoCloseBrackets = true,
        .enableSearch = true,
        .highlightOccurrences = true,
        .matchBrackets = true,
        .enableBreakpoints = true,
        .breakpoints = &s_state.breakpoints,
        .executionLine = s_state.executionLine,
        .showDebugToolbar = s_state.showDebugToolbar,
        .diagnostics = s_state.diagnostics,
        .inlineValues = s_state.inlineValues,
        .debugVariables = s_state.debugVariables
    });

    if (resp.debugAction != guikit::CodeDebugAction::None &&
        resp.debugAction != guikit::CodeDebugAction::ToggleBreakpoint) {
        s_state.HandleDebugAction(resp.debugAction);
        ui.InvalidateAll();
    }
}

std::string GenerateCodeEditorCode(const DemoContext& ctx) {
    std::string synLine = "guikit::SyntaxConfig::Cpp()";
    if (s_state.syntaxMode == 1) synLine = "guikit::SyntaxConfig::Matlab()";
    else if (s_state.syntaxMode == 2) synLine = "guikit::SyntaxConfig::Json()";
    else if (s_state.syntaxMode == 3) synLine = "guikit::SyntaxConfig::Python()";
    else if (s_state.syntaxMode == 4) synLine = "customSyntaxConfig";

    std::string palLine = "std::nullopt";
    if (s_state.colorScheme == 1) palLine = "guikit::SyntaxPalette::VsCodeDark()";
    else if (s_state.colorScheme == 2) palLine = "guikit::SyntaxPalette::Monokai()";
    else if (s_state.colorScheme == 3) palLine = "guikit::SyntaxPalette::Dracula()";
    else if (s_state.colorScheme == 4) palLine = "guikit::SyntaxPalette::OneDark()";
    else if (s_state.colorScheme == 5) palLine = "guikit::SyntaxPalette::MatlabClassic()";
    else if (s_state.colorScheme == 6) palLine = "guikit::SyntaxPalette::Solarized()";
    else if (s_state.colorScheme == 7) palLine = "guikit::SyntaxPalette::GitHubLight()";

    return std::format(
        "auto resp = ui.TextArea(sourceBuffer, {{\n"
        "    .size = guikit::UiSize::{},\n"
        "    .showLineNumbers = {},\n"
        "    .showWhitespace = {},\n"
        "    .showMinimap = {},\n"
        "    .showStatusBar = {},\n"
        "    .codeFolding = {},\n"
        "    .showIndentGuides = {},\n"
        "    .showDebugToolbar = {},\n"
        "    .breakpoints = &breakpoints,\n"
        "    .executionLine = debugExecLine,\n"
        "    .diagnostics = diagnostics,\n"
        "    .inlineValues = inlineValues,\n"
        "    .debugVariables = debugVariables,\n"
        "    .syntax = {},\n"
        "    .syntaxPalette = {},\n"
        "    .heightMode = guikit::SizeMode::Stretch\n"
        "}});\n"
        "if (resp.debugAction == guikit::CodeDebugAction::StepOver) {{\n"
        "    StepDebugger();\n"
        "}}",
        SizeName(ctx.controlsSize),
        s_state.lineNumbers ? "true" : "false",
        s_state.showWhitespace ? "true" : "false",
        s_state.showMinimap ? "true" : "false",
        s_state.showStatusBar ? "true" : "false",
        s_state.codeFolding ? "true" : "false",
        s_state.showIndentGuides ? "true" : "false",
        s_state.showDebugToolbar ? "true" : "false",
        synLine,
        palLine
    );
}

} // namespace guikit::demo
