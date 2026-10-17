# GuiKit

**GuiKit** is a modern, autonomous C++20 immediate-mode vector GUI and 2D visualization engine powered by **ThorVG**, **SDL3**, and **WebGL2 / OpenGL GLSL** for both Native Desktop and WebAssembly.

## 👉 [Live Interactive Demo (WebAssembly): https://guikit.github.io/demo/](https://guikit.github.io/demo/)

| **Workspace & Rich Markdown Engine** | **S-52 / Navionics Marine GIS Vector Chart** |
| :---: | :---: |
| ![Workspace & Rich Markdown](docs/screenshots/markdown_workspace.png) | ![Marine GIS Chart](docs/screenshots/gis_chart.png) |
| **PPI 360° Sonar Radar (`SonarView`)** | **Waveform + STFT Spectrogram (`SignalView`)** |
| ![Sonar Radar](docs/screenshots/sonar_radar.png) | ![Signal View](docs/screenshots/signal_view.png) |
| **RTSA Spectral Waterfall (`WaterfallView`)** | **Avionics Primary Flight Display (`AttitudeView`)** |
| ![Spectral Waterfall](docs/screenshots/waterfall_view.png) | ![Attitude PFD](docs/screenshots/attitude_pfd.png) |
| **Automotive Cockpit & ADAS (`CarClusterView`)** | **BPMN 2.0 Modeler & Token Sim (`BpmnEditor`)** |
| ![Car Cluster](docs/screenshots/car_cluster.png) | ![BPMN 2.0 Modeler](docs/screenshots/bpmn_modeler.png) |

---

## Key Highlights

- **Pure Vector Rendering & GLSL Underlays**: Razor-sharp UI at any DPI and scale via **ThorVG**, combined with hardware-accelerated **OpenGL / WebGL2 GLSL** shader underlays (`SdlGlShaderEngine`) for real-time DSP, sonar, and waterfall displays.
- **100% Autonomous C++20 Immediate Mode**: Zero dependency on Dear ImGui. Clean, ergonomic C++20 API using designated initializers, RAII container scopes, and reactive dirty-rectangle damage tracking:
  ```cpp
  if (auto card = ui.Card({.gap = 8.0f, .padding = Padding::All(12.0f)})) {
      ui.Heading({.title = "Mission Control", .size = UiSize::Md});
      if (ui.Button({.label = "Engage Autopilot", .variant = UiVariant::Primary})) {
          EngageAutopilot();
      }
  }
  ```
- **Zero-File Embedded Assets (Native + WebAssembly)**: Automatic build-time asset embedding (`guikit::Assets`) compiles fonts, images, and GeoJSON charts directly into the binary so both Native `.exe` and `.wasm` builds run as self-contained bundles.
- **Reactive Event Loop & Dirty Rect Tracking**: Sleeps at 0% CPU when idle and repaints only damaged screen regions at sub-millisecond frame times.

---

## Component & Scene Suite

### 1. Core Elements & Inputs
- **Typography & Display**: `Heading` (H1–H4, subtitles, dividers), `Text` (semantic roles & variants), `Image` (PNG/JPG/SVG with object-fit & rounding), procedural vector `Icon` library
- **Buttons & Toggles**: `Button`, `Switch`, `Checkbox`, `RadioButton`
- **Numeric & Selection**: `Slider`, `RangeSlider`, `StepSlider`, `SpinBox`, `ProgressBar`, `InputField` (text & numeric), `Select` (searchable/scrollable popups with badges & icons)

### 2. Layouts, Panels & Navigation
- **Layout Solver**: `Row`, `Column`, `Wrap`, `Grid` (12-column responsive layout), `Spacer`
- **Panels & Containers**: `Box`, `Card`, `ScrollArea`, `Sidebar`, `Carousel`
- **Navigation**: `TabBar` (with automatic carousel overflow), `Toolbar`, `SidebarMenu`
- **Virtualized Data Grid**: `Table` — smooth 60+ FPS scrolling, sorting, filtering, and row selection over **100,000+ rows**
- **Rich Content**: `Markdown` viewer (GFM tables, alerts, task lists, code blocks, embedded assets) & `CodeEditor` (syntax highlighting, breakpoints, folding, minimap)

