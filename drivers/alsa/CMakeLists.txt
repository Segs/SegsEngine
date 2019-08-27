find_package(ALSA)

set(source_files
    "audio_driver_alsa.cpp"
    "audio_driver_alsa.h"
)

target_sources(${tgt}_drivers PRIVATE ${source_files})
if(ALSA_FOUND)
    message("Enabling ALSA")
    target_compile_definitions(${tgt}_drivers PUBLIC ALSA_ENABLED)
    target_link_libraries(${tgt}_drivers PRIVATE ${ALSA_LIBRARIES})
else()
    message(ALSA libraries not found, disabling driver)
endif()

