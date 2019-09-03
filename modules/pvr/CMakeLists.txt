# Godot source files
file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )

# Thirdparty source files
set(thirdparty_sources
    ${PROJECT_SOURCE_DIR}/thirdparty/pvrtccompressor/BitScale.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/pvrtccompressor/MortonTable.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/pvrtccompressor/PvrTcDecoder.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/pvrtccompressor/PvrTcEncoder.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/pvrtccompressor/PvrTcPacket.cpp
)



foreach(tgt ${global_targets})
    add_library(${tgt}_pvr OBJECT ${source_files} ${header_files} ${thirdparty_sources})

    target_include_directories(${tgt}_pvr PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/pvrtccompressor/)
    target_link_libraries(${tgt}_pvr PRIVATE ${ALSA_LIBRARIES})

    target_link_libraries(${tgt}_pvr PUBLIC ${tgt}_core)
    set_common_target_properties(${tgt}_pvr)
endforeach()
