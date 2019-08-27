# Godot source files
file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )

# Thirdparty source files
set(thirdparty_sources
)

foreach(tgt ${global_targets})
    # also requires libogg and libvorbis
    add_library(${tgt}_webm OBJECT ${source_files} ${header_files} ${thirdparty_sources})
    if(OPTION_BUILTIN_LIBOGG)
        target_include_directories(${tgt}_webm PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/libogg)
    endif()
    if(OPTION_BUILTIN_LIBVORBIS)
        target_include_directories(${tgt}_webm PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/libvorbis)
    endif()
    target_include_directories(${tgt}_webm PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/libtheora/)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        target_compile_definitions(${tgt}_webm PRIVATE OC_X86_64_ASM)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        target_compile_definitions(${tgt}_webm PRIVATE OC_X86_ASM)
    endif()

    target_link_libraries(${tgt}_webm PUBLIC ${tgt}_core)
    set_common_target_properties(${tgt}_webm)
endforeach()
