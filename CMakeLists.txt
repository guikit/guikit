cmake_minimum_required(VERSION 3.20)
project(guikit CXX C)

# Uniform C++20 standard across the project
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Single-configuration generators (Ninja, Makefiles): default to Release
if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Source files in UTF-8: MSVC otherwise assumes ANSI code page
if (MSVC)
    add_compile_options(/utf-8 /Zc:__cplusplus /MP)
endif()

# --- Module Build Options ---
option(GUIKIT_BUILD_SCENES "Build GuiKit scientific/DSP/GIS 2D scene modules (SignalView, SonarView, WaterfallView, MapView)" ON)
option(GUIKIT_BUILD_BACKEND_THORVG "Build GuiKit ThorVG software/vector rasterizer backend" ON)
option(GUIKIT_BUILD_BACKEND_SDL3 "Build GuiKit SDL3 + OpenGL ES3/WebGL2 backend" ON)
option(GUIKIT_BUILD_DEMO "Build GuiKit interactive demo" ON)
option(GUIKIT_BUILD_TESTS "Build GuiKit unit tests" ON)

# --- Threads (Cross-Platform) ---
if (NOT EMSCRIPTEN)
    find_package(Threads REQUIRED)
endif()

# ==============================================================================
# 1. Module: guikit::core
#    Context, LayoutSolver, Element, Theme, ICanvas, Base Widgets & Panels
#    Zero external OS / Windowing / GPU dependencies (Pure C++20)
# ==============================================================================
set(GUIKIT_CORE_SOURCES
    src/guikit/Context.cpp
    src/guikit/core/ContainerScope.cpp
    src/guikit/core/Response.cpp
    src/guikit/core/Theme.cpp
    src/guikit/core/Assets.cpp
    src/guikit/core/TextUtil.cpp
    src/guikit/core/Control.cpp
    src/guikit/core/Element.cpp
    src/guikit/core/Solver.cpp
    src/guikit/graphics/Colormap.cpp
    src/guikit/graphics/Icon.cpp
    src/guikit/layout/Row.cpp
    src/guikit/layout/Column.cpp
    src/guikit/layout/Wrap.cpp
    src/guikit/layout/Grid.cpp
    src/guikit/layout/Spacer.cpp
    src/guikit/panels/Accordion.cpp
    src/guikit/panels/Box.cpp
    src/guikit/panels/Card.cpp
    src/guikit/panels/Carousel.cpp
    src/guikit/panels/ScrollArea.cpp
    src/guikit/panels/Scene.cpp
    src/guikit/panels/Sidebar.cpp
    src/guikit/panels/Table.cpp
    src/guikit/panels/Toolbar.cpp
    src/guikit/panels/TreeView.cpp
    src/guikit/widgets/Button.cpp
    src/guikit/widgets/Checkbox.cpp
    src/guikit/widgets/Heading.cpp
    src/guikit/widgets/Histogram.cpp
    src/guikit/widgets/Image.cpp
    src/guikit/widgets/ImageView.cpp
    src/guikit/widgets/InputField.cpp
    src/guikit/widgets/Knob.cpp
    src/guikit/widgets/Markdown.cpp
    src/guikit/widgets/Radio.cpp
    src/guikit/widgets/Select.cpp
    src/guikit/widgets/SidebarMenu.cpp
    src/guikit/widgets/Slider.cpp
    src/guikit/widgets/SpinBox.cpp
    src/guikit/widgets/Switch.cpp
    src/guikit/widgets/TabBar.cpp
    src/guikit/widgets/Text.cpp
    src/guikit/widgets/TextArea.cpp
)

