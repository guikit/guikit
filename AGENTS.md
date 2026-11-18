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
            ui.Heading({.label = "Audio Channel", .separator = guikit::HeadingSeparator::Bottom});
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

### Rule 2: Wrap Form Controls in `ui.Element(...)` & Use `ui.Heading(...)` for Section Headers — Do NOT Build Manual Label/Row Scaffolding
Never manually nest `Column` + `Row` + `Text` just to attach a label, description, icon, or badge to one or more controls:
- **`ui.Element(...)`** is a pure semantic single-slot form-field wrapper (`Label + [Control Slot] + Description`) with 5 self-contained `ElementLayout` placements (`Stacked`, `Top`, `Bottom`, `Leading`, `Trailing`) and automatic `AxisSize` / `SizeMode` resolution. For composite multi-control slots, place a `ui.Row(...)` / `ui.Column(...)` / `ui.Wrap(...)` inside `ui.Element(...)`.
- **`ui.Heading(...)`** is a standalone section header leaf widget (`HeadingLevel::H1`–`H6`, default `H3`, independent of `UiSize`, with `HeadingLayout::Stacked` / `Overline` / `Inline` and `HeadingSeparator::None` / `Bottom` / `Trailing`).
- **Surfaces & Borders** (`.background` / `.border` on Panels, `.style.background` / `.style.border` via `SurfaceStyle` on boxed controls) belong solely to `Panels` (`ui.Card(...)`, `ui.Box(...)`, `Modal`, `Table`, `Scene`, etc.) and intrinsic-box controls (`Button`, `InputField`, `Select`, `SpinBox`, `Segmented`, `DatePicker`, `ColorPicker`, `TagInput`, `Image`, `TextArea`) — never on `Element`, `Heading`, or frameless controls (`Switch`, `Checkbox`, `Radio`, `Knob`, `Slider`, `StepSlider`, `RangeSlider`, `ProgressBar`, `LevelMeter`, `Breadcrumb`, `Pagination`, which use `ControlStyle` with `.size` and `.color` only).

```cpp
// GOOD: Section Heading + declarative Element form-field wrapper (use ui.Row inside Element for multi-control slots)
ui.Heading({.label = "Filter Stage", .level = guikit::HeadingLevel::H3, .separator = guikit::HeadingSeparator::Bottom});
if (auto el = ui.Element({
    .label = "Cutoff Frequency",
    .description = "Low-pass filter corner (-3 dB)",
    .icon = guikit::Icon::Flash(),
    .badge = "Hz",
    .layout = guikit::ElementLayout::Leading
})) {
    ui.SpinBox(cutoffHz, 20.0f, 20000.0f, 10.0f, {.valueFormat = "%.0f Hz"});
}

// BAD: Do NOT manually construct Row + Column + Text(label) + Text(description) + Spacer + Control
```

### Rule 3: Use Widget `.style.size` (`UiSize`) — Do NOT Hardcode Pixel Heights or Font Sizes
GuiKit provides `UiSize` (`UiSize::Xs`, `Sm`, `Md`, `Lg`, `Xl`, or custom `UiSize::Dp(dp)` / `UiSize::Factor(f)`). Size is a property of the control itself (`Md` by default, nested under `.style`), NOT an ambient zone property inherited by children:

```cpp
if (ui.Button({.text = "Compact Action", .style = {.size = guikit::UiSize::Sm}})) { /* ... */ }
if (ui.Button({.text = "Custom Height", .style = {.size = guikit::UiSize::Dp(34.0f)}})) { /* ... */ }
```

Controls group orthogonal geometry and visual fields into nested sub-options so you can pass reusable style presets cleanly:
- **`.layout` (`ControlLayout`)**: `.width`, `.height`, `.minWidth`, `.maxWidth`, `.minHeight`, `.maxHeight` (omitted on fixed-glyph indicators `Switch`, `Checkbox`, `Radio` and rotary `Knob`).
- **`.style` (`ControlStyle` or `SurfaceStyle`)**:
  - `ControlStyle` (`.size`, `.color`) for frameless controls (`Switch`, `Checkbox`, `Radio`, `Knob`, `Slider`, `StepSlider`, `RangeSlider`, `ProgressBar`, `LevelMeter`, `Breadcrumb`, `Pagination`).
  - `SurfaceStyle` (`.size`, `.color`, `.background`, `.border`) for boxed controls (`Button`, `InputField`, `SpinBox`, `Select`, `Segmented`, `DatePicker`, `ColorPicker`, `TagInput`, `Image`, `TextArea`).

