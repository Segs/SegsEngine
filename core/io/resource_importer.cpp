/*************************************************************************/
/*  resource_importer.cpp                                                */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
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
#include "core/io/resource_loader.h"
#include "core/os/os.h"
#include "core/property_info.h"
#include "core/se_string.h"
#include "core/string_utils.h"
#include "core/string_utils.inl"
#include "core/variant_parser.h"
#include "EASTL/sort.h"

IMPL_GDCLASS(ResourceImporter)

bool ResourceFormatImporter::SortImporterByName::operator()(const ResourceImporterInterface *p_a, const ResourceImporterInterface *p_b) const {
    return se_string_view(p_a->get_importer_name()) < se_string_view(p_b->get_importer_name());
}

Error ResourceFormatImporter::_get_path_and_type(se_string_view p_path, PathAndType &r_path_and_type, bool *r_valid) const {

    Error err;
    FileAccess *f = FileAccess::open(se_string(p_path) + ".import", FileAccess::READ, &err);

    if (!f) {
        if (r_valid) {
            *r_valid = false;
        }
        return err;
    }

    VariantParser::Stream *stream=VariantParser::get_file_stream(f);

    se_string assign;
    Variant value;
    VariantParser::Tag next_tag;

    if (r_valid) {
        *r_valid = true;
    }

    int lines = 0;
    se_string error_text;
    bool path_found = false; //first match must have priority
    while (true) {

        assign = Variant().as<se_string>();
        next_tag.fields.clear();
        next_tag.name.clear();

        err = VariantParser::parse_tag_assign_eof(stream, lines, error_text, next_tag, assign, value, nullptr, true);
        if (err == ERR_FILE_EOF) {
            VariantParser::release_stream(stream);
            memdelete(f);
            return OK;
        } else if (err != OK) {
            ERR_PRINT("ResourceFormatImporter::load - " + se_string(p_path) + ".import:" + ::to_string(lines) + " error: " + error_text)
            VariantParser::release_stream(stream);
            memdelete(f);
            return err;
        }

        if (!assign.empty()) {
            if (!path_found && StringUtils::begins_with(assign,"path.") && r_path_and_type.path.empty()) {
                se_string_view feature(StringUtils::get_slice(assign,'.', 1));
                if (OS::get_singleton()->has_feature(feature)) {
                    r_path_and_type.path = value.as<se_string>();
                    path_found = true; //first match must have priority
                }

            } else if (!path_found && assign == "path") {
                r_path_and_type.path = value.as<se_string>();
                path_found = true; //first match must have priority
            } else if (assign == "type") {
                r_path_and_type.type = value.as<se_string>();
            } else if (assign == "importer") {
                r_path_and_type.importer = value.as<se_string>();
            } else if (assign == "group_file") {
                r_path_and_type.group_file = value.as<se_string>();
            } else if (assign == "metadata") {
                r_path_and_type.metadata = value;
            } else if (assign == "valid") {
                if (r_valid) {
                    *r_valid = value.as<bool>();
                }
            }

        } else if (next_tag.name != "remap") {
            break;
        }
    }

    VariantParser::release_stream(stream);
    memdelete(f);

    if (r_path_and_type.path.empty() || r_path_and_type.type.empty()) {
        return ERR_FILE_CORRUPT;
    }
    return OK;
}

RES ResourceFormatImporter::load(se_string_view p_path, se_string_view p_original_path, Error *r_error) {

    PathAndType pat;
    Error err = _get_path_and_type(p_path, pat);

    if (err != OK) {

        if (r_error)
            *r_error = err;

        return RES();
    }

    RES res(ResourceLoader::_load(pat.path, p_path, pat.type, false, r_error));

#ifdef TOOLS_ENABLED
    if (res) {
        res->set_import_last_modified_time(res->get_last_modified_time()); //pass this, if used
        res->set_import_path(pat.path);
    }
#endif

    return res;
}

void ResourceFormatImporter::get_recognized_extensions(PODVector<se_string> &p_extensions) const {

    Set<se_string> found;

    for (int i = 0; i < importers.size(); i++) {
        PODVector<se_string> local_exts;
        importers[i]->get_recognized_extensions(local_exts);
        for (auto & ext : local_exts) {
            if (!found.contains(ext)) {
                p_extensions.emplace_back(ext);
                found.insert(ext);
            }
        }
    }
    for (int i = 0; i < owned_importers.size(); i++) {
        PODVector<se_string> local_exts;
        owned_importers[i]->get_recognized_extensions(local_exts);
        for (size_t j=0,fin=local_exts.size(); j<fin; ++j) {
            if (!found.contains(local_exts[j])) {
                p_extensions.emplace_back(local_exts[j]);
                found.insert(local_exts[j]);
            }
        }
    }
}

