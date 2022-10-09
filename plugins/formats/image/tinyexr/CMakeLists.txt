# Godot source files
file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )

# Thirdparty source files
set(thirdparty_sources
    ${PROJECT_SOURCE_DIR}/thirdparty/tinyexr/tinyexr.cc
)

set_plugin_options(
    NAME tinyexr
    CLASSPROP ImageLoaderTinyEXR
    SOURCES ${source_files} ${header_files} ${thirdparty_sources}
    LIBS zlib_interface
    INCLUDES ${PROJECT_SOURCE_DIR}/thirdparty/tinyexr
    DEFINES TINYEXR_USE_THREAD
)
