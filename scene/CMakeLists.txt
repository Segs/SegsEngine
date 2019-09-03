file(GLOB_RECURSE source_files "*.cpp")
file(GLOB_RECURSE header_files "*.h")

foreach(tgt ${global_targets})

    list(APPEND editor_sources ${source_files} editor.qrc ${header_files})
    add_library(${tgt}_scene STATIC
        ${source_files} ${header_files}
        "${PROJECT_SOURCE_DIR}/thirdparty/misc/easing_equations.cpp"
        "${PROJECT_SOURCE_DIR}/thirdparty/misc/mikktspace.c"
    )

    target_link_libraries(${tgt}_scene PRIVATE ${tgt}_modules)
    target_link_libraries(${tgt}_scene PUBLIC ${tgt}_core)
    target_include_directories(${tgt}_scene PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/misc/)
    set_common_target_properties(${tgt}_scene)
endforeach()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${source_files} ${header_files})
