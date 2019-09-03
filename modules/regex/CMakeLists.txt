file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )

foreach(tgt ${global_targets})
    add_library(${tgt}_regex OBJECT ${source_files} ${header_files} ${thirdparty_sources})
    target_link_libraries(${tgt}_regex PUBLIC ${tgt}_core)
    target_link_libraries(${tgt}_regex PRIVATE regex)
    set_common_target_properties(${tgt}_regex)
endforeach()
