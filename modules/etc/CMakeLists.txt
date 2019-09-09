foreach(tgt ${global_targets})
    add_library(${tgt}_etc OBJECT
        image_etc.cpp
        image_etc.h
        register_types.cpp
        register_types.h
        texture_loader_pkm.cpp
        texture_loader_pkm.h
    )
    target_link_libraries(${tgt}_etc PUBLIC ${tgt}_core)
    target_link_libraries(${tgt}_etc PRIVATE etc2comp)
    set_module_properties(${tgt} etc)
    set_target_properties (${tgt}_etc PROPERTIES FOLDER ${tgt}/modules/ )

endforeach()
