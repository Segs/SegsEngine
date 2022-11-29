add_custom_command(
    COMMAND editor_to_header mono ${CMAKE_CURRENT_SOURCE_DIR}/glue/GodotSharp ${CMAKE_CURRENT_BINARY_DIR}/glue/cs_glue_files.gen.h ${CMAKE_CURRENT_BINARY_DIR}/glue/cs_glue_version.gen.h
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/glue/cs_glue_version.gen.h
    DEPENDS editor_to_header
    COMMENT "Generating mono glue helpers"
)

set(source_files

    utils/osx_utils.cpp
    utils/mono_reg_utils.cpp
    utils/path_utils.cpp
    utils/string_utils.cpp
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
    managed_callable.cpp
    csharp_script.cpp
    godotsharp_dirs.cpp
    mono_gc_handle.cpp
    signal_awaiter_utils.cpp
    register_types.cpp
)
set(editor_source_files
    editor/godotsharp_export.cpp
    editor/editor_internal_calls.cpp
    editor/code_completion.cpp
    editor/script_class_parser.cpp
    editor/csharp_project.cpp
)
set(client_source_files
)


set(header_files
    editor/godotsharp_export.h
    editor/editor_internal_calls.h
    editor/script_class_parser.h
    editor/code_completion.h
    editor/csharp_project.h
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
    managed_callable.h
    csharp_script.h
    signal_awaiter_utils.h
    godotsharp_defs.h
    mono_gc_handle.h
    register_types.h
    godotsharp_dirs.h
)

file(GLOB qrc_files "*.qrc" )
#find_package( Mono )
find_package(Mono REQUIRED)
find_package(Dotnet) # optional, used under windows to build things

#include(FindPkgConfig)  # we don't need the pkg-config path on OS X, but we need other macros in this file
#pkg_check_modules(MONO REQUIRED IMPORTED_TARGET mono-2)

add_subdirectory(binding_generator)

macro(add_glue_module name tgt)
    add_custom_command(
        COMMAND bindings_generator ${CMAKE_CURRENT_BINARY_DIR}/${name}.json ${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/glue
        OUTPUT
            ${CMAKE_CURRENT_SOURCE_DIR}/glue/${name}/cpp/${name}MetaData_icalls.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/glue/${name}/cpp/Godot_icalls.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/glue/${name}/cpp/${name}.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/glue/${name}/cpp/${name}.h
            ${CMAKE_CURRENT_SOURCE_DIR}/glue/${name}/cpp/${name}_plugin.json
            #${CMAKE_CURRENT_SOURCE_DIR}/glue/${name}/${name}.csproj
        DEPENDS
            bindings_generator ${tgt}_engine reflection_${tgt}_gen
            #${CMAKE_CURRENT_BINARY_DIR}/${name}.json not depending on source json, since reflection_${tgt}_gen depends on them and we depend on reflection_${tgt}_gen
        COMMENT "Generating mono glue for ${name}"
    )
    set(additional_sources)
    if(${name} STREQUAL "GodotCore")
        set(additional_sources
            glue/collections_glue.cpp
            glue/nodepath_glue.cpp
            glue/string_glue.cpp
            glue/string_name_glue.cpp
            glue/gd_glue.cpp
            glue/rid_glue.cpp
            glue/base_object_glue.cpp
            glue/opaque_glue.cpp
        )
    endif()
    add_library(${name}_glue SHARED
        ${additional_sources}
        ${CMAKE_CURRENT_SOURCE_DIR}/glue/${name}/cpp/${name}MetaData_icalls.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/glue/${name}/cpp/dummy.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/glue/${name}/cpp/Godot_icalls.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/glue/${name}/cpp/${name}.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/glue/${name}/cpp/${name}.h
        ${CMAKE_CURRENT_SOURCE_DIR}/glue/${name}/cpp/${name}_plugin.json
    )
    target_link_libraries(${name}_glue PRIVATE
        ${tgt}_engine
        Mono
        EnTT
    )
    set_target_properties(${name}_glue PROPERTIES AUTOMOC ON)
    if(MSVC)
        target_compile_options(${name}_glue PRIVATE /bigobj)
    elseif(MINGW)
        target_compile_options(${name}_glue PRIVATE -Wa,-mbig-obj)
    endif()
    set_target_properties(${name}_glue PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR}/plugins/mono_glue)
    set_target_properties(${name}_glue PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIR}/plugins/mono_glue)
    set_target_properties(${name}_glue PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${OUTPUT_DIR}/plugins/mono_glue)
    set_target_properties(${name}_glue PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${OUTPUT_DIR}/plugins/mono_glue)
    set_target_properties(${name}_glue PROPERTIES LIBRARY_OUTPUT_DIRECTORY_RELASE ${OUTPUT_DIR}/plugins/mono_glue)
    set_target_properties(${name}_glue PROPERTIES LIBRARY_OUTPUT_DIRECTORY_DEBUG ${OUTPUT_DIR}/plugins/mono_glue)
    # archive (on vs it's an import lib) not put in the output dir
    install(TARGETS ${name}_glue EXPORT SegsEngine
        LIBRARY DESTINATION bin/plugins/mono_glue
        RUNTIME DESTINATION bin/plugins/mono_glue
    )

    if(DOTNET_FOUND)
        ADD_DOTNET(
            ${CMAKE_CURRENT_SOURCE_DIR}/glue/${name}/${name}.csproj
            OUTPUT_PATH ${OUTPUT_DIR}/csharp/assemblies
            ARGUMENTS -noWarn:CS1591
        )
        add_dependencies(BUILD_${name} ${name}_glue)
    else()
        message("Dotnet utility was not found assembly for ${name} will not be built automatically")
    endif()
