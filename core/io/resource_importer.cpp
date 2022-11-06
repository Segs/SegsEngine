/*************************************************************************/
/*  resource_importer.cpp                                                */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "resource_importer.h"

#include "core/class_db.h"
#include "core/io/config_file.h"
#include "core/method_enum_caster.h"
#include "core/object_tooling.h"
#include "core/resource/resource_manager.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "core/property_info.h"
#include "core/string.h"
#include "core/string_utils.h"
#include "core/string_utils.inl"
#include "core/variant_parser.h"
#include "EASTL/sort.h"

IMPL_GDCLASS(ResourceImporter)
VARIANT_ENUM_CAST(ResourceImporter::ImportOrder);

bool ResourceFormatImporter::SortImporterByName::operator()(const ResourceImporterInterface *p_a, const ResourceImporterInterface *p_b) const {
    return StringView(p_a->get_importer_name()) < StringView(p_b->get_importer_name());
}

Error ResourceFormatImporter::_get_path_and_type(StringView p_path, PathAndType &r_path_and_type, bool *r_valid) const {
    ConfigFile import_info;
    bool path_found = false; //first match must have priority

    Error err = import_info.load(String(p_path) + ".import");
    if(err!=OK) {
        if (r_valid) {
            *r_valid = false;
        }
        return err;
    }
    if (r_valid) {
        *r_valid = true;
    }
    auto iter = import_info.all_values().find_as(StringView("remap"));
    if(iter== import_info.all_values().end())
        return ERR_FILE_CORRUPT;

    Vector<String> keys = import_info.get_section_keys(StringView("remap"));

    for(const auto &remap_kv : iter->second) {
        if (!path_found && StringUtils::begins_with(remap_kv.first, "path.") && r_path_and_type.path.empty()) {
            StringView feature(StringUtils::get_slice(remap_kv.first, '.', 1));
            if (OS::get_singleton()->has_feature(feature)) {
                r_path_and_type.path = remap_kv.second.as<String>();
                path_found = true; //first match must have priority
            }

        }
        else if (!path_found && remap_kv.first == "path") {
            r_path_and_type.path = remap_kv.second.as<String>();
            path_found = true; //first match must have priority
        }
        else if (remap_kv.first == "type") {
            r_path_and_type.type = remap_kv.second.as<String>();
        }
        else if (remap_kv.first == "importer") {
            r_path_and_type.importer = remap_kv.second.as<String>();
        }
        else if (remap_kv.first == "group_file") {
            r_path_and_type.group_file = remap_kv.second.as<String>();
        }
        else if (remap_kv.first == "metadata") {
            r_path_and_type.metadata = remap_kv.second;
        }
        else if (remap_kv.first == "valid") {
            if (r_valid) {
                *r_valid = remap_kv.second.as<bool>();
            }
        }

    }

    if (r_path_and_type.path.empty() || r_path_and_type.type.empty()) {
        return ERR_FILE_CORRUPT;
    }
    return OK;
}

RES ResourceFormatImporter::load(StringView p_path, StringView p_original_path, Error *r_error, bool p_no_subresource_cache) {

    PathAndType pat;
    const Error err = _get_path_and_type(p_path, pat);

    if (err != OK) {

        if (r_error)
            *r_error = err;

        return RES();
    }

    RES res(gResourceManager().load_internal(pat.path, p_path, pat.type, p_no_subresource_cache, r_error));

    Tooling::importer_load(res, pat.path);

    return res;
}

void ResourceFormatImporter::get_recognized_extensions(Vector<String> &p_extensions) const {

    HashSet<String> found;

    for (auto importer : importers) {
        Vector<String> local_exts;
        importer->get_recognized_extensions(local_exts);
        for (const auto & ext : local_exts) {
            if (!found.contains(ext)) {
                p_extensions.emplace_back(ext);
                found.insert(ext);
            }
        }
    }
    for (const auto &owned_importer : owned_importers) {
        Vector<String> local_exts;
        owned_importer->get_recognized_extensions(local_exts);
        for (const auto & local_ext : local_exts) {
            if (!found.contains(local_ext)) {
                p_extensions.emplace_back(local_ext);
                found.insert(local_ext);
            }
        }
    }
}

void ResourceFormatImporter::get_recognized_extensions_for_type(StringView p_type, Vector<String> &p_extensions) const {

    if (p_type.empty()) {
        get_recognized_extensions(p_extensions);
        return;
    }

    HashSet<String> found;

    for (ResourceImporterInterface* importer : importers) {
        StringName res_type(importer->get_resource_type());
        if (res_type.empty())
            continue;

        if (!ClassDB::is_parent_class(res_type, StringName(p_type)))
            continue;

        Vector<String> local_exts;
        importer->get_recognized_extensions(local_exts);
        for (auto &local_ext : local_exts) {
            if (!found.contains(local_ext)) {
                p_extensions.emplace_back(local_ext);
                found.emplace(local_ext);
            }
        }
    }
    for (const auto &owned_importer : owned_importers) {
        StringName res_type(owned_importer->get_resource_type());
        if (res_type.empty())
            continue;

        if (!ClassDB::is_parent_class(res_type, StringName(p_type)))
            continue;

        Vector<String>  local_exts;
        owned_importer->get_recognized_extensions(local_exts);
        for (auto & ext : local_exts) {
            if (!found.contains(ext)) {
                p_extensions.emplace_back(ext);
                found.insert(ext);
            }
        }
    }
}

