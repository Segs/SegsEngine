set(src
    ImageLoaderInterface.h
    ResourceImporterInterface.h
    PluginDeclarations.h
    load_params.h
)
add_library(core_plugin_interfaces STATIC ${src})
target_link_libraries(core_plugin_interfaces PUBLIC Qt5::Core EASTL_Import) # access to embedded resources
#target_include_directories(core_plugin_interfaces INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/..)
set_target_properties(core_plugin_interfaces PROPERTIES AUTOMOC TRUE)