endmacro()

foreach(tgt ${global_targets})
    # generate the reflection data.
    set(reflection_files ${CMAKE_CURRENT_BINARY_DIR}/GodotCore.json)
    if(${tgt} STREQUAL "editor")
        set(reflection_files ${reflection_files};${CMAKE_CURRENT_BINARY_DIR}/GodotEditor.json)
    endif()

    add_custom_command(
        COMMAND segs_${tgt} --gen-reflection ${CMAKE_CURRENT_BINARY_DIR}
        OUTPUT
            ${reflection_files}
        DEPENDS ${tgt}_engine
        COMMENT "Generating reflection data: ./segs_${tgt} --gen-reflection ${CMAKE_CURRENT_BINARY_DIR}"
    )
    set_source_files_properties( ${CMAKE_CURRENT_BINARY_DIR}/GodotCore.json PROPERTIES GENERATED TRUE )
    add_custom_target(reflection_${tgt}_gen
        # If the file exists, then commands related to that file won't be executed
        # DONOT let other target depends on the same OUTPUT as current target,
        #   or it may be bad when doing parallel make
        DEPENDS ${reflection_files}

        # to make quotes printable,for example
        VERBATIM
    )
    add_dependencies(reflection_${tgt}_gen  ${tgt}_engine)

    add_glue_module(GodotCore ${tgt})
    if(${tgt} STREQUAL "editor")
        add_glue_module(GodotEditor ${tgt})
        # record the hidden fact that GodotEditor csproj requires GodotCore
        add_dependencies(BUILD_GodotEditor BUILD_GodotCore)
    endif()
    add_dependencies(BUILD_GodotCore reflection_${tgt}_gen)
endforeach()

foreach(tgt ${global_targets})
    add_library(${tgt}_module_mono STATIC)
#TODO: only set hot reload on non-release builds
    target_compile_definitions(${tgt}_module_mono PRIVATE GD_MONO_HOT_RELOAD)
    target_sources(${tgt}_module_mono PRIVATE
            ${source_files} ${${tgt}_source_files} ${header_files} ${qrc_files}
            ${CMAKE_CURRENT_BINARY_DIR}/glue/cs_glue_version.gen.h)
    target_include_directories(${tgt}_module_mono PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(${tgt}_module_mono PRIVATE ${tgt}_core)
    set_target_properties(${tgt}_module_mono PROPERTIES AUTORCC ON)
    target_link_libraries(${tgt}_module_mono PRIVATE Mono)
    set_common_target_properties(${tgt}_module_mono)

    if(${tgt} STREQUAL "editor")
        if(DOTNET_FOUND)
            ADD_DOTNET(
                editor/GodotTools/GodotTools.sln
                OUTPUT_PATH ${OUTPUT_DIR}/csharp/assemblies
                PLATFORM x64
                ARGUMENTS -noWarn:CS1591 /p:GodotAssembliesPath="${OUTPUT_DIR}"
            )
            add_dependencies(BUILD_GodotTools BUILD_GodotCore)
            add_dependencies(BUILD_GodotTools BUILD_GodotEditor)
        endif()
    endif()
endforeach()


install(DIRECTORY ${OUTPUT_DIR}/csharp/ DESTINATION  bin/csharp)
