foreach(tgt ${global_targets})
    add_library(se_core_${tgt} STATIC
        resource/resource_tools.cpp
        resource/resource_manager_tooling.cpp
        engine_tooling.cpp
        object_${tgt}.cpp
        object_tooling_${tgt}.cpp
        translation_${tgt}.cpp
    )
    target_link_libraries(se_core_${tgt} PUBLIC ${tgt}_interface)
    target_link_libraries(se_core_${tgt} PRIVATE EASTL Qt5::Core EnTT::EnTT)
    set_common_target_properties(se_core_${tgt}) # hidden symbols, exports etc.
endforeach()
