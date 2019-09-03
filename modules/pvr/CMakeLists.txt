# Godot source files
file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )

foreach(tgt ${global_targets})
    add_library(${tgt}_pvr OBJECT ${source_files} ${header_files} ${thirdparty_sources})

    target_link_libraries(${tgt}_pvr PRIVATE pvr_lib)
    target_link_libraries(${tgt}_pvr PUBLIC ${tgt}_core)
    set_common_target_properties(${tgt}_pvr)
endforeach()