### Rule 3b: Wrap Subtrees in `ui.Zone(...)` for Ambient Environment & Geometry — Do NOT Pass Themes or Divide by the Shell Zoom by Hand
Ambient state flows through the transparent RAII scope wrapper `ui.Zone(ZoneOptions)` (`.theme`, `.scale`, `.density`, `.debugBounds`, `.inspectMode`). `ui.Zone(...)` creates no layout node and composes cleanly around containers or widget groups, inheriting unset fields from `ui.GetZone()` (`const guikit::Zone*`):
- **`.scale`** scales the subtree's geometry, text, strokes, and hit-testing proportionally (`displayScale × rootZoom × zoneScale`). Use `ui.ScaleForZoom(zoom)` for live previews, artboards, or canvases that must keep a fixed scale relative to the display DPI.
- **`.density`** (`float`, composed multiplicatively `parent × child`, default `1.0f`) scales ALL container `gap`, `runGap`, `padding`, and `ui.Gap(...)` spacing inside the subtree without scaling control metrics (`UiSize`), border widths, or typography. Explicit `.gap = 12.0f` / `.padding = 16.0f` are defined at `density = 1.0f` and scale with active density.
- **`.debugBounds`**, **`.inspectMode`** override debug tooling in the subtree; **`.disabled`** lives directly on containers and widgets (inheriting down from disabled containers to their children).

```cpp
if (auto zone = ui.Zone({
    .theme = previewTheme,
    .scale = ui.ScaleForZoom(1.25f),
    .density = previewDensity,
    .debugBounds = showBounds
})) {
    if (auto device = ui.Box({
        .width = 375.0f,
        .height = 784.0f,
        .background = {ui.GetThemePalette().surface},
        .clipChildren = true
    })) {
        ui.Button({.text = "Preview theme, fixed pixel size", .style = {.size = guikit::UiSize::Sm}});
    }
}
```
Responses, `GetMousePos()` and `GetPrevFrameBounds()` are reported in the units of the container they are called from. Use `ui.GetDisplayScale()` / `ui.GetRootZoom()` / `ui.GetPixelsPerDp()` / `ui.GetDensity()` to read frame factors.

### Rule 4: Use `ui.Format(...)` & `ui.String(...)` for Per-Frame Strings — Use `keel` Containers for State
`ui.Format(fmt, args...)` and `ui.String(text)` allocate directly into the per-frame `keel::Arena` (`ctx.GetArena()`) and return a zero-heap-allocation `keel::StringRef` (implicitly convertible to `UiText`) valid for the entire frame. For persistent widget state, prefer `keel::FixedString<N>`, `keel::SmallString<N>`, `keel::String`, `keel::SmallArray<T, N>`, and `keel::Array<T>` (external STL containers also bind seamlessly via zero-header duck-typed `keel` concepts):

```cpp
// GOOD: Zero heap allocation per frame
ui.Text(ui.Format("FPS: {:.1f} | Nodes: {}", fps, nodeCount));

// BAD: Heap-allocating temporary strings every frame
ui.Text(keel::format("FPS: {:.1f} | Nodes: {}", fps, nodeCount));
```

### Rule 5: Use `UiText::Rich(...)` & `ui.Markdown(...)` — Do NOT Write Custom Text Parsers
All `UiText` parameters (in `ui.Text`, `ui.Element`, `ui.Heading`, `ui.Toast`, etc.) natively support cached inline Markdown (`**bold**`, `*italic*`, `` `inline code` ``, `~~strikethrough~~`, `[link](url)`) via `UiText::Rich`:

```cpp
ui.Text(guikit::UiText::Rich("Status: **Online** | Latency: `1.4 ms` | [Docs](https://guikit.github.io)"));
```

