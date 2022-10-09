file(GLOB source_files "*.cpp")
file(GLOB header_files "*.h")

foreach(tgt ${global_targets})
    add_library(${tgt}_audio_wasapi STATIC ${source_files} ${header_files})

    target_link_libraries(${tgt}_audio_wasapi PRIVATE ${tgt}_core winmm ksuser)
    set_common_target_properties(${tgt}_audio_wasapi)

    target_link_libraries(${tgt}_drivers PUBLIC ${tgt}_audio_wasapi)
    source_group("audio_wasapi" FILES ${source_files} ${header_files} )
endforeach()
