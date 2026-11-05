# GuiKit Agent & Developer Guide (`AGENTS.md`)

This document is the authoritative architectural and API reference for AI coding agents and developers building applications with **GuiKit** or contributing to the **GuiKit** repository.

Always follow these idioms instead of inventing custom layout math, manual SDL/OpenGL loops, or ad-hoc widget wrappers.

---

## 1. Golden Rules (Do / Don't)

### Rule 1: Use `guikit::MainWindow` — Do NOT Write Manual SDL3 / OpenGL Loops
`MainWindow` encapsulates window creation, DPI scaling, reactive idle sleep (0% CPU when idle), dirty-rectangle damage tracking, and hardware GPU shader underlays. When background threads (e.g. hardware acquisition or compute pipelines) produce new data, call `app.WakeUp()` (thread-safe, deduplicated) instead of including `<SDL3/SDL.h>` or pushing raw `SDL_Event`s.

```cpp
#include "guikit/guikit.hpp"

int main(int, char*[]) {
    bool enabled = true;
    float gainDb = -6.0f;

    guikit::MainWindow app({
        .title = "My Application",
        .width = 1280,
        .height = 800
    });

    return app.Run([&](guikit::Context& ui) {
        if (auto card = ui.Card({.gap = 12.0f, .padding = guikit::Padding::All(16.0f)})) {
            ui.Heading({.label = "Audio Channel", .divider = true});
            if (auto el = ui.Element({.label = "Enable Stage"})) {
                ui.Switch(enabled);
            }
            if (auto el = ui.Element({.label = "Gain", .badge = "dB"})) {
                ui.Slider(gainDb, -60.0f, 12.0f, {.valueFormat = "%.1f dB"});
            }
        }
    });
}
```

### Rule 2: Wrap Form Controls in `ui.Element(...)` — Do NOT Build Manual Label/Row Scaffolding
Never manually nest `Column` + `Row` + `Text` just to attach a label, description, icon, badge, or validation frame to a control. `ui.Element` handles horizontal (`Left`/`Right`), vertical (`Top`), and `Inline` label placement automatically:

```cpp
// GOOD: Declarative Element wrapper
if (auto el = ui.Element({
    .label = "Cutoff Frequency",
    .description = "Low-pass filter corner (-3 dB)",
    .icon = guikit::Icon::Wave(),
    .badge = "Hz",
    .placement = guikit::ElementPlacement::Left
})) {
    ui.SpinBox(cutoffHz, 20.0f, 20000.0f, 10.0f, {.format = "%.0f Hz"});
}

// BAD: Do NOT manually construct Row + Column + Text(label) + Text(description) + Spacer + Control
```

### Rule 3: Use `ui.Size(...)` Scopes — Do NOT Hardcode Pixel Heights or Font Sizes
GuiKit provides 5 semantic component sizes (`UiSize::Xs`, `Sm`, `Md`, `Lg`, `Xl`). Apply sizes via a RAII scope or single-call method chaining:

```cpp
// Single widget chaining:
if (ui.Size(guikit::UiSize::Sm).Button({.text = "Compact Action"})) { /* ... */ }

// Block RAII scope (all widgets inside inherit UiSize::Sm):
{
    auto sz = ui.Size(guikit::UiSize::Sm);
    ui.Button({.text = "One"});
    ui.Button({.text = "Two"});
}
```

### Rule 4: Use `ui.Format(...)` for Per-Frame Strings — Do NOT Allocate `std::string` Every Frame
`ui.Format(fmt, args...)` formats directly into the per-frame arena allocator (`FrameAllocator`) and returns a zero-heap-allocation `std::string_view` valid for the entire frame:

```cpp
// GOOD: Zero heap allocation
ui.Text(ui.Format("FPS: {:.1f} | Nodes: {}", fps, nodeCount));

// BAD: Heap allocation every frame
ui.Text(std::format("FPS: {:.1f} | Nodes: {}", fps, nodeCount));
```

### Rule 5: Use `UiText::Rich(...)` & `ui.Markdown(...)` — Do NOT Write Custom Text Parsers
All `UiText` parameters (in `ui.Text`, `ui.Element`, `ui.Heading`, `ui.Toast`, etc.) natively support cached inline Markdown (`**bold**`, `*italic*`, `` `inline code` ``, `~~strikethrough~~`, `[link](url)`) via `UiText::Rich`:

```cpp
ui.Text(guikit::UiText::Rich("Status: **Online** | Latency: `1.4 ms` | [Docs](https://guikit.github.io)"));
```

