set(core_sources)

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
file(GLOB_RECURSE source_files "*.cpp")
file(GLOB_RECURSE header_files "*.h")
list(APPEND core_sources ${source_files} ${header_files})

list(APPEND core_sources ${CMAKE_CURRENT_BINARY_DIR}/license.gen.h)
list(APPEND core_sources ${CMAKE_CURRENT_BINARY_DIR}/authors.gen.h)
list(APPEND core_sources ${CMAKE_CURRENT_BINARY_DIR}/donors.gen.h)

add_subdirectory(plugin_interfaces)
add_subdirectory(service_interfaces)
add_subdirectory(crypto)

#add_subdirectory(helper)
# Build it all as a library
# Make binders
foreach(tgt ${global_targets})
    OPTION(OPTION_${tgt}_USE_PTRCALL "Use ptrcall in GDNative" ON)

    add_library(${tgt}_core STATIC
        ${CMAKE_CURRENT_BINARY_DIR}/script_encryption_key.gen.cpp
        ${core_sources}
        service_interfaces/CoreInterface.h
    )


    target_link_libraries(${tgt}_core PRIVATE core_plugin_interfaces ) # access to embedded resources
    target_link_libraries(${tgt}_core PUBLIC core_service_interfaces Qt5::Core)
    target_link_libraries(${tgt}_core PUBLIC zstd_internal thirdparty_misc )
    target_link_libraries(${tgt}_core PRIVATE EASTL )
    target_link_libraries(${tgt}_core INTERFACE EASTL_Import )

    target_compile_definitions(${tgt}_core PUBLIC BUILTIN_CERTS_ENABLED)
    target_include_directories(${tgt}_core PRIVATE ${Qt5Core_PRIVATE_INCLUDE_DIRS})
    if(OPTION_${tgt}_USE_PTRCALL)
        target_compile_definitions(${tgt}_core PUBLIC PTRCALL_ENABLED)
    endif()

    target_compile_options(${tgt}_core PUBLIC
            $<$<CXX_COMPILER_ID:MSVC>:/W3>
            $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wno-unused-parameter>
            $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wno-unused-parameter -Wno-deprecated-copy>
    )
    target_include_directories(${tgt}_core PUBLIC ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/..)
    target_include_directories(${tgt}_core PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/include)

    set_common_target_properties(${tgt}_core)
endforeach()