bool ResourceFormatImporter::exists(StringView p_path) const {

    return FileAccess::exists(String(p_path) + ".import");
}

bool ResourceFormatImporter::recognize_path(StringView p_path, StringView /*p_for_type*/) const {

    return FileAccess::exists(String(p_path) + ".import");
}

bool ResourceFormatImporter::can_be_imported(StringView p_path) const {

    return ResourceFormatLoader::recognize_path(p_path);
}

int ResourceFormatImporter::get_import_order(StringView p_path) const {

    ResourceImporterInterface *importer = nullptr;

    if (FileAccess::exists(String(p_path) + ".import")) {

        PathAndType pat;
        Error err = _get_path_and_type(p_path, pat);

        if (err == OK) {
            importer = get_importer_by_name(pat.importer);
        }
    } else {

        importer = get_importer_by_extension(StringUtils::to_lower(PathUtils::get_extension(p_path)));
    }

    if (importer!=nullptr)
        return importer->get_import_order();

    return 0;
}

bool ResourceFormatImporter::handles_type(StringView p_type) const {

    for (auto importer : importers) {

        StringName res_type(importer->get_resource_type());
        if (res_type.empty())
            continue;
        if (ClassDB::is_parent_class(res_type, StringName(p_type)))
            return true;
    }
    for (const auto &ownded_importer : owned_importers) {

        StringName res_type(ownded_importer->get_resource_type());
        if (res_type.empty())
            continue;
        if (ClassDB::is_parent_class(res_type, StringName(p_type)))
            return true;
    }

    return true;
}

String ResourceFormatImporter::get_internal_resource_path(StringView p_path) const {

    PathAndType pat;
    Error err = _get_path_and_type(p_path, pat);

    if (err != OK) {

        return String();
    }

    return pat.path;
}

void ResourceFormatImporter::get_internal_resource_path_list(StringView p_path, Vector<String> *r_paths) const {

    Error err;
    FileAccess *f = FileAccess::open(String(p_path) + ".import", FileAccess::READ, &err);

    if (!f)
        return;

    VariantParserStream *stream=VariantParser::get_file_stream(f);

    Variant value;
    VariantParser::Tag next_tag;

    int lines = 0;
    String error_text;
    while (true) {

        String assign = Variant().as<String>();
        next_tag.fields.clear();
        next_tag.name.clear();

        err = VariantParser::parse_tag_assign_eof(stream, lines, error_text, next_tag, assign, value, nullptr, true);
        if (err == ERR_FILE_EOF) {
            VariantParser::release_stream(stream);
            memdelete(f);
            return;
        } else if (err != OK) {
            ERR_PRINT("ResourceFormatImporter::get_internal_resource_path_list - " + String(p_path) + ".import:" + ::to_string(lines) +
                      " error: " + error_text);
            VariantParser::release_stream(stream);
            memdelete(f);
            return;
        }

        if (!assign.empty()) {
            if (StringUtils::begins_with(assign,"path.")) {
                r_paths->emplace_back(value.as<String>());
            } else if (assign == "path") {
                r_paths->emplace_back(value.as<String>());
            }
        } else if (next_tag.name != "remap") {
            break;
        }
    }
    VariantParser::release_stream(stream);
    memdelete(f);
}

String ResourceFormatImporter::get_import_group_file(StringView p_path) const {

    bool valid = true;
    PathAndType pat;
    _get_path_and_type(p_path, pat, &valid);
    return valid ? pat.group_file : String();
}

bool ResourceFormatImporter::is_import_valid(StringView p_path) const {

    bool valid = true;
    PathAndType pat;
    _get_path_and_type(p_path, pat, &valid);
    return valid;
}

String ResourceFormatImporter::get_resource_type(StringView p_path) const {

    PathAndType pat;
    Error err = _get_path_and_type(p_path, pat);

    if (err != OK) {

        return String();
    }

    return pat.type;
}

Variant ResourceFormatImporter::get_resource_metadata(StringView p_path) const {
    PathAndType pat;
    Error err = _get_path_and_type(p_path, pat);

    if (err != OK) {

        return Variant();
    }

    return pat.metadata;
}

void ResourceFormatImporter::get_dependencies(StringView p_path, Vector<String> &p_dependencies, bool p_add_types) {

    PathAndType pat;
    Error err = _get_path_and_type(p_path, pat);

    if (err != OK) {

        return;
    }

    gResourceManager().get_dependencies(pat.path, p_dependencies, p_add_types);
}

