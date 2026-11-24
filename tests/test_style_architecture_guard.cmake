# Automated Design-System Architecture Guard
# Ensures zero ad-hoc theme/chrome branching inside widget/panel/view implementations.
# All visual chrome physics must be delegated through IStylePainter / FieldFrame.

if(NOT DEFINED GUIKIT_ROOT OR NOT EXISTS "${GUIKIT_ROOT}")
    message(FATAL_ERROR "GUIKIT_ROOT must point to the guikit source directory")
endif()

set(WIDGET_DIRS
    "${GUIKIT_ROOT}/src/guikit/actions"
    "${GUIKIT_ROOT}/src/guikit/fields"
    "${GUIKIT_ROOT}/src/guikit/controls"
    "${GUIKIT_ROOT}/src/guikit/surfaces"
    "${GUIKIT_ROOT}/src/guikit/overlays"
    "${GUIKIT_ROOT}/src/guikit/display"
    "${GUIKIT_ROOT}/src/guikit/views"
)

set(FORBIDDEN_PATTERNS
    "theme\\.chrome\\."
    "(^|[^A-Za-z0-9_])ThemePreset::"
    "(^|[^A-Za-z0-9_])ChromeStyle::"
    "(^|[^A-Za-z0-9_])FrameTreatment::"
    "(^|[^A-Za-z0-9_])FieldTreatment::"
    "(^|[^A-Za-z0-9_])FieldButtonStyle::"
    "(^|[^A-Za-z0-9_])SelectionHighlightStyle::"
    "(^|[^A-Za-z0-9_])CheckControlStyle::"
    "(^|[^A-Za-z0-9_])ProgressFillStyle::"
    "(^|[^A-Za-z0-9_])TabBarChrome::"
    "(^|[^A-Za-z0-9_])BadgeShape::"
    "(^|[^A-Za-z0-9_])SliderThumbShape::"
    "(^|[^A-Za-z0-9_])SliderTrackStyle::"
    "(^|[^A-Za-z0-9_])DisabledTextStyle::"
)

set(VIOLATIONS "")
set(SCANNED_COUNT 0)

foreach(dir IN LISTS WIDGET_DIRS)
    if(NOT EXISTS "${dir}")
        message(FATAL_ERROR "Expected element class directory missing: ${dir}")
    endif()
    file(GLOB_RECURSE files "${dir}/*.cpp" "${dir}/*.hpp")
    foreach(file IN LISTS files)
        math(EXPR SCANNED_COUNT "${SCANNED_COUNT} + 1")
        file(STRINGS "${file}" lines ENCODING UTF-8)
        set(line_num 0)
        foreach(line IN LISTS lines)
            math(EXPR line_num "${line_num} + 1")
            # Ignore pure single-line comments
            if(line MATCHES "^[ \t]*//")
                continue()
            endif()
            foreach(pat IN LISTS FORBIDDEN_PATTERNS)
                if(line MATCHES "${pat}")
                    file(RELATIVE_PATH rel "${GUIKIT_ROOT}" "${file}")
                    string(APPEND VIOLATIONS "  ${rel}:${line_num}: matched '${pat}' -> ${line}\n")
                endif()
            endforeach()
        endforeach()
    endforeach()
endforeach()

if(SCANNED_COUNT LESS 30)
    message(FATAL_ERROR "Expected at least 30 element source files, only scanned ${SCANNED_COUNT}")
endif()

if(NOT "${VIOLATIONS}" STREQUAL "")
    message(FATAL_ERROR
        "Design-System Architecture Guard FAILED!\n"
        "Direct chrome/preset checks are forbidden inside element implementations; route visual physics through theme.Painter() (IStylePainter):\n"
        "${VIOLATIONS}"
    )
endif()

message(STATUS "GuiKit Style Architecture Guard passed (${SCANNED_COUNT} files scanned, 0 violations).")
