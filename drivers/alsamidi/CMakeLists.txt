set(source_files
    "midi_driver_alsamidi.cpp"
    "midi_driver_alsamidi.h"
)
add_library(alsamidi OBJECT ${source_files})
add_library(alsamidi_interface INTERFACE)
target_link_libraries(alsamidi_interface INTERFACE ${ALSA_LIBRARIES})

