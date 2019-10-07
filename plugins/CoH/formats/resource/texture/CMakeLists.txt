
file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )

set_plugin_options(
    NAME coh_texture
    SHARED ON
    CLASSPROP ResourceImporterCoHTexture
    TYPE resource
    SOURCES ${source_files} ${header_files} ${thirdparty_sources}
)


