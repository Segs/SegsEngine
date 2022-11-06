cmake_minimum_required(VERSION 3.20)
project(SegsEngine)

# Tell cmake to use new policies upto our required version
cmake_policy(VERSION 3.20)
#set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR}/../SegsEngine_installdir/)
# NOTE: when running on linux the following commands allow for ccache based build acceleration
#set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
#set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE /usr/bin/include-what-you-use)
#set(CMAKE_C_INCLUDE_WHAT_YOU_USE /usr/bin/include-what-you-use)
#link_directories(/usr/lib/clang/10.0.1/lib/linux)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing(true)

if(NOT OUTPUT_DIR)
    set(OUTPUT_DIR ${PROJECT_SOURCE_DIR}/bin)
endif()

if(MSVC)
    if (CMAKE_GENERATOR MATCHES "Ninja")
        message(FATAL_ERROR "Ninja generator fails to create a valid build on windows, please select another generator.")
        return()
    endif()

    add_definitions(-D_CRT_SECURE_NO_WARNINGS -DNOMINMAX)
    add_compile_options (/wd4251) # disable warning related to dll interface needed ... clients of class
    add_compile_options (/W2) # disable warning related to dll interface needed ... clients of class
    if (NOT (MSVC_VERSION LESS 1910))
      add_compile_options (/MP)
      add_compile_options (/permissive-)
      #add_compile_options (/d2cgsummary)
      #add_compile_options (/d1reportTime)
      add_compile_options (/diagnostics:caret)
      #add_compile_options (/sdl)
      #add_compile_options (/arch:AVX2)
    endif()
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-sign-compare
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-copy>
    )
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-sign-compare
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-copy>
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-nested-anon-types>
    )
endif()

include(CMakeParseArguments)

SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH})

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(AUTOGEN_TARGETS_FOLDER ON)


include(LocateQt5)
find_package(Qt5 COMPONENTS Core REQUIRED)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

#find_package(Dotnet)
include(GenerateExportHeader)
include(Helpers)
include(PluginSupport)

######################################
include(version.cmake)

option(USE_UNITY_BUILDS "Use unity builds" ON)
option(USE_PRECOMPILED_HEADERS "Use precompiled headers" ON)
option(USE_TRACY_PROFILER "Embed a tracy profiler data collection client" ON)

set(DEFAULT_UNITY_BATCH_SIZE 20)
set(global_targets "" CACHE INTERNAL "")