For multi-block documents (GFM tables, alerts `[!NOTE]`, task lists, fenced code blocks, images), use `ui.Markdown(markdownSource, opts)`.

### Rule 6: Use Semantic Theme Tokens & `StateColor` — Do NOT Hardcode RGB Colors
Hardcoded colors break Dark/Light theme switching and hover/active states.
- For interactive controls (`Button`, `Switch`, `Slider`, etc.), set `.style = {.color = StateColor::Primary()}` (`Success()`, `Warning()`, `Danger()`, `Ghost()`).
- For surfaces and custom drawing, read tokens from `const auto& pal = ui.GetThemePalette();` (`pal.background`, `pal.surface`, `pal.surfaceElevated`, `pal.border`, `pal.textPrimary`, `pal.textSecondary`, `pal.accent`).

### Rule 7: Automatic Loop ID Disambiguation (`MakeChildId`) & Stable Scene Item IDs
Immediate-mode widgets (`Button`, `Switch`, `Slider`, `Element`, etc.) automatically derive their state ID via `ctx.MakeChildId(loc)`, which hashes `(childIndex, keel::SourceLocation, parentId)`. Widgets rendered in a `for` loop inside a container (`Column`, `Row`, `Card`, `Toolbar`) are automatically disambiguated by `childIndex` without passing manual `.id` fields:

```cpp
for (keel::usize i = 0; i < channels.size(); ++i) {
    if (ui.Button({.text = "Mute"})) {
        channels[i].muted = !channels[i].muted;
    }
}
```
When creating interactive `Scene` items (`sc.Item(...)`), pass a stable `.id = ui.Format("pt{}", row)` (see Rule 9a).

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

### Rule 9a: `Scene` repaints itself when its drawing changes — use draggable `Item`s for handles, never hand-rolled mouse tracking
- Every `Draw*` call is folded into the scene's content fingerprint (text, geometry, colors, pixel buffers), so hover highlights, live data and moving markers are repainted without camera changes. Do NOT call `InvalidateRect(sc.GetViewportRect())` / `RequestRepaint` from scene bodies. For large `DrawPixels` buffers that change rarely, pass `contentVersion` (non-zero, bumped on change) to skip per-frame hashing.
- Points, markers and gates the user moves are `sc.Item({.id = stableId, .hitRect = ..., .draggable = true})`: the scene captures the pointer (no panning, works outside the viewport), sets the move cursor, and cancels on Escape. The caller owns the value: save it on `drag.started`, apply `drag.Delta()` while `drag.active`, restore it on `drag.canceled`. A press-and-release in place is a `Clicked()`. Keep `id` stable across frames (e.g. the table row, not the sorted index).
- Tooltips: `SceneOptions::tooltip` (scene hovered) and `SceneItemOptions::tooltip` (item hovered; not while dragging).

```cpp
if (auto it = sc.Item({.id = ui.Format("pt{}", row), .hitRect = Rect{p.x - 7, p.y - 7, 14, 14},
                       .showSelectionFrame = false, .draggable = true})) {
    const auto& d = it.GetDrag();
    if (d.started) saved = value;
    if (d.active) value = saved + toValue(d.Delta());
    if (d.canceled) value = saved;
    sc.DrawCircle(p, it.IsHovered() ? 5.5f : 3.5f, color);
}
```

### Rule 10: Let `SizeMode::Stretch` Viewports Fill `BottomPanel` / `SplitView` — Do NOT Subtract Magic Toolbar Heights
Inside a `BottomPanel`, `Sidebar`, or `SplitView`, any child with `.height = SizeMode::Stretch` (`Scene`, `SignalView`, `SonarView`, `WaterfallView`, `MapView`, `ImageView`, `Table`) automatically stretches and shrinks to fit the exact remaining space after sibling `Toolbar`s or headers:
- Never pass external panel heights with magic offsets like `bottomPanel.Height() - 42.0f` into scene rendering functions.
- If you draw in screen-proportional coordinates inside `Scene`, read `const Rect vp = sc.GetViewportRect();` directly.

