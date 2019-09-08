file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )

foreach(tgt ${global_targets})
    add_library(${tgt}_ogg OBJECT ${source_files} ${header_files} ${thirdparty_sources})
    target_link_libraries(${tgt}_ogg PUBLIC ${tgt}_core)
    target_link_libraries(${tgt}_ogg PRIVATE ogg)
    set_module_properties(${tgt} ogg)
endforeach()