function(define_target )
    set(oneValueArgs NAME CLASSPROP)
    set(multiValueArgs OPTIONS MODULES SOURCES LIBS)
    cmake_parse_arguments(target_args "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN} )
    list(APPEND global_targets ${target_args_NAME})
    list(REMOVE_DUPLICATES global_targets)
    # store value in global cache
    set(global_targets ${global_targets} CACHE INTERNAL "")

    foreach(mod ${target_args_MODULES})
        add_subdirectory(modules/${mod})
    endforeach()

    set(${target_args_NAME}_MODULES ${target_args_MODULES} CACHE INTERNAL "")
    set(${target_args_NAME}_SOURCES ${${target_args_NAME}_SOURCES} PARENT_SCOPE)
#    add_executable("${target_args_NAME}")
endfunction()
add_library(common_interface INTERFACE)
target_include_directories(common_interface INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/SegsEngine>
)
target_compile_definitions(common_interface INTERFACE
    $<$<CONFIG:Debug>:DEBUG_ENABLED>
    $<$<CONFIG:RelWithDebInfo>:DEBUG_ENABLED>
)
target_compile_options(common_interface INTERFACE
    "$<$<C_COMPILER_ID:MSVC>:/utf-8>"
    "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

add_library(editor_interface INTERFACE)
target_compile_definitions(editor_interface INTERFACE
    TOOLS_ENABLED
    $<$<CONFIG:Debug>:DEBUG_ENABLED>
    $<$<CONFIG:RelWithDebInfo>:DEBUG_ENABLED>
)


target_link_libraries(editor_interface INTERFACE common_interface)

add_library(client_ex_interface INTERFACE) # export client -> thin exe without debug/profiling support

add_library(server_interface INTERFACE) # serve executable
target_compile_definitions(server_interface INTERFACE _3D_DISABLED)

define_target(
    NAME editor
#   MODULES freetype
)

#define_target(
#    NAME client
#   MODULES freetype
#)

macro(define_per_target_option name description initial_value)
    foreach(tgt ${global_targets})
        option(${tgt}_${name} ${description} ${initial_value})
        list(APPEND ${tgt}_OPTIONS ${tgt}_${name})
        list(REMOVE_DUPLICATES ${tgt}_OPTIONS)
        set(${tgt}_OPTIONS ${${tgt}_OPTIONS} CACHE INTERNAL "")
    endforeach()
endmacro()

define_per_target_option(DISABLE_3D "Disable 3D nodes for smaller executable" OFF)
# add global engine targets

foreach(tgt ${global_targets})
    add_library(${tgt}_engine SHARED)

    target_link_libraries(${tgt}_engine PUBLIC Qt5::Core EASTL_Import ${tgt}_interface)
    set_target_properties(${tgt}_engine PROPERTIES AUTOMOC TRUE)
    set_target_properties(${tgt}_engine PROPERTIES AUTORCC_OPTIONS "--compress;7;--verbose")
    set_target_properties(${tgt}_engine PROPERTIES AUTORCC ON)

    set_common_target_properties(${tgt}_engine)
    if(USE_TRACY_PROFILER)
        target_sources(${tgt}_engine PRIVATE thirdparty/tracy/TracyClient.cpp)
        if(WIN32)
            # under windows tracy client needs dbghelp as well
            target_link_libraries(${tgt}_engine PRIVATE dbghelp)
        endif()

    endif()

    add_executable(segs_${tgt} ${PROJECT_SOURCE_DIR}/executable/main.cpp)

    if(UNIX AND NOT APPLE)
        target_link_options(segs_${tgt} PUBLIC -Wl,--exclude-libs,ALL)
    endif()
    target_link_libraries(segs_${tgt} ${tgt}_engine Qt5::Core EASTL_Import)
    set_target_properties(segs_${tgt} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})
    set_target_properties(segs_${tgt} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${OUTPUT_DIR})
    set_target_properties(segs_${tgt} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${OUTPUT_DIR})
    # add selected Qt path to environment on visual studio
    set_target_properties(segs_${tgt} PROPERTIES VS_DEBUGGER_ENVIRONMENT "PATH=${CMAKE_MSVCIDE_RUN_PATH};%PATH%")

    set_target_properties(${tgt}_engine PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})
    set_target_properties(${tgt}_engine PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${OUTPUT_DIR})
    set_target_properties(${tgt}_engine PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${OUTPUT_DIR})

    target_compile_definitions(segs_${tgt} PRIVATE ${godot_${tgt}_DEFINES})
    target_include_directories(segs_${tgt} PRIVATE ${PROJECT_BINARY_DIR})
    if(USE_TRACY_PROFILER)
        #target_sources(segs_${tgt} PRIVATE thirdparty/tracy/TracyClientDLL.cpp)
    endif()
endforeach()

#define_target( NAME client
#)

set(platform_list)  # list of platforms
set(platform_opts)  # options for each platform
set(platform_flags)  # flags for each platform

set(active_platforms)
set(active_platform_ids)
set(platform_exporters)


if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    # include platform specific options now
    include(platform/linuxbsd/options.cmake)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    # include platform specific options now
    include(platform/windows/options.cmake)
endif()

OPTION(OPTION_TOOLS "Build the tools a.k.a. the Godot editor" ON)
OPTION(OPTION_USE_LTO "Use linking time optimization" OFF)
IF(OPTION_USE_LTO)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

OPTION( OPTION_USE_STATIC_CPP "Link libgcc and libstdc++ statically for better portability" OFF)
OPTION( OPTION_USE_SANITIZER "Use LLVM compiler address sanitizer" OFF)
OPTION( OPTION_USE_LEAK_SANITIZER "Use LLVM compiler memory leaks sanitizer (implies use_sanitizer)" OFF)

OPTION( OPTION_SEPARATE_DEBUG_SYMBOLS "Create a separate file with the debug symbols" OFF)
OPTION( OPTION_TOUCH "Enable touch events" ON)