For multi-block documents (GFM tables, alerts `[!NOTE]`, task lists, fenced code blocks, images), use `ui.Markdown(markdownSource, opts)`.

### Rule 6: Use Semantic Theme Tokens & `StateColor` — Do NOT Hardcode RGB Colors
Hardcoded colors break Dark/Light theme switching and hover/active states.
- For interactive widgets (`Button`, `Switch`, `Slider`, etc.), use `StateColor::Primary()`, `StateColor::Success()`, `StateColor::Warning()`, `StateColor::Danger()`, `StateColor::Ghost()`.
- For surfaces and custom drawing, read tokens from `const auto& pal = ui.GetThemePalette();` (`pal.background`, `pal.surface`, `pal.surfaceElevated`, `pal.border`, `pal.textPrimary`, `pal.textSecondary`, `pal.accent`) and scale pixel distances via `ui.GetTheme().Scale(px)`.

### Rule 7: Disambiguate IDs in Loops via `ui.MakeId(...)`
Immediate-mode widgets derive their state ID from their label or parent scope. When rendering multiple items with identical labels in a loop, pass `.id = ui.MakeId(i)`:

```cpp
for (size_t i = 0; i < channels.size(); ++i) {
    if (ui.Button({.id = ui.MakeId(i), .text = "Mute"})) {
        channels[i].muted = !channels[i].muted;
    }
}
```

### Rule 8: Use `guikit::Assets` & `ICanvas::LoadFontFile` for Cross-Platform / WebAssembly Portability
Files in `assets/` are compiled directly into the binary via `cmake/GenerateEmbeddedAssets.cmake`.
- Query embedded or runtime assets via `guikit::Assets::Find("assets/...")`.
- Load fonts via `canvas.LoadFontFile(path, FontStyle)` or `canvas.LoadFontMemory(name, bytes, FontStyle)`:
  - `FontStyle::Regular`, `FontStyle::Bold`, `FontStyle::Italic`, `FontStyle::BoldItalic`, `FontStyle::Monospace`.
  - Never create separate monospace loader functions; pass `FontStyle::Monospace` to `LoadFontFile` / `LoadFontMemory`.

### Rule 9: Use `SceneOptions::worldBounds` + `fitPadding` & `SceneTextOptions` — Do NOT Write Manual Viewport Scaling Math
When drawing engineering diagrams, sensor schematics, or CAD geometry in `ui.Scene(...)`:
- Pass `.worldBounds = Rect(minX, minY, spanX, spanY)` and `.fitPadding = Padding(left, top, right, bottom)` so `Scene` frames your physical coordinates (e.g. millimeters) automatically on the current frame with zero 1-frame lag and reserves screen-space margins for labels.
- Remember that `DrawText(text, pos, ...)` uses the **Top-Left** corner of the glyph box by default, NOT the baseline. Never guess pixel offsets like `pos.x - 14.0f` or `pos.y + 18.0f` for tick/point callouts; use `SceneTextOptions` with `.align` (`{0.5f, 0.0f}` = top-center below X-axis tick, `{0.5f, 1.0f}` = bottom-center above point, `{1.0f, 0.5f}` = middle-right to the left of Y-axis tick) and `.offset` in screen `dp`:

```cpp
// GOOD: Declarative world framing + anchor-aligned callout labels in screen dp
if (auto sc = ui.Scene({
    .worldBounds = guikit::Rect(minX, minZ, spanX, spanZ),
    .fitPadding  = guikit::Padding(48.0f, 28.0f, 48.0f, 34.0f),
    .zoomable    = false,
    .pannable    = false,
    .showControls = false,
    .backgroundStyle = guikit::SceneBackground::Solid
})) {
    sc.DrawLine({minX, 0.0f}, {maxX, 0.0f}, surfaceCol, 1.5f);
    sc.DrawLine({0.0f, -0.5f}, {0.0f, 0.5f}, axisCol, 1.2f);
    sc.DrawText("X = 0", {0.0f, 0.0f}, 13.0f, axisCol, {
        .align  = {0.5f, 0.0f}, // Top-center anchor
        .offset = {0.0f, 6.0f}  // 6 dp below world point (0, 0) regardless of zoom
    });
}

// BAD: Do NOT manually compute avail_w/avail_h/scale/ox/oz from GetViewportRect(), call CenterOn() every frame, or guess DrawText pixel offsets
```

