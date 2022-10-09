# Godot source files
file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )

set_plugin_options(
    NAME png
    CLASSPROP ImageLoaderPNG
    SOURCES ${source_files} ${header_files} ${thirdparty_sources}
    INCLUDES ${driver_includes}
    LIBS internal_png zlib_interface
    DEFINES PNG_ARM_NEON_OPT=0
)