SET(OPTION_DEBUG_SYMBOLS "no" CACHE STRING "debug symbol level")

# # Components
OPTION(OPTION_DEPRECATED "Enable deprecated features" ON)
OPTION(OPTION_GDSCRIPT "Build GDSCript support" ON)
OPTION(OPTION_MINIZIP "Build minizip archive support" ON)
OPTION(OPTION_XAUDIO2 "XAudio2 audio driver" ON)
OPTION(OPTION_XML "XML format support for resources" ON)

# # Advanced options
OPTION(OPTION_DISABLE_ADVANCED_GUI "Disable advanced 3D gui nodes and behaviors" OFF)
OPTION(OPTION_DEV "If yes, alias for verbose=yes warnings=all" OFF)

# # Thirdparty libraries
OPTION(OPTION_BUILTIN_BULLET "Use the builtin bullet library" ON)
OPTION(OPTION_BUILTIN_ENET "Use the builtin enet library" ON)
OPTION(OPTION_BUILTIN_FREETYPE "Use the built-in freetype library" ON)
OPTION(OPTION_BUILTIN_LIBOGG "Use the built-in libogg library" ON)
OPTION(OPTION_BUILTIN_LIBPNG "Use the built-in libpng library" ON)
OPTION(OPTION_BUILTIN_LIBTHEORA "Use the built-in libtheora library" ON)
OPTION(OPTION_BUILTIN_LIBVORBIS "Use the built-in libvorbis library" ON)
OPTION(OPTION_BUILTIN_LIBVPX "Use the built-in libvpx library" ON)
OPTION(OPTION_BUILTIN_LIBWEBP "Use the built-in libwebp library" ON)
OPTION(OPTION_BUILTIN_MBEDTLS "Use the built-in mbedTLS library" ON)
OPTION(OPTION_BUILTIN_OPUS "Use the built-in opus library" ON)
OPTION(OPTION_BUILTIN_PCRE2 "Use the built-in pcre2 library)" ON)
OPTION(OPTION_BUILTIN_RECAST "Use the built-in recast library" ON)
OPTION(OPTION_BUILTIN_SQUISH "Use the built-in squish library" ON)
OPTION(OPTION_BUILTIN_THEKLA_ATLAS "Use the built-in thekla_altas library" ON)
OPTION(OPTION_BUILTIN_ZLIB "Use the built-in zlib library" ON)
OPTION(OPTION_BUILTIN_ZSTD "Use the built-in zstd library" ON)
OPTION(OPTION_PRECISE_MATH_CHECKS "Math checks use very precise epsilon (useful to debug the engine)" OFF)


file(GLOB platform_exporters RELATIVE ${PROJECT_SOURCE_DIR} "platform/*/export/*_export.cpp" )
file(GLOB platform_detectors RELATIVE ${PROJECT_SOURCE_DIR}/platform "platform/*/detect.cmake" )

foreach(detector ${platform_detectors})
    set(can_build FALSE)
    string(REPLACE "/detect.cmake" "" detector2 ${detector} )
    include("platform/${detector}")
    list(APPEND active_platforms ${name})
    list(APPEND active_platform_ids platform/${detector2})
    if(can_build)
        message("Can build for platform ${name}")
    endif()
endforeach()

save_active_platforms(active_platforms active_platform_ids)

# # Target build options

LIST(GET platform_list 0 first_platform)

SET(OPTION_PLATFORM ${first_platform} CACHE STRING "Target platform")
SET_PROPERTY(CACHE OPTION_PLATFORM PROPERTY STRINGS ${platform_list})

# # add default include paths

if(NOT OPTION_DEPRECATED)
    add_definitions(-DDISABLE_DEPRECATED)
endif()

#target_compile_definitions(client_interface INTERACE TOOLS_ENABLED)

if(OPTION_DISABLE_3D)
    add_definitions(-D_3D_DISABLED)
endif()
if(OPTION_DISABLE_ADVANCED_GUI)
    add_definitions(-DADVANCED_GUI_DISABLED)
endif()
if(OPTION_MINIZIP)
    add_definitions(-DMINIZIP_ENABLED)
endif()

