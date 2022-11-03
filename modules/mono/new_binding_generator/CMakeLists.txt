add_executable(new_bindings_generator
        bindings_generator.cpp

        cmake_support.cpp
        cmake_support.h
        docs_helpers.cpp
        docs_helpers.h
        doc_resolution_pass.cpp
        doc_resolution_pass.h
        type_registration_pass.cpp
        type_registration_pass.h
        reflection_visitor_support.cpp
        reflection_visitor_support.h
        cpp_generator.cpp
        cpp_generator.h
        cs_generator_visitor.h
        cs_generator_visitor.cpp
        icall_cs_generator.cpp
        icall_cs_generator.h
        generator_helpers.cpp
        generator_helpers.h
        property_generator.cpp
        property_generator.h
        type_system.cpp
        type_system.h
        type_mapper.cpp
        type_mapper.h
        type_generator.cpp
        type_generator.h
        sln_support.cpp
        sln_support.h
        # integrate doc_data support.
        ${PROJECT_SOURCE_DIR}/core/doc_support/doc_data.cpp
        ${PROJECT_SOURCE_DIR}/core/doc_support/doc_data.h
        ${PROJECT_SOURCE_DIR}/core/reflection_support/reflection_data.cpp
        ${PROJECT_SOURCE_DIR}/core/reflection_support/reflection_data.h
        ${PROJECT_SOURCE_DIR}/core/string_name.cpp
        ${PROJECT_SOURCE_DIR}/core/string_name.h
        ${PROJECT_SOURCE_DIR}/core/string_builder.cpp
        ${PROJECT_SOURCE_DIR}/core/string_builder.h
        ${PROJECT_SOURCE_DIR}/core/os/memory.cpp
        ${PROJECT_SOURCE_DIR}/core/os/memory.h
    )
target_link_libraries(new_bindings_generator PRIVATE Qt5::Core EASTL
    EnTT
    Mono) #se_core
set_target_properties(new_bindings_generator PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set_target_properties(new_bindings_generator PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${OUTPUT_DIR})
set_target_properties(new_bindings_generator PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${OUTPUT_DIR})
# add selected Qt path to environment on visual studio
set_target_properties(new_bindings_generator PROPERTIES VS_DEBUGGER_ENVIRONMENT "PATH=${CMAKE_MSVCIDE_RUN_PATH};%PATH%")
target_compile_definitions(new_bindings_generator PRIVATE GODOT_STATIC_DEFINE)
target_include_directories(new_bindings_generator PRIVATE ${PROJECT_SOURCE_DIR})
# put the new_bindings_generator project in a fake location ( fake since it does not match actual path)
set_target_properties(new_bindings_generator PROPERTIES FOLDER utils/soc/)

if(WIN32 AND NOT(CMAKE_GENERATOR MATCHES "Visual Studio"))
    #TODO: this is bonkers, we should be able to provide qt path to all code generator tools that run during build :|
    add_custom_command(TARGET new_bindings_generator POST_BUILD
        COMMAND cmake -E copy ${Qt5_BIN_DIR}/Qt5Cored.dll ${OUTPUT_DIR}
        COMMAND cmake -E copy ${Qt5_BIN_DIR}/Qt5Core.dll ${OUTPUT_DIR})
endif()
if(WIN32)
    install(TARGETS new_bindings_generator EXPORT SegsEngine
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION bin
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
    )
else()
    install(TARGETS new_bindings_generator EXPORT SegsEngine
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION bin
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
    )
endif()

