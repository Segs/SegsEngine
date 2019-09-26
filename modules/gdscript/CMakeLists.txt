file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )
file(GLOB qrc_files "*.qrc" )

foreach(tgt ${global_targets})
    if(OPTION_TOOLS)
        list(APPEND source_files editor/gdscript_highlighter.cpp editor/gdscript_highlighter.h)
    endif()

    add_library(${tgt}_gdscript OBJECT ${source_files} ${header_files} ${qrc_files})
    set_target_properties(${tgt}_gdscript PROPERTIES AUTORCC ON)
    # Those two modules are required for the language server protocol
    if(module_jsonrpc_enabled AND module_websocket_enabled)
        file(GLOB lsp_source_files "language_server/*.cpp" )
        file(GLOB lsp_header_files "language_server/*.h" )
        target_sources(${tgt}_gdscript ${lsp_source_files} ${lsp_header_files})
    else()
        # Using a define in the disabled case, to avoid having an extra define
        # in regular builds where all modules are enabled.
        target_compile_definitions(${tgt}_gdscript PRIVATE GDSCRIPT_NO_LSP)
    endif()
    target_link_libraries(${tgt}_gdscript PUBLIC ${tgt}_core)
    set_module_properties(${tgt} gdscript)
endforeach()
