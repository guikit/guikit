cmake_minimum_required(VERSION 3.21)
project(guikit VERSION 1.0.0 LANGUAGES CXX)

# Uniform C++20 standard when built standalone (targets propagate cxx_std_20 via target_compile_features)
if (PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    # Single-configuration generators (Ninja, Makefiles): default to Release
    if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    endif()
endif()

# Source files in UTF-8: MSVC otherwise assumes ANSI code page
if (MSVC)
    add_compile_options(/utf-8 /Zc:__cplusplus /MP)
endif()

# --- Build Options ---
option(GUIKIT_BUILD_EXAMPLES "Build GuiKit headless example application" ${PROJECT_IS_TOP_LEVEL})
option(GUIKIT_BUILD_TESTS "Build GuiKit unit tests" ${PROJECT_IS_TOP_LEVEL})
option(GUIKIT_INSTALL "Enable CMake install targets for GuiKit" ${PROJECT_IS_TOP_LEVEL})

if (GUIKIT_BUILD_TESTS)
    enable_testing()
endif()

# Optimize standalone WebAssembly builds for compact binary size (-Oz + Link-Time Optimization)
if (EMSCRIPTEN AND PROJECT_IS_TOP_LEVEL)
    set(CMAKE_C_FLAGS_RELEASE "-Oz -flto -DNDEBUG" CACHE STRING "Emscripten C Release flags" FORCE)
    set(CMAKE_CXX_FLAGS_RELEASE "-Oz -flto -DNDEBUG" CACHE STRING "Emscripten CXX Release flags" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-Oz -flto" CACHE STRING "Emscripten Release linker flags" FORCE)
endif()

# --- Threads (Cross-Platform) ---
if (NOT EMSCRIPTEN)
    find_package(Threads REQUIRED)
endif()

# ==============================================================================
# 0. Dependency: keel (0BSD C++20 Foundation Library)
# ==============================================================================
if (NOT TARGET keel::keel)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/keel/CMakeLists.txt")
        if (GUIKIT_INSTALL)
            add_subdirectory(third_party/keel)
        else()
            add_subdirectory(third_party/keel EXCLUDE_FROM_ALL)
        endif()
    else()
        find_package(keel REQUIRED)
    endif()
endif()

if (NOT TARGET keel::compress)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/compress/CMakeLists.txt")
        set(KEEL_COMPRESS_BUILD_TESTS OFF)
        if (GUIKIT_INSTALL)
            add_subdirectory(third_party/compress)
        else()
            add_subdirectory(third_party/compress EXCLUDE_FROM_ALL)
        endif()
    else()
        find_package(keel_compress QUIET)
    endif()
endif()

if (NOT TARGET keel::imagefile)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imagefile/CMakeLists.txt")
        set(KEEL_IMAGEFILE_BUILD_TESTS OFF)
        if (GUIKIT_INSTALL)
            add_subdirectory(third_party/imagefile)
        else()
            add_subdirectory(third_party/imagefile EXCLUDE_FROM_ALL)
        endif()
    else()
        find_package(keel_imagefile QUIET)
    endif()
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/GuiKitGuidelines.cmake")

# ==============================================================================
# 1. Module: guikit::core
#    Context, LayoutSolver, Element, Theme, ICanvas, IPlatformWindow, MainWindow,
#    NullCanvas, Base Widgets & Panels
#    Zero external OS / Windowing / GPU / C-Library dependencies (Pure C++20 + keel)
# ==============================================================================
set(GUIKIT_CORE_SOURCES
    src/guikit/context.cpp
    src/guikit/core/container_scope.cpp
    src/guikit/core/damage_tracker.cpp
    src/guikit/core/debug_inspector.cpp
    src/guikit/core/focus_manager.cpp
    src/guikit/core/frame_metrics.cpp
    src/guikit/core/hit_test_tree.cpp
    src/guikit/core/image_cache.cpp
    src/guikit/core/main_window.cpp
    src/guikit/core/overlay_manager.cpp
    src/guikit/core/response.cpp
    src/guikit/core/state_store.cpp
    src/guikit/style/style_painter.cpp
    src/guikit/core/text_buffer.cpp
    src/guikit/style/theme.cpp
    src/guikit/core/assets.cpp
    src/guikit/core/text_util.cpp
    src/guikit/core/rich_text.cpp
    src/guikit/core/scrollbar.cpp
    src/guikit/core/shell_controls.cpp
    src/guikit/core/vector_backend.cpp
    src/guikit/core/control.cpp
    src/guikit/core/element.cpp
    src/guikit/core/solver.cpp
    src/guikit/core/zone.cpp
    src/guikit/graphics/colormap.cpp
    src/guikit/graphics/draw_list.cpp
    src/guikit/graphics/icon.cpp
    src/guikit/layout/row.cpp
    src/guikit/layout/column.cpp
    src/guikit/layout/wrap.cpp
    src/guikit/layout/grid.cpp
    src/guikit/display/separator.cpp
    src/guikit/layout/spacer.cpp
    src/guikit/surfaces/accordion.cpp
    src/guikit/surfaces/activity_bar.cpp
    src/guikit/surfaces/bottom_panel.cpp
    src/guikit/surfaces/box.cpp
    src/guikit/surfaces/card.cpp
    src/guikit/surfaces/carousel.cpp
    src/guikit/overlays/command_palette.cpp
    src/guikit/layout/split_view.cpp
    src/guikit/actions/breadcrumb.cpp
    src/guikit/fields/date_picker.cpp
    src/guikit/backend/null_canvas.cpp
    src/guikit/backend/system_theme.cpp
    src/guikit/backend/text_compositor.cpp
    src/guikit/display/level_meter.cpp
    src/guikit/actions/pagination.cpp
    src/guikit/display/readout.cpp
    src/guikit/display/tag.cpp
    src/guikit/fields/tag_input.cpp
    src/guikit/overlays/context_menu.cpp
    src/guikit/surfaces/menu_bar.cpp
    src/guikit/overlays/modal.cpp
    src/guikit/overlays/popover.cpp
    src/guikit/layout/scroll_area.cpp
    src/guikit/views/scene.cpp
    src/guikit/surfaces/sidebar.cpp
    src/guikit/surfaces/status_bar.cpp
    src/guikit/views/table.cpp
    src/guikit/surfaces/toolbar.cpp
    src/guikit/views/tree_view.cpp
    src/guikit/actions/button.cpp
    src/guikit/controls/checkbox.cpp
    src/guikit/fields/color_picker.cpp
    src/guikit/display/heading.cpp
    src/guikit/display/histogram.cpp
    src/guikit/display/image.cpp
    src/guikit/views/image_view.cpp
    src/guikit/fields/input_field.cpp
    src/guikit/controls/knob.cpp
    src/guikit/views/markdown.cpp
    src/guikit/views/markdown_parser.cpp
    src/guikit/display/progress_bar.cpp
    src/guikit/controls/radio.cpp
    src/guikit/controls/range_slider.cpp
    src/guikit/actions/segmented.cpp
    src/guikit/fields/select.cpp
    src/guikit/fields/select_multi.cpp
    src/guikit/actions/sidebar_menu.cpp
    src/guikit/controls/slider.cpp
    src/guikit/fields/spin_box.cpp
    src/guikit/controls/switch.cpp
    src/guikit/actions/tab_bar.cpp
    src/guikit/display/text.cpp
    src/guikit/fields/text_area.cpp
    src/guikit/fields/text_area_highlight.cpp
    src/guikit/fields/text_area_render.cpp
    src/guikit/overlays/toast.cpp
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
set_target_properties(guikit_core PROPERTIES EXPORT_NAME core)

target_compile_features(guikit_core PUBLIC cxx_std_20)
target_include_directories(guikit_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(guikit_core PUBLIC keel::keel)
set(GUIKIT_HAS_KEEL_IMAGEFILE OFF)
if (TARGET keel::imagefile)
    target_link_libraries(guikit_core PRIVATE keel::imagefile)
    target_compile_definitions(guikit_core PRIVATE GUIKIT_HAS_KEEL_IMAGEFILE=1)
    set(GUIKIT_HAS_KEEL_IMAGEFILE ON)
endif()

guikit_enforce_guidelines(guikit_core
    HEADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include"
    STANDALONE_HEADERS ${GUIKIT_BUILD_TESTS}
    TEST_NAME guikit_header_purity_scan
)

# ==============================================================================
# 2. Umbrella Target: guikit::guikit (INTERFACE)
# ==============================================================================
add_library(guikit INTERFACE)
add_library(guikit::guikit ALIAS guikit)
target_link_libraries(guikit INTERFACE guikit::core)

# --- Examples ---
if (GUIKIT_BUILD_EXAMPLES AND NOT EMSCRIPTEN)
    add_executable(guikit_headless
        examples/headless/main.cpp
    )
    target_link_libraries(guikit_headless PRIVATE guikit::core)
    guikit_enforce_guidelines(guikit_headless)
endif()

# --- CMake Install Rules ---
if (GUIKIT_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(TARGETS guikit_core guikit
        EXPORT guikitTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/guikit"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )
    install(EXPORT guikitTargets
        FILE guikitTargets.cmake
        NAMESPACE guikit::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/guikit"
    )
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/guikitConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/guikitConfig.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/guikit"
    )
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/guikitConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/guikitConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/guikitConfigVersion.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GuiKitGuidelines.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateEmbeddedAssets.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/guikit"
    )
endif()

# --- Unit Tests ---
if (GUIKIT_BUILD_TESTS)
    add_executable(test_core
        tests/test_core.cpp
    )
    target_link_libraries(test_core PRIVATE guikit::core)
    guikit_enforce_guidelines(test_core)
    add_test(NAME core COMMAND test_core)

    add_executable(test_layout
        tests/test_layout.cpp
    )
    target_link_libraries(test_layout PRIVATE guikit::core)
    guikit_enforce_guidelines(test_layout)
    add_test(NAME layout COMMAND test_layout)

    add_executable(test_context
        tests/test_context.cpp
    )
    target_link_libraries(test_context PRIVATE guikit::core)
    # test_context intentionally exercises duck-typed std::string / std::vector interop
    keel_enforce_guidelines(test_context BAN_STL OFF)
    add_test(NAME context COMMAND test_context)

    add_executable(test_guikit_header_purity
        tests/test_header_purity.cpp
    )
    target_link_libraries(test_guikit_header_purity PRIVATE guikit::core)
    keel_enforce_guidelines(test_guikit_header_purity WARNINGS_AS_ERRORS ON)
    add_test(NAME guikit_header_purity COMMAND test_guikit_header_purity)

    add_test(
        NAME guikit_guidelines_self_test
        COMMAND ${CMAKE_COMMAND}
            "-DCHECK_SCRIPT=${_KEEL_GUIDELINES_CMAKE_DIR}/KeelCheckGuidelines.cmake"
            "-DSCRATCH_ROOT=${CMAKE_CURRENT_BINARY_DIR}/guidelines_self_test"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_guidelines_enforcement.cmake"
    )
endif()
