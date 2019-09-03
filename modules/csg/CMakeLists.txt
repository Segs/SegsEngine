file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )

foreach(tgt ${global_targets})
    add_library(${tgt}_csg OBJECT ${source_files} ${header_files} ${thirdparty_sources})
    target_link_libraries(${tgt}_csg PUBLIC ${tgt}_core)
    set_common_target_properties(${tgt}_csg)
endforeach()
