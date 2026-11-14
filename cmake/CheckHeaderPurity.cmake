# Verifies that all public guikit headers in include/guikit/**/*.hpp:
# 1. Only contain minimal C/freestanding angle-bracket includes:
#    <cassert>, <cmath>, <cstring>, <initializer_list>, <new> (no <cstdint> or <cstddef>).
# 2. Do not contain `using namespace`, STL vocabulary types (`std::string`, `std::vector`, etc.),
#    or raw C/C++ integer types (`uint32_t`, `size_t`, `int`, etc. — use `keel::*` instead) outside of comments/strings.
# 3. Compile standalone and do not transitively include any system header outside keel's minimal baseline.

if(NOT DEFINED GUIKIT_INCLUDE_DIR)
    message(FATAL_ERROR "GUIKIT_INCLUDE_DIR must be passed to CheckHeaderPurity.cmake")
endif()
if(NOT DEFINED KEEL_INCLUDE_DIR)
    message(FATAL_ERROR "KEEL_INCLUDE_DIR must be passed to CheckHeaderPurity.cmake")
endif()

file(GLOB_RECURSE GUIKIT_PUBLIC_HEADERS "${GUIKIT_INCLUDE_DIR}/guikit/*.hpp")
list(SORT GUIKIT_PUBLIC_HEADERS)

set(ALLOWED_STD_HEADERS
    "cassert"
    "cmath"
    "cstring"
    "initializer_list"
    "new"
)

set(FORBIDDEN_STD_REGEX
    "std::(string|wstring|u8string|string_view|vector|span|optional|nullopt|array|function|unique_ptr|shared_ptr|weak_ptr|variant|monostate|unordered_map|unordered_set|map|set|source_location|type_index|chrono)([^A-Za-z0-9_]|$)"
)

set(FORBIDDEN_RAW_INT_REGEX
    "(^|[^A-Za-z0-9_])(u?int(8|16|32|64|ptr|max)_t|size_t|ptrdiff_t|short|long|unsigned|signed|int)([^A-Za-z0-9_]|$)"
)

foreach(HDR IN LISTS GUIKIT_PUBLIC_HEADERS)
    file(RELATIVE_PATH HDR_REL "${GUIKIT_INCLUDE_DIR}" "${HDR}")

    # 1. Angle-bracket include allowlist
    file(STRINGS "${HDR}" INCLUDE_LINES REGEX "^[ \t]*#[ \t]*include[ \t]*<[^>]+>")
    foreach(LINE IN LISTS INCLUDE_LINES)
        string(REGEX REPLACE "^[ \t]*#[ \t]*include[ \t]*<([^>]+)>.*$" "\\1" INC_NAME "${LINE}")
        if(NOT INC_NAME IN_LIST ALLOWED_STD_HEADERS)
            message(FATAL_ERROR
                "Header purity violation in ${HDR_REL}: forbidden '#include <${INC_NAME}>'. "
                "Only <cassert>, <cmath>, <cstring>, <initializer_list>, <new> are allowed (use \"keel/core.h\" for integer types)."
            )
        endif()
    endforeach()

    # 2. Strip comments & string/char literals and check for `using namespace`, STL vocabulary types, or raw C integers
    file(READ "${HDR}" HDR_CONTENT)
    string(REGEX REPLACE "/\\*([^*]|\\*+[^*/])*\\*+/" " " HDR_NO_BLOCK "${HDR_CONTENT}")
    string(REGEX REPLACE "//[^\r\n]*" "" HDR_NO_COMMENTS "${HDR_NO_BLOCK}")
    # Allow ISO C++ [over.literal] required signatures on user-defined `_dp` literals
    string(REGEX REPLACE "operator\"\"_dp\\((long double|unsigned long long)[^)]*\\)" "operator_dp()" HDR_NO_UDL "${HDR_NO_COMMENTS}")
    string(REGEX REPLACE "\"([^\"\\\\]|\\\\.)*\"" "\"\"" HDR_NO_STR "${HDR_NO_UDL}")
    string(REGEX REPLACE "'([^'\\\\]|\\\\.)*'" "''" HDR_CLEAN "${HDR_NO_STR}")

    if(HDR_CLEAN MATCHES "using[ \t]+namespace[ \t]+")
        message(FATAL_ERROR "Header purity violation in ${HDR_REL}: 'using namespace' is forbidden in public headers.")
    endif()

    if(HDR_CLEAN MATCHES "${FORBIDDEN_STD_REGEX}")
        string(REGEX MATCH "${FORBIDDEN_STD_REGEX}" MATCHED_TOKEN "${HDR_CLEAN}")
        message(FATAL_ERROR
            "Header purity violation in ${HDR_REL}: forbidden STL type '${MATCHED_TOKEN}' in public header (use keel::* instead)."
        )
    endif()

    if(HDR_CLEAN MATCHES "${FORBIDDEN_RAW_INT_REGEX}")
        string(REGEX MATCH "${FORBIDDEN_RAW_INT_REGEX}" MATCHED_INT "${HDR_CLEAN}")
        message(FATAL_ERROR
            "Header purity violation in ${HDR_REL}: forbidden raw integer type '${MATCHED_INT}' in public header (use keel::i8..i64 / keel::u8..u64 / keel::isize / keel::usize instead)."
        )
    endif()