### Rule 11: Adaptive `SizeMode::Stretch` Views (No `ScrollArea`) vs Fixed-Height `SizeMode::Fit` Forms (Wrap in `ScrollArea`)
When laying out a workspace that shares space with a resizable `BottomPanel` or `SplitView`:
- **Graphs, diagrams, images, scenes, and tables that must adapt to available size (`SizeMode::Stretch`)**: Place them directly inside a `ui.Column({.height = SizeMode::Stretch})` **WITHOUT** a `ScrollArea`. Wrapping a `Stretch` viewport in a `ScrollArea` prevents it from shrinking when `BottomPanel` expands.
- **Multi-row forms and settings cards (`SizeMode::Fit`)**: Form controls (`ui.Element`, `SpinBox`, `Select`) have a fixed minimum row height and cannot compress vertically; wrap long settings forms in `ui.ScrollArea({.height = SizeMode::Stretch})` so they scroll cleanly when `BottomPanel` is opened.

---

## 2. Layout System Quick Reference

| Container | Usage | Key Options |
| :--- | :--- | :--- |
| `ui.Row(opts)` | Horizontal flex container | `.gap`, `.padding`, `.align`, `.justify`, `.width`, `.height`, `.minWidth`, `.maxWidth`, `.flexGrow` |
| `ui.Column(opts)` | Vertical flex container | `.gap`, `.padding`, `.align`, `.justify`, `.width`, `.height`, `.minHeight`, `.maxHeight`, `.flexGrow` |
| `ui.Wrap(opts)` | Multi-line flow container (chips, tags, cards) | `.gap`, `.runGap`, `.padding`, `.width`, `.height` |
| `ui.Grid(opts)` | 12-column responsive CSS-like grid | `.columns = 12`, `.gap`, `.runGap`; children use `grid.Cell(span)` |
| `ui.Spacer(flexGrow)` | Flexible spring (`flexGrow = 1.0f`) | Pushes subsequent siblings to the right/bottom proportionally |
| `ui.Gap(dp)` | Fixed-pixel spacing node (`dp > 0`) | Inserts an exact non-stretching offset along the main axis |
| `ui.Box(opts)` / `ui.Card(opts)` | Styled container with background, border, radius & shadow | `.background`, `.border`, `.padding`, `.gap`, `.width`, `.height`, `.minWidth`, `.maxWidth`, `.flexGrow` |
| `ui.ScrollArea(opts)` | Scrollable viewport with automatic scrollbars | `.orientation`, `.scrollbar`, `.width`, `.height`, `.maxHeight`, `.flexGrow` |
| `ui.SplitView(ratio, opts)` | Resizable horizontal/vertical 2-pane splitter | `if (auto p1 = split.First())`, `if (auto p2 = split.Second())`; `.width`, `.height` |

**Unified Sizing Policy (`AxisSize`, `SizeMode`, & `ControlLayout`):**
Containers and panels expose top-level `.width` and `.height` options (`Element` and `Heading` expose `.width`), while resizable controls expose `.layout` (`ControlLayout` with `.width`, `.height`, `.minWidth`, `.maxWidth`, `.minHeight`, `.maxHeight`) of type `guikit::AxisSize`; intrinsic fixed-glyph indicators (`Switch`, `Checkbox`, `Radio`) are always `SizeMode::Fit` (scaled via `.style.size`), and rotary `Knob` exposes scalar `.diameter` (`keel::Optional<float>`):
- `AxisSize::Auto()` (default `{}`): Resolves to the widget or container's contextual default (`SizeMode::Fit` or `SizeMode::Stretch`).
- `SizeMode::Fit` / `AxisSize::Fit()`: Shrink-wraps intrinsic content (respecting `.minWidth` / `.maxWidth` / `.minHeight` / `.maxHeight`). Default for compact controls (`Button`, `Image`, `Breadcrumb`, `Pagination`).
- `SizeMode::Stretch` / `AxisSize::Stretch()`: Expands to fill available parent space along that axis (weighted by `.flexGrow` on flex containers). Default for full-width form inputs, pickers, tracks, panels, `Heading`, and 2D domain views (`InputField`, `Select`, `ColorPicker`, `DatePicker`, `Slider`, `SpinBox`, `Table`, `Scene`, etc.).
- `float dp` / `AxisSize::Fixed(dp)`: Uses an explicit logical dimension in `dp` (e.g. `.width = 240.0f`; values `<= 0.0f` resolve to `Auto`).