### Rule 10: Let `SizeMode::Stretch` Viewports Fill `BottomPanel` / `SplitView` — Do NOT Subtract Magic Toolbar Heights
Inside a `BottomPanel`, `Sidebar`, or `SplitView`, any child with `.heightMode = SizeMode::Stretch` (`Scene`, `SignalView`, `SonarView`, `WaterfallView`, `MapView`, `ImageView`, `Table`) automatically stretches and shrinks to fit the exact remaining space after sibling `Toolbar`s or headers:
- Never pass external panel heights with magic offsets like `bottomPanel.Height() - 42.0f` into scene rendering functions.
- If you draw in screen-proportional coordinates inside `Scene`, read `const Rect vp = sc.GetViewportRect();` directly.

### Rule 11: Adaptive `SizeMode::Stretch` Views (No `ScrollArea`) vs Fixed-Height `SizeMode::Fit` Forms (Wrap in `ScrollArea`)
When laying out a workspace that shares space with a resizable `BottomPanel` or `SplitView`:
- **Graphs, diagrams, images, scenes, and tables that must adapt to available size (`SizeMode::Stretch`)**: Place them directly inside a `ui.Column({.heightMode = SizeMode::Stretch})` **WITHOUT** a `ScrollArea`. Wrapping a `Stretch` viewport in a `ScrollArea` prevents it from shrinking when `BottomPanel` expands.
- **Multi-row forms and settings cards (`SizeMode::Fit`)**: Form controls (`ui.Element`, `SpinBox`, `Select`) have a fixed minimum row height and cannot compress vertically; wrap long settings forms in `ui.ScrollArea({.heightMode = SizeMode::Stretch})` so they scroll cleanly when `BottomPanel` is opened.

---

## 2. Layout System Quick Reference

| Container | Usage | Key Options |
| :--- | :--- | :--- |
| `ui.Row(opts)` | Horizontal flex container | `.gap`, `.padding`, `.align`, `.justify`, `.widthMode`, `.heightMode` |
| `ui.Column(opts)` | Vertical flex container | `.gap`, `.padding`, `.align`, `.justify`, `.widthMode`, `.heightMode` |
| `ui.Wrap(opts)` | Multi-line flow container (chips, tags, cards) | `.gap`, `.rowGap`, `.padding` |
| `ui.Grid(opts)` | 12-column responsive CSS-like grid | `.columns = 12`, `.gap`, `.rowGap`; children use `grid.Span(cols)` |
| `ui.Spacer(size)` | Flexible spring (`size = 0`) or fixed gap (`size > 0`) | Pushes subsequent siblings to the right/bottom |
| `ui.Box(opts)` / `ui.Card(opts)` | Styled container with background, border, radius & shadow | `.background`, `.border`, `.padding`, `.gap` |
| `ui.ScrollArea(opts)` | Scrollable viewport with automatic scrollbars | `.scrollX`, `.scrollY`, `.maxHeight` |
| `ui.SplitView(ratio, opts)` | Resizable horizontal/vertical 2-pane splitter | `if (auto p1 = split.First())`, `if (auto p2 = split.Second())` |

**Sizing Modes (`SizeMode` / `Sizing`):**
- `SizeMode::Fit`: Shrink-wraps children content.
- `SizeMode::Stretch`: Expands to fill available parent space along that axis.
- `SizeMode::Fixed`: Uses explicit `.width` / `.height` (or `.minWidth` / `.maxWidth`).

---

## 3. Built-In Component Inventory (Do NOT Reinvent These)

Before drawing custom shapes on `ICanvas`, check this inventory:

