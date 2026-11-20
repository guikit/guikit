#include "../demo_context.hpp"

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

    keel::usize syntaxMode = 0; // 0 = C++20, 1 = MATLAB, 2 = JSON, 3 = Python, 4 = Custom
    static constexpr keel::StaticArray<keel::StringRef, 5> syntaxLabels = {
        "C++20", "MATLAB", "JSON", "Python", "Custom"
    };
    keel::usize colorScheme = 0; // 0 = Theme Default, 1 = VS Code Dark+, 2 = Monokai, 3 = Dracula, 4 = One Dark, 5 = MATLAB, 6 = Solarized, 7 = GitHub Light
    static constexpr keel::StaticArray<keel::StringRef, 8> colorSchemeLabels = {
        "Theme Default",
        "VS Code Dark+",
        "Monokai Pro",
        "Dracula",
        "One Dark Pro",
        "MATLAB Classic",
        "Solarized Dark",
        "GitHub Light"
    };
    static constexpr keel::StaticArray<keel::StringRef, 5> customKeywords = {"rule", "when", "then", "emit", "alert"};
    static constexpr keel::StaticArray<keel::StringRef, 4> customTypes = {"Sensor", "Signal", "Threshold", "Millis"};
    keel::usize debugToolbarAlignIdx = 1; // 0 = Left (Start), 1 = Center, 2 = Right (End)
    static constexpr keel::StaticArray<keel::StringRef, 3> debugToolbarAlignLabels = {
        "Left", "Center", "Right"
    };

    keel::String editorBuffer{};
    keel::Array<keel::usize> breakpoints{};
    keel::usize executionLine = 14;
    keel::i32 stepCounter = 0;
    keel::FixedString<128> lastDebugEvent = "Paused at breakpoint";

    keel::Array<keel::String> ownedStrings{};
    keel::Array<CodeDiagnostic> diagnostics{};
    keel::Array<CodeInlineValue> inlineValues{};
    keel::Array<CodeDebugVar> debugVariables{};

    CodeEditorState() {
        LoadPreset(0);
    }

    keel::StringRef StoreString(keel::String s) {
        ownedStrings.push_back(keel::move(s));
        return ownedStrings.back().as_ref();
    }

    keel::usize FindLineContaining(keel::StringRef needle, keel::usize fallbackLine) const {
        if (needle.empty()) return fallbackLine;
        const keel::StringRef buf = editorBuffer.as_ref();
        keel::usize lineNum = 1;
        keel::usize pos = 0;
        while (pos <= buf.size()) {
            const keel::usize nextNl = buf.find('\n', pos);
            const keel::usize len = (nextNl == keel::StringRef::npos) ? (buf.size() - pos) : (nextNl - pos);
            const keel::StringRef lineView = buf.substr(pos, len);
            if (lineView.contains(needle)) {
                return lineNum;
            }
            if (nextNl == keel::StringRef::npos) break;
            pos = nextNl + 1;
            ++lineNum;
        }
        return fallbackLine;
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
                    .code = "C4244",
                    .anchor = "coeffs[k] * gain"
                });
                diagnostics.push_back({
                    .line = 23,
                    .startCol = 8,
                    .endCol = 20,
                    .severity = DiagnosticSeverity::Info,
                    .message = "Consider constexpr evaluation for filter normalization",
                    .code = "modernize-constexpr",
                    .anchor = "static_cast<float>"
                });
            }
            if (showInlineValues && executionLine > 0) {
                const float gainVal = 1.85f + 0.05f * static_cast<float>(stepCounter % 5);
                const float rmsVal = 0.707f * gainVal;
                const keel::usize loopLine = FindLineContaining("for (size_t k", 16);
                const keel::usize accLine = FindLineContaining("float weighted", 18);
                inlineValues.push_back({11, "sampleRate = 48000.0 Hz, nyquist = 24000.0 Hz", UiVariant::Info, "nyquist"});
                inlineValues.push_back({14, StoreString(keel::format("gain = {:.2f}, cutoffHz = 2400.0f", gainVal)), UiVariant::Success, "energyAcc = 0.0"});
                if (executionLine >= loopLine) {
                    inlineValues.push_back({16, StoreString(keel::format("k = {}, omega = {:.4f} rad", stepCounter % 4, 0.3142f * ((stepCounter % 4) + 1))), UiVariant::Info, "for (size_t k"});
                }
                if (executionLine >= accLine) {
                    inlineValues.push_back({18, StoreString(keel::format("energyAcc = {:.4f}, rms = {:.3f}", rmsVal * 4.0f, rmsVal)), UiVariant::Warning, "float weighted"});
                }
            }
            if (showHoverDataTips) {
                const float gainVal = 1.85f + 0.05f * static_cast<float>(stepCounter % 5);
                debugVariables.assign({
                    {"sampleRate", "48000.0", "double"},
                    {"cutoffHz", "2400.0f", "float"},
                    {"gain", StoreString(keel::format("{:.3f}f", gainVal)), "float"},
                    {"energyAcc", StoreString(keel::format("{:.5f}", 2.828f * gainVal)), "double"},
                    {"coeffs", "size=4 [0.25f, 0.50f, 0.25f, 0.0f]", "keel::ArrayRef<float>"},
                    {"nyquist", "24000.0", "const double"}
                });
            }
        } else if (syntaxMode == 1) { // MATLAB
            if (showDiagnostics) {
                diagnostics.push_back({
                    .line = 18,
                    .startCol = 8,
                    .endCol = 15,
                    .severity = DiagnosticSeverity::Warning,
                    .message = "Preallocate 'P_dB' before loop for faster MATLAB execution",
                    .code = "MLINT-AGROW",
                    .anchor = "P_dB(k)"
                });
            }
            if (showInlineValues && executionLine > 0) {
                const keel::usize fftLine = FindLineContaining("H = fft(", 15);
                const keel::usize snrLine = FindLineContaining("snr_dB = max(", 20);
                inlineValues.push_back({9, "Fs = 10000, N = 1024, f0 = 1250", UiVariant::Info, "N = length(x)"});
                inlineValues.push_back({13, "w = 1024x1 double [hann window]", UiVariant::Success, "w = hann("});
                if (executionLine >= fftLine) {
                    inlineValues.push_back({15, StoreString(keel::format("H = 1024x1 complex, peakBin = {}", 128 + (stepCounter % 3))), UiVariant::Info, "H = fft("});
                }
                if (executionLine >= snrLine) {
                    inlineValues.push_back({20, "snr_dB = +28.42 dB, peakFreq = 1250.0 Hz", UiVariant::Success, "snr_dB = max("});
                }
            }
            if (showHoverDataTips) {
                debugVariables.assign({
                    {"Fs", "10000", "double (1x1)"},
                    {"N", "1024", "double (1x1)"},
                    {"f0", "1250", "double (1x1)"},
                    {"x", "[0.000, 0.707, 1.000, 0.707, ...]", "double (1x1024)"},
                    {"w", "hann(1024) [0.000 .. 1.000]", "double (1024x1)"},
                    {"H", "max |H| = 512.0 + 0.0i", "complex double (1024x1)"},
                    {"P_dB", "[-42.1, -38.9, ..., 0.0 dB]", "double (1024x1)"},
                    {"snr_dB", "28.4215", "double (1x1)"}
                });
            }
        } else if (syntaxMode == 2) { // JSON
            if (showDiagnostics) {
                diagnostics.push_back({
                    .line = 15,
                    .startCol = 8,
                    .endCol = 20,
                    .severity = DiagnosticSeverity::Info,
                    .message = "Schema hint: maxThreads > 16 enables NUMA pool allocation",
                    .code = "JSON-SCHEMA",
                    .anchor = "\"maxThreads\""
                });
            }
            if (showInlineValues && executionLine > 0) {
                inlineValues.push_back({2, "schema = \"guikit.ide.v2\"", UiVariant::Info, "\"schema\""});
                inlineValues.push_back({15, "maxThreads = 32 (active)", UiVariant::Success, "\"maxThreads\""});
            }
            if (showHoverDataTips) {
                debugVariables.assign({
                    {"engine", "{ name: \"GuiKit Code Editor\", version: \"2.4.0\" }", "object"},
                    {"maxThreads", "32", "number"},
                    {"simd", "\"AVX-512\"", "string"},
                    {"languages", "[\"C++20\", \"MATLAB\", \"JSON\", \"Python\"]", "array[4]"}
                });
            }
        } else if (syntaxMode == 3) { // Python
            if (showDiagnostics) {
                diagnostics.push_back({
                    .line = 12,
                    .startCol = 12,
                    .endCol = 24,
                    .severity = DiagnosticSeverity::Warning,
                    .message = "Type hint expected 'float', got 'np.float64'",
                    .code = "mypy-type",
                    .anchor = "math.sin("
                });
            }
            if (showInlineValues && executionLine > 0) {
                inlineValues.push_back({6, "epochs = 12, lr = 0.0015", UiVariant::Info, "self.epochs"});
                inlineValues.push_back({11, StoreString(keel::format("idx = {}, loss = {:.5f}", (stepCounter % 12), 0.42f / ((stepCounter % 6) + 1))), UiVariant::Success, "for idx, w"});
            }
            if (showHoverDataTips) {
                debugVariables.assign({
                    {"epochs", "12", "int"},
                    {"lr", "0.0015", "float"},
                    {"loss", "0.08412", "float"},
                    {"weights", "device='cuda:0', dtype=float32", "Tensor[64, 64]"}
                });
            }
        }
    }

    void LoadPreset(keel::usize mode) {
        syntaxMode = mode;
        stepCounter = 0;
        if (mode == 0) { // C++20
            editorBuffer = keel::String::from(
                "#include <cmath>\n"
                "#include \"keel/keel.h\"\n\n"
                "constexpr double kPi = 3.14159265358979323846;\n"
                "// === DSP Biquad & Spectral Energy Kernel ===\n"
                "/* Multi-line block comment:\n"
                "   Try Ctrl+F (Find), Ctrl+H (Replace), Ctrl+G (Go to Line),\n"
                "   F9 (Breakpoint), F10 (Step Over), F5 (Continue), or click gutter chevrons! */\n"
                "namespace dsp::kernel {\n\n"
                "float ComputeBandEnergy(keel::ArrayRef<float> coeffs, double sampleRate) {\n"
                "    const double nyquist = sampleRate * 0.5;\n"
                "    float cutoffHz = 2400.0f;\n"
                "    float gain = 1.85f;\n"
                "    double energyAcc = 0.0;\n"
                "    for (size_t k = 0; k < coeffs.size(); ++k) {\n"
                "        const double omega = (2.0 * kPi * cutoffHz) / nyquist;\n"
                "        float weighted = coeffs[k] * gain * std::cos(omega * double(k));\n"
                "        energyAcc += weighted * weighted;\n"
                "    }\n"
                "    if (energyAcc > 1.0) {\n"
                "        gain *= 0.95f;\n"
                "        return static_cast<float>(std::sqrt(energyAcc));\n"
                "    }\n"
                "    return 0.0f;\n"
                "}\n\n"
                "} // namespace dsp::kernel\n"
            );
            breakpoints.assign({14, 18, 22});
            executionLine = 14;
            lastDebugEvent = "Paused at dsp::kernel::ComputeBandEnergy (Ln 14)";
        } else if (mode == 1) { // MATLAB
            editorBuffer = keel::String::from(
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
                "end\n"
            );
            breakpoints.assign({13, 15, 20});
            executionLine = 13;
            lastDebugEvent = "Paused in analyzeSpectrum.m (Ln 13)";
        } else if (mode == 2) { // JSON
            editorBuffer = keel::String::from(
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
                "}\n"
            );
            breakpoints.assign({2, 15});
            executionLine = 15;
            lastDebugEvent = "Inspecting JSON configuration node (Ln 15)";
        } else if (mode == 3) { // Python
            editorBuffer = keel::String::from(
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
                "        return round(loss, 5)\n"
            );
            breakpoints.assign({10, 12});
            executionLine = 11;
            lastDebugEvent = "Paused in SpectralTrainer.fit_epoch (Ln 11)";
        } else { // Custom DSL
            editorBuffer = keel::String::from(
                "# Custom Rule Engine DSL (Declarative SyntaxConfig)\n"
                "rule OverheatProtection {\n"
                "    when Sensor.temp > 85.5 then {\n"
                "        emit Signal(\"EMERGENCY_STOP\", 100);\n"
                "        alert Threshold(85.5);\n"
                "    }\n"
                "}\n"
            );
            breakpoints.assign({3, 4});
            executionLine = 4;
            lastDebugEvent = "Rule engine triggered on line 4";
        }
        RebuildDynamicDebugData();
    }

    keel::usize CountLines() const {
        keel::usize count = 1;
        for (char c : editorBuffer) {
            if (c == '\n') ++count;
        }
        return count;
    }

    bool IsLineExecutable(keel::usize line1Based) const {
        const keel::StringRef buf = editorBuffer.as_ref();
        keel::usize cur = 1;
        keel::usize pos = 0;
        while (pos <= buf.size()) {
            const keel::usize nextNl = buf.find('\n', pos);
            const keel::usize len = (nextNl == keel::StringRef::npos) ? (buf.size() - pos) : (nextNl - pos);
            if (cur == line1Based) {
                const keel::StringRef trimmed = buf.substr(pos, len).trim();
                if (trimmed.empty()) return false;
                if (trimmed.starts_with("//") || trimmed.starts_with("#") || trimmed.starts_with("%") ||
                    trimmed.starts_with("/*") || trimmed.starts_with("*") || trimmed == "{" || trimmed == "}") {
                    return false;
                }
                return true;
            }
            if (nextNl == keel::StringRef::npos) break;
            pos = nextNl + 1;
            ++cur;
        }
        return false;
    }

    void HandleDebugAction(CodeDebugAction action) {
        const keel::usize totalLines = CountLines();
        if (action == CodeDebugAction::Stop) {
            executionLine = 0;
            lastDebugEvent = "Debugger stopped (Press F5 or Restart to begin)";
        } else if (action == CodeDebugAction::Restart) {
            stepCounter = 0;
            executionLine = !breakpoints.empty() ? breakpoints.front() : 1;
            lastDebugEvent = keel::format_fixed<128>("Debugger restarted at Ln {}", executionLine);
        } else if (action == CodeDebugAction::StepOver || action == CodeDebugAction::StepInto) {
            ++stepCounter;
            keel::usize nextLine = (executionLine == 0) ? 1 : (executionLine + 1);
            for (keel::usize tries = 0; tries < totalLines; ++tries) {
                if (nextLine > totalLines) nextLine = 1;
                if (IsLineExecutable(nextLine)) break;
                ++nextLine;
            }
            executionLine = nextLine;
            lastDebugEvent = keel::format_fixed<128>("Stepped to Ln {} (step #{})", executionLine, stepCounter);
        } else if (action == CodeDebugAction::StepOut) {
            ++stepCounter;
            executionLine = keel::min(totalLines, keel::max<keel::usize>(1, executionLine + 3));
            lastDebugEvent = keel::format_fixed<128>("Stepped out to Ln {}", executionLine);
        } else if (action == CodeDebugAction::Continue) {
            ++stepCounter;
            if (breakpoints.empty()) {
                executionLine = (executionLine == 0) ? 1 : 0;
                lastDebugEvent = (executionLine > 0) ? "Running — paused at entry" : "Program finished";
            } else {
                keel::usize foundBp = breakpoints.front();
                for (keel::usize bp : breakpoints) {
                    if (bp > executionLine) {
                        foundBp = bp;
                        break;
                    }
                }
                executionLine = foundBp;
                lastDebugEvent = keel::format_fixed<128>("Continued to breakpoint at Ln {}", executionLine);
            }
        }
        RebuildDynamicDebugData();
    }
};

