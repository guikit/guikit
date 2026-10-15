# ThorVG software/CPU vector engine build configuration for GuiKit

set(THORVG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/thorvg")
set(THORVG_CONFIG_DIR "${CMAKE_CURRENT_BINARY_DIR}/thorvg_config")

if (EMSCRIPTEN)
    set(THORVG_THREAD_DEFINE "// #undef THORVG_THREAD_SUPPORT\n")
else()
    set(THORVG_THREAD_DEFINE "#define THORVG_THREAD_SUPPORT 1\n")
endif()

# Generate config.h so ThorVG submodule stays pristine and doesn't require Meson
file(MAKE_DIRECTORY "${THORVG_CONFIG_DIR}")
file(WRITE "${THORVG_CONFIG_DIR}/config.h"
"#ifndef THORVG_CONFIG_H\n"
"#define THORVG_CONFIG_H\n\n"
"#define THORVG_VERSION_STRING \"1.1.0\"\n"
"#define THORVG_CPU_ENGINE_SUPPORT 1\n"
"#define THORVG_SVG_LOADER_SUPPORT 1\n"
"#define THORVG_PNG_LOADER_SUPPORT 1\n"
"#define THORVG_JPG_LOADER_SUPPORT 1\n"
"#define THORVG_SFNT_LOADER_SUPPORT 1\n"
"#define THORVG_TTF_LOADER_SUPPORT 1\n"
"#define THORVG_FILE_IO_SUPPORT 1\n"
"${THORVG_THREAD_DEFINE}"
"#define TVG_STATIC 1\n\n"
"#endif // THORVG_CONFIG_H\n"
)

set(THORVG_SOURCES
    # Common
    ${THORVG_DIR}/src/common/tvgCompressor.cpp
    ${THORVG_DIR}/src/common/tvgMath.cpp
    ${THORVG_DIR}/src/common/tvgStr.cpp

    # Renderer base
    ${THORVG_DIR}/src/renderer/tvgAccessor.cpp
    ${THORVG_DIR}/src/renderer/tvgAnimation.cpp
    ${THORVG_DIR}/src/renderer/tvgCanvas.cpp
    ${THORVG_DIR}/src/renderer/tvgFill.cpp
    ${THORVG_DIR}/src/renderer/tvgInitializer.cpp
    ${THORVG_DIR}/src/renderer/tvgLoaderMgr.cpp
    ${THORVG_DIR}/src/renderer/tvgPaint.cpp
    ${THORVG_DIR}/src/renderer/tvgPicture.cpp
    ${THORVG_DIR}/src/renderer/tvgRender.cpp
    ${THORVG_DIR}/src/renderer/tvgSaver.cpp
    ${THORVG_DIR}/src/renderer/tvgScene.cpp
    ${THORVG_DIR}/src/renderer/tvgShape.cpp
    ${THORVG_DIR}/src/renderer/tvgTaskScheduler.cpp
    ${THORVG_DIR}/src/renderer/tvgText.cpp

    # CPU (Software) engine
    ${THORVG_DIR}/src/renderer/cpu_engine/tvgSwBlendOp.cpp
    ${THORVG_DIR}/src/renderer/cpu_engine/tvgSwFill.cpp
    ${THORVG_DIR}/src/renderer/cpu_engine/tvgSwImage.cpp
    ${THORVG_DIR}/src/renderer/cpu_engine/tvgSwMemPool.cpp
    ${THORVG_DIR}/src/renderer/cpu_engine/tvgSwPostEffect.cpp
    ${THORVG_DIR}/src/renderer/cpu_engine/tvgSwRaster.cpp
    ${THORVG_DIR}/src/renderer/cpu_engine/tvgSwRasterCore.cpp
    ${THORVG_DIR}/src/renderer/cpu_engine/tvgSwRenderer.cpp
    ${THORVG_DIR}/src/renderer/cpu_engine/tvgSwRle.cpp
    ${THORVG_DIR}/src/renderer/cpu_engine/tvgSwShape.cpp
    ${THORVG_DIR}/src/renderer/cpu_engine/tvgSwStroke.cpp
    ${THORVG_DIR}/src/renderer/cpu_engine/tvgSwUtil.cpp

    # Loaders (Raw, SFNT/TTF, SVG, PNG, JPG)
    ${THORVG_DIR}/src/loaders/raw/tvgRawLoader.cpp
    ${THORVG_DIR}/src/loaders/sfnt/tvgOtfReader.cpp
    ${THORVG_DIR}/src/loaders/sfnt/tvgSfntLoader.cpp
    ${THORVG_DIR}/src/loaders/sfnt/tvgSfntReader.cpp
    ${THORVG_DIR}/src/loaders/sfnt/tvgTtfReader.cpp
    ${THORVG_DIR}/src/loaders/svg/tvgSvgBuilder.cpp
    ${THORVG_DIR}/src/loaders/svg/tvgSvgCssStyle.cpp
    ${THORVG_DIR}/src/loaders/svg/tvgSvgLoader.cpp
    ${THORVG_DIR}/src/loaders/svg/tvgSvgPath.cpp
    ${THORVG_DIR}/src/loaders/svg/tvgSvgUtil.cpp
    ${THORVG_DIR}/src/loaders/svg/tvgXmlParser.cpp
    ${THORVG_DIR}/src/loaders/png/tvgLodePng.cpp
    ${THORVG_DIR}/src/loaders/png/tvgLodePngUtil.cpp
    ${THORVG_DIR}/src/loaders/png/tvgPngLoader.cpp
    ${THORVG_DIR}/src/loaders/jpg/tvgJpgd.cpp
    ${THORVG_DIR}/src/loaders/jpg/tvgJpgLoader.cpp
)

add_library(thorvg STATIC ${THORVG_SOURCES})

target_compile_definitions(thorvg PUBLIC
    TVG_STATIC
    NOMINMAX
)

target_include_directories(thorvg PUBLIC
    ${THORVG_DIR}/inc
)

target_include_directories(thorvg PRIVATE
    ${THORVG_CONFIG_DIR}
    ${THORVG_DIR}
    ${THORVG_DIR}/src/common
    ${THORVG_DIR}/src/renderer
    ${THORVG_DIR}/src/renderer/cpu_engine
    ${THORVG_DIR}/src/loaders/raw
    ${THORVG_DIR}/src/loaders/sfnt
    ${THORVG_DIR}/src/loaders/svg
    ${THORVG_DIR}/src/loaders/png
    ${THORVG_DIR}/src/loaders/jpg
)
