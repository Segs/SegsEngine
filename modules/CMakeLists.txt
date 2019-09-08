include(CMakeDependentOption)
set(module_list)

macro(set_module_properties tgt name)
    set_common_target_properties(${tgt}_${name})
    set_target_properties (${tgt}_${name} PROPERTIES FOLDER ${tgt}/modules/ )
endmacro()
# Visit each subdirectory, see if it has CMakeLists.txt
# For each global target add an option to enable located module.
macro(detect_modules)
    set(includes_cpp "")
    set(register_cpp "")
    set(unregister_cpp "")
    file(GLOB files "*/CMakeLists.txt")
    list(SORT files) # so register_module_types does not change that often, and also plugins are registered in alphabetic order
    foreach(file ${files})
        get_filename_component(module_dir ${file} DIRECTORY)
        get_filename_component(module_name ${module_dir} NAME)
        list(APPEND module_list ${module_name})
        string(TOUPPER ${module_name} upname)
        foreach(tgt ${global_targets})
            OPTION(MODULE_${tgt}_${upname}_ENABLE "Enable module '${module_name}'" ON)
        endforeach()
        if(EXISTS "${module_dir}/register_types.h")
            string(APPEND includes_cpp "#include \"${module_name}/register_types.h\"\n")
            string(TOUPPER ${module_name} module_upper)
            string(APPEND register_cpp "#ifdef MODULE_${module_upper}_ENABLED\n")
            string(APPEND register_cpp "    register_${module_name}_types();\n")
            string(APPEND register_cpp "#endif\n")
            string(APPEND unregister_cpp "#ifdef MODULE_${module_upper}_ENABLED\n")
            string(APPEND unregister_cpp "    unregister_${module_name}_types();\n")
            string(APPEND unregister_cpp "#endif\n")
        endif()
    endforeach()
    configure_file(register_module_types.cpp.cmake register_module_types.gen.cpp)
endmacro()

macro(create_simple_module_target name)
    file(GLOB source_files "*.cpp" )
    file(GLOB header_files "*.h" )

    foreach(tgt ${global_targets})
        add_library(${tgt}_${name} OBJECT ${source_files} ${header_files} ${thirdparty_sources})
        target_link_libraries(${tgt}_${name} PUBLIC ${tgt}_core)
        string(TOUPPER ${name} upname)
        set_common_target_properties(${tgt}_${name})
        set_target_properties (${tgt}_${name} PROPERTIES
            FOLDER ${tgt}/modules/
        )

    endforeach()
endmacro()

macro(add_module tgt name)
    string(TOUPPER ${name} upname)
    if(MODULE_${tgt}_${upname}_ENABLE)
        add_subdirectory(${name})
        target_link_libraries(${tgt}_modules PUBLIC ${tgt}_${name})
        target_compile_definitions(${tgt}_modules PUBLIC MODULE_${upname}_ENABLED)
    endif()
endmacro()

detect_modules()
CMAKE_DEPENDENT_OPTION(MODULE_VORBIS_ENABLE "Enable module 'vorbis'" ON
                       "NOT MODULE_STB_VORBIS_ENABLE" OFF)

list(APPEND module_sources ${CMAKE_CURRENT_BINARY_DIR}/register_module_types.gen.cpp)

foreach(tgt ${global_targets})
    add_library(${tgt}_modules ${module_sources})
    foreach(module_dir ${module_list})
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${module_dir}/sources.cmake")
            message("WILL INCLUDE SOURCES ${module_dir}")
            include(${module_dir}/sources.cmake)
        elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${module_dir}/register_types.h")
            message("WILL LINK OBJECT ${module_dir}")
            add_module(${tgt} ${module_dir})
        else()
            #
        endif()
    endforeach()

    target_link_libraries(${tgt}_modules PRIVATE ${tgt}_core)
    set_common_target_properties(${tgt}_modules)
endforeach()

