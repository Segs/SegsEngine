file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )

set(thirdparty_sources
    ${PROJECT_SOURCE_DIR}/thirdparty/miniupnpc/miniupnpc/miniupnpc.c
    ${PROJECT_SOURCE_DIR}/thirdparty/miniupnpc/miniupnpc/upnpcommands.c
    ${PROJECT_SOURCE_DIR}/thirdparty/miniupnpc/miniupnpc/miniwget.c
    ${PROJECT_SOURCE_DIR}/thirdparty/miniupnpc/miniupnpc/upnpdev.c
    ${PROJECT_SOURCE_DIR}/thirdparty/miniupnpc/miniupnpc/igd_desc_parse.c
    ${PROJECT_SOURCE_DIR}/thirdparty/miniupnpc/miniupnpc/minissdpc.c
    ${PROJECT_SOURCE_DIR}/thirdparty/miniupnpc/miniupnpc/minisoap.c
    ${PROJECT_SOURCE_DIR}/thirdparty/miniupnpc/miniupnpc/minixml.c
    ${PROJECT_SOURCE_DIR}/thirdparty/miniupnpc/miniupnpc/connecthostport.c
    ${PROJECT_SOURCE_DIR}/thirdparty/miniupnpc/miniupnpc/receivedata.c
    ${PROJECT_SOURCE_DIR}/thirdparty/miniupnpc/miniupnpc/portlistingparse.c
    ${PROJECT_SOURCE_DIR}/thirdparty/miniupnpc/miniupnpc/upnpreplyparse.c

)

foreach(tgt ${global_targets})
    add_library(${tgt}_upnp OBJECT ${source_files} ${header_files} ${thirdparty_sources})
    target_include_directories(${tgt}_upnp PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/miniupnpc/)
    target_link_libraries(${tgt}_upnp PUBLIC ${tgt}_core)
    target_compile_definitions(${tgt}_upnp PRIVATE MINIUPNP_STATICLIB MINIUPNPC_SET_SOCKET_TIMEOUT)
    set_common_target_properties(${tgt}_upnp)
endforeach()
