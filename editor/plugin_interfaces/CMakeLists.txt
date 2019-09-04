set(src
    EditorSceneImporterInterface.h
    EditorSceneImporterInterface.cpp
)
add_library(editor_plugin_interfaces STATIC ${src})
target_link_libraries(editor_plugin_interfaces PUBLIC Qt5::Core) # access to embedded resources
target_include_directories(editor_plugin_interfaces INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/..)
set_target_properties(editor_plugin_interfaces PROPERTIES AUTOMOC TRUE)

