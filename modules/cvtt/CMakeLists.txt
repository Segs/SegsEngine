file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )

foreach(tgt ${global_targets})
    add_library(${tgt}_cvtt OBJECT ${source_files} ${header_files} ${thirdparty_sources})
    target_link_libraries(${tgt}_cvtt PUBLIC ${tgt}_core cvtt_lib)
    set_common_target_properties(${tgt}_cvtt)
endforeach()
