file(GLOB source_files "*.cpp")
file(GLOB header_files "*.h")

foreach(tgt ${global_targets})
    add_library(${tgt}_scene OBJECT
        ${source_files} ${header_files}
        "${PROJECT_SOURCE_DIR}/thirdparty/misc/easing_equations.cpp"
        "${PROJECT_SOURCE_DIR}/thirdparty/misc/mikktspace.c"
    )

    include(main/sources.cmake)
    include(animation/sources.cmake)
    include(audio/sources.cmake)
    include(2d/sources.cmake)
    include(3d/sources.cmake)
    include(gui/sources.cmake)
    include(resources/sources.cmake)

    target_link_libraries(${tgt}_scene PRIVATE ${tgt}_modules)
    target_link_libraries(${tgt}_scene PUBLIC ${tgt}_core)
    target_link_libraries(${tgt}_scene PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/misc/)
    set_common_target_properties(${tgt}_scene)
endforeach()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${target_CPP} ${target_INCLUDE} ${authserver_standaloneSRCS})
