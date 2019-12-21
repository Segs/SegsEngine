/*************************************************************************/
/*  resource_importer.h                                                  */
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

#pragma once

#include "core/io/resource_format_loader.h"
#include "core/property_info.h"

#include "core/plugin_interfaces/ResourceImporterInterface.h"

class ResourceImporter;

class GODOT_EXPORT ResourceFormatImporter : public ResourceFormatLoader {

    struct PathAndType {
        se_string path;
        se_string type;
        se_string importer;
        se_string group_file;
        Variant metadata;
    };

    Error _get_path_and_type(se_string_view p_path, PathAndType &r_path_and_type, bool *r_valid = nullptr) const;

    static ResourceFormatImporter *singleton;

    //need them to stay in order to compute the settings hash
    struct SortImporterByName {
        bool operator()(const ResourceImporterInterface *p_a, const ResourceImporterInterface *p_b) const;
    };

    Vector<ResourceImporterInterface *> importers; // Importers provided by plugins, not owned by this class
    Vector<Ref<ResourceImporter>> owned_importers; // Importers provided by scripts, co-owned by this class
public:
    static ResourceFormatImporter *get_singleton() { return singleton; }
    RES load(se_string_view p_path, se_string_view p_original_path = se_string_view(), Error *r_error = nullptr) override;
    void get_recognized_extensions(PODVector<se_string> &p_extensions) const override;
    void get_recognized_extensions_for_type(se_string_view p_type, PODVector<se_string> &p_extensions) const override;
    bool recognize_path(se_string_view p_path, se_string_view p_for_type = se_string_view()) const override;
    bool handles_type(se_string_view p_type) const override;
    se_string get_resource_type(se_string_view p_path) const override;
    virtual Variant get_resource_metadata(se_string_view p_path) const;
    bool is_import_valid(se_string_view p_path) const override;
    void get_dependencies(se_string_view p_path, PODVector<se_string> &p_dependencies, bool p_add_types = false) override;
    bool is_imported(se_string_view p_path) const override {
        return recognize_path(p_path);
    }
    se_string get_import_group_file(se_string_view p_path) const override;
    bool exists(se_string_view p_path) const override;

    virtual bool can_be_imported(se_string_view p_path) const;
    int get_import_order(se_string_view p_path) const override;

    se_string get_internal_resource_path(se_string_view p_path) const;
    void get_internal_resource_path_list(se_string_view p_path, DefList<se_string> *r_paths);

    void add_importer(ResourceImporterInterface *p_importer) {
        importers.push_back(p_importer);
    }
    void add_importer(const Ref<ResourceImporter> &p_importer) {
        owned_importers.push_back(p_importer);
    }
    void remove_importer(const Ref<ResourceImporter> &p_importer) { owned_importers.erase(p_importer); }
    void remove_importer(ResourceImporterInterface *p_importer) { importers.erase(p_importer); }

    ResourceImporterInterface * get_importer_by_name(se_string_view p_name) const;
    ResourceImporterInterface * get_importer_by_extension(se_string_view p_extension) const;

    void get_importers_for_extension(se_string_view p_extension, Vector<ResourceImporterInterface *> *r_importers);

    bool are_import_settings_valid(se_string_view p_path) const;
    se_string get_import_settings_hash() const;

    se_string get_import_base_path(se_string_view p_for_file) const;
    ResourceFormatImporter();
};



class ResourceImporter : public RefCounted,public ResourceImporterInterface {

    GDCLASS(ResourceImporter, RefCounted)

public:
    float get_priority() const override { return 1.0; }
    int get_import_order() const override { return 0; }
    int get_preset_count() const override { return 0; }
    StringName get_preset_name(int /*p_idx*/) const override { return {}; }
    StringName get_option_group_file() const override { return {}; }
    Error import(se_string_view p_source_file, se_string_view p_save_path, const Map<StringName, Variant> &p_options,
            DefList<se_string> *r_platform_variants, DefList<se_string> *r_gen_files = nullptr,
            Variant *r_metadata = nullptr) override = 0;
    Error import_group_file(se_string_view  /*p_group_file*/,
            const Map<se_string, Map<StringName, Variant>> & /*p_source_file_options*/,
            const Map<se_string, se_string> & /*p_base_paths*/) override {
        return ERR_UNAVAILABLE;
    }
    bool are_import_settings_valid(se_string_view /*p_path*/) const override { return true; }
    se_string get_import_settings_string() const override { return se_string(); }
};
