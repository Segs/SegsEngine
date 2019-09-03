# Godot's own sources
file(GLOB_RECURSE source_files "*.cpp")
file(GLOB_RECURSE header_files "*.h")

foreach(tgt ${global_targets})
    add_library(${tgt}_servers ${source_files} ${header_files})

    target_link_libraries(${tgt}_servers PUBLIC ${tgt}_core)
    set_common_target_properties(${tgt}_servers)
endforeach()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${source_files} ${header_files})
