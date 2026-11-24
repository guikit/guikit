# Verifies positive and negative enforcement cases of KeelCheckGuidelines.cmake.

if(NOT DEFINED CHECK_SCRIPT OR NOT EXISTS "${CHECK_SCRIPT}")
    message(FATAL_ERROR "CHECK_SCRIPT must point to KeelCheckGuidelines.cmake")
endif()
if(NOT DEFINED SCRATCH_ROOT)
    message(FATAL_ERROR "SCRATCH_ROOT must be provided")
endif()

file(REMOVE_RECURSE "${SCRATCH_ROOT}")
file(MAKE_DIRECTORY "${SCRATCH_ROOT}/inc" "${SCRATCH_ROOT}/src")

function(run_guideline_case CASE_NAME EXPECT_PASS FILE_NAME FILE_CONTENT EXTRA_CFG)
    set(CASE_DIR "${SCRATCH_ROOT}/${CASE_NAME}")
    file(MAKE_DIRECTORY "${CASE_DIR}")
    set(TEST_FILE "${CASE_DIR}/${FILE_NAME}")
    file(WRITE "${TEST_FILE}" "${FILE_CONTENT}")

    set(CFG_FILE "${CASE_DIR}/config.cmake")
    set(CFG_STR "")
    string(APPEND CFG_STR "set(KG_TARGET \"${CASE_NAME}\")\n")
    string(APPEND CFG_STR "set(KG_SCOPE \"ALL\")\n")
    string(APPEND CFG_STR "set(KG_BAN_STL ON)\n")
    string(APPEND CFG_STR "set(KG_BAN_RAW_INTS ON)\n")
    string(APPEND CFG_STR "set(KG_ALLOW_STDEXCEPT ON)\n")
    string(APPEND CFG_STR "set(KG_ALLOW_STDEXCEPT_IN_HEADERS OFF)\n")
    string(APPEND CFG_STR "set(KG_SOURCE_ROOT \"${CASE_DIR}\")\n")
    string(APPEND CFG_STR "set(KG_BINARY_DIR \"${CASE_DIR}\")\n")
    string(APPEND CFG_STR "set(KG_FILES \"${TEST_FILE}\")\n")
    string(APPEND CFG_STR "${EXTRA_CFG}\n")
    file(WRITE "${CFG_FILE}" "${CFG_STR}")

    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DCONFIG_FILE=${CFG_FILE}" "-DRUN_STANDALONE_HEADERS=OFF" -P "${CHECK_SCRIPT}"
        OUTPUT_VARIABLE OUT
        ERROR_VARIABLE ERR
        RESULT_VARIABLE RES
    )

    if(EXPECT_PASS AND NOT RES EQUAL 0)
        message(FATAL_ERROR "Case '${CASE_NAME}' expected PASS, but failed (exit ${RES}):\n${OUT}\n${ERR}")
    elseif(NOT EXPECT_PASS AND RES EQUAL 0)
        message(FATAL_ERROR "Case '${CASE_NAME}' expected FAIL, but passed!")
    endif()
endfunction()

