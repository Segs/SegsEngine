
add_subdirectory(formats)
add_subdirectory(io)

set(source_files
    plugin_registry.cpp
    plugin_registry.h
    plugin_registry_interface.h
)
foreach(tgt ${global_targets})
add_library(${tgt}_plug_registry STATIC ${source_files})
add_plugins_to_target(${tgt}_plug_registry ${tgt})
endforeach()


