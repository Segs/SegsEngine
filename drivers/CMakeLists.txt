

foreach(tgt ${global_targets})
    OPTION(OPTION_${tgt}_DISABLE_RENDERING "Useful for some tests or a server build" OFF)
    OPTION(OPTION_${tgt}_ENABLE_MIDI "Support for midi access" OFF)

    add_library(${tgt}_drivers OBJECT )

    set(drivers_sources)

    if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        include(alsa/sources.cmake)
        include(pulseaudio/sources.cmake)
    elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
        add_subdirectory(wasapi)
    elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
        add_subdirectory(coreaudio)
    endif()

    # OS drivers
    include(unix/sources.cmake)
    include(windows/sources.cmake)
    # Sounds drivers

    if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
        target_link_libraries(${tgt}_drivers PUBLIC ${tgt}_wasapi)
    endif()


    list(APPEND drivers_sources "register_driver_types.cpp")


    include(gl_context/sources.cmake)
    include(convex_decomp/sources.cmake)
    if(OPTION_${tgt}_ENABLE_MIDI)
        if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
            add_subdirectory(alsamidi)
        elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
            add_subdirectory(coremidi)
        elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
            add_subdirectory(winmidi)
        endif()
    endif()


    target_link_libraries(${tgt}_drivers PUBLIC zlib)
    if(NOT OPTION_${tgt}_DISABLE_RENDERING)
        target_include_directories(${tgt}_drivers PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/..)
        target_include_directories(${tgt}_drivers PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
        include(gles3/sources.cmake)
    else()
        add_subdirectory(dummy)
    endif()

    target_link_libraries(${tgt}_drivers PUBLIC ${tgt}_core)
    target_link_libraries(${tgt}_drivers PUBLIC gles3_shaders)

    target_sources(${tgt}_drivers PRIVATE ${drivers_sources})

    set_common_target_properties(${tgt}_drivers)
    set_target_properties(${tgt}_drivers PROPERTIES COTIRE_ADD_UNITY_BUILD FALSE)
    set_target_properties(${tgt}_drivers PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "${PROJECT_SOURCE_DIR}/core/precompiled.h")
    if(ENABLE_COTIRE)
        cotire(${tgt}_drivers)
    endif()
endforeach()

