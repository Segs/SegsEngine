if (NOT ("editor" IN_LIST global_targets))
    return()
endif()

set(src
    EditorSceneImporterInterface.h
    EditorSceneImporterInterface.cpp
    EditorExportPlatformInterface.h
    PluginDeclarations.h

)
add_library(editor_plugin_interfaces STATIC ${src})
set_common_target_properties(editor_plugin_interfaces)
target_link_libraries(editor_plugin_interfaces PUBLIC editor_interface)

target_link_libraries(editor_plugin_interfaces PUBLIC Qt5::Core EnTT::EnTT) # access to embedded resources
target_link_libraries(editor_plugin_interfaces PUBLIC EASTL_Import)
target_include_directories(editor_plugin_interfaces INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/..)
set_target_properties(editor_plugin_interfaces PROPERTIES AUTOMOC TRUE)

