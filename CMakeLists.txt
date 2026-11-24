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
#    NullCanvas, PlatformWindowBackend, Base Widgets & Panels
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
    src/guikit/core/main_window.cpp
    src/guikit/core/overlay_manager.cpp
    src/guikit/core/response.cpp
    src/guikit/core/state_store.cpp
    src/guikit/core/style_painter.cpp
    src/guikit/core/text_buffer.cpp
    src/guikit/core/theme.cpp
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
    src/guikit/graphics/icon.cpp
    src/guikit/layout/row.cpp
    src/guikit/layout/column.cpp
    src/guikit/layout/wrap.cpp
    src/guikit/layout/grid.cpp
    src/guikit/layout/separator.cpp
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
    src/guikit/backend/null_canvas.cpp
    src/guikit/backend/system_theme.cpp
    src/guikit/backend/text_compositor.cpp
    src/guikit/widgets/level_meter.cpp
    src/guikit/widgets/pagination.cpp
    src/guikit/widgets/readout.cpp
    src/guikit/widgets/tag.cpp
    src/guikit/widgets/tag_input.cpp
    src/guikit/panels/context_menu.cpp
    src/guikit/panels/menu_bar.cpp
    src/guikit/panels/modal.cpp
    src/guikit/panels/popover.cpp
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
