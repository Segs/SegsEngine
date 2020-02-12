configure_file(EASTL/SegsEngine_config.h.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/EASTL/SegsEngine_config.h
)
file(GLOB all_includes "${CMAKE_CURRENT_SOURCE_DIR}/EASTL/include/EASTL/*.h")
file(GLOB all_includes_2 "${CMAKE_CURRENT_SOURCE_DIR}/EASTL/include/EASTL/EABase/*.h")
set(SOURCE
EASTL/source/allocator_eastl.cpp
EASTL/source/assert.cpp
EASTL/source/fixed_pool.cpp
EASTL/source/hashtable.cpp
EASTL/source/intrusive_list.cpp
EASTL/source/numeric_limits.cpp
EASTL/source/red_black_tree.cpp
EASTL/source/string.cpp
EASTL/source/thread_support.cpp
${all_includes}
${CMAKE_CURRENT_SOURCE_DIR}/EASTL/SegsEngine_config.h
${INC}
)
if(MSVC)
set(SOURCE_VIS
    EASTL/doc/EASTL.natvis
)
endif()
add_library(EASTL OBJECT ${SOURCE} ${SOURCE_VIS})
add_library(EASTL_Import INTERFACE)

set_property(TARGET EASTL PROPERTY POSITION_INDEPENDENT_CODE ON)
set(eastl_public_headers ${CMAKE_CURRENT_SOURCE_DIR}/EASTL/SegsEngine_config.h)
target_include_directories(EASTL PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/EASTL/include>
    $<INSTALL_INTERFACE:include>
)
target_include_directories(EASTL_Import INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/EASTL/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(EASTL PUBLIC
    $<BUILD_INTERFACE:EASTL_USER_CONFIG_HEADER=\"${CMAKE_CURRENT_SOURCE_DIR}/EASTL/SegsEngine_config.h\">
    $<INSTALL_INTERFACE:EASTL_USER_CONFIG_HEADER=\"include/SegsEngine_config.h\">
)
target_compile_definitions(EASTL_Import INTERFACE
    $<BUILD_INTERFACE:EASTL_USER_CONFIG_HEADER=\"${CMAKE_CURRENT_SOURCE_DIR}/EASTL/SegsEngine_config.h\">
    $<INSTALL_INTERFACE:EASTL_USER_CONFIG_HEADER=\"include/SegsEngine_config.h\">
)

target_compile_definitions(EASTL PRIVATE
    GODOT_EXPORTS
)

install(TARGETS EASTL_Import EXPORT SegsEngine
    ARCHIVE DESTINATION ${DEST_ARCHIVE_DIR}
)
INSTALL( DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/EASTL/include/" DESTINATION include )
