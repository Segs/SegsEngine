file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )
if(OPTION_BUILTIN_LIBOGG)
    set(thirdparty_sources
        ${PROJECT_SOURCE_DIR}/thirdparty/libogg/bitwise.c
        ${PROJECT_SOURCE_DIR}/thirdparty/libogg/framing.c
    )
endif()


foreach(tgt ${global_targets})
    add_library(${tgt}_ogg OBJECT ${source_files} ${header_files} ${thirdparty_sources})
    if(OPTION_BUILTIN_LIBOGG)
        target_include_directories(${tgt}_ogg PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/libogg/)
    endif()
    target_link_libraries(${tgt}_ogg PUBLIC ${tgt}_core)
    set_common_target_properties(${tgt}_ogg)
endforeach()
