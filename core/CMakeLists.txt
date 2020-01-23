# Generate AES256 script encryption key
add_custom_command(
    COMMAND editor_to_header encryption ${CMAKE_CURRENT_BINARY_DIR}
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/script_encryption_key.gen.cpp
    DEPENDS editor_to_header
    COMMENT "Generating controllers header"
)

update_version("")


add_custom_command(
    COMMAND editor_to_header license "${PROJECT_SOURCE_DIR}/COPYRIGHT.txt" "${PROJECT_SOURCE_DIR}/LICENSE.txt" ${CMAKE_CURRENT_BINARY_DIR}/license.gen.h
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/license.gen.h
    DEPENDS editor_to_header ${PROJECT_SOURCE_DIR}/COPYRIGHT.txt ${PROJECT_SOURCE_DIR}/LICENSE.txt
    COMMENT "Generating license header"
)
add_custom_command(
    COMMAND editor_to_header authors "${PROJECT_SOURCE_DIR}/AUTHORS.md" ${CMAKE_CURRENT_BINARY_DIR}/authors.gen.h
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/authors.gen.h
    DEPENDS editor_to_header ${PROJECT_SOURCE_DIR}/AUTHORS.md
    COMMENT "Generating authors header"
)
add_custom_command(
    COMMAND editor_to_header donors "${PROJECT_SOURCE_DIR}/DONORS.md" ${CMAKE_CURRENT_BINARY_DIR}/donors.gen.h
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/donors.gen.h
    DEPENDS editor_to_header ${PROJECT_SOURCE_DIR}/DONORS.md
    COMMENT "Generating donors header"
)

# Godot's own sources
set(core_sources
    bind/core_bind.cpp
    bind/core_bind.h
    crypto/crypto.cpp
    crypto/crypto.h
    crypto/crypto_core.cpp
    crypto/crypto_core.h
    crypto/hashing_context.cpp
    crypto/hashing_context.h
    io/config_file.cpp
    io/config_file.h
    io/file_access_buffered.cpp
    io/file_access_buffered.h
    io/file_access_buffered_fa.h
    io/file_access_compressed.cpp
    io/file_access_compressed.h
    io/file_access_encrypted.cpp
    io/file_access_encrypted.h
    io/file_access_memory.cpp
    io/file_access_memory.h
    io/file_access_network.cpp
    io/file_access_network.h
    io/file_access_pack.cpp
    io/file_access_pack.h
    io/http_client.cpp
    io/http_client.h
    io/image_loader.cpp
    io/image_loader.h
    io/image_saver.cpp
    io/image_saver.h
    io/ip.cpp
    io/ip.h
    io/marshalls.cpp
    io/marshalls.h
    io/multiplayer_api.cpp
    io/multiplayer_api.h
    io/net_socket.cpp
    io/net_socket.h
    io/networked_multiplayer_peer.cpp
    io/networked_multiplayer_peer.h
    io/networked_multiplayer_peer_enum_casters.h
    io/packet_peer.cpp
    io/packet_peer.h
    io/packet_peer_udp.cpp
    io/packet_peer_udp.h
    io/pck_packer.cpp
    io/pck_packer.h
    io/resource_format_binary.cpp
    io/resource_format_binary.h
    io/resource_format_loader.h
    io/resource_importer.cpp
    io/resource_importer.h
    io/resource_loader.cpp
    io/resource_loader.h
    io/resource_saver.cpp
    io/resource_saver.h
    io/stream_peer.cpp
    io/stream_peer.h
    io/stream_peer_ssl.cpp
    io/stream_peer_ssl.h
    io/stream_peer_tcp.cpp
    io/stream_peer_tcp.h
    io/tcp_server.cpp
    io/tcp_server.h
    io/translation_loader_po.cpp
    io/translation_loader_po.h
    io/xml_parser.cpp
    io/xml_parser.h
    math/a_star.cpp
    math/a_star.h
    math/bsp_tree.cpp
    math/bsp_tree.h
    math/disjoint_set.cpp
    math/disjoint_set.h
    math/expression.cpp
    math/expression.h
    math/octree.h
    math/random_number_generator.cpp
    math/random_number_generator.h
    math/triangle_mesh.cpp
    math/triangle_mesh.h

    os/alloc_wrapper.h
    os/input.cpp
    os/input.h
    os/input_event.cpp
    os/input_event.h
    os/main_loop.cpp
    os/main_loop.h
    plugin_interfaces/ImageLoaderInterface.cpp
    plugin_interfaces/ImageLoaderInterface.h
    plugin_interfaces/PackSourceInterface.h
    plugin_interfaces/PluginDeclarations.h
    plugin_interfaces/ResourceImporterInterface.h
    plugin_interfaces/ResourceLoaderInterface.h
    plugin_interfaces/load_params.h

    class_db.cpp
    class_db.h
    command_queue_mt.cpp
    command_queue_mt.h
    compressed_translation.cpp
    compressed_translation.h
    cowdata.cpp
    cowdata.h
    cowdata_impl.h
    engine.cpp
    engine.h
    func_ref.cpp
    func_ref.h
    global_constants.cpp
    global_constants.h
    hashfuncs.h
    image.cpp
    image.h
    image_enum_casters.h
    input_map.cpp
    input_map.h
    message_queue.cpp
    message_queue.h
    method_arg_casters.h
    method_bind.cpp
    method_bind.h
    method_bind_interface.h
    method_enum_caster.h
    method_info.h
    method_ptrcall.h
    object_tooling.h
    object_tooling.cpp
    object_db.h
    packed_data_container.cpp
    packed_data_container.h
    precompiled.h
    project_settings.cpp
    project_settings.h
    ref_ptr.cpp
    ref_ptr.h
    reference.cpp
    reference.h
    register_core_types.cpp
    register_core_types.h
    resource.cpp
    resource.h
    rid.cpp
    rid.h
    script_debugger_local.cpp
    script_debugger_local.h
    script_language.cpp
    script_language.h
    translation.cpp
    translation.h
    translation_helpers.h
    type_info.h
    undo_redo.cpp
    undo_redo.h
    variant.cpp
    variant.h
    variant_call.cpp
    variant_op.cpp
    variant_parser.cpp
    variant_parser.h
)