---

## 3. Built-In Component Inventory (Do NOT Reinvent These)

Before drawing custom shapes on `ICanvas`, check this inventory:

- **Typography & Structure**: `Heading` (standalone `HeadingLevel::H1`–`H6` with default `H3`, `HeadingLayout::Stacked`/`Overline`/`Inline`, description, icon, badge & `HeadingSeparator`), `Text` (semantic `TextRole` + `UiVariant` + `UiText::Rich`), `Element` (pure semantic `Label + [Control Slot] + Description` form-field wrapper with `ElementLayout::Stacked`/`Top`/`Bottom`/`Leading`/`Trailing`; compose `ui.Row`/`ui.Column` in the slot for multiple controls), `Image` (PNG/JPG/SVG with `ObjectFit::Contain`/`Cover`/`ScaleDown` & corner rounding), procedural vector `Icon::*` library (`Icon::Play()`, `Icon::Settings()`, `Icon::Search()`, etc.).
- **Buttons & Selection**: `Button` (solid/outline/ghost, icons, `.badge`, `.loading` spinner), `Switch`, `Checkbox` (with `indeterminate` support), `Radio`, `Segmented` (single-select `IndexLike` integer or `enum class`) & `SegmentedMask` (multi-select bitmask).
- **Numeric & Metering**: `Slider`, `RangeSlider` (`minVal`, `maxVal`), `StepSlider` (`IndexLike` integer or `enum class`) — horizontal, or vertical via `.orientation = Orientation::Vertical` (min at the bottom, `.layout.height = SizeMode::Stretch` fills height by default, StepSlider labels right of the ticks), `ProgressBar` (smooth, discrete LED segments, `.indeterminate`, horizontal or vertical), `LevelMeter` (single & multi-channel audio/telemetry dB bargraph with 3-zone coloring, peak-hold & clip LED), `Knob` (rotary encoder with bipolar arc & radial ticks), `SpinBox` (64-bit integer & float numeric stepper with typematic hold-repeat).
- **Text, Tags & Dropdowns**: `InputField` (in-place zero-copy editing of `keel::FixedString<N>`, `keel::SmallString<N>`, `keel::String`, plus duck-typed `WritableString` interop; password, `InputFilter::Integer`/`Decimal`/`Hex`, `.maxLength`, `.selectAllOnFocus`, prefix/suffix icons, clear button), `TagInput` (`keel::SmallArray` / `keel::Array` of strings or `TagItem`, plus duck-typed list interop), `Select` (`IndexLike` integer/`enum`, `keel::FixedString<N>`, `keel::SmallString<N>`, `keel::String`), `ComboBox` (searchable select), `Autocomplete` (free-text with suggestions), `MultiSelect` (`keel::SmallArray<keel::usize, N>` / `keel::Array<keel::usize>`, plus duck-typed index list interop).
- **Pickers**: `ColorPicker` (2D HSV saturation/value canvas, hue/alpha sliders, HEX input, custom `.swatches`), `DatePicker` (`Date`, `DateTime`, `Time` modes with `DateTime::Today()`, `minDate`/`maxDate`, localized month/weekday labels & leap-year math).
- **Navigation & Data**: `TabBar` (underline/pill/card/folder styles, `.closable` tabs returning `TabBarResponse::closedIndex`, `IndexLike` integer/`enum` binding, overflow scroll), `Breadcrumb` (with interactive `...` collapse menu), `Pagination` (ellipsis windowing), `Toolbar` (`ToolButton` / `Action`, `Toggle`, `Item`, `Dropdown`, `SplitButton`, `SplitToggle`, `Separator`, `Spacer` with `AutoCompact`/`Scroll`/`Wrap`/`Clip` overflow), `Sidebar`, `SidebarMenu`, `ActivityBar`, `BottomPanel`, `StatusBar`, `Accordion`, `Carousel`, `TreeView` (multi-column, filterable hierarchy), `Table` (virtualized 100k+ row data grid with sorting & selection).
- **Overlays**: `Modal` (`if (auto m = ui.Modal(open, opts))` — pure backdrop & input/focus trap portal around `Card`/`Box`/widgets), `Popover` (`if (auto p = ui.Popover(open, opts))` — non-modal anchored portal around `Card`/`Box`/widgets), `ContextMenu`, `CommandPalette` (`Ctrl+K` fuzzy command launcher), `Toast` (`ui.ShowToast(...)` + `ui.ToastHost()`).
- **Rich Editors & 2D Domain Scenes (`guikit::scenes`)**: `Markdown`, `TextArea` / `CodeEditor` (`ui.CodeEditor(...)`, `keel::String`/`FixedString` support, `.rows`, `.autoHeight`, `.tabSize`, `.insertSpaces`, syntax highlighting, breakpoints, code folding, minimap, find/replace), `Scene` (infinite pan/zoom vector canvas with grid & rulers), `MapView` (S-52/ENC vector marine GIS chart + 7 geodetic projections), `SignalView` (waveform + STFT spectrogram), `SonarView` (PPI 360° / Sector / B-Scan radar), `WaterfallView` (RTSA spectral waterfall), `ImageView` + `Histogram` (16-bit scientific image workstation with Window/Level & pixel loupe), `AttitudeView` (avionics PFD), `CarClusterView` (automotive cockpit), `BpmnEditor` (interactive BPMN 2.0 modeler & token simulator).

