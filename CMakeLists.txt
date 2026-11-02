cmake_minimum_required(VERSION 3.21)
project(guikit VERSION 1.0.0 LANGUAGES CXX C)

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
option(GUIKIT_BUILD_DEMO "Build GuiKit interactive demo and examples" ${PROJECT_IS_TOP_LEVEL})
option(GUIKIT_BUILD_TESTS "Build GuiKit unit tests" ${PROJECT_IS_TOP_LEVEL})
option(GUIKIT_INSTALL "Enable CMake install targets for GuiKit" ${PROJECT_IS_TOP_LEVEL})

# --- Threads (Cross-Platform) ---
if (NOT EMSCRIPTEN)
    find_package(Threads REQUIRED)
endif()

# ==============================================================================
# 1. Module: guikit::core
#    Context, LayoutSolver, Element, Theme, ICanvas, MainWindow, Base Widgets & Panels
#    Zero external OS / Windowing / GPU dependencies (Pure C++20)
# ==============================================================================
set(GUIKIT_CORE_SOURCES
    src/guikit/context.cpp
    src/guikit/core/container_scope.cpp
    src/guikit/core/main_window.cpp
    src/guikit/core/response.cpp
    src/guikit/core/theme.cpp
    src/guikit/core/assets.cpp
    src/guikit/core/text_util.cpp
    src/guikit/core/rich_text.cpp
    src/guikit/core/scrollbar.cpp
    src/guikit/core/vector_backend.cpp
    src/guikit/core/control.cpp
    src/guikit/core/element.cpp
    src/guikit/core/solver.cpp
    src/guikit/graphics/colormap.cpp
    src/guikit/graphics/icon.cpp
    src/guikit/layout/row.cpp
    src/guikit/layout/column.cpp
    src/guikit/layout/wrap.cpp
    src/guikit/layout/grid.cpp
    src/guikit/layout/spacer.cpp
    src/guikit/panels/accordion.cpp
    src/guikit/panels/activity_bar.cpp
    src/guikit/panels/bottom_panel.cpp
    src/guikit/panels/box.cpp
    src/guikit/panels/card.cpp
    src/guikit/panels/carousel.cpp
    src/guikit/panels/command_palette.cpp
    src/guikit/panels/split_view.cpp
    src/guikit/widgets/breadcrumb.cpp
    src/guikit/widgets/date_picker.cpp
    src/guikit/widgets/level_meter.cpp
    src/guikit/widgets/pagination.cpp
    src/guikit/widgets/tag_input.cpp
    src/guikit/panels/context_menu.cpp
    src/guikit/panels/modal.cpp
    src/guikit/panels/scroll_area.cpp
    src/guikit/panels/scene.cpp
    src/guikit/panels/sidebar.cpp
    src/guikit/panels/status_bar.cpp
    src/guikit/panels/table.cpp
    src/guikit/panels/toolbar.cpp
    src/guikit/panels/tree_view.cpp
    src/guikit/widgets/button.cpp
    src/guikit/widgets/checkbox.cpp
    src/guikit/widgets/color_picker.cpp
    src/guikit/widgets/heading.cpp
    src/guikit/widgets/histogram.cpp
    src/guikit/widgets/image.cpp
    src/guikit/widgets/image_view.cpp
    src/guikit/widgets/input_field.cpp
    src/guikit/widgets/knob.cpp
    src/guikit/widgets/markdown.cpp
    src/guikit/widgets/radio.cpp
    src/guikit/widgets/segmented.cpp
    src/guikit/widgets/select.cpp
    src/guikit/widgets/sidebar_menu.cpp
    src/guikit/widgets/slider.cpp
    src/guikit/widgets/spin_box.cpp
    src/guikit/widgets/switch.cpp
    src/guikit/widgets/tab_bar.cpp
    src/guikit/widgets/text.cpp
    src/guikit/widgets/text_area.cpp
    src/guikit/widgets/toast.cpp
)

