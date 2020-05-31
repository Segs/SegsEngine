#pragma once
#include "common.h"

struct CppProject {
    Set<String> needed_headers;
    Set<String> project_defines;
    String m_name;
    String m_target_api; // editor/client/server
    String m_project_name;


    void setup(const String & project_name, const String &tgt_api);

    void parse(const String &to_process);

    bool add_plugin(const String &name, const String &path);

    String generate_cmake_contents();
    String generate();

};
