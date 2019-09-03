file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )
set(thirdparty_sources

    ${PROJECT_SOURCE_DIR}/thirdparty/wslay/wslay_net.c
    ${PROJECT_SOURCE_DIR}/thirdparty/wslay/wslay_event.c
    ${PROJECT_SOURCE_DIR}/thirdparty/wslay/wslay_queue.c
    ${PROJECT_SOURCE_DIR}/thirdparty/wslay/wslay_stack.c
    ${PROJECT_SOURCE_DIR}/thirdparty/wslay/wslay_frame.c
)

list(APPEND thirdparty_sources ${plat_source_files} ${plat_header_files})

foreach(tgt ${global_targets})
    add_library(${tgt}_websocket OBJECT ${source_files} ${header_files} ${thirdparty_sources})

    if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows") # Winsock
        target_compile_definitions(${tgt}_websocket PRIVATE HAVE_WINSOCK2_H)
    else() # Unix socket
        target_compile_definitions(${tgt}_websocket PRIVATE HAVE_NETINET_IN_H)
    endif()

    target_include_directories(${tgt}_websocket PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/wslay/includes)

    target_link_libraries(${tgt}_websocket PUBLIC ${tgt}_core)
    set_common_target_properties(${tgt}_websocket)
endforeach()