void ResourceFormatImporter::get_recognized_extensions_for_type(se_string_view p_type, PODVector<se_string> &p_extensions) const {

    if (p_type.empty()) {
        get_recognized_extensions(p_extensions);
        return;
    }

    Set<se_string> found;

    for (int i = 0; i < importers.size(); i++) {
        StringName res_type(importers[i]->get_resource_type());
        if (res_type.empty())
            continue;

        if (!ClassDB::is_parent_class(res_type, StringName(p_type)))
            continue;

        PODVector<se_string> local_exts;
        importers[i]->get_recognized_extensions(local_exts);
        for (size_t j=0,fin=local_exts.size(); j<fin; ++j) {
            if (!found.contains(local_exts[j])) {
                p_extensions.emplace_back(local_exts[j]);
                found.insert(local_exts[j]);
            }
        }
    }
    for (int i = 0; i < owned_importers.size(); i++) {
        StringName res_type(owned_importers[i]->get_resource_type());
        if (res_type.empty())
            continue;

        if (!ClassDB::is_parent_class(res_type, StringName(p_type)))
            continue;

        PODVector<se_string>  local_exts;
        owned_importers[i]->get_recognized_extensions(local_exts);
        for (size_t j=0,fin=local_exts.size(); j<fin; ++j) {
            if (!found.contains(local_exts[j])) {
                p_extensions.emplace_back(local_exts[j]);
                found.insert(local_exts[j]);
            }
        }
    }
}

bool ResourceFormatImporter::exists(se_string_view p_path) const {

    return FileAccess::exists(se_string(p_path) + ".import");
}

bool ResourceFormatImporter::recognize_path(se_string_view p_path, se_string_view /*p_for_type*/) const {

    return FileAccess::exists(se_string(p_path) + ".import");
}

bool ResourceFormatImporter::can_be_imported(se_string_view p_path) const {

    return ResourceFormatLoader::recognize_path(p_path);
}

