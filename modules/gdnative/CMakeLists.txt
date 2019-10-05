file(GLOB_RECURSE source_files "*.cpp" )
file(GLOB_RECURSE header_files "*.h" )
file(GLOB qrc_files "*.qrc" )

add_custom_command(
    COMMAND editor_to_header gdnative "${CMAKE_CURRENT_SOURCE_DIR}/gdnative_api.json" "${CMAKE_CURRENT_BINARY_DIR}"
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/gdnative_api_struct.gen.cpp ${CMAKE_CURRENT_BINARY_DIR}/gdnative_api_struct.gen.h
    DEPENDS editor_to_header "${CMAKE_CURRENT_SOURCE_DIR}/gdnative_api.json"
    COMMENT "Generating gdnative header/source"
)

foreach(tgt ${global_targets})
    add_library(${tgt}_module_gdnative OBJECT ${source_files} ${header_files} ${qrc_files}
        ${CMAKE_CURRENT_BINARY_DIR}/gdnative_api_struct.gen.cpp ${CMAKE_CURRENT_BINARY_DIR}/gdnative_api_struct.gen.h
        )
    target_include_directories(${tgt}_module_gdnative PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/)
    target_include_directories(${tgt}_module_gdnative PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(${tgt}_module_gdnative PRIVATE ${tgt}_core)
    set_target_properties(${tgt}_module_gdnative PROPERTIES AUTORCC ON)
    set_common_target_properties(${tgt}_module_gdnative)

endforeach()
