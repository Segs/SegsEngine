file(GLOB_RECURSE source_files "*.cpp")
file(GLOB_RECURSE header_files "*.h")

foreach(tgt ${global_targets})
    target_sources(${tgt}_engine PRIVATE
        ${source_files} ${header_files}
        "${PROJECT_SOURCE_DIR}/thirdparty/misc/easing_equations.cpp"
        "${PROJECT_SOURCE_DIR}/thirdparty/misc/mikktspace.c"
    )

    target_link_libraries(${tgt}_engine PRIVATE ${tgt}_modules)
    # NOTE: this makes feetype a hard dependency, maybe a module should provide an implementation hiding ?
    target_link_libraries(${tgt}_engine PRIVATE freetype)
    target_include_directories(${tgt}_engine PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/misc/)
    set_common_target_properties(${tgt}_engine)
endforeach()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${source_files} ${header_files})
