file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )
set(thirdparty_sources)

set_plugin_options(
    NAME bmp
    CLASSPROP ImageLoaderBMP
    TYPE image
    SOURCES ${source_files} ${header_files} ${thirdparty_sources}
    )
