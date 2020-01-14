#set(source_files
#    register_types.cpp register_types.h
#    csharp_script.cpp
#    )
file(GLOB_RECURSE source_files "*.cpp" )
file(GLOB_RECURSE header_files "*.h" )
#file(GLOB qrc_files "*.qrc" )
#find_package( Mono )
include(FindPkgConfig)  # we don't need the pkg-config path on OS X, but we need other macros in this file
pkg_check_modules(MONO REQUIRED IMPORTED_TARGET mono-2)

foreach(tgt ${global_targets})
    add_library(${tgt}_module_mono STATIC)
#    if(OPTION_TOOLS)
#        list(APPEND source_files ${CMAKE_CURRENT_SOURCE_DIR}/editor/gdscript_highlighter.cpp ${CMAKE_CURRENT_SOURCE_DIR}/editor/gdscript_highlighter.h)
#    endif()
    # Those two modules are required for the language server protocol
#    if(MODULE_${tgt}_JSONRPC_ENABLE AND MODULE_${tgt}_WEBSOCKET_ENABLE)
#        file(GLOB lsp_source_files "language_server/*.cpp" )
#        file(GLOB lsp_header_files "language_server/*.h" )
#        list(APPEND source_files ${lsp_source_files} ${lsp_header_files})
#    else()
        # Using a define in the disabled case, to avoid having an extra define
        # in regular builds where all modules are enabled.
#        target_compile_definitions(${tgt}_module_gdscript PRIVATE GDSCRIPT_NO_LSP)
#    endif()
#TODO: only set hot reload on non-release builds
    target_compile_definitions(${tgt}_module_mono PRIVATE GD_MONO_HOT_RELOAD)
    target_sources(${tgt}_module_mono PRIVATE ${source_files} ${header_files} ${qrc_files})
    target_link_libraries(${tgt}_module_mono PRIVATE ${tgt}_core)
    set_target_properties(${tgt}_module_mono PROPERTIES AUTORCC ON)
    target_link_libraries(${tgt}_module_mono PRIVATE PkgConfig::MONO)
    set_common_target_properties(${tgt}_module_mono)

endforeach()