file(GLOB_RECURSE GUIKIT_EMBEDDED_ASSET_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/*"
)
set(GUIKIT_GENERATED_ASSETS_CPP "${CMAKE_CURRENT_BINARY_DIR}/generated/EmbeddedAssets.cpp")

add_custom_command(
    OUTPUT "${GUIKIT_GENERATED_ASSETS_CPP}"
    COMMAND "${CMAKE_COMMAND}"
        "-DASSETS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "-DOUTPUT_FILE=${GUIKIT_GENERATED_ASSETS_CPP}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateEmbeddedAssets.cmake"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateEmbeddedAssets.cmake"
        ${GUIKIT_EMBEDDED_ASSET_FILES}
    COMMENT "Embedding GuiKit assets into C++ binary (Native + WASM)"
    VERBATIM
)

add_library(guikit_core STATIC
    ${GUIKIT_CORE_SOURCES}
    "${GUIKIT_GENERATED_ASSETS_CPP}"
)
add_library(guikit::core ALIAS guikit_core)

target_compile_features(guikit_core PUBLIC cxx_std_20)
target_include_directories(guikit_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# ==============================================================================
# 2. Module: guikit::scenes
#    Scientific / DSP / GIS 2D Views (SignalView, SonarView, WaterfallView, MapView)
# ==============================================================================
if (GUIKIT_BUILD_SCENES)
    set(GUIKIT_SCENES_SOURCES
        src/guikit/geo/GeoJsonIO.cpp
        src/guikit/geo/GeoTypes.cpp
        src/guikit/geo/GisRenderEngine.cpp
        src/guikit/geo/PolygonTriangulator.cpp
        src/guikit/panels/MapView.cpp
        src/guikit/widgets/AttitudeView.cpp
        src/guikit/widgets/BpmnEditor.cpp
        src/guikit/widgets/CarClusterView.cpp
        src/guikit/widgets/SignalView.cpp
        src/guikit/widgets/SonarView.cpp
        src/guikit/widgets/WaterfallView.cpp
    )

    add_library(guikit_scenes STATIC ${GUIKIT_SCENES_SOURCES})
    add_library(guikit::scenes ALIAS guikit_scenes)

    target_compile_features(guikit_scenes PUBLIC cxx_std_20)
    target_link_libraries(guikit_scenes PUBLIC guikit::core)
    if (NOT EMSCRIPTEN)
        target_link_libraries(guikit_scenes PUBLIC Threads::Threads)
    endif()
endif()

# ==============================================================================
# 3. Module: guikit::backend_thorvg
#    CPU / Headless Vector Rasterizer Backend (ThorCanvas)
# ==============================================================================
if (GUIKIT_BUILD_BACKEND_THORVG)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/thorvg/inc/thorvg.h")
        include(cmake/ThorVG.cmake)
    else()
        find_package(thorvg REQUIRED)
    endif()

    add_library(guikit_backend_thorvg STATIC
        src/guikit/backend/ThorCanvas.cpp
    )
    add_library(guikit::backend_thorvg ALIAS guikit_backend_thorvg)

    target_compile_features(guikit_backend_thorvg PUBLIC cxx_std_20)
    target_link_libraries(guikit_backend_thorvg PUBLIC guikit::core thorvg)
    if (NOT EMSCRIPTEN)
        target_link_libraries(guikit_backend_thorvg PUBLIC Threads::Threads)
    endif()
    if (TARGET guikit_scenes)
        target_link_libraries(guikit_scenes PUBLIC guikit::backend_thorvg)
    endif()
endif()

# ==============================================================================
# 4. Module: guikit::backend_sdl3
#    Windowing & Hardware GPU Shader Backend (SdlCanvas, SdlGlShaderEngine)
# ==============================================================================
if (GUIKIT_BUILD_BACKEND_SDL3)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL/CMakeLists.txt")
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
        set(SDL_TESTS OFF CACHE BOOL "" FORCE)
        set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        add_subdirectory(third_party/SDL EXCLUDE_FROM_ALL)
    else()
        find_package(SDL3 REQUIRED CONFIG)
    endif()

    add_library(guikit_backend_sdl3 STATIC
        src/guikit/backend/SdlCanvas.cpp
        src/guikit/backend/SdlGlShaderEngine.cpp
    )
    add_library(guikit::backend_sdl3 ALIAS guikit_backend_sdl3)

    target_compile_features(guikit_backend_sdl3 PUBLIC cxx_std_20)
    target_link_libraries(guikit_backend_sdl3 PUBLIC guikit::core SDL3::SDL3-static)
endif()

# ==============================================================================
# 5. Umbrella Target: guikit::guikit (INTERFACE)
#    Aggregates all enabled GuiKit modules for one-line integration
# ==============================================================================
add_library(guikit INTERFACE)
add_library(guikit::guikit ALIAS guikit)

target_link_libraries(guikit INTERFACE guikit::core)
if (TARGET guikit::scenes)
    target_link_libraries(guikit INTERFACE guikit::scenes)
endif()
if (TARGET guikit::backend_thorvg)
    target_link_libraries(guikit INTERFACE guikit::backend_thorvg)
endif()
if (TARGET guikit::backend_sdl3)
    target_link_libraries(guikit INTERFACE guikit::backend_sdl3)
endif()

# --- Interactive Showcase Demo ---
if (GUIKIT_BUILD_DEMO)
    add_executable(guikit_demo
        examples/demo/main.cpp
        examples/demo/elements/HeadingScreen.cpp
        examples/demo/elements/ButtonScreen.cpp
        examples/demo/elements/SliderScreen.cpp
        examples/demo/elements/KnobScreen.cpp
        examples/demo/elements/SpinBoxScreen.cpp
        examples/demo/elements/InputScreen.cpp
        examples/demo/elements/SelectScreen.cpp
        examples/demo/elements/SwitchScreen.cpp
        examples/demo/elements/CheckboxScreen.cpp
        examples/demo/elements/RadioScreen.cpp
        examples/demo/elements/ImageScreen.cpp
        examples/demo/panels/CardScreen.cpp
        examples/demo/panels/TabBarScreen.cpp
        examples/demo/panels/SidebarScreen.cpp
        examples/demo/panels/TreeViewScreen.cpp
        examples/demo/panels/TableScreen.cpp
        examples/demo/content/MarkdownScreen.cpp
        examples/demo/content/CodeEditorScreen.cpp
        examples/demo/scenes/SchematicScreen.cpp
        examples/demo/scenes/OsmMapScreen.cpp
        examples/demo/scenes/SignalScreen.cpp
        examples/demo/scenes/SonarScreen.cpp
        examples/demo/scenes/WaterfallScreen.cpp
        examples/demo/scenes/ImageViewScreen.cpp
        examples/demo/scenes/AttitudeScreen.cpp
        examples/demo/scenes/CarDashboardScreen.cpp
        examples/demo/scenes/BpmnScreen.cpp
        examples/demo/assemblies/DashboardScreen.cpp
        examples/demo/assemblies/FormControlsScreen.cpp
        examples/demo/assemblies/FlexRibbonScreen.cpp
        examples/demo/assemblies/DiagnosticsScreen.cpp
        examples/demo/assemblies/ThemeStudioScreen.cpp
    )
    target_link_libraries(guikit_demo PRIVATE guikit::guikit)
    if (EMSCRIPTEN)
        set_target_properties(guikit_demo PROPERTIES SUFFIX ".html")
        target_link_options(guikit_demo PRIVATE
            "-sALLOW_MEMORY_GROWTH=1"
            "-sINITIAL_MEMORY=67108864"
            "-sMAX_WEBGL_VERSION=2"
            "-sMIN_WEBGL_VERSION=1"
            "-sEXIT_RUNTIME=0"
            "--embed-file" "${CMAKE_CURRENT_SOURCE_DIR}/assets@assets"
            "--shell-file" "${CMAKE_CURRENT_SOURCE_DIR}/examples/demo/shell.html"
        )
    endif()
endif()

# --- Unit Tests ---
if (GUIKIT_BUILD_TESTS)
    enable_testing()

    # test_core & test_layout link ONLY against guikit::core (zero external dependencies)
    add_executable(test_core
        tests/test_core.cpp
    )
    target_link_libraries(test_core PRIVATE guikit::core)
    add_test(NAME core COMMAND test_core)

    add_executable(test_layout
        tests/test_layout.cpp
    )
    target_link_libraries(test_layout PRIVATE guikit::core)
    add_test(NAME layout COMMAND test_layout)

    add_executable(test_context
        tests/test_context.cpp
    )
    target_link_libraries(test_context PRIVATE guikit::guikit)
    add_test(NAME context COMMAND test_context)
endif()
