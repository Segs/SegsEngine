file(GLOB source_files "*.cpp")
file(GLOB header_files "*.h")
target_sources(edit_tools PRIVATE ${source_files} ${header_files})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${source_files} ${header_files})
