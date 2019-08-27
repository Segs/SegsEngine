set(core_sources)

# Generate AES256 script encryption key
add_custom_command(
    COMMAND editor_to_header encryption ${CMAKE_CURRENT_BINARY_DIR}
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/script_encryption_key.gen.cpp
    DEPENDS editor_to_header
    COMMENT "Generating controllers header"
)

update_version("")

# Add required thirdparty code. Header paths are hardcoded, we don't need to append
# to the include path (saves a few chars on the compiler invocation for touchy MSVC...)
set(thirdparty_sources
    # C sources
    ${PROJECT_SOURCE_DIR}/thirdparty/misc/fastlz.c
    ${PROJECT_SOURCE_DIR}/thirdparty/misc/smaz.c
    # C++ sources
    ${PROJECT_SOURCE_DIR}/thirdparty/misc/hq2x.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/misc/pcg.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/misc/triangulator.cpp
# Minizip library, can be unbundled in theory
# However, our version has some custom modifications, so it won't compile with the system one
    ${PROJECT_SOURCE_DIR}/thirdparty/minizip/ioapi.c
    ${PROJECT_SOURCE_DIR}/thirdparty/minizip/unzip.c
    ${PROJECT_SOURCE_DIR}/thirdparty/minizip/zip.c
)
list(APPEND core_sources ${thirdparty_sources})


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
file(GLOB source_files "*.cpp")
file(GLOB header_files "*.h")
list(APPEND core_sources ${source_files} ${header_files})
list(APPEND core_sources ${CMAKE_CURRENT_BINARY_DIR}/license.gen.h)
list(APPEND core_sources ${CMAKE_CURRENT_BINARY_DIR}/authors.gen.h)
list(APPEND core_sources ${CMAKE_CURRENT_BINARY_DIR}/donors.gen.h)

add_subdirectory(plugin_interfaces)
add_subdirectory(service_interfaces)
add_subdirectory(crypto)
add_subdirectory(os)
add_subdirectory(math)
add_subdirectory(io)
add_subdirectory(bind)
#add_subdirectory(helper)
# Build it all as a library
# Make binders
add_custom_command(
    COMMAND make_binders 0 ${CMAKE_CURRENT_BINARY_DIR}/method_bind.gen.h
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/method_bind.gen.h
    DEPENDS make_binders
    COMMENT "Generating method binders"
)
add_custom_command(
    COMMAND make_binders 1 ${CMAKE_CURRENT_BINARY_DIR}/method_bind_ext.gen.h
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/method_bind_ext.gen.h
    DEPENDS make_binders
    COMMENT "Generating ext method binders"
)
add_custom_command(
    COMMAND make_binders 2 ${CMAKE_CURRENT_BINARY_DIR}/method_bind_free_func.gen.h
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/method_bind_free_func.gen.h
    DEPENDS make_binders
    COMMENT "Generating free func ext method binders"
)

foreach(tgt ${global_targets})
    OPTION(OPTION_${tgt}_USE_PTRCALL "Use ptrcall in GDNative" ON)

    add_library(${tgt}_core STATIC
        ${CMAKE_CURRENT_BINARY_DIR}/method_bind.gen.h
        ${CMAKE_CURRENT_BINARY_DIR}/method_bind_ext.gen.h
        ${CMAKE_CURRENT_BINARY_DIR}/method_bind_free_func.gen.h
        ${CMAKE_CURRENT_BINARY_DIR}/script_encryption_key.gen.cpp
        ${core_sources}
        service_interfaces/CoreInterface.h
    )


    target_link_libraries(${tgt}_core PRIVATE core_plugin_interfaces) # access to embedded resources
    target_link_libraries(${tgt}_core PUBLIC core_service_interfaces Qt5::Core)
    target_link_libraries(${tgt}_core PUBLIC zstd_internal zlib )

    target_compile_definitions(${tgt}_core PUBLIC BUILTIN_CERTS_ENABLED)

    if(OPTION_${tgt}_USE_PTRCALL)
        target_compile_definitions(${tgt}_core PUBLIC PTRCALL_ENABLED)
    endif()

    target_compile_options(${tgt}_core PUBLIC
            $<$<CXX_COMPILER_ID:MSVC>:/W3>
            $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wno-unused-parameter>
            $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wno-unused-parameter>
    )
    target_include_directories(${tgt}_core PUBLIC ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/..)
    target_include_directories(${tgt}_core PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/include)

    set_common_target_properties(${tgt}_core)
endforeach()
