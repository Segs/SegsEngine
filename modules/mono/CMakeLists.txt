add_custom_command(
    COMMAND editor_to_header mono ${CMAKE_CURRENT_SOURCE_DIR}/glue/GodotSharp ${CMAKE_CURRENT_BINARY_DIR}/glue/cs_glue_files.gen.h ${CMAKE_CURRENT_BINARY_DIR}/glue/cs_glue_version.gen.h
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/glue/cs_glue_version.gen.h
    DEPENDS editor_to_header
    COMMENT "Generating mono glue helpers"
)

set(source_files

    editor/godotsharp_export.cpp
    editor/editor_internal_calls.cpp
    editor/bindings_generator.cpp
    editor/code_completion.cpp
    editor/script_class_parser.cpp
    editor/csharp_project.cpp
    utils/osx_utils.cpp
    utils/mono_reg_utils.cpp
    utils/path_utils.cpp
    utils/string_utils.cpp
    glue/collections_glue.cpp
    glue/nodepath_glue.cpp
    glue/string_glue.cpp
    glue/gd_glue.cpp
    glue/rid_glue.cpp
    glue/base_object_glue.cpp
    mono_gd/gd_mono_internals.cpp
    mono_gd/gd_mono.cpp
    mono_gd/gd_mono_assembly.cpp
    mono_gd/gd_mono_field.cpp
    mono_gd/gd_mono_log.cpp
    mono_gd/gd_mono_utils.cpp
    mono_gd/managed_type.cpp
    mono_gd/gd_mono_property.cpp
    mono_gd/gd_mono_cache.cpp
    mono_gd/gd_mono_method.cpp
    mono_gd/gd_mono_class.cpp
    mono_gd/gd_mono_marshal.cpp
    class_db_api_json.cpp
    csharp_script.cpp
    godotsharp_dirs.cpp
    mono_gc_handle.cpp
    signal_awaiter_utils.cpp
    register_types.cpp
)

set(header_files
    editor/godotsharp_export.h
    editor/editor_internal_calls.h
    editor/script_class_parser.h
	editor/code_completion.h
    editor/csharp_project.h
    editor/bindings_generator.h
    utils/osx_utils.h
    utils/macros.h
    utils/mono_reg_utils.h
    utils/path_utils.h
    utils/string_utils.h
    glue/nodepath_glue.h
    glue/rid_glue.h
    glue/gd_glue.h
    glue/string_glue.h
    glue/glue_header.h
    glue/base_object_glue.h
    glue/collections_glue.h
    glue/arguments_vector.h
    mono_gd/gd_mono.h
    mono_gd/gd_mono_method_thunk.h
    mono_gd/gd_mono_class.h
    mono_gd/managed_type.h
    mono_gd/gd_mono_utils.h
    mono_gd/i_mono_class_member.h
    mono_gd/gd_mono_marshal.h
    mono_gd/gd_mono_field.h
    mono_gd/gd_mono_property.h
    mono_gd/gd_mono_log.h
    mono_gd/gd_mono_header.h
    mono_gd/gd_mono_internals.h
    mono_gd/gd_mono_method.h
    mono_gd/gd_mono_assembly.h
    mono_gd/gd_mono_cache.h
    mono_gd/android_mono_config.h
    csharp_script.h
    signal_awaiter_utils.h
    godotsharp_defs.h
    mono_gc_handle.h
    register_types.h
    class_db_api_json.h
    godotsharp_dirs.h
)

file(GLOB qrc_files "*.qrc" )
#find_package( Mono )
find_package(Mono REQUIRED)
#include(FindPkgConfig)  # we don't need the pkg-config path on OS X, but we need other macros in this file
#pkg_check_modules(MONO REQUIRED IMPORTED_TARGET mono-2)
OPTION(MONO_GLUE_AVAILABLE "Use mono glue?" OFF)

foreach(tgt ${global_targets})
    add_library(${tgt}_module_mono STATIC)
    if(MONO_GLUE_AVAILABLE)
        target_sources(${tgt}_module_mono PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/glue/mono_glue.gen.cpp)
        target_compile_definitions(${tgt}_module_mono PRIVATE MONO_GLUE_ENABLED)
    endif()
#TODO: only set hot reload on non-release builds
    target_compile_definitions(${tgt}_module_mono PRIVATE GD_MONO_HOT_RELOAD)
    target_sources(${tgt}_module_mono PRIVATE ${source_files} ${header_files} ${qrc_files} ${CMAKE_CURRENT_BINARY_DIR}/glue/cs_glue_version.gen.h)
    target_include_directories(${tgt}_module_mono PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(${tgt}_module_mono PRIVATE ${tgt}_core)
    set_target_properties(${tgt}_module_mono PROPERTIES AUTORCC ON)
    target_link_libraries(${tgt}_module_mono PRIVATE Mono)
    set_common_target_properties(${tgt}_module_mono)
    if(MSVC)
        target_compile_options(${tgt}_module_mono PRIVATE /bigobj)
    elseif(MINGW)
        target_compile_options(${tgt}_module_mono PRIVATE -Wa,-mbig-obj)
    endif()
endforeach()
