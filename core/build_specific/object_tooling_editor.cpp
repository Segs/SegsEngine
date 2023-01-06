#include "core/object_tooling.h"

#include "core/resource/resource_tools.h"

#include "core/class_db.h"
#include "core/compressed_translation.h"
#include "core/engine.h"
#include "core/hash_set.h"
#include "core/io/compression.h"
#include "core/list.h"
#include "core/method_info.h"
#include "core/object.h"
#include "core/os/file_access.h"
#include "core/os/memory.h"
#include "core/property_info.h"
#include "core/script_language.h"
#include "core/set.h"
#include "core/string.h"

namespace {
struct _PHashTranslationCmp {
    int orig_len;
    String compressed;
    int offset;
};
struct ObjectToolingImpl final : public IObjectTooling {
    Set<String> editor_section_folding;
    HashSet<Object *> change_receptors;
    uint32_t _edited_version;
    bool _edited;

    // IObjectTooling interface
public:
    void set_edited(bool p_edited, bool increment_version = true) override {
        _edited = p_edited;
        if (increment_version) {
            _edited_version++;
        }
    }
    bool is_edited() const override { return _edited; }
    uint32_t get_edited_version() const override { return _edited_version; }
    void editor_set_section_unfold(StringView p_section, bool p_unfolded) override {
        set_edited(true);
        if (p_unfolded) {
            editor_section_folding.insert(p_section);
        } else {
            auto iter = editor_section_folding.find_as(p_section);
            if (iter != editor_section_folding.end()) {
                editor_section_folding.erase(iter);
            }
        }
    }
    [[nodiscard]] bool editor_is_section_unfolded(StringView p_section) const override {
        return editor_section_folding.contains_as(p_section);
    }
    [[nodiscard]] const Set<String> &editor_get_section_folding() const override { return editor_section_folding; }
    void editor_clear_section_folding() override { editor_section_folding.clear(); }
    ObjectToolingImpl() {
        _edited = false;
        _edited_version = 0;
    }
};
} // namespace

void Object_change_notify(Object *self, const StringName &p_property) {
    auto tooling_iface = (ObjectToolingImpl *)self->get_tooling_interface();
    tooling_iface->set_edited(true, false);
    for (Object *E : tooling_iface->change_receptors) {
        E->_changed_callback(self, p_property);
    }
}

void relase_tooling(IObjectTooling *s) {
    memdelete(s);
}

IObjectTooling *create_tooling_for(Object *self) {
    return memnew(ObjectToolingImpl);
}

void Object_add_change_receptor(Object *self, Object *p_receptor) {
    auto tooling_iface = (ObjectToolingImpl *)self->get_tooling_interface();
    tooling_iface->change_receptors.insert(p_receptor);
}
void Object_remove_change_receptor(Object *self, Object *p_receptor) {
    auto tooling_iface = (ObjectToolingImpl *)self->get_tooling_interface();
    tooling_iface->change_receptors.erase(p_receptor);
}

void Object_set_edited(Object *self, bool p_edited, bool increment_version) {
    auto tooling_iface = (ObjectToolingImpl *)self->get_tooling_interface();
    tooling_iface->set_edited(p_edited, increment_version);
}

bool Object_set_fallback(Object *self, const StringName &p_name, const Variant &p_value) {
    bool valid = false;
    auto si = self->get_script_instance();
    if (si) {
        si->property_set_fallback(p_name, p_value, &valid);
    }
    return valid;
}

Variant Object_get_fallback(const Object *self, const StringName &p_name, bool &r_valid) {
    auto si = self->get_script_instance();
    Variant ret;
    if (si) {
        bool valid;
        ret = si->property_get_fallback(p_name, &valid);
        if (valid) {
            r_valid = true;
            return ret;
        }
    }
    r_valid = false;
    return ret;
}

bool Object_script_signal_validate(const RefPtr &script) {
    // allow connecting signals anyway if script is invalid, see issue #17070
    if (!refFromRefPtr<Script>(script)->is_valid()) {
        return true;
    }
    return false;
}

bool Object_allow_disconnect(uint32_t f) {
    if ((f & ObjectNS::CONNECT_PERSIST) && Engine::get_singleton()->is_editor_hint()) {
        // this signal was connected from the editor, and is being edited. just don't disconnect for now
        return false;
    }
    return true;
}