configure_file(configuration.h.cmake configuration.h)
#those files are common to all modes editor/projectmanager/client/server/etc.
set(generic_core_sources
    io/compression.cpp
    io/compression.h
    io/ip_address.cpp
    io/ip_address.h
    io/json.cpp
    io/json.h
    io/zip_io.cpp
    io/zip_io.h

    io/logger.cpp
    io/logger.h
    rotated_file_loger.h

    math/aabb.cpp
    math/aabb.h
    math/audio_frame.cpp
    math/audio_frame.h
    math/basis.cpp
    math/basis.h
    math/camera_matrix.cpp
    math/camera_matrix.h
    math/delaunay.h
    math/face3.cpp
    math/face3.h
    math/geometry.cpp
    math/geometry.h
    math/math_defs.h
    math/math_funcs.cpp
    math/math_funcs.h
    math/plane.cpp
    math/plane.h
    math/quat.cpp
    math/quat.h
    math/quick_hull.cpp
    math/quick_hull.h
    math/random_pcg.cpp
    math/random_pcg.h
    math/rect2.cpp
    math/rect2.h
    math/transform.cpp
    math/transform.h
    math/transform_2d.cpp
    math/transform_2d.h
    math/triangulate.cpp
    math/triangulate.h
    math/vector2.cpp
    math/vector2.h
    math/vector3.cpp
    math/vector3.h

    os/dir_access.cpp
    os/dir_access.h
    os/file_access.cpp
    os/file_access.h
    os/keyboard.cpp
    os/keyboard.h
    os/memory.cpp
    os/memory.h
    os/midi_driver.cpp
    os/midi_driver.h
    os/os.cpp
    os/os.h
    os/mutex.cpp
    os/mutex.h
    os/rw_lock.cpp
    os/rw_lock.h
    os/semaphore.cpp
    os/semaphore.h
    os/thread.cpp
    os/thread.h
    os/thread_dummy.cpp
    os/thread_dummy.h
    os/thread_safe.cpp
    os/thread_safe.h
    os/threaded_array_processor.h

    service_interfaces/CoreInterface.h

    array.cpp
    array.h
    dictionary.cpp
    dictionary.h

    color.cpp
    color.h
    core_string_names.cpp
    core_string_names.h
    error_list.h
    error_macros.cpp
    error_macros.h
    external_profiler.h
    forward_decls.h
    godot_export.h
    image_data.h
    node_path.cpp
    node_path.h
    print_string.cpp
    print_string.h
    safe_refcount.cpp
    safe_refcount.h
    string_builder.cpp
    string_builder.h
    string_formatter.h
    string_name.cpp
    string_name.h
    timing_assert.h
    typedefs.h

    object.cpp
    object.h

    property_info.cpp
    property_info.h
    typesystem_decls.h

    hash_map.h
    list.h
    map.h
    ordered_hash_map.h
    oa_hash_map.h
    pair.h
    pool_vector.cpp
    pool_vector.h
    ring_buffer.h
    se_string.cpp
    se_string.h
    self_list.h
    set.h
    ustring.cpp
    ustring.h
    string_utils.h
    string_utils.inl
    vector.cpp
    vector.h
    vmap.h
    vset.h

    container_tools.h
    sort_array.h

    version.h
    version_generated.gen.h
    configuration.h
)
set(tool_core_sources
    math/math_fieldwise.cpp
    math/math_fieldwise.h
)

