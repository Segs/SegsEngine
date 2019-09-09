# Godot source files
file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )

# Thirdparty source files

foreach(tgt ${global_targets})
    add_library(${tgt}_bullet OBJECT ${source_files} ${header_files} ${thirdparty_sources})
    target_link_libraries(${tgt}_bullet PRIVATE bullet)
    target_link_libraries(${tgt}_bullet PUBLIC ${tgt}_core)
    set_module_properties(${tgt} bullet)
endforeach()