CodeEditorState s_state;

} // namespace

void RenderCodeEditorInspector(guikit::Context& ui, DemoContext&) {
    if (auto el = ui.Element({.label = "Language Preset"})) {
        if (ui.Select(s_state.syntaxMode, CodeEditorState::syntaxLabels).changed) {
            s_state.LoadPreset(s_state.syntaxMode);
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Syntax Color Scheme"})) {
        if (ui.Select(s_state.colorScheme, CodeEditorState::colorSchemeLabels).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Show Hidden Whitespace"})) {
        if (ui.Switch(s_state.showWhitespace).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Show Code Minimap"})) {
        if (ui.Switch(s_state.showMinimap).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Code Folding (Gutter)"})) {
        if (ui.Switch(s_state.codeFolding).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Show Indent Guides"})) {
        if (ui.Switch(s_state.showIndentGuides).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Show IDE Status Bar"})) {
        if (ui.Switch(s_state.showStatusBar).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Floating Debug Toolbar"})) {
        if (ui.Switch(s_state.showDebugToolbar).changed) ui.InvalidateAll();
    }
    if (s_state.showDebugToolbar) {
        if (auto el = ui.Element({.label = "Debug Toolbar Position"})) {
            if (ui.Segmented(s_state.debugToolbarAlignIdx, CodeEditorState::debugToolbarAlignLabels).changed) {
                ui.InvalidateAll();
            }
        }
    }
    if (auto el = ui.Element({.label = "Inline Debug Values"})) {
        if (ui.Switch(s_state.showInlineValues).changed) {
            s_state.RebuildDynamicDebugData();
            ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({.label = "Hover Variable DataTips"})) {
        if (ui.Switch(s_state.showHoverDataTips).changed) {
            s_state.RebuildDynamicDebugData();
            ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({.label = "Diagnostic Squiggles"})) {
        if (ui.Switch(s_state.showDiagnostics).changed) {
            s_state.RebuildDynamicDebugData();
            ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({.label = "Line Numbers Gutter"})) {
        if (ui.Switch(s_state.lineNumbers).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Word Wrap"})) {
        if (ui.Switch(s_state.wordWrap).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Read-Only Mode"})) {
        if (ui.Switch(s_state.readOnly).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Editor Zoom"})) {
        if (ui.Slider(s_state.zoom, 0.5f, 2.5f, {.valueFormat = "%.2fx"}).changed) ui.InvalidateAll();
    }
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

    keel::Optional<guikit::SyntaxPalette> synPalette = keel::kNullOpt;
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

    const guikit::Align dbgAlign = (s_state.debugToolbarAlignIdx == 0)
        ? guikit::Align::Start
        : ((s_state.debugToolbarAlignIdx == 2) ? guikit::Align::End : guikit::Align::Center);

    guikit::TextAreaResponse resp{};
    if (auto el = ui.Element({
        .label = s_state.showLabel ? "Code Editor & Debugger" : "",
        .description = s_state.showDesc ? s_state.lastDebugEvent.as_ref() : keel::StringRef{},
        .size = ctx.elemSize
    })) {
        resp = ui.TextArea(s_state.editorBuffer, {
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
            .size = ctx.controlsSize,
            .height = guikit::SizeMode::Stretch,
            .background = guikit::StateColor{},
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
            .executionLinePtr = &s_state.executionLine,
            .showDebugToolbar = s_state.showDebugToolbar,
            .debugToolbarAlign = dbgAlign,
            .diagnostics = s_state.diagnostics,
            .inlineValues = s_state.inlineValues,
            .debugVariables = s_state.debugVariables
        });
    }

    if (resp.lineDelta != 0) {
        s_state.RebuildDynamicDebugData();
    }

    if (resp.debugAction != guikit::CodeDebugAction::None &&
        resp.debugAction != guikit::CodeDebugAction::ToggleBreakpoint) {
        s_state.HandleDebugAction(resp.debugAction);
        ui.InvalidateAll();
    }
}

keel::String GenerateCodeEditorCode(const DemoContext& ctx) {
    keel::StringRef synLine = "guikit::SyntaxConfig::Cpp()";
    if (s_state.syntaxMode == 1) synLine = "guikit::SyntaxConfig::Matlab()";
    else if (s_state.syntaxMode == 2) synLine = "guikit::SyntaxConfig::Json()";
    else if (s_state.syntaxMode == 3) synLine = "guikit::SyntaxConfig::Python()";
    else if (s_state.syntaxMode == 4) synLine = "customSyntaxConfig";

    keel::StringRef palLine = "keel::kNullOpt";
    if (s_state.colorScheme == 1) palLine = "guikit::SyntaxPalette::VsCodeDark()";
    else if (s_state.colorScheme == 2) palLine = "guikit::SyntaxPalette::Monokai()";
    else if (s_state.colorScheme == 3) palLine = "guikit::SyntaxPalette::Dracula()";
    else if (s_state.colorScheme == 4) palLine = "guikit::SyntaxPalette::OneDark()";
    else if (s_state.colorScheme == 5) palLine = "guikit::SyntaxPalette::MatlabClassic()";
    else if (s_state.colorScheme == 6) palLine = "guikit::SyntaxPalette::Solarized()";
    else if (s_state.colorScheme == 7) palLine = "guikit::SyntaxPalette::GitHubLight()";

    const keel::StringRef dbgAlignStr = (s_state.debugToolbarAlignIdx == 0)
        ? "guikit::Align::Start"
        : ((s_state.debugToolbarAlignIdx == 2) ? "guikit::Align::End" : "guikit::Align::Center");

    const keel::String sizeLine = (ctx.controlsSize != guikit::UiSize::None && ctx.controlsSize != guikit::UiSize::Md)
        ? keel::format("    .size = guikit::UiSize::{},\n", SizeName(ctx.controlsSize))
        : keel::String{};

    return keel::format(
        "auto resp = ui.TextArea(sourceBuffer, {{\n"
        "    .showLineNumbers = {},\n"
        "    .syntax = {},\n"
        "    .syntaxPalette = {},\n"
        "{}"
        "    .height = guikit::SizeMode::Stretch,\n"
        "    .showMinimap = {},\n"
        "    .showStatusBar = {},\n"
        "    .showIndentGuides = {},\n"
        "    .showWhitespace = {},\n"
        "    .codeFolding = {},\n"
        "    .breakpoints = &breakpoints,\n"
        "    .executionLine = debugExecLine,\n"
        "    .showDebugToolbar = {},\n"
        "    .debugToolbarAlign = {},\n"
        "    .diagnostics = diagnostics,\n"
        "    .inlineValues = inlineValues,\n"
        "    .debugVariables = debugVariables\n"
        "}});\n"
        "if (resp.debugAction == guikit::CodeDebugAction::StepOver) {{\n"
        "    StepDebugger();\n"
        "}}",
        s_state.lineNumbers ? "true" : "false",
        synLine,
        palLine,
        sizeLine,
        s_state.showMinimap ? "true" : "false",
        s_state.showStatusBar ? "true" : "false",
        s_state.showIndentGuides ? "true" : "false",
        s_state.showWhitespace ? "true" : "false",
        s_state.codeFolding ? "true" : "false",
        s_state.showDebugToolbar ? "true" : "false",
        dbgAlignStr
    );
}

} // namespace guikit::demo