list(APPEND core_sources ${CMAKE_CURRENT_BINARY_DIR}/license.gen.h)
list(APPEND core_sources ${CMAKE_CURRENT_BINARY_DIR}/authors.gen.h)
list(APPEND core_sources ${CMAKE_CURRENT_BINARY_DIR}/donors.gen.h)

add_subdirectory(plugin_interfaces)
add_subdirectory(service_interfaces)
add_subdirectory(crypto)

add_library(se_core STATIC ${generic_core_sources})
set_target_properties(se_core PROPERTIES AUTOMOC TRUE)
#target_precompile_headers(se_core PRIVATE precompiled.h)
if(USE_UNITY_BUILDS)
    set_target_properties(se_core PROPERTIES UNITY_BUILD ON)
    set_target_properties(se_core PROPERTIES UNITY_BUILD_BATCH_SIZE ${DEFAULT_UNITY_BATCH_SIZE})
endif()
if(USE_PRECOMPILED_HEADERS)
	
    target_precompile_headers(se_core
      PRIVATE
      "$<$<COMPILE_LANGUAGE:C>:<stddef.h$<ANGLE-R>>" # had to add this, otherwise the generated c-header was buggy
      $<$<COMPILE_LANGUAGE:CXX>:core/precompiled.h>
    )
    #Fixme: under visual studio precompiled header is not used when we're doing unity builds as well!
    set_property(SOURCE ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/se_core.dir/cmake_pch.cxx PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
endif()

target_link_libraries(se_core PUBLIC core_service_interfaces Qt5::Core)
target_link_libraries(se_core PRIVATE EASTL zstd_internal minizip thirdparty_misc )
target_include_directories(se_core PUBLIC ${PROJECT_BINARY_DIR})
set_common_target_properties(se_core)

#add_subdirectory(helper)
# Build it all as a library
# Make binders
foreach(tgt ${global_targets})
    OPTION(OPTION_${tgt}_USE_PTRCALL "Use ptrcall in GDNative" OFF)

    add_library(${tgt}_core STATIC
        ${CMAKE_CURRENT_BINARY_DIR}/script_encryption_key.gen.cpp
        ${core_sources} ${tool_core_sources}
        service_interfaces/CoreInterface.h
    )
if(USE_PRECOMPILED_HEADERS)
    foreach(tgt ${global_targets})
        target_precompile_headers(${tgt}_core REUSE_FROM se_core)
    endforeach()
endif()

    target_link_libraries(${tgt}_core PRIVATE core_plugin_interfaces )
    target_link_libraries(${tgt}_core PUBLIC se_core)
    target_link_libraries(${tgt}_core PUBLIC EASTL_Import zlib_interface)



    target_link_libraries(${tgt}_engine PRIVATE EASTL )
    target_link_libraries(${tgt}_engine INTERFACE EASTL_Import )
    target_link_libraries(${tgt}_engine PRIVATE minizip)
    target_link_libraries(${tgt}_engine INTERFACE minizip_interface)
    INSTALL (
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DESTINATION include/SegsEngine
        FILES_MATCHING PATTERN "*.h"
    )

    target_include_directories(${tgt}_core PRIVATE ${Qt5Core_PRIVATE_INCLUDE_DIRS})
    target_include_directories(${tgt}_core PUBLIC ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/..)
    target_include_directories(${tgt}_core PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/include)
    set_target_properties(${tgt}_core PROPERTIES AUTOMOC TRUE)

    target_include_directories(${tgt}_engine INTERFACE
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
        $<INSTALL_INTERFACE:include/SegsEngine>
    )

    set_common_target_properties(${tgt}_core)
if(USE_UNITY_BUILDS)
    set_target_properties(${tgt}_core PROPERTIES UNITY_BUILD ON)
    set_target_properties(${tgt}_core PROPERTIES UNITY_BUILD_BATCH_SIZE ${DEFAULT_UNITY_BATCH_SIZE})
endif()

endforeach()
