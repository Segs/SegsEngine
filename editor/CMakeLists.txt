set(editor_sources)


if(NOT OPTION_TOOLS)
    return()
endif()

# Register exporters
set(reg_exporters_inc "#include \"register_exporters.h\"\n")
set(reg_exporters "void register_exporters() {\n")
foreach(e ${platform_exporters})
    string(REPLACE "/" ";" path_parts ${e})
    list(APPEND editor_sources ${PROJECT_SOURCE_DIR}/${e})
    list(GET path_parts 1 exporter_name)

    string(APPEND reg_exporters "    register_${exporter_name}_exporter();\n")
    string(APPEND reg_exporters_inc "#include \"platform/${exporter_name}/export/export.h\"\n")
endforeach()
string(APPEND reg_exporters "}\n")
configure_file(register_exporters.cpp.cmake ${CMAKE_CURRENT_BINARY_DIR}/register_exporters.gen.cpp)
list(APPEND editor_sources ${CMAKE_CURRENT_BINARY_DIR}/register_exporters.gen.cpp)

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
file(GLOB source_files "*.cpp")
list(APPEND editor_sources ${source_files} editor.qrc)
list(APPEND editor_sources ${PROJECT_SOURCE_DIR}/thirdparty/misc/clipper.cpp)
list(APPEND editor_sources ${CMAKE_CURRENT_BINARY_DIR}/doc_data_compressed.gen.h)
list(APPEND editor_sources ${CMAKE_CURRENT_BINARY_DIR}/doc_data_class_path.gen.h)
list(APPEND editor_sources ${CMAKE_CURRENT_BINARY_DIR}/translations.gen.h)
#list(APPEND editor_sources register_exporters.gen.cpp)
add_subdirectory(collada)
add_subdirectory(doc)
add_subdirectory(fileserver)
add_subdirectory(icons)
add_subdirectory(import)
add_subdirectory(plugins)

add_library(editor ${editor_sources})
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
target_link_libraries(editor PUBLIC ${editor_MODULES})

foreach(tgt ${global_targets})
    target_link_libraries(editor PUBLIC ${tgt}_core ${tgt}_scene ${tgt}_modules)
endforeach()

set_common_target_properties(editor)
