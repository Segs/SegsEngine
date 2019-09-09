# Godot source files
file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )
# Thirdparty source files
# Thirdparty code
set(thirdparty_sources
    ${PROJECT_SOURCE_DIR}/thirdparty/libpng/png.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libpng/pngerror.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libpng/pngget.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libpng/pngmem.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libpng/pngpread.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libpng/pngread.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libpng/pngrio.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libpng/pngrtran.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libpng/pngrutil.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libpng/pngset.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libpng/pngtrans.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libpng/pngwio.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libpng/pngwrite.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libpng/pngwtran.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libpng/pngwutil.c
)
set_plugin_options(
    NAME png
    CLASSPROP ImageLoaderPNG
    SOURCES ${source_files} ${header_files} ${thirdparty_sources}
    INCLUDES ${PROJECT_SOURCE_DIR}/thirdparty/zlib ${driver_includes} ${PROJECT_SOURCE_DIR}/thirdparty/libpng
    LIBS zlib
    DEFINES PNG_ARM_NEON_OPT=0
)

