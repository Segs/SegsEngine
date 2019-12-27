file(GLOB source_files "*.cpp")

foreach(tgt ${global_targets})
    add_library(${tgt}_wasapi STATIC ${source_files})

    target_link_libraries(${tgt}_wasapi PRIVATE ${tgt}_core winmm ksuser)
    set_common_target_properties(${tgt}_wasapi)

    target_link_libraries(${tgt}_drivers PUBLIC ${tgt}_wasapi)
endforeach()
