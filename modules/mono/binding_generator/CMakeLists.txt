add_executable(binding_generator
        bindings_generator.h
        bindings_generator.cpp
        sln_support.cpp
        sln_support.h
        cmake_support.cpp
        cmake_support.h
        common.h

        # integrate doc_data support.
        ${PROJECT_SOURCE_DIR}/core/doc_support/doc_data.cpp
        ${PROJECT_SOURCE_DIR}/core/doc_support/doc_data.h
        ${PROJECT_SOURCE_DIR}/core/reflection_support/reflection_data.cpp
        ${PROJECT_SOURCE_DIR}/core/reflection_support/reflection_data.h
        ${PROJECT_SOURCE_DIR}/core/string_builder.cpp
        ${PROJECT_SOURCE_DIR}/core/string_builder.h
        ${PROJECT_SOURCE_DIR}/core/os/memory.cpp
        ${PROJECT_SOURCE_DIR}/core/os/memory.h
        ${PROJECT_SOURCE_DIR}/core/safe_refcount.cpp
    )
target_link_libraries(binding_generator PRIVATE Qt5::Core EASTL) #se_core
set_target_properties(binding_generator PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
#target_compile_definitions(binding_generator PRIVATE GODOT_STATIC_DEFINE)
target_include_directories(binding_generator PRIVATE ${PROJECT_SOURCE_DIR})