endforeach()

# 3. Standalone compilation & transitive system header check
if(DEFINED COMPILER_PATH AND EXISTS "${COMPILER_PATH}")
    set(SCRATCH_DIR "${CMAKE_BINARY_DIR}/guikit_header_purity_check")
    file(MAKE_DIRECTORY "${SCRATCH_DIR}")

    set(SYS_INC_FLAGS "")
    if(DEFINED STD_INC_DIRS AND NOT STD_INC_DIRS STREQUAL "")
        string(REPLACE "|" ";" STD_INC_LIST "${STD_INC_DIRS}")
        foreach(SDIR IN LISTS STD_INC_LIST)
            if(COMPILER_ID MATCHES "MSVC")
                list(APPEND SYS_INC_FLAGS "/I${SDIR}")
            else()
                list(APPEND SYS_INC_FLAGS "-isystem" "${SDIR}")
            endif()
        endforeach()
    endif()

    if(COMPILER_ID MATCHES "MSVC" AND "$ENV{INCLUDE}" STREQUAL "" AND SYS_INC_FLAGS STREQUAL "")
        get_filename_component(CL_DIR1 "${COMPILER_PATH}" DIRECTORY)
        get_filename_component(CL_DIR2 "${CL_DIR1}" DIRECTORY)
        get_filename_component(CL_DIR3 "${CL_DIR2}" DIRECTORY)
        get_filename_component(MSVC_ROOT "${CL_DIR3}" DIRECTORY)
        if(EXISTS "${MSVC_ROOT}/include")
            list(APPEND SYS_INC_FLAGS "/I${MSVC_ROOT}/include")
        endif()
        file(GLOB WIN_SDK_VERSIONS "C:/Program Files (x86)/Windows Kits/10/Include/10.*")
        if(WIN_SDK_VERSIONS)
            list(SORT WIN_SDK_VERSIONS ORDER DESCENDING)
            list(GET WIN_SDK_VERSIONS 0 LATEST_SDK)
            if(EXISTS "${LATEST_SDK}/ucrt")
                list(APPEND SYS_INC_FLAGS "/I${LATEST_SDK}/ucrt")
            endif()
            if(EXISTS "${LATEST_SDK}/shared")
                list(APPEND SYS_INC_FLAGS "/I${LATEST_SDK}/shared")
            endif()
        endif()
    endif()

    set(BASELINE_SRC "${SCRATCH_DIR}/baseline.cpp")
    file(WRITE "${BASELINE_SRC}"
        "#include <cassert>\n#include <cmath>\n#include <cstddef>\n#include <cstdint>\n#include <cstring>\n#include <initializer_list>\n#include <new>\nint main() { return 0; }\n"
    )

    if(COMPILER_ID MATCHES "MSVC")
        execute_process(
            COMMAND "${COMPILER_PATH}" /nologo /std:c++20 /Zs /showIncludes ${SYS_INC_FLAGS} "${BASELINE_SRC}"
            OUTPUT_VARIABLE BASELINE_OUT
            ERROR_VARIABLE BASELINE_ERR
            RESULT_VARIABLE BASELINE_RES
        )
    else()
        execute_process(
            COMMAND "${COMPILER_PATH}" -std=c++20 -fsyntax-only -H ${SYS_INC_FLAGS} "${BASELINE_SRC}"
            OUTPUT_VARIABLE BASELINE_OUT
            ERROR_VARIABLE BASELINE_ERR
            RESULT_VARIABLE BASELINE_RES
        )
    endif()
    set(COMBINED_BASELINE "${BASELINE_OUT}\n${BASELINE_ERR}")

    if(NOT BASELINE_RES EQUAL 0)
        message(FATAL_ERROR "Failed to compile baseline for guikit header purity check:\n${COMBINED_BASELINE}")
    endif()

    set(ALLOWED_PATHS "")
    string(REPLACE "\r\n" "\n" COMBINED_BASELINE "${COMBINED_BASELINE}")
    string(REPLACE "\n" ";" BASELINE_LINES "${COMBINED_BASELINE}")
    foreach(BLINE IN LISTS BASELINE_LINES)
        if(COMPILER_ID MATCHES "MSVC")
            if(BLINE MATCHES "Note: including file:[ \t]*(.+)$")
                string(STRIP "${CMAKE_MATCH_1}" INC_PATH)
                file(TO_CMAKE_PATH "${INC_PATH}" INC_PATH_NORM)
                string(TOLOWER "${INC_PATH_NORM}" INC_PATH_NORM)
                list(APPEND ALLOWED_PATHS "${INC_PATH_NORM}")
            endif()
        else()
            if(BLINE MATCHES "^\\.+ ([^ ]+)$")
                string(STRIP "${CMAKE_MATCH_1}" INC_PATH)
                file(TO_CMAKE_PATH "${CMAKE_MATCH_1}" INC_PATH_NORM)
                list(APPEND ALLOWED_PATHS "${INC_PATH_NORM}")
            endif()
        endif()
    endforeach()
    list(REMOVE_DUPLICATES ALLOWED_PATHS)

    file(TO_CMAKE_PATH "${GUIKIT_INCLUDE_DIR}" GUIKIT_INC_NORM)
    file(TO_CMAKE_PATH "${KEEL_INCLUDE_DIR}" KEEL_INC_NORM)
    if(COMPILER_ID MATCHES "MSVC")
        string(TOLOWER "${GUIKIT_INC_NORM}" GUIKIT_INC_NORM)
        string(TOLOWER "${KEEL_INC_NORM}" KEEL_INC_NORM)
    endif()

    foreach(HDR IN LISTS GUIKIT_PUBLIC_HEADERS)
        file(RELATIVE_PATH HDR_REL "${GUIKIT_INCLUDE_DIR}" "${HDR}")
        string(REPLACE "/" "_" SAFE_NAME "${HDR_REL}")
        set(TEST_SRC "${SCRATCH_DIR}/check_${SAFE_NAME}.cpp")
        file(WRITE "${TEST_SRC}" "#include \"${HDR_REL}\"\nint main() { return 0; }\n")

        if(COMPILER_ID MATCHES "MSVC")
            execute_process(
                COMMAND "${COMPILER_PATH}" /nologo /std:c++20 /Zs /showIncludes ${SYS_INC_FLAGS}
                    "/I${GUIKIT_INCLUDE_DIR}" "/I${KEEL_INCLUDE_DIR}" "${TEST_SRC}"
                OUTPUT_VARIABLE HDR_OUT
                ERROR_VARIABLE HDR_ERR
                RESULT_VARIABLE HDR_RES
            )
        else()
            execute_process(
                COMMAND "${COMPILER_PATH}" -std=c++20 -fsyntax-only -H ${SYS_INC_FLAGS}
                    "-I${GUIKIT_INCLUDE_DIR}" "-I${KEEL_INCLUDE_DIR}" "${TEST_SRC}"
                OUTPUT_VARIABLE HDR_OUT
                ERROR_VARIABLE HDR_ERR
                RESULT_VARIABLE HDR_RES
            )
        endif()

        set(COMBINED_HDR "${HDR_OUT}\n${HDR_ERR}")
        if(NOT HDR_RES EQUAL 0)
            message(FATAL_ERROR "Standalone compilation failed for ${HDR_REL}:\n${COMBINED_HDR}")
        endif()

        string(REPLACE "\r\n" "\n" COMBINED_HDR "${COMBINED_HDR}")
        string(REPLACE "\n" ";" HDR_LINES "${COMBINED_HDR}")
        foreach(HLINE IN LISTS HDR_LINES)
            set(EXTRACTED "")
            if(COMPILER_ID MATCHES "MSVC")
                if(HLINE MATCHES "Note: including file:[ \t]*(.+)$")
                    string(STRIP "${CMAKE_MATCH_1}" EXTRACTED)
                    file(TO_CMAKE_PATH "${EXTRACTED}" EXTRACTED)
                    string(TOLOWER "${EXTRACTED}" EXTRACTED)
                endif()
            else()
                if(HLINE MATCHES "^\\.+ ([^ ]+)$")
                    string(STRIP "${CMAKE_MATCH_1}" EXTRACTED)
                    file(TO_CMAKE_PATH "${EXTRACTED}" EXTRACTED)
                endif()
            endif()

            if(NOT EXTRACTED STREQUAL "")
                string(FIND "${EXTRACTED}" "${GUIKIT_INC_NORM}" IS_GUIKIT_HDR)
                string(FIND "${EXTRACTED}" "${KEEL_INC_NORM}" IS_KEEL_HDR)
                if(NOT IS_GUIKIT_HDR EQUAL 0 AND NOT IS_KEEL_HDR EQUAL 0)
                    if(NOT EXTRACTED IN_LIST ALLOWED_PATHS)
                        message(FATAL_ERROR
                            "Transitive header purity violation in ${HDR_REL}: unexpected system header '${EXTRACTED}'"
                        )
                    endif()
                endif()
            endif()
        endforeach()
    endforeach()
endif()

list(LENGTH GUIKIT_PUBLIC_HEADERS NUM_GUIKIT_HEADERS)
message(STATUS "GuiKit header purity check passed for all ${NUM_GUIKIT_HEADERS} public headers.")
