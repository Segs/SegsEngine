find_package(PulseAudio)

file(GLOB source_files "*.cpp")
file(GLOB header_files "*.h")

target_sources(${tgt}_drivers PRIVATE ${source_files} ${header_files})

OPTION( OPTION_${tgt}_PULSEAUDIO "Detect & use pulseaudio" ON)

if(OPTION_${tgt}_PULSEAUDIO)
    if (PULSEAUDIO_FOUND) # 0 means found
        message("Enabling PulseAudio")
        target_compile_definitions(${tgt}_drivers PUBLIC PULSEAUDIO_ENABLED)
    else()
        message("PulseAudio development libraries not found, disabling driver")
    endif()
endif()

target_link_libraries(${tgt}_drivers PRIVATE ${PULSEAUDIO_LIBRARY})

