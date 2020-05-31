#include "cmake_support.h"

void CppProject::setup(const String &project_name, const String &tgt_api) {
    m_project_name = project_name;
    m_target_api = tgt_api;
    m_name = m_project_name;
}

String CppProject::generate_cmake_contents() {

    String contents = String(
                R"raw(

                add_library(%1_%3_mono SHARED %1_%3_cs_bindings.gen.cpp)

                target_link_libraries(%1_%3_mono PRIVATE %1_%3 Qt5::Core mono_utils) # for plugin support functionality.
                target_compile_definitions(%1_%3_mono PRIVATE TARGET_%2)

                install(TARGETS %1_%3_mono EXPORT install_%1_%3
                LIBRARY DESTINATION bin/plugins/
                RUNTIME DESTINATION bin/plugins
                )
                set_target_properties(%1_%3_mono PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/plugin)

                )raw");
    contents.replace("%1",m_name.to_lower());
    contents.replace("%2",m_target_api.to_upper());
    contents.replace("%3",m_target_api);
    return contents;
}

