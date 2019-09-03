file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )
set(thirdparty_sources
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_auto_possess.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_chartables.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_compile.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_config.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_context.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_dfa_match.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_error.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_find_bracket.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_jit_compile.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_maketables.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_match.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_match_data.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_newline.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_ord2utf.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_pattern_info.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_serialize.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_string_utils.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_study.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_substitute.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_substring.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_tables.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_ucd.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_valid_utf.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_xclass.c

    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_convert.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_extuni.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_script_run.c

)

foreach(tgt ${global_targets})
    add_library(${tgt}_regex OBJECT ${source_files} ${header_files} ${thirdparty_sources})

    target_include_directories(${tgt}_regex PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/)
    target_compile_definitions(${tgt}_regex PRIVATE PCRE2_CODE_UNIT_WIDTH=16 PCRE2_STATIC HAVE_CONFIG_H SUPPORT_JIT)

    target_link_libraries(${tgt}_regex PUBLIC ${tgt}_core)
    set_common_target_properties(${tgt}_regex)
endforeach()
