# GuiKit

**GuiKit** is a modern, autonomous immediate-mode vector GUI engine written in C++20. Powered by **ThorVG** for hardware-accelerated vector graphics and **SDL3** for cross-platform windowing and event handling.

Designed for high-performance desktop tools, embedded displays, and design-system-driven applications requiring pixel-crisp rendering at arbitrary display scales.

---

## Key Highlights

- **Pure Vector Rendering**: Razor-sharp UI at any DPI and resolution with smooth anti-aliased curves via ThorVG, plus a fast fallback `SdlCanvas` backend.
- **Autonomous Architecture**: 100% standalone with zero dependency on Dear ImGui.
- **Modern C++20 Immediate Mode**: Clean syntax with designated initializers, frame allocators, and reactive state:
  ```cpp
  if (ui.Button("Save Project", {.variant = UiVariant::Primary, .size = UiSize::Lg})) {
      Save();
  }
  ```
- **Flexbox & 12-Column Responsive Grid**: Built-in `LayoutSolver` supporting rows, columns, wrapping, intrinsic sizing, and responsive grid collapse.
- **Component Palette**:
  - **Layout**: `Row`, `Column`, `Wrap`, `Grid`, `Spacer`
  - **Panels & Surfaces**: `Box`, `Card`, `ScrollArea`, `Carousel`
  - **Inputs**: `InputField` (text & numeric), `Select` (dropdowns), `Switch`, `Checkbox`
  - **Navigation**: `TabBar` (with responsive carousel overflow), `Toolbar`
  - **Typography & Display**: `Heading` (H1-H4), `Text`, `Icon` (vector icons)
- **Token-Driven Design System**: `UiStyle`, `UiVariant` (Default, Primary, Success, Warning, Danger, Info, Ghost), `UiSize`, and `UiDensity` presets.
- **High Performance**: Zero runtime heap churn in render loops using an internal `Allocator`.

---

## Directory Structure

```text
guikit/
├── include/guikit/      # Canonical public C++20 headers
│   ├── backend/         # Hardware abstraction (ThorCanvas, SdlCanvas, ICanvas)
│   ├── core/            # Tokens, Types, Allocator, Id, FrameMetrics, ContainerScope
│   ├── graphics/        # Vector Icon definitions and rendering
│   ├── layout/          # Flexbox & Grid solver (Solver, Node, Row, Column, Grid...)
│   ├── panels/          # Surface frames & viewports (Box, Card, ScrollArea, Carousel)
│   ├── widgets/         # Immediate-mode UI widgets
│   ├── Context.hpp      # Main UI Context
│   └── GuiKit.hpp       # Umbrella master include
├── src/guikit/          # Implementation units (.cpp)
├── examples/            # Interactive component showcase
├── tests/               # Automated unit tests (core, layout, context)
├── assets/              # Typography assets and branding
└── third_party/         # SDL3 and ThorVG git submodules
```

---

## Building from Source

### Prerequisites

- C++20 compliant compiler (MSVC 2022+, GCC 12+, Clang 15+)
- CMake 3.20+
- Git

### Cloning with Submodules

```bash
git clone --recursive https://github.com/guikit/guikit.git
cd guikit
```

### Build (Windows)

```cmd
scripts\windows\build.cmd Release
```

Or via standard CMake:

```cmd
cmake -B build -S .
cmake --build build --config Release
```

### Run Tests

```cmd
ctest -C Release --output-on-failure --test-dir build
```

---

## License

MIT License. See [LICENSE](LICENSE) for details.