#add_custom_target(genexdebug COMMAND ${CMAKE_COMMAND} -E echo "$<$<CONFIG:Debug>:DEBUG_ENABLED>")

add_subdirectory("${PROJECT_SOURCE_DIR}/thirdparty/zstd")


add_subdirectory(utils)

set(doc_class_path "${PROJECT_SOURCE_DIR}/doc/classes")
# all module docs are included, even if the module itself is disabled, this makes the cmake logic much easier, the resulting executable will be larger though
file(GLOB module_docs "${PROJECT_SOURCE_DIR}/modules/*/doc_classes" )
configure_file(doc_paths.cmake ${PROJECT_BINARY_DIR}/doc_paths.txt)

add_subdirectory(thirdparty)
# # build subdirs, the build order is dependent on link order.
add_subdirectory(core) # has to be visited first to set the paths to binary dirs that contain generated files
add_subdirectory(drivers)
add_subdirectory(plugins)
add_subdirectory(modules)
add_subdirectory(servers)
add_subdirectory(scene)
add_subdirectory(editor)
add_subdirectory(platform)
add_subdirectory(main)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    add_subdirectory(platform/linuxbsd)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    add_subdirectory(platform/windows)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    add_subdirectory(platform/osx)
endif()
foreach(FILE ${NO_UNITY_SOURCES})
    set_source_files_properties(${FILE} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
endforeach()

if(USE_UNITY_BUILDS)
    foreach(tgt ${global_targets})
        set_target_properties(${tgt}_engine PROPERTIES UNITY_BUILD ON)
        set_target_properties(${tgt}_engine PROPERTIES UNITY_BUILD_BATCH_SIZE ${DEFAULT_UNITY_BATCH_SIZE})
        if(MSVC)
            target_compile_options(${tgt}_engine PRIVATE /bigobj)
        elseif(MINGW)
            target_compile_options(${tgt}_engine PRIVATE -Wa,-mbig-obj)
        endif()
    endforeach()
endif()
if(USE_PRECOMPILED_HEADERS)
    foreach(tgt ${global_targets})
        target_precompile_headers(${tgt}_engine REUSE_FROM se_core)
    endforeach()
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    set(CUSTOM_INSTALL_CONFIGDIR cmake)
else()
    set(CUSTOM_INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/SegsEngine)
endif()

foreach(tgt ${global_targets})
    if(WIN32)
        install(TARGETS ${tgt}_engine ${tgt}_interface common_interface EXPORT SegsEngine
            ARCHIVE DESTINATION lib
            LIBRARY DESTINATION bin
            RUNTIME DESTINATION bin
            INCLUDES DESTINATION include
        )
    else()
        install(TARGETS ${tgt}_engine ${tgt}_interface common_interface EXPORT SegsEngine
            ARCHIVE DESTINATION lib
            LIBRARY DESTINATION bin
            RUNTIME DESTINATION bin
            INCLUDES DESTINATION include
        )
    endif()
    install(EXPORT SegsEngine
        FILE ${tgt}_engine_Targets.cmake
        NAMESPACE SegsEngine::
        DESTINATION ${CUSTOM_INSTALL_CONFIGDIR}
    )
    target_link_libraries(${tgt}_engine PRIVATE ${LIBRARIES})
    target_link_libraries(${tgt}_engine PUBLIC EnTT::EnTT)
    #get_target_property(all_src ${tgt}_engine SOURCES)
    #source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX all FILES ${all_src})
endforeach()



#
# Install tree package config file
#

include(CMakePackageConfigHelpers)

configure_package_config_file(
    cmake/SegsEngineConfig.cmake.in
    ${CUSTOM_INSTALL_CONFIGDIR}/SegsEngineConfig.cmake
    INSTALL_DESTINATION ${CUSTOM_INSTALL_CONFIGDIR}
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/SegsEngineConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(
    FILES

        ${CMAKE_CURRENT_BINARY_DIR}/${CUSTOM_INSTALL_CONFIGDIR}/SegsEngineConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/SegsEngineConfigVersion.cmake
    DESTINATION ${CUSTOM_INSTALL_CONFIGDIR}
)

export(PACKAGE SegsEngine)

