file(GLOB source_files "*.cpp")
list(APPEND editor_sources ${source_files})
set(editor_sources ${editor_sources} PARENT_SCOPE)

