file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )
file(GLOB qrc_files "*.qrc" )

foreach(tgt ${global_targets})
    add_library(${tgt}_module_gdscript STATIC)
    if(OPTION_TOOLS)
        list(APPEND source_files ${CMAKE_CURRENT_SOURCE_DIR}/editor/gdscript_highlighter.cpp ${CMAKE_CURRENT_SOURCE_DIR}/editor/gdscript_highlighter.h)
    endif()
    # Those two modules are required for the language server protocol
    if(MODULE_${tgt}_JSONRPC_ENABLE AND MODULE_${tgt}_WEBSOCKET_ENABLE)
        file(GLOB lsp_source_files "language_server/*.cpp" )
        file(GLOB lsp_header_files "language_server/*.h" )
        list(APPEND source_files ${lsp_source_files} ${lsp_header_files})
    else()
        # Using a define in the disabled case, to avoid having an extra define
        # in regular builds where all modules are enabled.
        target_compile_definitions(${tgt}_module_gdscript PRIVATE GDSCRIPT_NO_LSP)
    endif()
    target_sources(${tgt}_module_gdscript PRIVATE ${source_files} ${header_files} ${qrc_files})
    target_link_libraries(${tgt}_module_gdscript PRIVATE ${tgt}_core)
    set_target_properties(${tgt}_module_gdscript PROPERTIES AUTORCC ON)
    set_common_target_properties(${tgt}_module_gdscript)

endforeach()
