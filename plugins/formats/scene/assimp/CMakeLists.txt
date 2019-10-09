
file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )

set_plugin_options(
    NAME assimp
    SHARED ON
    CLASSPROP EditorSceneImporterAssimp
    SOURCES ${source_files} ${header_files} ${thirdparty_sources}
    LIBS assimp_lib zlib_interface
    )
