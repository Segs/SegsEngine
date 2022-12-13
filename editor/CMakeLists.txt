set(editor_sources)
set(exporter_sources)
set(editor_sources_gen)
set(editor_3rdparty)

if (NOT ("editor" IN_LIST global_targets))
    return()
endif()

add_library(edit_tools STATIC)
set_common_target_properties(edit_tools)
target_link_libraries(edit_tools PUBLIC editor_interface)

add_subdirectory(plugin_interfaces)

list(APPEND editor_sources
    doc/doc_builder.cpp
    doc/doc_builder.h

    fileserver/editor_file_server.cpp
    fileserver/editor_file_server.h
)
add_subdirectory(import)
add_subdirectory(plugins)

# Register exporters
set(reg_exporters_inc "#include \"editor/register_exporters.h\"\n")
set(reg_exporters "void register_exporters() {\n")
foreach(e ${platform_exporters})
    string(REPLACE "/" ";" path_parts ${e})
    list(APPEND exporter_sources ${PROJECT_SOURCE_DIR}/${e})
    list(GET path_parts 1 exporter_name)

    source_group(exporters/${exporter_name} ${PROJECT_SOURCE_DIR}/${e})

    string(APPEND reg_exporters "    register_${exporter_name}_exporter();\n")
    string(APPEND reg_exporters_inc "#include \"platform/${exporter_name}/export/${exporter_name}_export.h\"\n")
endforeach()
string(APPEND reg_exporters "}\n")
configure_file(register_exporters.cpp.cmake ${CMAKE_CURRENT_BINARY_DIR}/register_exporters.gen.cpp)
list(APPEND editor_sources_gen ${CMAKE_CURRENT_BINARY_DIR}/register_exporters.gen.cpp)

if(NOT FREETYPE_FOUND AND NOT OPTION_BUILTIN_FREETYPE)
    message(FATAL_ERROR "Editor requires freetype font library")
endif()
add_custom_command(
    COMMAND editor_to_header docs ${PROJECT_BINARY_DIR}/doc_paths.txt ${CMAKE_CURRENT_BINARY_DIR}
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/doc_data_compressed.gen.h ${CMAKE_CURRENT_BINARY_DIR}/doc_data_class_path.gen.h
    DEPENDS editor_to_header ${PROJECT_BINARY_DIR}/doc_paths.txt
    COMMENT "Generating compressed docs header"
)
list(APPEND editor_sources_gen ${CMAKE_CURRENT_BINARY_DIR}/doc_data_compressed.gen.h ${CMAKE_CURRENT_BINARY_DIR}/doc_data_class_path.gen.h)
file(GLOB translation_files "translations/*.po")
add_custom_command(
    COMMAND editor_to_header translations ${CMAKE_CURRENT_SOURCE_DIR}/translations ${CMAKE_CURRENT_BINARY_DIR}
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/translations.gen.h
    DEPENDS editor_to_header ${translation_files}
    COMMENT "Generating compressed translations header"
)
list(APPEND editor_sources_gen ${CMAKE_CURRENT_BINARY_DIR}/translations.gen.h)

# Godot's own sources
file(GLOB source_files "*.cpp")
file(GLOB header_files "*.h")

list(APPEND editor_sources ${source_files} editor.qrc ${header_files})

target_sources(edit_tools PRIVATE ${editor_sources} ${exporter_sources}
    ${editor_sources_gen}
    ${editor_3rdparty}
)

target_include_directories(edit_tools PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
set_property(TARGET edit_tools PROPERTY POSITION_INDEPENDENT_CODE ON)

set_target_properties(edit_tools PROPERTIES AUTORCC_OPTIONS "--compress;7;--verbose")
set_target_properties(edit_tools PROPERTIES AUTORCC ON)
#target_link_libraries(edit_tools PUBLIC drivers scene)

target_link_libraries(edit_tools PRIVATE editor_core editor_modules)
target_link_libraries(editor_engine PRIVATE edit_tools)

if(USE_UNITY_BUILDS)
    set_target_properties(edit_tools PROPERTIES UNITY_BUILD ON)
    set_target_properties(edit_tools PROPERTIES UNITY_BUILD_BATCH_SIZE ${DEFAULT_UNITY_BATCH_SIZE}) # NOTE: this has been reduce to allow VS2017 to preperly create obj file.
    if(MSVC)
        target_compile_options(edit_tools PRIVATE /bigobj)
    elseif(MINGW)
        target_compile_options(edit_tools PRIVATE -Wa,-mbig-obj)
    endif()
endif()


if(USE_PRECOMPILED_HEADERS)
    target_precompile_headers(edit_tools REUSE_FROM se_core)
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${editor_sources})
source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${editor_3rdparty})
source_group(Generated FILES ${editor_sources_gen})


INSTALL (
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DESTINATION include/SegsEngine
    FILES_MATCHING PATTERN "*.h"
)
