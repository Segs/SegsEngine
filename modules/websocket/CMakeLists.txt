file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )

foreach(tgt ${global_targets})
    add_library(${tgt}_websocket OBJECT ${source_files} ${header_files})

    target_link_libraries(${tgt}_websocket PUBLIC ${tgt}_core)
    target_link_libraries(${tgt}_websocket PRIVATE wslay)
    set_module_properties(${tgt} websocket)
endforeach()