ResourceImporterInterface *ResourceFormatImporter::get_importer_by_name(StringView p_name) const {

    for(auto importer : importers) {
        if (importer->get_importer_name() == p_name) {
            return importer;
        }
    }
    for (const auto& owned_importer : owned_importers) {
        if (owned_importer->get_importer_name() == p_name) {
            return const_cast<ResourceImporterInterface *>(static_cast<const ResourceImporterInterface *>(owned_importer.get()));
        }
    }
    return nullptr;
}

void ResourceFormatImporter::get_importers_for_extension(StringView p_extension, Vector<ResourceImporterInterface *> *r_importers) const {

    for (auto importer : importers) {
        Vector<String> local_exts;
        importer->get_recognized_extensions(local_exts);
        for (const auto &local_ext : local_exts) {
            if (StringUtils::to_lower(p_extension) == local_ext) {
                r_importers->push_back(importer);
            }
        }
    }
    for (const auto& owned_importer : owned_importers) {
        Vector<String> local_exts;
        owned_importer->get_recognized_extensions(local_exts);
        for (const auto &local_ext : local_exts) {
            if (StringUtils::to_lower(p_extension) == local_ext) {
                r_importers->push_back(owned_importer.get());
            }
        }
    }
}

void ResourceFormatImporter::get_importers(Vector<ResourceImporterInterface *> *r_importers) const
{
    r_importers->assign(importers.begin(),importers.end());
}
/**
 * \brief Check if any importer can actually import give file.
 * \param filepath - a full path to the file ( some importers might require full path to decide )
 * \return true if any importer can import given file.
 */
bool ResourceFormatImporter::any_can_import(StringView filepath) const
{
    String ext = StringUtils::to_lower(PathUtils::get_extension(filepath));
    Vector<ResourceImporterInterface*> importers;
    get_importers_for_extension(ext, &importers);

    for (auto imp : importers)
    {
        if (imp->can_import(filepath))
        {
            return true;
        }
    }
    return false;
}

ResourceImporterInterface *ResourceFormatImporter::get_importer_by_extension(StringView p_extension) const {

    ResourceImporterInterface *importer = nullptr;
    float priority = 0;

    for (ResourceImporterInterface *i : importers) {

        Vector<String> local_exts;
        i->get_recognized_extensions(local_exts);
        for (const String & local_ext : local_exts) {
            if (StringUtils::to_lower(p_extension) == local_ext && i->get_priority() > priority) {
                importer = i;
                priority = i->get_priority();
            }
        }
    }
    for (size_t i = 0; i < owned_importers.size(); i++) {
        Vector<String> local_exts;
        owned_importers[i]->get_recognized_extensions(local_exts);
        for (const String & local_ext : local_exts) {
            if (StringUtils::to_lower(p_extension) == local_ext && importers[i]->get_priority() > priority) {
                importer = const_cast<ResourceImporterInterface *>(static_cast<const ResourceImporterInterface *>(owned_importers[i].get()));
                priority = owned_importers[i]->get_priority();
            }
        }
    }
    return importer;
}

String ResourceFormatImporter::get_import_base_path(StringView p_for_file) const {
    return PathUtils::plus_file(ProjectSettings::get_singleton()->get_project_data_path(),
            String(PathUtils::get_file(p_for_file)) + "-" + StringUtils::md5_text(p_for_file));
}

bool ResourceFormatImporter::are_import_settings_valid(StringView p_path) const {

    bool valid = true;
    PathAndType pat;
    _get_path_and_type(p_path, pat, &valid);

    if (!valid) {
        return false;
    }
    StringView pat_importer(pat.importer);
    for (auto importer : importers) {
        if (importer->get_importer_name() == pat_importer) {
            if (!importer->are_import_settings_valid(p_path)) { //importer thinks this is not valid
                return false;
            }
        }
    }
    for (const auto &owned_importer : owned_importers) {
        if (owned_importer->get_importer_name() == pat_importer) {
            if (!owned_importer->are_import_settings_valid(p_path)) { //importer thinks this is not valid
                return false;
            }
        }
    }
    return true;
}

String ResourceFormatImporter::get_import_settings_hash() const {

    FixedVector<const ResourceImporterInterface *,64,true> sorted_importers;
    sorted_importers.assign(importers.begin(), importers.end());
    for (const auto &owned_importer : owned_importers)
        sorted_importers.push_back(owned_importer.get());
    eastl::sort(sorted_importers.begin(),sorted_importers.end(),SortImporterByName());

    String hash;
    for (const ResourceImporterInterface *imp : sorted_importers) {
        hash += String(":") + imp->get_importer_name() + ":" + imp->get_import_settings_string();
    }
    return StringUtils::md5_text(hash);
}

ResourceFormatImporter *ResourceFormatImporter::singleton = nullptr;

ResourceFormatImporter::ResourceFormatImporter() {
    singleton = this;
}

void ResourceImporter::_bind_methods() {
    BIND_ENUM_CONSTANT(IMPORT_ORDER_DEFAULT);
    BIND_ENUM_CONSTANT(IMPORT_ORDER_SCENE);
}
