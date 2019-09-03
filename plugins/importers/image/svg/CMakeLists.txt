# Godot source files
file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )
# Thirdparty source files
set(thirdparty_sources
    ${PROJECT_SOURCE_DIR}/thirdparty/nanosvg/nanosvg.cc
)
set_plugin_options(
    NAME svg
    CLASSPROP ImageLoaderSVG
    SOURCES ${source_files} ${header_files} ${thirdparty_sources}
    INCLUDES ${PROJECT_SOURCE_DIR}/thirdparty/nanosvg
    DEFINES  SVG_ENABLED
)

