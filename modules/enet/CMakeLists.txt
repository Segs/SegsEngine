file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )

foreach(tgt ${global_targets})
    add_library(${tgt}_enet OBJECT ${source_files} ${header_files})
    target_compile_definitions(${tgt}_enet PUBLIC GODOT_ENET)
    target_link_libraries(${tgt}_enet PUBLIC ${tgt}_core enet_lib)
    set_module_properties(${tgt} enet)
endforeach()
