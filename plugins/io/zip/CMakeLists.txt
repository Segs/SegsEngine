
file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )

set_plugin_options(
    NAME zip_access
    CLASSPROP ZipArchive
    TYPE io
    LIBS zlib_interface
    SOURCES ${source_files} ${header_files} ${thirdparty_sources}
    )