int ResourceFormatImporter::get_import_order(se_string_view p_path) const {

    ResourceImporterInterface *importer = nullptr;

    if (FileAccess::exists(se_string(p_path) + ".import")) {

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

bool ResourceFormatImporter::handles_type(se_string_view p_type) const {

    for (int i = 0; i < importers.size(); i++) {

        StringName res_type(importers[i]->get_resource_type());
        if (res_type.empty())
            continue;
        if (ClassDB::is_parent_class(res_type, StringName(p_type)))
            return true;
    }
    for (int i = 0; i < owned_importers.size(); i++) {

        StringName res_type(owned_importers[i]->get_resource_type());
        if (res_type.empty())
            continue;
        if (ClassDB::is_parent_class(res_type, StringName(p_type)))
            return true;
    }

    return true;
}

se_string ResourceFormatImporter::get_internal_resource_path(se_string_view p_path) const {

    PathAndType pat;
    Error err = _get_path_and_type(p_path, pat);

    if (err != OK) {

        return se_string();
    }

    return pat.path;
}

void ResourceFormatImporter::get_internal_resource_path_list(se_string_view p_path, List<se_string> *r_paths) {

    Error err;
    FileAccess *f = FileAccess::open(se_string(p_path) + ".import", FileAccess::READ, &err);

    if (!f)
        return;

    VariantParser::Stream *stream=VariantParser::get_file_stream(f);

    se_string assign;
    Variant value;
    VariantParser::Tag next_tag;

    int lines = 0;
    se_string error_text;
    while (true) {

        assign = Variant().as<se_string>();
        next_tag.fields.clear();
        next_tag.name.clear();

        err = VariantParser::parse_tag_assign_eof(stream, lines, error_text, next_tag, assign, value, nullptr, true);
        if (err == ERR_FILE_EOF) {
            VariantParser::release_stream(stream);
            memdelete(f);
            return;
        } else if (err != OK) {
            ERR_PRINT("ResourceFormatImporter::get_internal_resource_path_list - " + se_string(p_path) + ".import:" + ::to_string(lines) +
                      " error: " + error_text)
            VariantParser::release_stream(stream);
            memdelete(f);
            return;
        }

        if (!assign.empty()) {
            if (StringUtils::begins_with(assign,"path.")) {
                r_paths->push_back(value.as<se_string>());
            } else if (assign == "path") {
                r_paths->push_back(value.as<se_string>());
            }
        } else if (next_tag.name != "remap") {
            break;
        }
    }
    VariantParser::release_stream(stream);
    memdelete(f);
}

se_string ResourceFormatImporter::get_import_group_file(se_string_view p_path) const {

    bool valid = true;
    PathAndType pat;
    _get_path_and_type(p_path, pat, &valid);
    return valid ? pat.group_file : se_string();
}

bool ResourceFormatImporter::is_import_valid(se_string_view p_path) const {

    bool valid = true;
    PathAndType pat;
    _get_path_and_type(p_path, pat, &valid);
    return valid;
}

se_string ResourceFormatImporter::get_resource_type(se_string_view p_path) const {

    PathAndType pat;
    Error err = _get_path_and_type(p_path, pat);

    if (err != OK) {

        return se_string();
    }

    return pat.type;
}

Variant ResourceFormatImporter::get_resource_metadata(se_string_view p_path) const {
    PathAndType pat;
    Error err = _get_path_and_type(p_path, pat);

    if (err != OK) {

        return Variant();
    }

    return pat.metadata;
}

void ResourceFormatImporter::get_dependencies(se_string_view p_path, PODVector<se_string> &p_dependencies, bool p_add_types) {

    PathAndType pat;
    Error err = _get_path_and_type(p_path, pat);

    if (err != OK) {

        return;
    }

    ResourceLoader::get_dependencies(pat.path, p_dependencies, p_add_types);
}

ResourceImporterInterface *ResourceFormatImporter::get_importer_by_name(se_string_view p_name) const {

    for (int i = 0; i < importers.size(); i++) {
        if (importers[i]->get_importer_name() == p_name) {
            return importers[i];
        }
    }
    for (int i = 0; i < owned_importers.size(); i++) {
        if (owned_importers[i]->get_importer_name() == p_name) {
            return const_cast<ResourceImporterInterface *>(static_cast<const ResourceImporterInterface *>(owned_importers[i].get()));
        }
    }
    return nullptr;
}

void ResourceFormatImporter::get_importers_for_extension(se_string_view p_extension, Vector<ResourceImporterInterface *> *r_importers) {

    for (int i = 0; i < importers.size(); i++) {
        PODVector<se_string> local_exts;
        importers[i]->get_recognized_extensions(local_exts);
        for (size_t j=0,fin=local_exts.size(); j<fin; ++j) {
            if (StringUtils::to_lower(p_extension) == local_exts[j]) {
                r_importers->push_back(importers[i]);
            }
        }
    }
    for (int i = 0; i < owned_importers.size(); i++) {
        PODVector<se_string> local_exts;
        owned_importers[i]->get_recognized_extensions(local_exts);
        for (size_t j=0,fin=local_exts.size(); j<fin; ++j) {
            if (StringUtils::to_lower(p_extension) == local_exts[j]) {
                r_importers->push_back(owned_importers.write[i].get());
            }
        }
    }
}

ResourceImporterInterface *ResourceFormatImporter::get_importer_by_extension(se_string_view p_extension) const {

    ResourceImporterInterface *importer = nullptr;
    float priority = 0;

    for (int i = 0; i < importers.size(); i++) {

        PODVector<se_string> local_exts;
        importers[i]->get_recognized_extensions(local_exts);
        for (size_t j=0,fin=local_exts.size(); j<fin; ++j) {
            if (StringUtils::to_lower(p_extension) == local_exts[j] && importers[i]->get_priority() > priority) {
                importer = importers[i];
                priority = importers[i]->get_priority();
            }
        }
    }
    for (int i = 0; i < owned_importers.size(); i++) {
        PODVector<se_string> local_exts;
        owned_importers[i]->get_recognized_extensions(local_exts);
        for (size_t j=0,fin=local_exts.size(); j<fin; ++j) {
            if (StringUtils::to_lower(p_extension) == local_exts[j] && importers[i]->get_priority() > priority) {
                importer = const_cast<ResourceImporterInterface *>(static_cast<const ResourceImporterInterface *>(owned_importers[i].get()));
                priority = owned_importers[i]->get_priority();
            }
        }
    }
    return importer;
}

se_string ResourceFormatImporter::get_import_base_path(se_string_view p_for_file) const {

    return "res://.import/" + se_string(PathUtils::get_file(p_for_file)) + "-" + StringUtils::md5_text(p_for_file);
}

bool ResourceFormatImporter::are_import_settings_valid(se_string_view p_path) const {

    bool valid = true;
    PathAndType pat;
    _get_path_and_type(p_path, pat, &valid);

    if (!valid) {
        return false;
    }
    se_string_view pat_importer(pat.importer);
    for (int i = 0; i < importers.size(); i++) {
        if (importers[i]->get_importer_name() == pat_importer) {
            if (!importers[i]->are_import_settings_valid(p_path)) { //importer thinks this is not valid
                return false;
            }
        }
    }
    for (int i = 0; i < owned_importers.size(); i++) {
        if (owned_importers[i]->get_importer_name() == pat_importer) {
            if (!owned_importers[i]->are_import_settings_valid(p_path)) { //importer thinks this is not valid
                return false;
            }
        }
    }
    return true;
}

se_string ResourceFormatImporter::get_import_settings_hash() const {

    FixedVector<const ResourceImporterInterface *,64,true> sorted_importers;

    for(int i=0; i<importers.size(); ++i)
        sorted_importers.push_back(importers[i]);
    for(int i=0; i<owned_importers.size(); ++i)
        sorted_importers.push_back(owned_importers[i].get());
    eastl::sort(sorted_importers.begin(),sorted_importers.end(),SortImporterByName());

    se_string hash;
    for (const ResourceImporterInterface *imp : sorted_importers) {
        hash += se_string(":") + imp->get_importer_name() + ":" + imp->get_import_settings_string();
    }
    return StringUtils::md5_text(hash);
}

ResourceFormatImporter *ResourceFormatImporter::singleton = nullptr;

ResourceFormatImporter::ResourceFormatImporter() {
    singleton = this;
}


