set(servers_sources)

# Godot's own sources
file(GLOB source_files "*.cpp")
list(APPEND servers_sources ${source_files})

foreach(tgt ${global_targets})
    add_library(${tgt}_servers ${servers_sources})
    include(arvr/sources.cmake)
    include(camera/sources.cmake)

    include(physics/sources.cmake)
    include(physics_2d/sources.cmake)
    include(visual/sources.cmake)
    include(audio/sources.cmake)

    target_link_libraries(${tgt}_servers PUBLIC ${tgt}_core)
    set_common_target_properties(${tgt}_servers)
endforeach()