### 3. Specialized 2D Scenes & Domain Workstations (`guikit_scenes`)
- **GIS / Marine Chart (`MapView` + `GisRenderEngine`)**: S-57 / Navionics vector Electronic Navigational Chart (`.geojson` / `.nv2`) with S-52 Day/Dusk/Night palettes, safety contour isobaths, spot soundings, IALA seamarks, 256×256 vector tile caching, and 7 geodetic projections (`EPSG:3857`, `EPSG:3395`, `EPSG:4326`, `UTM 36N`, `GSK-2011`, `SK-42`, `Arctic Polar`)
- **Signal View (`SignalView`)**: Dual waveform + STFT spectrogram analyzer with GPU shader acceleration and harmonic cursor readouts
- **Sonar Radar (`SonarView`)**: Polar PPI 360°, Sector Wedge, and B-Scan sonar/radar display with TVG, clutter gating, and target tracking
- **Waterfall (`WaterfallView`)**: Hardware circular ring-buffer RTSA spectrum waterfall & DOA target tracker
- **Image View (`ImageView` + `Histogram`)**: 16-bit scientific & multi-channel RGB workstation with interactive Window/Level transfer functions and pixel-value loupe
- **Attitude PFD (`AttitudeView`)**: Avionics Primary Flight Display with artificial horizon, pitch ladder, airspeed/altitude tapes, and compass rose
- **Car Cluster (`CarClusterView`)**: Digital automotive instrument cluster & ADAS cockpit
- **BPMN 2.0 Modeler (`BpmnEditor`)**: Full interactive BPMN 2.0 diagram editor (`bpmn-js` parity) with context pad, palette, orthogonal Manhattan waypoint routing, swimlanes, and live token simulation

---

## Directory Structure

```text
guikit/
├── include/guikit/      # Public C++20 headers
│   ├── backend/         # Canvas & GPU shader abstractions (ThorCanvas, SdlCanvas, SdlGlShaderEngine)
│   ├── core/            # Tokens, Theme, Types, Assets, Allocator, Id, FrameMetrics
│   ├── geo/             # Geodesy projections (MapGrid), GeoJSON parser, S-52 GIS render engine
│   ├── graphics/        # Procedural vector Icons & Colormaps
│   ├── layout/          # Flexbox & 12-column Grid solver (Row, Column, Wrap, Grid, Spacer)
│   ├── panels/          # Containers & viewports (Box, Card, ScrollArea, Scene, MapView, Table...)
│   ├── widgets/         # Autonomous immediate-mode widgets & domain instruments
│   ├── Context.hpp      # Primary immediate-mode UI Context
│   ├── Core.hpp         # Lightweight core + widgets header
│   ├── Scenes.hpp       # Domain scenes & GIS header
│   └── GuiKit.hpp       # Master umbrella include
├── src/guikit/          # Implementation units (.cpp)
├── examples/demo/       # Interactive showcase application (Desktop + WebAssembly)
├── assets/              # Embedded fonts, images (lena.png), and GIS chart (demo_bay.geojson)
├── scripts/             # Build, local server, and GitHub Pages deployment scripts
├── tests/               # Automated unit test suites (test_core, test_layout, test_context)
└── third_party/         # SDL3 and ThorVG submodules
```

---

## Quick Start & Building

### Prerequisites
- C++20 compiler (MSVC 2022+, Clang 15+, GCC 12+, or Emscripten for WebAssembly)
- CMake 3.24+
- Git

### 1. Clone with Submodules
```bash
git clone --recursive https://github.com/guikit/guikit.git
cd guikit
```

### 2. Desktop Build & Run (Windows)
```cmd
:: Configure & build Release
scripts\windows\desktop-build.cmd

:: Build & launch the interactive demo
scripts\windows\desktop-run.cmd
```

Or via standard CMake:
```cmd
cmake -B build -S .
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

### 3. WebAssembly Build, Local Run & GitHub Pages Deploy
```cmd
:: Build WebAssembly bundle (auto-installs local .emsdk if needed)
scripts\windows\web-build.cmd

:: Build, start local HTTP server, and open in browser
scripts\windows\web-run.cmd

:: Build and publish to GitHub Pages (https://guikit.github.io/demo/)
scripts\windows\web-publish.cmd
```

---

## License

MIT License. See [LICENSE](LICENSE) for details.
