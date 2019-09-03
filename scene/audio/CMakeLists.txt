file(GLOB source_files "*.cpp")
list(APPEND scene_sources ${source_files})
set(scene_sources ${scene_sources} PARENT_SCOPE)
