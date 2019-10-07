#TODO: consider fixing/reducing size of those plugins.
set(source_files
    layered_texture_impl.h
    layered_texture_impl.cpp
)
set(plugin_sources
    resource_importer_layered_texture.h
)

set_plugin_options(
    NAME texture_array
    CLASSPROP ResourceImporterTextureArray
    TYPE resource
    SOURCES ${source_files} ${plugin_sources}
    )

set(plugin_sources
    resource_importer_atlas_texture.h
)
set_plugin_options(
    NAME texture_3d
    CLASSPROP ResourceImporter3DTexture
    TYPE resource
    SOURCES ${source_files} ${plugin_sources}
)
