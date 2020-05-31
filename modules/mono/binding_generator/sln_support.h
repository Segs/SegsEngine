#pragma once

#include "common.h"

#include <EASTL/string.h>
#include <EASTL/vector.h>

class QUuid;
class QByteArray;

class SLNTransformer {
    struct GlobalSection {
        String name;
        String header;
        Vector<String> entries;
    };
    Vector<String> project_definitions;
    Vector<GlobalSection> global_sections;
    void add_to_section(const char * section_type, const char * section_name, const String & proj_uuid);
public:
    void parse(const QByteArray &to_process);

    bool add_project_guid(const QUuid &uuid, const String &name, const String &path);
    String generate();

};
