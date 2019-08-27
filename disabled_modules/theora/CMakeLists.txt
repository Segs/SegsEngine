# Godot source files
file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )

# Thirdparty source files
set(thirdparty_sources
    ${PROJECT_SOURCE_DIR}/thirdparty/libtheora/bitpack.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libtheora/cpu.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libtheora/decinfo.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libtheora/decode.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libtheora/dequant.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libtheora/fragment.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libtheora/huffdec.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libtheora/idct.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libtheora/info.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libtheora/internal.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libtheora/quant.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libtheora/state.c
)

set(thirdparty_sources_x86
    ${PROJECT_SOURCE_DIR}/thirdparty/libtheora/x86/mmxfrag.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libtheora/x86/mmxidct.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libtheora/x86/mmxstate.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libtheora/x86/x86state.c
)

set(thirdparty_sources_x86_vc
    ${PROJECT_SOURCE_DIR}/thirdparty/libtheora/x86_vc/mmxfrag.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libtheora/x86_vc/mmxidct.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libtheora/x86_vc/mmxstate.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libtheora/x86_vc/x86state.c
)
IF(MSVC)
    list(APPEND thirdparty_sources ${thirdparty_sources_x86_vc})
else()
    list(APPEND thirdparty_sources ${thirdparty_sources_x86})
endif()


foreach(tgt ${global_targets})
    # also requires libogg and libvorbis
    add_library(${tgt}_theora OBJECT ${source_files} ${header_files} ${thirdparty_sources})
    if(OPTION_BUILTIN_LIBOGG)
        target_include_directories(${tgt}_theora PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/libogg)
    endif()
    if(OPTION_BUILTIN_LIBVORBIS)
        target_include_directories(${tgt}_theora PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/libvorbis)
    endif()
    target_include_directories(${tgt}_theora PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/libtheora/)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        target_compile_definitions(${tgt}_theora PRIVATE OC_X86_64_ASM)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        target_compile_definitions(${tgt}_theora PRIVATE OC_X86_ASM)
    endif()

    target_link_libraries(${tgt}_theora PUBLIC ${tgt}_core)
    set_common_target_properties(${tgt}_theora)
endforeach()
