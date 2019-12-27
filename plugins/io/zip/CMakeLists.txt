
file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )

set_plugin_options(
    NAME zip_access
    CLASSPROP ZipArchive
    TYPE io
    WHEN_SHARED_LIBS zlib_interface
    LIBS zlib
    SOURCES ${source_files} ${header_files} ${thirdparty_sources}
    )
