file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )
set(thirdparty_sources
    ${PROJECT_SOURCE_DIR}/thirdparty/enet/godot.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/enet/callbacks.c
    ${PROJECT_SOURCE_DIR}/thirdparty/enet/compress.c
    ${PROJECT_SOURCE_DIR}/thirdparty/enet/host.c
    ${PROJECT_SOURCE_DIR}/thirdparty/enet/list.c
    ${PROJECT_SOURCE_DIR}/thirdparty/enet/packet.c
    ${PROJECT_SOURCE_DIR}/thirdparty/enet/peer.c
    ${PROJECT_SOURCE_DIR}/thirdparty/enet/protocol.c

)

foreach(tgt ${global_targets})
    add_library(${tgt}_enet OBJECT ${source_files} ${header_files} ${thirdparty_sources})
    target_compile_definitions(${tgt}_enet PUBLIC GODOT_ENET)
    target_include_directories(${tgt}_enet PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/enet/)
    target_link_libraries(${tgt}_enet PUBLIC ${tgt}_core)
    set_common_target_properties(${tgt}_enet)
endforeach()