# 1. Valid keel::* + C++ exceptions (<exception>, try/catch/throw) + URL inside quotes + keel-lint: ignore + main() -> PASS
run_guideline_case("valid_keel_and_exceptions" TRUE "app.cpp"
"#include <exception>
#include \"keel/core.h\"
#include \"keel/exception.h\"
#include \"keel/math.h\"

extern \"C\" long c_abi_call(); // keel-lint: ignore

keel::i32 Compute(keel::usize count) {
    const char* docs = \"https://guikit.github.io // int vector<int> inside string\";
    const char* shader1 = R\"(uniform int u_Mode; void main() {})\";
    const char* shader2 = R\"glsl(#include <vector>
    int rawInsideRawString = 0;)glsl\";
    (void)docs;
    (void)shader1;
    (void)shader2;
    if (count == 0) {
        throw keel::Exception(\"empty input\");
    }
    return static_cast<keel::i32>(count);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    try {
        return Compute(1);
    } catch (const std::exception&) {
        return 1;
    }
}
" "")

# 2. Valid header with <exception>, UDL, and postfix operator++(int) -> PASS
run_guideline_case("valid_header" TRUE "widget.hpp"
"#pragma once
#include <exception>
#include \"keel/core.h\"

namespace demo {
struct Iter {
    keel::usize idx{0};
    constexpr Iter operator++(int) noexcept { Iter tmp = *this; ++idx; return tmp; }
};
constexpr keel::f32 operator\"\"_px(long double v) noexcept { return keel::as<keel::f32>(v); }
}
" "")

# 3. Forbidden `#include <cstdint>` -> FAIL
run_guideline_case("reject_cstdint" FALSE "bad_cstdint.hpp"
"#pragma once
#include <cstdint>
" "")

# 4. Forbidden `#include <vector>` -> FAIL
run_guideline_case("reject_stl_vector" FALSE "bad_stl.cpp"
"#include <vector>
" "")

# 5. Forbidden raw `uint32_t` -> FAIL
run_guideline_case("reject_raw_uint32" FALSE "bad_int.hpp"
"#pragma once
#include \"keel/core.h\"
struct S { uint32_t x{0}; };
" "")

# 6. Forbidden raw `int` field -> FAIL
run_guideline_case("reject_raw_int" FALSE "bad_raw_int.cpp"
"#include \"keel/core.h\"
keel::i32 Foo() { int x = 5; return x; }
" "")

# 7. Forbidden `<stdexcept>` in header (pulls in `<string>`) -> FAIL
run_guideline_case("reject_stdexcept_in_header" FALSE "bad_exc.hpp"
"#pragma once
#include <stdexcept>
" "")

# 8. Allowed `<stdexcept>` in `.cpp` by default (for catching third-party SDK exceptions) -> PASS
run_guideline_case("allow_stdexcept_in_cpp" TRUE "ok_exc.cpp"
"#include <stdexcept>
#include \"keel/core.h\"
keel::i32 CatchSdk() {
    try { return 0; } catch (const std::runtime_error&) { return -1; }
}
" "")

# 9. Forbidden `<SDL3/SDL.h>` under GuiKit UI rules -> FAIL
run_guideline_case("reject_sdl_in_ui" FALSE "bad_ui.cpp"
"#include <SDL3/SDL.h>
" "set(KG_FORBID_HEADERS \"SDL3/SDL.h;thorvg.h\")")

# 10. Forbidden `"thorvg.h"` under GuiKit UI rules -> FAIL
run_guideline_case("reject_thorvg_in_ui" FALSE "bad_tvg.cpp"
"#include \"thorvg.h\"
" "set(KG_FORBID_HEADERS \"SDL3/SDL.h;thorvg.h\")")

# 11. Forbidden qualified `std::size_t` -> FAIL
run_guideline_case("reject_std_size_t" FALSE "bad_std_size_t.cpp"
"#include \"keel/core.h\"
keel::usize Foo(std::size_t n) { return n; }
" "")

# 12. Forbidden POSIX `ssize_t` -> FAIL
run_guideline_case("reject_ssize_t" FALSE "bad_ssize_t.cpp"
"#include \"keel/core.h\"
keel::isize Bar(ssize_t n) { return n; }
" "")

# 13. Forbidden raw `float` -> FAIL
run_guideline_case("reject_raw_float" FALSE "bad_raw_float.cpp"
"#include \"keel/core.h\"
keel::f32 Baz(float v) { return v; }
" "")

# 14. Forbidden `#include <cmath>` -> FAIL
run_guideline_case("reject_cmath" FALSE "bad_cmath.cpp"
"#include <cmath>
" "")

message(STATUS "All 14 Keel/GuiKit guideline self-tests passed.")
