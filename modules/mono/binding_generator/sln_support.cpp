#include "sln_support.h"

#include <EASTL/span.h>

#include <QTextStream>
#include <QStringBuilder>
#include <QUuid>

void SLNTransformer::parse(const QByteArray &to_process) {
    QTextStream ts(to_process);
    QString collected_project;
    GlobalSection collected_section;
    int in_section=-1; // 0 in project, 1 in global
    while(!ts.atEnd()) {
        QString sln_line = ts.readLine().trimmed();
        if(in_section==0) {
            collected_project.append(sln_line % "\n");
        }
        else if (in_section==1) {
            collected_section.entries.push_back((sln_line % "\n").toUtf8().data());
        }
        if(in_section==-1 && sln_line.startsWith("Project(")) {
            collected_project.append(sln_line % "\n");
            in_section=0;
        }
        if (in_section == -1 && sln_line.startsWith("GlobalSection(")) {
            collected_section.header = (sln_line % "\n").toUtf8().data();
            collected_section.name = sln_line.splitRef("=").last().toString().trimmed().toUtf8().data();
            in_section = 1;
        }
        if(in_section==0 && sln_line=="EndProject") {
            project_definitions.push_back(collected_project.toUtf8().data());
            collected_project.clear();
            in_section=-1;
        }
        if (in_section == 1 && sln_line == "EndGlobalSection") {
            collected_section.entries.pop_back();
            global_sections.push_back(collected_section);
            collected_section = GlobalSection();
            in_section = -1;
        }
    }
}

String SLNTransformer::generate() {
    String new_contents = R"raw(Microsoft Visual Studio Solution File, Format Version 12.00
                           # Visual Studio Version 16
                           MinimumVisualStudioVersion = 15.0.0
                           )raw";
    for(const String & proj : project_definitions) {
        new_contents = new_contents + proj;
    }
    new_contents += "Global\n";
    for (const GlobalSection& glob : global_sections) {
        new_contents = new_contents + glob.header;
        for(const auto &entry : glob.entries)
            new_contents = new_contents + "    " + entry;

        new_contents += "\nEndGlobalSection\n";
    }

    new_contents += "EndGlobal\n";
    return new_contents;
}

void CSProjGenerator::add_file_set(Span<const String> files)
{
    m_project_sources.insert(files.begin(), files.end());
}

void CSProjGenerator::add_defines(Span<const StringView> defines)
{
    m_project_defines.insert(defines.begin(),defines.end());
}

void CSProjGenerator::add_references(Span<const StringView> refs)
{
    m_project_references.insert(refs.begin(), refs.end());
}

String CSProjGenerator::generate()
{
    //assert(false);
    return "";
}

void SLNTransformer::add_to_section(const char *section_type, const char *section_name, const String &proj_uuid) {
    const char* default_build_options[] = {
        "Debug|Any CPU.ActiveCfg = Debug|Any CPU",
        "Debug|Any CPU.Build.0 = Debug|Any CPU",
        "Release|Any CPU.ActiveCfg = Release|Any CPU",
        "Release|Any CPU.Build.0 = Release|Any CPU"
    };
    for (GlobalSection& t : global_sections) {
        if (t.name == section_name && t.header.contains(section_type)) {
            for(const char * opt : default_build_options)
                t.entries.push_back(proj_uuid + "." + opt + "\n");
            return;
        }
    }
    // not added, missing the required section ?
    GlobalSection new_section;
    new_section.name = section_name;
    new_section.header = String().sprintf("GlobalSection(%s) = %s\n",section_type,section_name);
    for (const char* opt : default_build_options)
        new_section.entries.push_back(proj_uuid + "." + opt + "\n");
    global_sections.push_back(new_section);
}
bool SLNTransformer::add_project_guid(const QUuid &uuid, const String &name, const String &path) {

    bool already_in_projects=false;
    for(const String &t : project_definitions) {
        if(t.contains(uuid.toString().toUtf8().data(),false)) {
            already_in_projects = true;
            break;
        }
    }
    if(!already_in_projects) {
        const String to_insert(
                String().sprintf("Project(\"{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}\") = \"%s\", \"%s\", \"%s\"\nEndProject\n",
                        name.c_str(), path.c_str(), qPrintable(uuid.toString())));
        project_definitions.push_back(to_insert);
    }
    bool already_in_globals = false;
    for (const SLNTransformer::GlobalSection& t : global_sections) {
        if(t.name=="postSolution" && t.header.contains("ProjectConfigurationPlatforms")) {
            for(const String &ln : t.entries) {
                if (ln.contains((uuid.toString().toUtf8() + ".Debug").data(), false)) {
                    already_in_globals = true;
                    break;
                }
            }
            if(already_in_globals)
                break;
        }

    }
    if(!already_in_globals) {
        add_to_section("ProjectConfigurationPlatforms", "postSolution",qPrintable(uuid.toString()));
    }
    return true;
}