---

## 4. Repository Code Style & Conventions

When modifying or extending GuiKit itself:

1. **Header & Source File Naming (`snake_case` only) & Zero-STL Header Purity**:
   - Public headers live in `include/guikit/<module>/<file_name>.hpp` and implementation files in `src/guikit/<module>/<file_name>.cpp`.
   - **`keel` is the sole foundational systems layer** (do NOT define `keel` re-export aliases in `namespace guikit`; use `keel::*` directly, including `keel::i8`..`keel::i64`, `keel::u8`..`keel::u64`, `keel::isize`, `keel::usize`, and `keel::kInvalidIndex` instead of raw `int`, `unsigned`, `size_t`, or `<cstdint>` types).
   - Public headers in `include/guikit/` must **never** include `<cstdint>`, `<cstddef>`, or STL container/utility headers (`<string>`, `<string_view>`, `<vector>`, `<span>`, `<optional>`, `<array>`, `<functional>`, `<memory>`, `<unordered_map>`, `<variant>`, `<chrono>`, `<source_location>`, `<typeindex>`, `<algorithm>`, `<type_traits>`, `<utility>`, `<concepts>`, `<compare>`, `<limits>`) or leak `<windows.h>`, `<SDL3/SDL.h>`, `"thorvg.h"`, or internal `using namespace` directives. Only minimal C/freestanding headers (`<cassert>`, `<cmath>`, `<cstring>`, `<exception>`, `<initializer_list>`, `<new>`) and `"keel/*.h"` are allowed (`guikit_enforce_guidelines` / `keel_enforce_guidelines` enforce this automatically).
   - **C++ Exceptions & Opt-In Downstream CMake Guidelines**:
     - C++ `try` / `catch` / `throw` is fully supported: prefer `keel::Exception` (`#include "keel/exception.h"`, zero-heap `std::exception` subclass) over `<stdexcept>` so headers never pull in `<string>`.
     - Downstream projects can opt into the same automated checks by calling `guikit_enforce_guidelines(<target> [HEADERS_DIR ...] [STANDALONE_HEADERS ON])` (or `keel_enforce_guidelines(<target> ...)` for non-GUI targets) after `find_package(guikit)` / `add_subdirectory(guikit)`, and annotate unavoidable C/OS ABI lines with `// keel-lint: ignore`.
   - Umbrella headers:
     - `#include "guikit/guikit.hpp"` (master umbrella header)
     - `#include "guikit/core.hpp"` (`guikit::core` widgets, panels, layouts)
     - `#include "guikit/scenes.hpp"` (`guikit::scenes` scientific/GIS views)
     - `#include "guikit/context.hpp"` (UI context)
