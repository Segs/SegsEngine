set(editor_sources)
set(exporter_sources)

if(NOT OPTION_TOOLS)
    return()
endif()

add_subdirectory(plugin_interfaces)

# Register exporters
set(reg_exporters_inc "#include \"register_exporters.h\"\n")
set(reg_exporters "void register_exporters() {\n")
foreach(e ${platform_exporters})
    string(REPLACE "/" ";" path_parts ${e})
    list(APPEND exporter_sources ${PROJECT_SOURCE_DIR}/${e})
    source_group(exporters/${exporter_name} ${PROJECT_SOURCE_DIR}/${e})
    list(GET path_parts 1 exporter_name)

    string(APPEND reg_exporters "    register_${exporter_name}_exporter();\n")
    string(APPEND reg_exporters_inc "#include \"platform/${exporter_name}/export/export.h\"\n")
endforeach()
string(APPEND reg_exporters "}\n")
configure_file(register_exporters.cpp.cmake ${CMAKE_CURRENT_BINARY_DIR}/register_exporters.gen.cpp)

if(NOT FREETYPE_FOUND AND NOT OPTION_BUILTIN_FREETYPE)
    message(FATAL_ERROR "Editor requires freetype font library")
endif()
add_custom_command(
    COMMAND editor_to_header docs ${PROJECT_BINARY_DIR}/doc_paths.txt ${CMAKE_CURRENT_BINARY_DIR}
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/doc_data_compressed.gen.h ${CMAKE_CURRENT_BINARY_DIR}/doc_data_class_path.gen.h
    DEPENDS editor_to_header ${PROJECT_BINARY_DIR}/doc_paths.txt
    COMMENT "Generating compressed docs header"
)

file(GLOB translation_files "translations/*.po")
add_custom_command(
    COMMAND editor_to_header translations ${CMAKE_CURRENT_SOURCE_DIR}/translations ${CMAKE_CURRENT_BINARY_DIR}
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/translations.gen.h
    DEPENDS editor_to_header ${translation_files}
    COMMENT "Generating compressed translations header"
)
# Godot's own sources
file(GLOB_RECURSE source_files "*.cpp")
file(GLOB_RECURSE header_files "*.h")
list(APPEND editor_sources ${source_files} editor.qrc ${header_files})
#list(APPEND editor_sources register_exporters.gen.cpp)

add_library(editor ${editor_sources} ${exporter_sources}
    ${CMAKE_CURRENT_BINARY_DIR}/register_exporters.gen.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/misc/clipper.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/doc_data_compressed.gen.h
    ${CMAKE_CURRENT_BINARY_DIR}/doc_data_class_path.gen.h
    ${CMAKE_CURRENT_BINARY_DIR}/translations.gen.h
)
add_plugins_to_target(editor)
target_include_directories(editor PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_compile_options(editor PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W3>
        $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wno-unused-parameter>
        $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wno-unused-parameter>
)
set_target_properties(editor PROPERTIES AUTORCC_OPTIONS "--compress;7;--verbose")
set_target_properties(editor PROPERTIES AUTORCC ON)
#target_link_libraries(editor PUBLIC drivers scene)

target_link_libraries(editor PUBLIC Qt5::Core)

foreach(tgt ${global_targets})
    target_link_libraries(editor PUBLIC ${tgt}_core ${tgt}_scene ${tgt}_modules)
endforeach()

set_common_target_properties(editor)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${editor_sources})
