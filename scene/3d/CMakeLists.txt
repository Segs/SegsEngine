file(GLOB source_files "*.cpp")
file(GLOB header_files "*.h")
list(APPEND scene_sources ${source_files} ${header_files})
set(scene_sources ${scene_sources} PARENT_SCOPE)