2. **Symbol Naming**:
   - **Namespaces**: `snake_case` (`guikit`, `guikit::geo`, `guikit::detail`).
   - **Classes, Structs, `enum class`, Enumerators, and Methods**: `PascalCase` (`Context`, `ButtonOptions`, `UiSize::Md`, `DrawParagraph`).
   - **Struct Fields, Parameters, Local Variables**: `camelCase` (`fontSize`, `maxWidth`, `borderRadius`).
   - **Private Class Members**: `m_camelCase` (`m_activeId`, `m_hadModalLastFrame`).
   - **Compile-Time Constants**: `kPascalCase` (`kDefaultFontSize`, `kMaxChannels`).
3. **Zero-Heap-Allocation Hot Paths & Frame Arena Lifetimes (`keel`)**:
   - Across `include/guikit/` and `src/guikit/`, use `keel::i8`..`keel::i64`, `keel::u8`..`keel::u64`, `keel::isize`, `keel::usize`, `keel::kInvalidIndex`, `keel::StringRef`, `keel::ArrayRef<T>`, `keel::MutArrayRef<T>`, `keel::Optional<T>` (`keel::kNullOpt`), `keel::StaticArray<T, N>`, `keel::UniquePtr<T>`, `keel::Arc<T>`, `keel::String`, `keel::SmallString<N>`, `keel::FixedString<N>`, `keel::Array<T>`, `keel::SmallArray<T, N>`, `keel::FixedArray<T, N>`, `keel::HashMap<K, V>`, `keel::Function`, `keel::SmallFunction`, `keel::FunctionRef`, `keel::SourceLocation`, `keel::TypeId`, and `keel::monotonic_ns()` (`float`, `double`, `bool`, `char`, `void` remain standard built-ins).
   - For math constants, numeric limits, and float-to-integer rounding (`#include "keel/math.h"`), use `keel::kPi` / `keel::kPiF`, `keel::kTau` / `keel::kTauF`, `keel::kHalfPi` / `keel::kHalfPiF`, `keel::deg_to_rad` / `keel::rad_to_deg`, `keel::Limits<T>` (`kMin`, `kLowest`, `kMax`, `infinity()`, `quiet_nan()`), `keel::is_nan` / `keel::is_inf` / `keel::is_finite`, `keel::abs`, `keel::lerp` / `keel::remap`, and UB-free saturating `keel::round_i32` / `keel::round_i64` / `keel::round_u32` / `keel::round_u64` / `keel::floor_i32` / `keel::ceil_i32` instead of `<numbers>`, `<limits>`, C `NAN` / `HUGE_VAL` macros, or `std::lround` / `std::llround`. Keep standard `<cmath>` transcendentals (`std::sin`, `std::cos`, `std::sqrt`, `std::pow`, etc.) directly.
   - Inside widget and layout hot paths (`src/guikit/`), avoid per-frame heap allocations: use `keel::SmallArray<T, N>`, `ctx.GetArena()` (`keel::Arena`), `ctx.AllocPayload<T>(...)`, `keel::format_buf(...)`, and `keel::format_arena(...)`.
   - Whenever widget options (`UiText`, `UiAffix`, `Icon`) are stored in a `Node` payload or captured into a deferred `ctx.RegisterOverlay(...)` callback, copy their string/path views into the frame arena via `ctx.AllocText(...)`, `ctx.AllocAffix(...)`, `ctx.AllocIcon(...)`, or `ctx.AllocString(...)` so temporary caller buffers never dangle during `EndFrame`.
4. **Formatting & Quality**:
   - C++20 standard (`constexpr`, `[[nodiscard]]`, `noexcept`, `keel::StringRef`, `keel::ArrayRef<T>`).
   - Formatting is enforced by `.clang-format` (4-space indent, 120-column limit, `Attach` braces, case-sensitive sorted includes) and `.editorconfig` (UTF-8, LF line endings).
5. **Building & Running Tests**:
   ```powershell
   cmake -B build
   cmake --build build --target test_core test_layout test_context test_guikit_header_purity guikit_demo
   ctest --test-dir build -C Debug --output-on-failure
   ```
   Every new widget, layout rule, or dirty-rect fix must include deterministic unit tests in `tests/test_core.cpp`, `tests/test_layout.cpp`, `tests/test_context.cpp`, or `tests/test_header_purity.cpp`.
