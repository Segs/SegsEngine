/*************************************************************************/
/*  resource_format_binary.h                                             */
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

#pragma once

#include "core/io/resource_format_loader.h"
#include "core/hash_map.h"
#include "core/set.h"
#include "core/list.h"
#include "core/io/resource_saver.h"
#include "core/os/file_access.h"

class ResourceInteractiveLoaderBinary : public ResourceInteractiveLoader {
    friend class ResourceFormatLoaderBinary;

    struct ExtResource {
        String path;
        String type;
    };
    struct IntResource {
        String path;
        uint64_t offset;
    };

    HashMap<String, String> remaps;
    Vector<char> str_buf;
    Vector<StringName> string_map;
    Vector<IntResource> internal_resources;
    Vector<ExtResource> external_resources;
    List<RES> resource_cache;
    String local_path;
    String res_path;
    String type;
    Ref<Resource> resource;
    uint32_t ver_format;
    FileAccess *f=nullptr;
    uint64_t importmd_ofs;
    Error error = OK;
    int stage = 0;
    bool translation_remapped = false;

    StringName _get_string();
    String get_unicode_string();
    void _advance_padding(uint32_t p_len);

    Error parse_variant(Variant &r_v);

public:
    void set_local_path(StringView p_local_path) override;
    const Ref<Resource> &get_resource() override;
    Error poll() override;
    int get_stage() const override;
    int get_stage_count() const override;
    void set_translation_remapped(bool p_remapped) override;

    void set_remaps(const HashMap<String, String> &p_remaps) { remaps = p_remaps; }
    void open(FileAccess *p_f);
    String recognize(FileAccess *p_f);
    void get_dependencies(FileAccess *p_f, Vector<String> &p_dependencies, bool p_add_types);

    ResourceInteractiveLoaderBinary() = default;
    ~ResourceInteractiveLoaderBinary() override;
};

class ResourceFormatLoaderBinary : public ResourceFormatLoader {
public:
    Ref<ResourceInteractiveLoader> load_interactive(StringView p_path, StringView p_original_path = StringView(), Error *r_error = nullptr) override;
    void get_recognized_extensions_for_type(StringView p_type, Vector<String> &p_extensions) const override;
    void get_recognized_extensions(Vector<String> &p_extensions) const override;
    bool handles_type(StringView /*p_type*/) const override;
    String get_resource_type(StringView p_path) const override;
    void get_dependencies(StringView p_path, Vector<String> &p_dependencies, bool p_add_types = false) override;
    Error rename_dependencies(StringView p_path, const HashMap<String, String> &p_map) override;
};

class ResourceFormatSaverBinaryInstance {

    String local_path;
    String path;

    bool relative_paths;
    bool bundle_resources;
    bool skip_editor;
    bool big_endian;
    bool takeover_paths;
    FileAccess *f;
    String magic;
    Set<RES> resource_set;

    struct NonPersistentKey { //for resource properties generated on the fly
        RES base;
        StringName property;
        bool operator<(const NonPersistentKey &p_key) const { return base == p_key.base ? property < p_key.property : base < p_key.base; }
        bool operator==(const NonPersistentKey &p_key) const { return base == p_key.base && property == p_key.property; }
        // Used by default eastl::hash<T>
        explicit operator size_t() const {
            return (uint64_t(property.hash())<<32) | eastl::hash<intptr_t>()(intptr_t(base.get())/next_power_of_2(sizeof(Resource)));
        }
    };

    HashMap<NonPersistentKey, RES> non_persistent_map;
    HashMap<StringName, int> string_map;
    Vector<StringName> strings;

    HashMap<RES, int> external_resources;
    List<RES> saved_resources;

    static void _pad_buffer(FileAccess *f, int p_bytes);
    void _write_variant(const Variant &p_property);
    void _find_resources(const Variant &p_variant, bool p_main = false);
    static void save_unicode_string(FileAccess *f, StringView p_string, bool p_bit_on_len = false);
    int get_string_index(const StringName &p_string);

public:
    Error save(StringView p_path, const RES &p_resource, uint32_t p_flags = 0);
    static void write_variant(FileAccess *f, const Variant &p_property, Set<RES> &resource_set, HashMap<RES, int> &external_resources, HashMap<StringName, int> &string_map);
};

class ResourceFormatSaverBinary : public ResourceFormatSaver {
public:
    static ResourceFormatSaverBinary *singleton;
    Error save(StringView p_path, const RES &p_resource, uint32_t p_flags = 0) override;
    bool recognize(const RES &p_resource) const override;
    void get_recognized_extensions(const RES &p_resource, Vector<String> &p_extensions) const override;

    ResourceFormatSaverBinary();
};
