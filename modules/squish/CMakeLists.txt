# Godot source files
file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )

foreach(tgt ${global_targets})
    add_library(${tgt}_squish OBJECT ${source_files} ${header_files} ${thirdparty_sources})

    target_link_libraries(${tgt}_squish PUBLIC ${tgt}_core)
    target_link_libraries(${tgt}_squish PRIVATE squish)

    set_module_properties(${tgt} squish)
endforeach()
