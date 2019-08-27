cmake_minimum_required(VERSION 3.13)
project(godot)

SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
#set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "/usr/bin/time -f \"%E,%M,%C,%O\"")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CMakeParseArguments)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH})

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Tell cmake to honor CXX_VISIBILITY_PRESET and ignore it
cmake_policy(SET CMP0063 NEW)
cmake_policy(SET CMP0065 NEW)
cmake_policy(SET CMP0072 NEW)
cmake_policy(SET CMP0079 NEW)


set(VD_SRC "${VisualDesigner_SOURCE_DIR}/src/visualdesigner")
file(GLOB_RECURSE ALL_PROJECT_FILES
    "*.cpp"
    "*.h"
    "*.glsl"
    "*.xml"
)
source_group(TREE ${CMAKE_CURRENT_LIST_DIR} FILES ${ALL_PROJECT_FILES})


include(LocateQt5)
find_package(Qt5 COMPONENTS Core REQUIRED)

include(GenerateExportHeader)
include(Helpers)
include(PluginSupport)

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


define_target(
    NAME editor
#   MODULES freetype
)
macro(define_per_target_option name description initial_value)
    foreach(tgt ${global_targets})
        option(${tgt}_${name} ${description} ${initial_value})
        list(APPEND ${tgt}_OPTIONS ${tgt}_${name})
        list(REMOVE_DUPLICATES ${tgt}_OPTIONS)
        set(${tgt}_OPTIONS ${${tgt}_OPTIONS} CACHE INTERNAL "")
    endforeach()
endmacro()

define_per_target_option(DISABLE_3D "Disable 3D nodes for smaller executable" OFF)

message(${global_targets})
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
    include(platform/x11/options.cmake)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    # include platform specific options now
    include(platform/windows/options.cmake)
endif()

file(GLOB platform_exporters RELATIVE ${PROJECT_SOURCE_DIR} "platform/*/export/export.cpp" )
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
message(${platform_list})
SET(OPTION_PLATFORM ${first_platform} CACHE STRING "Target platform")
SET_PROPERTY(CACHE OPTION_PLATFORM PROPERTY STRINGS ${platform_list})

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
OPTION(OPTION_VERBOSE "Enable verbose output for the compilation" OFF)
OPTION(OPTION_PROGRESS "Show a progress indicator during build" ON)
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
OPTION(OPTION_NO_EDITOR_SPLASH "Don't use the custom splash screen for the editor" OFF)
OPTION(OPTION_PRECISE_MATH_CHECKS "Math checks use very precise epsilon (useful to debug the engine)" OFF)


# # add default include paths
include_directories(
    ${PROJECT_SOURCE_DIR}/core
    ${PROJECT_SOURCE_DIR}/core/math
    ${PROJECT_SOURCE_DIR}/editor
    ${PROJECT_SOURCE_DIR}/drivers
    ${PROJECT_SOURCE_DIR}
)

if (OPTION_NO_EDITOR_SPLASH)
    add_definitions(-DNO_EDITOR_SPLASH)
endif()
if(NOT OPTION_DEPRECATED)
    add_definitions(-DDISABLE_DEPRECATED)
endif()

if(OPTION_TOOLS)
    add_definitions(-DTOOLS_ENABLED)
endif()
if(OPTION_DISABLE_3D)
    add_definitions(-D_3D_DISABLED)
endif()
if(OPTION_GDSCRIPT)
    add_definitions(-DGDSCRIPT_ENABLED)
endif()
if(OPTION_DISABLE_ADVANCED_GUI)
    add_definitions(-DADVANCED_GUI_DISABLED)
endif()
if(OPTION_MINIZIP)
    add_definitions(-DMINIZIP_ENABLED)
endif()
if(OPTION_PRECISE_MATH_CHECKS)
    add_definitions(-DPRECISE_MATH_CHECKS)
endif()

add_subdirectory("${PROJECT_SOURCE_DIR}/thirdparty/zstd")
add_subdirectory("${PROJECT_SOURCE_DIR}/thirdparty/zlib")


add_subdirectory(utils)

set(doc_class_path "${PROJECT_SOURCE_DIR}/doc/classes")
# all module docs are included, even if the module itself is disabled, this makes the cmake logic much easier, the resulting executable will be larger though
file(GLOB module_docs "${PROJECT_SOURCE_DIR}/modules/*/doc_classes" )
configure_file(doc_paths.cmake ${PROJECT_BINARY_DIR}/doc_paths.txt)

# # build subdirs, the build order is dependent on link order.
add_subdirectory(core) # has to be visited first to set the paths to binary dirs that contain generated files
add_subdirectory(drivers)
add_subdirectory(modules)
add_subdirectory(plugins)
add_subdirectory(servers)
add_subdirectory(scene)
add_subdirectory(editor)
add_subdirectory(platform)
add_subdirectory(main)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    add_subdirectory(platform/x11)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    add_subdirectory(platform/windows)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    add_subdirectory(platform/osx)
endif()
