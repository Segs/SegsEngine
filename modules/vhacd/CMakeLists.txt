file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )

foreach(tgt ${global_targets})
    add_library(${tgt}_vhacd OBJECT ${source_files} ${header_files} ${thirdparty_sources})

    target_link_libraries(${tgt}_vhacd PUBLIC ${tgt}_core)
    target_link_libraries(${tgt}_vhacd PRIVATE vhacd)

    set_module_properties(${tgt} vhacd)
endforeach()
