file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )

foreach(tgt ${global_targets})
    add_library(${tgt}_upnp OBJECT ${source_files} ${header_files})
    target_link_libraries(${tgt}_upnp PUBLIC ${tgt}_core)
    target_link_libraries(${tgt}_upnp PRIVATE miniupnpc)
    set_module_properties(${tgt} upnp)
endforeach()
