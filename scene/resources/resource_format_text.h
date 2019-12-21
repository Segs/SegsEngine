/*************************************************************************/
/*  resource_format_text.h                                               */
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

#include "core/io/resource_loader.h"
#include "core/io/resource_format_loader.h"
#include "core/io/resource_saver.h"
#include "core/os/file_access.h"
#include "core/list.h"
#include "core/set.h"
#include "core/variant_parser.h"
#include "scene/resources/packed_scene.h"

class ResourceInteractiveLoaderText : public ResourceInteractiveLoader {

    bool translation_remapped;
    se_string local_path;
    se_string res_path;
    se_string error_text;

    FileAccess *f;

    VariantParser::Stream *stream=nullptr;

    struct ExtResource {
        se_string path;
        se_string type;
    };

    bool is_scene;
    StringName res_type;

    bool ignore_resource_parsing;

    //Map<String,String> remaps;

    Map<int, ExtResource> ext_resources;

    int resources_total;
    int resource_current;
    se_string resource_type;

    VariantParser::Tag next_tag;

    mutable int lines;

    Map<se_string, se_string> remaps;
    //void _printerr();

    static Error _parse_sub_resources(
            void *p_self, VariantParser::Stream *p_stream, Ref<Resource> &r_res, int &line, se_string &r_err_str) {
        return reinterpret_cast<ResourceInteractiveLoaderText *>(p_self)->_parse_sub_resource(
                p_stream, r_res, line, r_err_str);
    }
    static Error _parse_ext_resources(
            void *p_self, VariantParser::Stream *p_stream, Ref<Resource> &r_res, int &line, se_string &r_err_str) {
        return reinterpret_cast<ResourceInteractiveLoaderText *>(p_self)->_parse_ext_resource(
                p_stream, r_res, line, r_err_str);
    }

    Error _parse_sub_resource(VariantParser::Stream *p_stream, Ref<Resource> &r_res, int &line, se_string &r_err_str);
    Error _parse_ext_resource(VariantParser::Stream *p_stream, Ref<Resource> &r_res, int &line, se_string &r_err_str);

    // for converter
    class DummyResource : public Resource {
    public:
    };

    struct DummyReadData {

        Map<RES, int> external_resources;
        Map<int, RES> rev_external_resources;
        Set<RES> resource_set;
        Map<int, RES> resource_map;
    };

    static Error _parse_sub_resource_dummys(void *p_self, VariantParser::Stream *p_stream, Ref<Resource> &r_res, int &line, se_string &r_err_str) { return _parse_sub_resource_dummy((DummyReadData *)(p_self), p_stream, r_res, line, r_err_str); }
    static Error _parse_ext_resource_dummys(void *p_self, VariantParser::Stream *p_stream, Ref<Resource> &r_res, int &line, se_string &r_err_str) { return _parse_ext_resource_dummy((DummyReadData *)(p_self), p_stream, r_res, line, r_err_str); }

    static Error _parse_sub_resource_dummy(DummyReadData *p_data, VariantParser::Stream *p_stream, Ref<Resource> &r_res, int &line, se_string &r_err_str);
    static Error _parse_ext_resource_dummy(DummyReadData *p_data, VariantParser::Stream *p_stream, Ref<Resource> &r_res, int &line, se_string &r_err_str);

    VariantParser::ResourceParser rp;

    friend class ResourceFormatLoaderText;

    ListPOD<RES> resource_cache;
    Error error;

    RES resource;

    Ref<PackedScene> _parse_node_tag(VariantParser::ResourceParser &parser);

public:
    void set_local_path(se_string_view p_local_path) override;
    Ref<Resource> get_resource() override;
    Error poll() override;
    int get_stage() const override;
    int get_stage_count() const override;
    void set_translation_remapped(bool p_remapped) override;

    void open(FileAccess *p_f, bool p_skip_first_tag = false);
    se_string recognize(FileAccess *p_f);
    void get_dependencies(FileAccess *p_f, PODVector<se_string> &p_dependencies, bool p_add_types);
    Error rename_dependencies(FileAccess *p_f, se_string_view p_path, const Map<se_string, se_string> &p_map);

    Error save_as_binary(FileAccess *p_f, se_string_view p_path);
    ResourceInteractiveLoaderText();
    ~ResourceInteractiveLoaderText() override;
};

class ResourceFormatLoaderText : public ResourceFormatLoader {
public:
    static ResourceFormatLoaderText *singleton;
    Ref<ResourceInteractiveLoader> load_interactive(se_string_view p_path, se_string_view p_original_path = {}, Error *r_error = nullptr) override;
    void get_recognized_extensions_for_type(se_string_view p_type, PODVector<se_string> &p_extensions) const override;
    void get_recognized_extensions(PODVector<se_string> &p_extensions) const override;
    bool handles_type(se_string_view p_type) const override;
    se_string get_resource_type(se_string_view p_path) const override;
    void get_dependencies(se_string_view p_path, PODVector<se_string> &p_dependencies, bool p_add_types = false) override;
    Error rename_dependencies(se_string_view p_path, const Map<se_string, se_string> &p_map) override;

    static Error convert_file_to_binary(se_string_view p_src_path, se_string_view p_dst_path);

    ResourceFormatLoaderText() { singleton = this; }
};

class ResourceFormatSaverText : public ResourceFormatSaver {
public:
    static ResourceFormatSaverText *singleton;
    Error save(se_string_view p_path, const RES &p_resource, uint32_t p_flags = 0) override;
    bool recognize(const RES &p_resource) const override;
    void get_recognized_extensions(const RES &p_resource, PODVector<se_string> &p_extensions) const override;

    ResourceFormatSaverText();
};