void Object_add_tooling_methods() {
    MethodInfo miget("_get", PropertyInfo(VariantType::STRING, "property"));
    miget.return_val.name = "Variant";
    miget.return_val.usage |= PROPERTY_USAGE_NIL_IS_VARIANT;
    ClassDB::add_virtual_method(Object::get_class_static_name(), miget);

    MethodInfo plget(VariantType::ARRAY, "_get_property_list");
    ClassDB::add_virtual_method(Object::get_class_static_name(), plget);
}
namespace Tooling {

bool tooling_log() {
    return true;
}
void importer_load(const Ref<Resource> &res, const String &path) {
    // In tooling scenarios record modification times and source paths
    if (res) {
        ResourceTooling::set_import_last_modified_time(
                res.get(), ResourceTooling::get_last_modified_time(res.get())); // pass this, if used
        ResourceTooling::set_import_path(res.get(), path);
    }
}

void add_virtual_method(const StringName &string_name, const MethodInfo &method_info) {
    ClassDB::add_virtual_method(string_name, method_info);
}

bool class_can_instance_cb(ClassDB_ClassInfo *ti, const StringName &p_class) {
    if (ti->api == API_EDITOR && !Engine::get_singleton()->is_editor_hint()) {
        ERR_PRINT("Class '" + String(p_class) + "' can only be instantiated by editor.");
        return false;
    }
    return true;
}
void generate_phash_translation(PHashTranslation &tgt, const Ref<Translation> &p_from) {
    ERR_FAIL_COND(!p_from);
    List<StringName> keys;
    p_from->get_message_list(&keys);

    size_t size = Math::larger_prime(keys.size());

    Vector<Vector<Pair<int, String>>> buckets;
    Vector<HashMap<uint32_t, int>> table;
    Vector<uint32_t> hfunc_table;
    Vector<_PHashTranslationCmp> compressed;

    table.resize(size);
    hfunc_table.resize(size);
    buckets.resize(size);
    compressed.resize(keys.size());

    int idx = 0;
    int total_compression_size = 0;

    for (const StringName &E : keys) {
        // hash string
        StringView cs(E);
        uint32_t h = phash_calculate(0, E.asCString());
        Pair<int, String> p;
        p.first = idx;
        p.second = cs;
        buckets[h % size].push_back(p);

        // compress string
        StringView src_s(p_from->get_message(E));
        _PHashTranslationCmp ps;
        ps.orig_len = src_s.size();
        ps.offset = total_compression_size;

        if (ps.orig_len != 0) {
            String dst_s;
            dst_s.resize(src_s.size());
            int ret = Compression::compress_short_string(src_s.data(), src_s.size(), dst_s.data(), src_s.size());
            if (ret >= src_s.size()) {
                // if compressed is larger than original, just use original
                ps.orig_len = src_s.size();
                ps.compressed = src_s;
            } else {
                dst_s.resize(ret);
                // ps.orig_len=;
                ps.compressed = dst_s;
            }
        } else {
            ps.orig_len = 1;
            ps.compressed.resize(1);
            ps.compressed[0] = 0;
        }

        compressed[idx] = ps;
        total_compression_size += ps.compressed.size();
        idx++;
    }

    int bucket_table_size = 0;

    for (size_t i = 0; i < size; i++) {
        const Vector<Pair<int, String>> &b = buckets[i];
        HashMap<uint32_t, int> &t = table[i];

        if (b.empty()) {
            continue;
        }

        int d = 1;
        int item = 0;

        while (item < b.size()) {
            uint32_t slot = phash_calculate(d, b[item].second.data());
            if (t.contains(slot)) {
                item = 0;
                d++;
                t.clear();
            } else {
                t[slot] = b[item].first;
                item++;
            }
        }

        hfunc_table[i] = d;
        bucket_table_size += 2 + b.size() * 4;
    }

    ERR_FAIL_COND(bucket_table_size == 0);

    tgt.hash_table.resize(size);
    tgt.bucket_table.resize(bucket_table_size);

    auto &htwb = tgt.hash_table;
    auto &btwb = tgt.bucket_table;

    uint32_t *htw = (uint32_t *)&htwb[0];
    uint32_t *btw = (uint32_t *)&btwb[0];

    int btindex = 0;

    for (size_t i = 0; i < size; i++) {
        const HashMap<uint32_t, int> &t = table[i];
        if (t.empty()) {
            htw[i] = 0xFFFFFFFF; // nothing
            continue;
        }

        htw[i] = btindex;
        btw[btindex++] = t.size();
        btw[btindex++] = hfunc_table[i];

        for (const eastl::pair<const uint32_t, int> &E : t) {
            btw[btindex++] = E.first;
            btw[btindex++] = compressed[E.second].offset;
            btw[btindex++] = compressed[E.second].compressed.size();
            btw[btindex++] = compressed[E.second].orig_len;
        }
    }

    tgt.strings.resize(total_compression_size);
    Vector<uint8_t> &cw = tgt.strings;

    for (auto &i : compressed) {
        memcpy(&cw[i.offset], i.compressed.data(), i.compressed.size());
    }

    ERR_FAIL_COND(btindex != bucket_table_size);
    tgt.set_locale(p_from->get_locale());
}

bool check_resource_manager_load(StringView p_path) {
    FileAccessRef file_check = FileAccess::create(FileAccess::ACCESS_RESOURCES);
    return file_check->file_exists(p_path);
}

} // namespace Tooling