file(GLOB_RECURSE GUIKIT_EMBEDDED_ASSET_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/*"
)
set(GUIKIT_GENERATED_ASSETS_CPP "${CMAKE_CURRENT_BINARY_DIR}/generated/embedded_assets.cpp")

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
        src/guikit/geo/geo_json_io.cpp
        src/guikit/geo/geo_types.cpp
        src/guikit/geo/gis_render_engine.cpp
        src/guikit/geo/osm_raster_tile_provider.cpp
        src/guikit/geo/polygon_triangulator.cpp
        src/guikit/net/net_client.cpp
        src/guikit/panels/map_view.cpp
        src/guikit/widgets/attitude_view.cpp
        src/guikit/widgets/bpmn_editor.cpp
        src/guikit/widgets/car_cluster_view.cpp
        src/guikit/widgets/signal_view.cpp
        src/guikit/widgets/sonar_view.cpp
        src/guikit/widgets/waterfall_view.cpp
    )

    add_library(guikit_scenes STATIC ${GUIKIT_SCENES_SOURCES})
    add_library(guikit::scenes ALIAS guikit_scenes)

    target_compile_features(guikit_scenes PUBLIC cxx_std_20)
    target_link_libraries(guikit_scenes PUBLIC guikit::core)
    if (WIN32)
        target_link_libraries(guikit_scenes PUBLIC winhttp)
    elseif (EMSCRIPTEN)
        target_link_options(guikit_scenes PUBLIC "SHELL:-s FETCH=1" "-lwebsocket.js")
    else()
        find_package(CURL QUIET)
        if (CURL_FOUND)
            target_link_libraries(guikit_scenes PRIVATE CURL::libcurl)
            target_compile_definitions(guikit_scenes PRIVATE GUIKIT_HAS_CURL=1)
        endif()
    endif()
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
        src/guikit/backend/thor_canvas.cpp
    )
    add_library(guikit::backend_thorvg ALIAS guikit_backend_thorvg)

    target_compile_features(guikit_backend_thorvg PUBLIC cxx_std_20)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/thorvg/src/loaders/png/tvgLodePng.h")
        target_include_directories(guikit_backend_thorvg PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/thorvg/src/loaders/png"
        )
    endif()
    target_link_libraries(guikit_backend_thorvg PUBLIC guikit::core thorvg)
    if (NOT EMSCRIPTEN)
        target_link_libraries(guikit_backend_thorvg PUBLIC Threads::Threads)
    endif()
endif()

# ==============================================================================
# 4. Module: guikit::backend_sdl3
#    Windowing & Hardware GPU Shader Backend (SdlWindowBackend, SdlCanvas, SdlGlShaderEngine)
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
        src/guikit/backend/sdl_window_backend.cpp
        src/guikit/backend/sdl_canvas.cpp
        src/guikit/backend/sdl_gl_shader_engine.cpp
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
        examples/demo/demo_window.cpp
        examples/demo/elements/heading_screen.cpp
        examples/demo/elements/button_screen.cpp
        examples/demo/elements/slider_screen.cpp
        examples/demo/elements/knob_screen.cpp
        examples/demo/elements/spin_box_screen.cpp
        examples/demo/elements/input_screen.cpp
        examples/demo/elements/select_screen.cpp
        examples/demo/elements/segmented_screen.cpp
        examples/demo/elements/color_picker_screen.cpp
        examples/demo/elements/date_picker_screen.cpp
        examples/demo/elements/level_meter_screen.cpp
        examples/demo/elements/tag_input_screen.cpp
        examples/demo/panels/split_view_screen.cpp
        examples/demo/panels/breadcrumb_screen.cpp
        examples/demo/panels/pagination_screen.cpp
        examples/demo/elements/switch_screen.cpp
        examples/demo/elements/checkbox_screen.cpp
        examples/demo/elements/radio_screen.cpp
        examples/demo/elements/image_screen.cpp
        examples/demo/panels/card_screen.cpp
        examples/demo/panels/tab_bar_screen.cpp
        examples/demo/panels/toolbar_screen.cpp
        examples/demo/panels/sidebar_screen.cpp
        examples/demo/panels/tree_view_screen.cpp
        examples/demo/panels/table_screen.cpp
        examples/demo/panels/overlays_screen.cpp
        examples/demo/content/markdown_screen.cpp
        examples/demo/content/code_editor_screen.cpp
        examples/demo/scenes/schematic_screen.cpp
        examples/demo/scenes/osm_map_screen.cpp
        examples/demo/scenes/signal_screen.cpp
        examples/demo/scenes/sonar_screen.cpp
        examples/demo/scenes/waterfall_screen.cpp
        examples/demo/scenes/image_view_screen.cpp
        examples/demo/scenes/attitude_screen.cpp
        examples/demo/scenes/car_dashboard_screen.cpp
        examples/demo/scenes/bpmn_screen.cpp
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
    else()
        add_executable(guikit_minimal
            examples/minimal/main.cpp
        )
        target_link_libraries(guikit_minimal PRIVATE guikit::guikit)
    endif()
endif()

# --- CMake Install Rules ---
if (GUIKIT_INSTALL)
    include(GNUInstallDirs)
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/guikit"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )
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
