# ==============================================================================
# GuiKitGuidelines.cmake
# Opt-in CMake guidelines & asset helpers for applications built with GuiKit:
#   - `guikit_enforce_guidelines(<target> ...)`:
#     Enforces keel ecosystem rules (`keel::*` integers & containers, zero-STL,
#     first-class C++ `<exception>` / `keel::Exception` support, `// keel-lint: ignore`)
#     plus GuiKit UI layer isolation (forbids direct `<SDL3/SDL.h>` and `"thorvg.h"`).
#   - `guikit_embed_assets(<target> ASSETS_DIR <dir> [OUTPUT_FILE <cpp>])`:
#     Compiles application assets directly into C++20 translation units for
#     zero-filesystem Native & WebAssembly portability.
# ==============================================================================

include_guard(GLOBAL)

set(_GUIKIT_GUIDELINES_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE INTERNAL "Directory containing GuiKitGuidelines.cmake")

function(guikit_enforce_guidelines TARGET_NAME)
    if(NOT COMMAND keel_enforce_guidelines)
        message(FATAL_ERROR
            "guikit_enforce_guidelines(${TARGET_NAME}): 'keel_enforce_guidelines' is not defined. "
            "Ensure 'keel' is loaded via find_package(guikit) / add_subdirectory(guikit) before calling this function."
        )
    endif()

    set(OPTIONS "")
    set(ONE_VALUE_ARGS "")
    set(MULTI_VALUE_ARGS ALLOW_HEADERS FORBID_HEADERS ALLOWED_INCLUDE_ROOTS)
    cmake_parse_arguments(GG "${OPTIONS}" "${ONE_VALUE_ARGS}" "${MULTI_VALUE_ARGS}" ${ARGN})

    # Automatically forbid raw windowing/rasterizer backend headers in UI & app layers
    # unless explicitly whitelisted by the caller via ALLOW_HEADERS.
    set(_MERGED_FORBID ${GG_FORBID_HEADERS})
    foreach(_BACKEND_HDR IN ITEMS "SDL3/SDL.h" "thorvg.h")
        if(NOT _BACKEND_HDR IN_LIST GG_ALLOW_HEADERS AND NOT _BACKEND_HDR IN_LIST _MERGED_FORBID)
            list(APPEND _MERGED_FORBID "${_BACKEND_HDR}")
        endif()
    endforeach()

    # Automatically register GuiKit public include roots for standalone header compilation
    set(_MERGED_INC_ROOTS ${GG_ALLOWED_INCLUDE_ROOTS})
    if(TARGET guikit::core)
        get_target_property(_GK_INC_DIRS guikit::core INTERFACE_INCLUDE_DIRECTORIES)
        if(_GK_INC_DIRS)
            foreach(_GINC IN LISTS _GK_INC_DIRS)
                string(REGEX REPLACE "^\\$<BUILD_INTERFACE:([^>]+)>$" "\\1" _GINC_CLEAN "${_GINC}")
                if(NOT _GINC_CLEAN MATCHES "^\\$<" AND EXISTS "${_GINC_CLEAN}")
                    list(APPEND _MERGED_INC_ROOTS "${_GINC_CLEAN}")
                endif()
            endforeach()
        endif()
    endif()
    if(EXISTS "${_GUIKIT_GUIDELINES_CMAKE_DIR}/../include/guikit/guikit.hpp")
        get_filename_component(_LOCAL_GK_INC "${_GUIKIT_GUIDELINES_CMAKE_DIR}/../include" ABSOLUTE)
        list(APPEND _MERGED_INC_ROOTS "${_LOCAL_GK_INC}")
    elseif(EXISTS "${_GUIKIT_GUIDELINES_CMAKE_DIR}/../../../include/guikit/guikit.hpp")
        get_filename_component(_INSTALL_GK_INC "${_GUIKIT_GUIDELINES_CMAKE_DIR}/../../../include" ABSOLUTE)
        list(APPEND _MERGED_INC_ROOTS "${_INSTALL_GK_INC}")
    endif()
    list(REMOVE_DUPLICATES _MERGED_INC_ROOTS)

    keel_enforce_guidelines("${TARGET_NAME}"
        ${GG_UNPARSED_ARGUMENTS}
        ALLOW_HEADERS ${GG_ALLOW_HEADERS}
        FORBID_HEADERS ${_MERGED_FORBID}
        ALLOWED_INCLUDE_ROOTS ${_MERGED_INC_ROOTS}
    )

    get_target_property(_GK_TARGET_TYPE "${TARGET_NAME}" TYPE)
    if(NOT _GK_TARGET_TYPE STREQUAL "INTERFACE_LIBRARY" AND CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT MSVC)
        target_compile_options("${TARGET_NAME}" PRIVATE -Werror=dangling)
    endif()
