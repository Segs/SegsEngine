# Generate AES256 script encryption key
add_custom_command(
    COMMAND editor_to_header encryption ${CMAKE_CURRENT_BINARY_DIR}
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/script_encryption_key.gen.cpp
    DEPENDS editor_to_header
    COMMENT "Generating controllers header"
)

update_version("")


add_custom_command(
    COMMAND editor_to_header license "${PROJECT_SOURCE_DIR}/COPYRIGHT.txt" "${PROJECT_SOURCE_DIR}/LICENSE.md" ${CMAKE_CURRENT_BINARY_DIR}/license.gen.h
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/license.gen.h
    DEPENDS editor_to_header ${PROJECT_SOURCE_DIR}/COPYRIGHT.txt ${PROJECT_SOURCE_DIR}/LICENSE.md
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

add_subdirectory(build_specific)

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
    math/random_number_generator.cpp
    math/random_number_generator.h
    math/triangle_mesh.cpp
    math/triangle_mesh.h

    # cannot be put in common_core since it accesses projectsettings which accesses variant
    os/dir_access.cpp
    os/dir_access.h
    os/file_access.cpp
    os/file_access.h
    # cannot be put in common_core since it accesses variant
    io/json.cpp
    io/json.h
    # cannot be put in common_core since it accesses variant
    os/os.cpp
    os/os.h
    # cannot be put in common_core since it accesses input events
    os/keyboard.cpp
    os/keyboard.h
    os/midi_driver.cpp
    os/midi_driver.h
    # cannot be put in common_core since it accesses variant
    object_db.cpp
    object_db.h

    object.cpp
    object.h
    object_private.h
    os/input.cpp
    os/input.h
    os/main_loop.cpp
    os/main_loop.h
    plugin_interfaces/ImageLoaderInterface.cpp
    plugin_interfaces/ImageLoaderInterface.h
    plugin_interfaces/PackSourceInterface.h
    plugin_interfaces/PluginDeclarations.h
    plugin_interfaces/ResourceImporterInterface.h
    plugin_interfaces/ResourceLoaderInterface.h
    plugin_interfaces/load_params.h
    resource/manifest.cpp
    resource/manifest.h
    resource/resource_manager.h
    resource/resource_manager.cpp
    resource/resource_tools.h
    resource/resource_manager_tooling.h

    bitfield_dynamic.cpp
    bitfield_dynamic.h
    class_db.cpp
    class_db.h
    callable.cpp
    callable.h
    callable_method_pointer.cpp
    callable_method_pointer.h

    command_queue_mt.cpp
    command_queue_mt.h
    compressed_translation.cpp
    compressed_translation.h
    cowdata.cpp
    cowdata.h
    cowdata_impl.h
    engine.cpp
    engine.h
    global_constants.cpp
    global_constants.h
    hashfuncs.h
    image.cpp
    image.h
    image_enum_casters.h
    message_queue.cpp
    message_queue.h
    method_bind.cpp
    method_bind.h
    method_bind_interface.h
    method_enum_caster.h
    method_info.h
    method_ptrcall.h
    object_tooling.h
    pool_vector.cpp
    pool_vector.h

    packed_data_container.cpp
    packed_data_container.h
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
    threaded_callable_queue.h
    translation.cpp
    translation.h
    translation_helpers.cpp
    translation_helpers.h
    type_info.h
    undo_redo.cpp
    undo_redo.h

    array.cpp
    array.h
    dictionary.cpp
    dictionary.h

    variant.cpp
    variant.h
    variant_call.cpp
    variant_op.cpp
    variant_parser.cpp
    variant_parser.h
    ${CMAKE_CURRENT_BINARY_DIR}/version_hash.gen.cpp
)
set(editor_core_sources
    reflection_support/reflection_generator.cpp
    reflection_support/reflection_data.cpp
    reflection_support/reflection_data.h
)
configure_file(configuration.h.cmake configuration.h)
set(jlsignal_sources
    jlsignal/DoublyLinkedList.h
    jlsignal/FastFunc.h
    jlsignal/ObjectPool.cpp
    jlsignal/ObjectPool.h
    jlsignal/ObjectPoolScopedAllocator.h
    jlsignal/ScopedAllocator.h
    jlsignal/SignalBase.cpp
    jlsignal/SignalBase.h
    jlsignal/SignalDefinitions.h
    jlsignal/Signal.h
    #jlsignal/StaticSignalConnectionAllocators.h
    jlsignal/Utils.h
)
#those files are common to all modes editor/projectmanager/client/server/etc.
set(generic_core_sources
    io/compression.cpp
    io/compression.h
    io/ip_address.cpp
    io/ip_address.h
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
    math/convex_hull.cpp
    math/convex_hull.h
    math/camera_matrix.h
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
    math/vector3i.cpp
    math/vector3i.h

    os/memory.cpp
    os/memory.h
    os/mutex.cpp
    os/mutex.h
    os/spin_lock.h
    os/rw_lock.h
    os/semaphore.cpp
    os/semaphore.h
    os/thread.cpp
    os/thread.h
    os/thread_safe.cpp
    os/thread_safe.h
    os/thread_work_pool.cpp
    os/thread_work_pool.h
    os/threaded_array_processor.h

    os/time.h
    os/time.cpp
    service_interfaces/CoreInterface.h

    color.cpp
    color.h
    core_string_names.cpp
    core_string_names.h
    engine_tooling.h
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

    property_info.cpp
    property_info.h
    typesystem_decls.h

    engine_entities.cpp
    engine_entities.h

    hash_map.h
    hash_set.h
    list.h
    map.h
    #ordered_hash_map.h
    oa_hash_map.h
    pair.h
    ring_buffer.h
    string.cpp
    string.h
    self_list.h
    set.h
    ustring.cpp
    ustring.h
    string_utils.h
    string_utils.inl
    vector.cpp
    vector.h
    vset.h

    container_tools.h
    sort_array.h

    version.h
    version_generated.gen.h
    configuration.h

    doc_support/doc_data.cpp
    doc_support/doc_data.h
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
add_subdirectory(debugger)

########################################################################################################################
# Add 'input' entries, cmake failes to generate proper list of sources when core_sources is extended by
# ${CMAKE_CURRENT_BINARY_DIR}/default_controller_mappings.gen.cpp in subdirectory :|
########################################################################################################################
file(GLOB controller_databases "${CMAKE_CURRENT_SOURCE_DIR}/input/gamecontroller_db/*.txt" )

add_custom_command(
    COMMAND editor_to_header controllers ${CMAKE_CURRENT_BINARY_DIR}/input/default_controller_mappings.gen.cpp ${controller_databases}
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/input/default_controller_mappings.gen.cpp
    DEPENDS editor_to_header ${controller_databases}
    COMMENT "Generating controllers header"
)

file(GLOB source_files "input/*.cpp")
file(GLOB headers_files "input/*.h")
list(APPEND core_sources ${source_files} ${headers_files} ${CMAKE_CURRENT_BINARY_DIR}/input/default_controller_mappings.gen.cpp)

add_library(se_core STATIC ${generic_core_sources} ${jlsignal_sources})
set_target_properties(se_core PROPERTIES AUTOMOC TRUE)

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
#zstd_internal is in public part of target_link_libraries to propagate it's flags to all users of se_core
target_link_libraries(se_core PUBLIC EASTL core_service_interfaces Qt5::Core zstd_internal EnTT::EnTT)
target_link_libraries(se_core PRIVATE minizip thirdparty_misc stb_rect_pack)
target_include_directories(se_core PUBLIC ${PROJECT_BINARY_DIR} ${PROJECT_SOURCE_DIR})
set_common_target_properties(se_core)
# TODO: this should use interface library common to all targets!
target_link_libraries(se_core PRIVATE common_interface)

#add_subdirectory(helper)
# Build it all as a library
# Make binders

foreach(tgt ${global_targets})
    add_library(${tgt}_core STATIC
        ${CMAKE_CURRENT_BINARY_DIR}/script_encryption_key.gen.cpp
        ${core_sources} ${tool_core_sources} ${${tgt}_core_sources}
        service_interfaces/CoreInterface.h
    )
    if(USE_PRECOMPILED_HEADERS)
        foreach(tgt ${global_targets})
            target_precompile_headers(${tgt}_core REUSE_FROM se_core)
        endforeach()
    endif()

    target_link_libraries(${tgt}_core PRIVATE core_plugin_interfaces ${tgt}_interface)
    target_link_libraries(${tgt}_core PUBLIC se_core se_core_${tgt} EASTL_Import zlib_interface concurrent_queue)

    target_link_libraries(${tgt}_engine PRIVATE EASTL minizip zlib)
    target_link_libraries(${tgt}_engine INTERFACE EASTL_Import minizip_interface)
    INSTALL (
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DESTINATION include/SegsEngine
        FILES_MATCHING PATTERN "*.h" PATTERN "*.inl"
    )

    target_include_directories(${tgt}_core PUBLIC ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/..)
    target_include_directories(${tgt}_core PRIVATE ${Qt5Core_PRIVATE_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/include)
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

file(
     GLOB_RECURSE tree_source_list RELATIVE
     "${CMAKE_CURRENT_SOURCE_DIR}"
     *.c *.cpp *.h
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${tree_source_list})
