file(GLOB source_files "*.cpp")
file(GLOB header_files "*.h")
list(APPEND editor_sources ${source_files} ${header_files})
set(editor_sources ${editor_sources} PARENT_SCOPE)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${source_files} ${header_files})