- **Typography & Structure**: `Heading` (H1–H4 + description + icon + divider), `Text` (semantic `TextRole` + `UiVariant` + `UiText::Rich`), `Element` (universal form field decorator), `Image` (PNG/JPG/SVG with `ImageFit::Contain`/`Cover`/`ScaleDown` & corner rounding), procedural vector `Icon::*` library (`Icon::Play()`, `Icon::Settings()`, `Icon::Search()`, etc.).
- **Buttons & Selection**: `Button` (solid/outline/ghost, icons, badges, loading spinner), `Switch`, `Checkbox` (with `indeterminate` support), `Radio`, `Segmented` (single-select index) & `SegmentedMask` (multi-select bitmask).
- **Numeric & Metering**: `Slider` (horizontal, or vertical fader via `.orientation = Orientation::Vertical`: min at the bottom, `stretch = Fill` fills the height), `RangeSlider` (`minVal`, `maxVal`), `StepSlider`, `ProgressBar` (smooth, discrete LED segments, indeterminate), `LevelMeter` (single & multi-channel audio/telemetry dB bargraph with 3-zone coloring, peak-hold & clip LED), `Knob` (rotary encoder with bipolar arc & radial ticks), `SpinBox` (numeric stepper with drag-to-scrub).
- **Text, Tags & Dropdowns**: `InputField` (text, password, numeric, prefix/suffix icons, clear button, validation), `TagInput` (multi-tag chip editor), `Select` (dropdown), `ComboBox` (searchable select), `Autocomplete` (free-text with suggestions), `MultiSelect` (multi-chip dropdown with checkboxes & "Select All").
- **Pickers**: `ColorPicker` (2D HSV saturation/value canvas, hue/alpha sliders, HEX input, swatches), `DatePicker` (`Date`, `DateTime`, `Time` modes with calendar popup & leap-year math).
- **Navigation & Data**: `TabBar` (underline/pill/card/folder styles, closable tabs, overflow scroll), `Breadcrumb` (with interactive `...` collapse menu), `Pagination` (ellipsis windowing), `Toolbar`, `Sidebar`, `SidebarMenu`, `ActivityBar`, `BottomPanel`, `StatusBar`, `Accordion`, `Carousel`, `TreeView` (multi-column, filterable hierarchy), `Table` (virtualized 100k+ row data grid with sorting & selection).
- **Overlays**: `Modal` (`if (auto m = ui.Modal(open, opts))`), `Popover`, `ContextMenu`, `CommandPalette` (`Ctrl+K` fuzzy command launcher), `Toast` (`ui.ShowToast(...)` + `ui.RenderToasts()`).
- **Rich Editors & 2D Domain Scenes (`guikit::scenes`)**: `Markdown`, `TextArea` / `CodeEditor` (syntax highlighting, breakpoints, code folding, minimap, find/replace), `Scene` (infinite pan/zoom vector canvas with grid & rulers), `MapView` (S-52/ENC vector marine GIS chart + 7 geodetic projections), `SignalView` (waveform + STFT spectrogram), `SonarView` (PPI 360° / Sector / B-Scan radar), `WaterfallView` (RTSA spectral waterfall), `ImageView` + `Histogram` (16-bit scientific image workstation with Window/Level & pixel loupe), `AttitudeView` (avionics PFD), `CarClusterView` (automotive cockpit), `BpmnEditor` (interactive BPMN 2.0 modeler & token simulator).

---

## 4. Repository Code Style & Conventions

When modifying or extending GuiKit itself:

1. **Header & Source File Naming (`snake_case` only)**:
   - Public headers live in `include/guikit/<module>/<file_name>.hpp` and implementation files in `src/guikit/<module>/<file_name>.cpp`.
   - Umbrella headers:
     - `#include "guikit/guikit.hpp"` (master umbrella header)
     - `#include "guikit/core.hpp"` (`guikit::core` widgets, panels, layouts)
     - `#include "guikit/scenes.hpp"` (`guikit::scenes` scientific/GIS views)
     - `#include "guikit/context.hpp"` (UI context)
2. **Symbol Naming**:
   - **Namespaces**: `snake_case` (`guikit`, `guikit::geo`, `guikit::detail`).
   - **Classes, Structs, `enum class`, Enumerators, and Methods**: `PascalCase` (`Context`, `ButtonOpts`, `UiSize::Md`, `DrawParagraph`).
   - **Struct Fields, Parameters, Local Variables**: `camelCase` (`fontSize`, `maxWidth`, `borderRadius`).
   - **Private Class Members**: `m_camelCase` (`m_activeId`, `m_hadModalLastFrame`).
   - **Compile-Time Constants**: `kPascalCase` (`kDefaultFontSize`, `kMaxChannels`).
3. **Formatting & Quality**:
   - C++20 standard (`constexpr`, `[[nodiscard]]`, `noexcept`, `std::string_view`, `std::span`).
   - Formatting is enforced by `.clang-format` (4-space indent, 120-column limit, `Attach` braces, case-sensitive sorted includes) and `.editorconfig` (UTF-8, LF line endings).
4. **Building & Running Tests**:
   ```powershell
   cmake -B build
   cmake --build build --target test_core test_layout test_context guikit_demo
   ctest --test-dir build -C Debug --output-on-failure
   ```
   Every new widget, layout rule, or dirty-rect fix must include deterministic unit tests in `tests/test_core.cpp`, `tests/test_layout.cpp`, or `tests/test_context.cpp`.