endfunction()

function(guikit_embed_assets TARGET_NAME)
    if(NOT TARGET "${TARGET_NAME}")
        message(FATAL_ERROR "guikit_embed_assets: target '${TARGET_NAME}' does not exist.")
    endif()

    set(OPTIONS NO_AUTO_REGISTER)
    set(ONE_VALUE_ARGS ASSETS_DIR OUTPUT_FILE VIRTUAL_PREFIX NAMESPACE REGISTER_FUNCTION)
    set(MULTI_VALUE_ARGS "")
    cmake_parse_arguments(GEA "${OPTIONS}" "${ONE_VALUE_ARGS}" "${MULTI_VALUE_ARGS}" ${ARGN})

    if(NOT DEFINED GEA_ASSETS_DIR)
        message(FATAL_ERROR "guikit_embed_assets(${TARGET_NAME}): ASSETS_DIR is required.")
    endif()

    get_filename_component(_ABS_ASSETS "${GEA_ASSETS_DIR}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(DEFINED GEA_OUTPUT_FILE)
        get_filename_component(_OUT_CPP "${GEA_OUTPUT_FILE}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    else()
        set(_OUT_CPP "${CMAKE_CURRENT_BINARY_DIR}/generated/${TARGET_NAME}_embedded_assets.cpp")
    endif()

    set(_GEN_ARGS
        "-DASSET_MODE=APP"
        "-DASSETS_DIR=${_ABS_ASSETS}"
        "-DOUTPUT_FILE=${_OUT_CPP}"
    )
    if(DEFINED GEA_VIRTUAL_PREFIX)
        list(APPEND _GEN_ARGS "-DVIRTUAL_PREFIX=${GEA_VIRTUAL_PREFIX}")
    endif()
    if(DEFINED GEA_NAMESPACE)
        list(APPEND _GEN_ARGS "-DNAMESPACE=${GEA_NAMESPACE}")
    endif()
    if(DEFINED GEA_REGISTER_FUNCTION)
        list(APPEND _GEN_ARGS "-DREGISTER_FUNCTION=${GEA_REGISTER_FUNCTION}")
    endif()
    if(GEA_NO_AUTO_REGISTER)
        list(APPEND _GEN_ARGS "-DAUTO_REGISTER=OFF")
    else()
        list(APPEND _GEN_ARGS "-DAUTO_REGISTER=ON")
    endif()

    file(GLOB_RECURSE _ASSET_FILES CONFIGURE_DEPENDS "${_ABS_ASSETS}/*")
    set(_GEN_SCRIPT "${_GUIKIT_GUIDELINES_CMAKE_DIR}/GenerateEmbeddedAssets.cmake")

    add_custom_command(
        OUTPUT "${_OUT_CPP}"
        COMMAND "${CMAKE_COMMAND}" ${_GEN_ARGS} -P "${_GEN_SCRIPT}"
        DEPENDS "${_GEN_SCRIPT}" ${_ASSET_FILES}
        COMMENT "Embedding assets for ${TARGET_NAME} (Native + WASM)"
        VERBATIM
    )
    target_sources("${TARGET_NAME}" PRIVATE "${_OUT_CPP}")
endfunction()
