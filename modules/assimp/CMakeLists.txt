file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )

foreach(tgt ${global_targets})
    add_library(${tgt}_assimp OBJECT ${source_files} ${header_files} )
    target_link_libraries(${tgt}_assimp PUBLIC ${tgt}_core)
    target_link_libraries(${tgt}_assimp PRIVATE assimp_lib)

    set_module_properties(${tgt} assimp)
endforeach()

