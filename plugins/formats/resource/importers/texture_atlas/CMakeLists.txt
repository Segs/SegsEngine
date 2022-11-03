
file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )
set(resource_files texture_atlas.qrc )

set_plugin_options(
    NAME texture_atlas
    CLASSPROP ResourceImporterTextureAtlas
    TYPE resource
    SOURCES ${source_files} ${header_files} ${thirdparty_sources} ${resource_files}
    )
