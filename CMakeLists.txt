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
    add_compile_options(/utf-8 /Zc:__cplusplus)
endif()

# --- SDL3 (Submodule or System Package) ---
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

# --- ThorVG (Submodule or System Package) ---
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/thorvg/inc/thorvg.h")
    include(cmake/ThorVG.cmake)
else()
    find_package(thorvg REQUIRED)
endif()

# --- Threads (Cross-Platform) ---
if (NOT EMSCRIPTEN)
    find_package(Threads REQUIRED)
endif()

# --- GuiKit Autonomous Vector GUI Library ---
set(GUIKIT_SOURCES
    src/guikit/Context.cpp
    src/guikit/core/Theme.cpp
    src/guikit/backend/ThorCanvas.cpp
    src/guikit/backend/SdlCanvas.cpp
    src/guikit/graphics/Icon.cpp
    src/guikit/widgets/Button.cpp
    src/guikit/widgets/Carousel.cpp
    src/guikit/widgets/Checkbox.cpp
    src/guikit/widgets/Heading.cpp
    src/guikit/widgets/InputField.cpp
    src/guikit/widgets/Select.cpp
    src/guikit/widgets/Switch.cpp
    src/guikit/widgets/TabBar.cpp
    src/guikit/widgets/Text.cpp
    src/guikit/widgets/Toolbar.cpp
)

add_library(guikit STATIC ${GUIKIT_SOURCES})
add_library(guikit::guikit ALIAS guikit)

target_compile_features(guikit PUBLIC cxx_std_20)
target_include_directories(guikit PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(guikit PUBLIC thorvg SDL3::SDL3-static)
if (NOT EMSCRIPTEN)
    target_link_libraries(guikit PUBLIC Threads::Threads)
endif()

# --- Interactive Showcase Demo ---
option(GUIKIT_BUILD_DEMO "Build GuiKit interactive demo" ON)
if (GUIKIT_BUILD_DEMO)
    add_executable(guikit_demo
        examples/demo/main.cpp
    )
    target_link_libraries(guikit_demo PRIVATE guikit SDL3::SDL3-static)
endif()

# --- Unit Tests ---
option(GUIKIT_BUILD_TESTS "Build GuiKit unit tests" ON)
if (GUIKIT_BUILD_TESTS)
    enable_testing()

    add_executable(test_core
        tests/test_core.cpp
    )
    target_link_libraries(test_core PRIVATE guikit)
    add_test(NAME core COMMAND test_core)

    add_executable(test_layout
        tests/test_layout.cpp
    )
    target_link_libraries(test_layout PRIVATE guikit)
    add_test(NAME layout COMMAND test_layout)

    add_executable(test_context
        tests/test_context.cpp
    )
    target_link_libraries(test_context PRIVATE guikit)
    add_test(NAME context COMMAND test_context)
endif()
