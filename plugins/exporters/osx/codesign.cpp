/*************************************************************************/
/*  codesign.cpp                                                         */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
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

#include "codesign.h"

#include "lipo.h"
#include "macho.h"
#include "plist.h"

#include "core/print_string.h"
#include "core/string_utils.h"
#include "core/translation_helpers.h"
#include "core/os/os.h"
#include "core/pool_vector.h"
#include "core/string_formatter.h"
#include "editor/editor_settings.h"

#include <ctime>

/*************************************************************************/
/* CodeSignCodeResources                                                 */
/*************************************************************************/

String CodeSignCodeResources::hash_sha1_base64(const String &p_path) {
    FileAccessRef fa = FileAccess::open(p_path, FileAccess::READ);
    ERR_FAIL_COND_V_MSG(!fa, String(), FormatVE("CodeSign/CodeResources: Can't open file: \"%s\".", p_path.c_str()));

    CryptoCore::SHA1Context ctx;
    ctx.start();

    unsigned char step[4096];
    while (true) {
        uint64_t br = fa->get_buffer(step, 4096);
        if (br > 0) {
            ctx.update(step, br);
        }
        if (br < 4096) {
            break;
        }
    }

    unsigned char hash[0x14];
    ctx.finish(hash);
    fa->close();

    return CryptoCore::b64_encode_str(hash, 0x14);
}

String CodeSignCodeResources::hash_sha256_base64(const String &p_path) {
    FileAccessRef fa = FileAccess::open(p_path, FileAccess::READ);
    ERR_FAIL_COND_V_MSG(!fa, String(), FormatVE("CodeSign/CodeResources: Can't open file: \"%s\".", p_path.c_str()));

    CryptoCore::SHA256Context ctx;
    ctx.start();

    unsigned char step[4096];
    while (true) {
        uint64_t br = fa->get_buffer(step, 4096);
        if (br > 0) {
            ctx.update(step, br);
        }
        if (br < 4096) {
            break;
        }
    }

    unsigned char hash[0x20];
    ctx.finish(hash);
    fa->close();

    return CryptoCore::b64_encode_str(hash, 0x20);
}

void CodeSignCodeResources::add_rule1(const String &p_rule, const String &p_key, int p_weight, bool p_store) {
    rules1.push_back(CRRule(p_rule, p_key, p_weight, p_store));
}

void CodeSignCodeResources::add_rule2(const String &p_rule, const String &p_key, int p_weight, bool p_store) {
    rules2.push_back(CRRule(p_rule, p_key, p_weight, p_store));
}

CodeSignCodeResources::CRMatch CodeSignCodeResources::match_rules1(const String &p_path) const {
    CRMatch found = CRMatch::CR_MATCH_NO;
    int weight = 0;
    for (int i = 0; i < rules1.size(); i++) {
        RegEx regex(rules1[i].file_pattern);
        if (regex.search(p_path)) {
            if (rules1[i].key == "omit") {
                return CRMatch::CR_MATCH_NO;
            } else if (rules1[i].key == "nested") {
                if (weight <= rules1[i].weight) {
                    found = CRMatch::CR_MATCH_NESTED;
                    weight = rules1[i].weight;
                }
            } else if (rules1[i].key == "optional") {
                if (weight <= rules1[i].weight) {
                    found = CRMatch::CR_MATCH_OPTIONAL;
                    weight = rules1[i].weight;
                }
            } else {
                if (weight <= rules1[i].weight) {
                    found = CRMatch::CR_MATCH_YES;
                    weight = rules1[i].weight;
                }
            }
        }
    }
    return found;
}

CodeSignCodeResources::CRMatch CodeSignCodeResources::match_rules2(const String &p_path) const {
    CRMatch found = CRMatch::CR_MATCH_NO;
    int weight = 0;
    for (int i = 0; i < rules2.size(); i++) {
        RegEx regex(rules2[i].file_pattern);
        if (regex.search(p_path)) {
            if (rules2[i].key == "omit") {
                return CRMatch::CR_MATCH_NO;
            } else if (rules2[i].key == "nested") {
                if (weight <= rules2[i].weight) {
                    found = CRMatch::CR_MATCH_NESTED;
                    weight = rules2[i].weight;
                }
            } else if (rules2[i].key == "optional") {
                if (weight <= rules2[i].weight) {
                    found = CRMatch::CR_MATCH_OPTIONAL;
                    weight = rules2[i].weight;
                }
            } else {
                if (weight <= rules2[i].weight) {
                    found = CRMatch::CR_MATCH_YES;
                    weight = rules2[i].weight;
                }
            }
        }
    }
    return found;
}

bool CodeSignCodeResources::add_file1(StringView p_root, const String &p_path) {
    CRMatch found = match_rules1(p_path);
    if (found != CRMatch::CR_MATCH_YES && found != CRMatch::CR_MATCH_OPTIONAL) {
        return true; // No match.
    }

    CRFile f;
    f.name = p_path;
    f.optional = (found == CRMatch::CR_MATCH_OPTIONAL);
    f.nested = false;
    f.hash = hash_sha1_base64(PathUtils::plus_file(p_root,p_path));
    print_verbose(FormatVE("CodeSign/CodeResources: File(V1) %s hash1:%s", f.name.c_str(), f.hash.c_str()));

    files1.push_back(f);
    return true;
}

bool CodeSignCodeResources::add_file2(StringView p_root, const String &p_path) {
    CRMatch found = match_rules2(p_path);
    if (found == CRMatch::CR_MATCH_NESTED) {
        return add_nested_file(p_root, p_path, PathUtils::plus_file(p_root,p_path));
    }
    if (found != CRMatch::CR_MATCH_YES && found != CRMatch::CR_MATCH_OPTIONAL) {
        return true; // No match.
    }

    CRFile f;
    f.name = p_path;
    f.optional = (found == CRMatch::CR_MATCH_OPTIONAL);
    f.nested = false;
    f.hash = hash_sha1_base64(PathUtils::plus_file(p_root,p_path));
    f.hash2 = hash_sha256_base64(PathUtils::plus_file(p_root,p_path));

    print_verbose(FormatVE("CodeSign/CodeResources: File(V2) %s hash1:%s hash2:%s", f.name.c_str(), f.hash.c_str(), f.hash2.c_str()));

    files2.push_back(f);
    return true;
}

bool CodeSignCodeResources::add_nested_file(StringView p_root, const String &p_path, const String &p_exepath) {
#define CLEANUP()                                       \
    if (files_to_add.size() > 1) {                      \
        for (int j = 0; j < files_to_add.size(); j++) { \
            da->remove(files_to_add[j]);                \
        }                                               \
    }

    DirAccessRef da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
    ERR_FAIL_COND_V(!da, false);

    Vector<String> files_to_add;
    if (LipO::is_lipo(p_exepath)) {
        String tmp_path_name = PathUtils::plus_file(EditorSettings::get_singleton()->get_cache_dir(),"_lipo");
        Error err = da->make_dir_recursive(tmp_path_name);
        if (err != OK) {
            ERR_FAIL_V_MSG(false, FormatVE("CodeSign/CodeResources: Failed to create \"%s\" subfolder.", tmp_path_name.c_str()));
        }
        LipO lip;
        if (lip.open_file(p_exepath)) {
            for (int i = 0; i < lip.get_arch_count(); i++) {
                if (!lip.extract_arch(i, PathUtils::plus_file(tmp_path_name,"_rqexe_" + itos(i)))) {
                    CLEANUP();
                    ERR_FAIL_V_MSG(false, "CodeSign/CodeResources: Failed to extract thin binary.");
                }
                files_to_add.push_back(PathUtils::plus_file(tmp_path_name,"_rqexe_" + itos(i)));
            }
        }
    } else if (MachO::is_macho(p_exepath)) {
        files_to_add.push_back(p_exepath);
    }

    CRFile f;
    f.name = p_path;
    f.optional = false;
    f.nested = true;
    for (int i = 0; i < files_to_add.size(); i++) {
        MachO mh;
        if (!mh.open_file(files_to_add[i])) {
            CLEANUP();
            ERR_FAIL_V_MSG(false, "CodeSign/CodeResources: Invalid executable file.");
        }
        auto hash = mh.get_cdhash_sha256(); // Use SHA-256 variant, if available.
        if (hash.size() != 0x20) {
            hash = mh.get_cdhash_sha1(); // Use SHA-1 instead.
            if (hash.size() != 0x14) {
                CLEANUP();
                ERR_FAIL_V_MSG(false, "CodeSign/CodeResources: Unsigned nested executable file.");
            }
        }
        hash = hash.subspan(0,0x14); // Always clamp to 0x14 size.
        f.hash = CryptoCore::b64_encode_str(hash.data(), hash.size());

        auto rq_blob = mh.get_requirements();
        String req_string;
        if (rq_blob.size() > 8) {
            CodeSignRequirements rq(rq_blob);
            Vector<String> rqs = rq.parse_requirements();
            for (int j = 0; j < rqs.size(); j++) {
                if (rqs[j].starts_with("designated => ")) {
                    req_string = rqs[j].replaced("designated => ", "");
                }
            }
        }
        if (req_string.empty()) {
            req_string = "cdhash H\"" + StringUtils::hex_encode_buffer(hash.data(), hash.size()) + "\"";
        }
        print_verbose(FormatVE("CodeSign/CodeResources: Nested object %s (cputype: %d) cdhash:%s designated rq:%s", f.name.c_str(), mh.get_cputype(), f.hash.c_str(), req_string.c_str()));
        if (f.requirements != req_string) {
            if (i != 0) {
                f.requirements += " or ";
            }
            f.requirements += req_string;
        }
    }
    files2.push_back(f);

    CLEANUP();
    return true;

#undef CLEANUP
}

bool CodeSignCodeResources::add_folder_recursive(StringView p_root, const String &p_path, const String &p_main_exe_path) {
    DirAccessRef da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
    ERR_FAIL_COND_V(!da, false);
    Error err = da->change_dir(PathUtils::plus_file(p_root,p_path));
    ERR_FAIL_COND_V(err != OK, false);

    bool ret = true;
    da->list_dir_begin();
    String n = da->get_next();
    while (n != String()) {
        if (n != "." && n != "..") {
            String path = PathUtils::plus_file(PathUtils::plus_file(p_root,p_path),n);
            if (path == p_main_exe_path) {
                n = da->get_next();
                continue; // Skip main executable.
            }
            if (da->current_is_dir()) {
                CRMatch found = match_rules2(PathUtils::plus_file(p_path,n));
                String fmw_ver = "Current"; // Framework version (default).
                String info_path;
                String main_exe;
                String bundle_path;
                bool bundle = false;
                if (da->file_exists(PathUtils::plus_file(path,"Contents/Info.plist"))) {
                    info_path = PathUtils::plus_file(path,"Contents/Info.plist");
                    main_exe = PathUtils::plus_file(path,"Contents/MacOS");
                    bundle = true;
                } else if (da->file_exists(PathUtils::plus_file(path,FormatVE("Versions/%s/Resources/Info.plist", fmw_ver.c_str())))) {
                    info_path = PathUtils::plus_file(path,FormatVE("Versions/%s/Resources/Info.plist", fmw_ver.c_str()));
                    main_exe = PathUtils::plus_file(path,FormatVE("Versions/%s", fmw_ver.c_str()));
                    bundle = true;
                } else if (da->file_exists(PathUtils::plus_file(path,"Info.plist"))) {
                    info_path = PathUtils::plus_file(path,"Info.plist");
                    main_exe = path;
                    bundle = true;
                }
                if (bundle && found == CRMatch::CR_MATCH_NESTED && !info_path.empty()) {
                    // Read Info.plist.
                    PList info_plist;
                    if (info_plist.load_file(info_path)) {
                        if (info_plist.get_root()->data_type == PList::PLNodeType::PL_NODE_TYPE_DICT && info_plist.get_root()->data_dict.contains("CFBundleExecutable")) {
                            main_exe = PathUtils::plus_file(main_exe,info_plist.get_root()->data_dict["CFBundleExecutable"]->data_string);
                        } else {
                            ERR_FAIL_V_MSG(false, "CodeSign/CodeResources: Invalid Info.plist, no exe name.");
                        }
                    } else {
                        ERR_FAIL_V_MSG(false, "CodeSign/CodeResources: Invalid Info.plist, can't load.");
                    }
                    ret = ret && add_nested_file(p_root, PathUtils::plus_file(p_path,n), main_exe);
                } else {
                    ret = ret && add_folder_recursive(p_root, PathUtils::plus_file(p_path,n), p_main_exe_path);
                }
            } else {
                ret = ret && add_file1(p_root, PathUtils::plus_file(p_path,n));
                ret = ret && add_file2(p_root, PathUtils::plus_file(p_path,n));
            }
        }

        n = da->get_next();
    }

    da->list_dir_end();
    return ret;
}

bool CodeSignCodeResources::save_to_file(const String &p_path) {
    PList pl;

    print_verbose(FormatVE("CodeSign/CodeResources: Writing to file: %s", p_path.c_str()));

    // Write version 1 hashes.
    Ref<PListNode> files1_dict = PListNode::new_dict();
    pl.get_root()->push_subnode(files1_dict, "files");
    for (int i = 0; i < files1.size(); i++) {
        if (files1[i].optional) {
            Ref<PListNode> file_dict = PListNode::new_dict();
            files1_dict->push_subnode(file_dict, files1[i].name);

            file_dict->push_subnode(PListNode::new_data(files1[i].hash), "hash");
            file_dict->push_subnode(PListNode::new_bool(true), "optional");
        } else {
            files1_dict->push_subnode(PListNode::new_data(files1[i].hash), files1[i].name);
        }
    }

    // Write version 2 hashes.
    Ref<PListNode> files2_dict = PListNode::new_dict();
    pl.get_root()->push_subnode(files2_dict, "files2");
    for (int i = 0; i < files2.size(); i++) {
        Ref<PListNode> file_dict = PListNode::new_dict();
        files2_dict->push_subnode(file_dict, files2[i].name);

        if (files2[i].nested) {
            file_dict->push_subnode(PListNode::new_data(files2[i].hash), "cdhash");
            file_dict->push_subnode(PListNode::new_string(files2[i].requirements), "requirement");
        } else {
            file_dict->push_subnode(PListNode::new_data(files2[i].hash), "hash");
            file_dict->push_subnode(PListNode::new_data(files2[i].hash2), "hash2");
            if (files2[i].optional) {
                file_dict->push_subnode(PListNode::new_bool(true), "optional");
            }
        }
    }

    // Write version 1 rules.
    Ref<PListNode> rules1_dict = PListNode::new_dict();
    pl.get_root()->push_subnode(rules1_dict, "rules");
    for (int i = 0; i < rules1.size(); i++) {
        if (rules1[i].store) {
            if (rules1[i].key.empty() && rules1[i].weight <= 0) {
                rules1_dict->push_subnode(PListNode::new_bool(true), rules1[i].file_pattern);
            } else {
                Ref<PListNode> rule_dict = PListNode::new_dict();
                rules1_dict->push_subnode(rule_dict, rules1[i].file_pattern);
                if (!rules1[i].key.empty()) {
                    rule_dict->push_subnode(PListNode::new_bool(true), rules1[i].key);
                }
                if (rules1[i].weight != 1) {
                    rule_dict->push_subnode(PListNode::new_real(rules1[i].weight), "weight");
                }
            }
        }
    }

    // Write version 2 rules.
    Ref<PListNode> rules2_dict = PListNode::new_dict();
    pl.get_root()->push_subnode(rules2_dict, "rules2");
    for (int i = 0; i < rules2.size(); i++) {
        if (rules2[i].store) {
            if (rules2[i].key.empty() && rules2[i].weight <= 0) {
                rules2_dict->push_subnode(PListNode::new_bool(true), rules2[i].file_pattern);
            } else {
                Ref<PListNode> rule_dict = PListNode::new_dict();
                rules2_dict->push_subnode(rule_dict, rules2[i].file_pattern);
                if (!rules2[i].key.empty()) {
                    rule_dict->push_subnode(PListNode::new_bool(true), rules2[i].key);
                }
                if (rules2[i].weight != 1) {
                    rule_dict->push_subnode(PListNode::new_real(rules2[i].weight), "weight");
                }
            }
        }
    }
    String text = pl.save_text();
    ERR_FAIL_COND_V_MSG(text.empty(), false, "CodeSign/CodeResources: Generating resources PList failed.");

    FileAccessRef fa = FileAccess::open(p_path, FileAccess::WRITE);
    ERR_FAIL_COND_V_MSG(!fa, false, FormatVE("CodeSign/CodeResources: Can't open file: \"%s\".", p_path.c_str()));

    fa->store_buffer((const uint8_t *)text.data(), text.size());
    fa->close();
    return true;
}

/*************************************************************************/
/* CodeSignRequirements                                                  */
/*************************************************************************/

CodeSignRequirements::CodeSignRequirements() {
#define _W(a, b, c, d) \
    blob.push_back(a); \
    blob.push_back(b); \
    blob.push_back(c); \
    blob.push_back(d);
    _W(0xFA, 0xDE, 0x0C, 0x01); // Requirement set magic.
    _W(0x00, 0x00, 0x00, 0x0C); // Length of requirements set (12 bytes).
    _W(0x00, 0x00, 0x00, 0x00); // Empty.
#undef _W
}

CodeSignRequirements::CodeSignRequirements(const Vector<uint8_t> &p_data) {
    blob = p_data;
}

_FORCE_INLINE_ void CodeSignRequirements::_parse_certificate_slot(uint32_t &r_pos, String &r_out, uint32_t p_rq_size) const {
#define _R(x) BSWAP32(*(uint32_t *)(blob.data() + x))
    ERR_FAIL_COND_MSG(r_pos >= p_rq_size, "CodeSign/Requirements: Out of bounds.");
    r_out += "certificate ";
    uint32_t tag_slot = _R(r_pos);
    if (tag_slot == 0x00000000) {
        r_out += "leaf";
    } else if (tag_slot == 0xffffffff) {
        r_out += "root";
    } else {
        r_out += itos((int32_t)tag_slot);
    }
    r_pos += 4;
#undef _R
}

_FORCE_INLINE_ void CodeSignRequirements::_parse_key(uint32_t &r_pos, String &r_out, uint32_t p_rq_size) const {
#define _R(x) BSWAP32(*(uint32_t *)(blob.data() + x))
    ERR_FAIL_COND_MSG(r_pos >= p_rq_size, "CodeSign/Requirements: Out of bounds.");
    uint32_t key_size = _R(r_pos);
    ERR_FAIL_COND_MSG(r_pos + key_size > p_rq_size, "CodeSign/Requirements: Out of bounds.");
    String key;
    key.resize(key_size);
    memcpy((void *)key.data(), blob.data() + r_pos + 4, key_size);
    r_pos += 4 + key_size + PAD(key_size, 4);
    r_out += "[" + key + "]";
#undef _R
}

_FORCE_INLINE_ void CodeSignRequirements::_parse_oid_key(uint32_t &r_pos, String &r_out, uint32_t p_rq_size) const {
#define _R(x) BSWAP32(*(uint32_t *)(blob.data() + x))

    ERR_FAIL_COND_MSG(r_pos >= p_rq_size, "CodeSign/Requirements: Out of bounds.");
    uint32_t key_size = _R(r_pos);
    ERR_FAIL_COND_MSG(r_pos + key_size > p_rq_size, "CodeSign/Requirements: Out of bounds.");
    r_out += "[field.";
    r_out += itos(blob[r_pos + 4] / 40) + ".";
    r_out += itos(blob[r_pos + 4] % 40);
    uint32_t spos = r_pos + 5;
    while (spos < r_pos + 4 + key_size) {
        r_out += ".";
        if (blob[spos] <= 127) {
            r_out += itos(blob[spos]);
            spos += 1;
        } else {
            uint32_t x = (0x7F & blob[spos]) << 7;
            spos += 1;
            while (blob[spos] > 127) {
                x = (x + (0x7F & blob[spos])) << 7;
                spos += 1;
            }
            x = (x + (0x7F & blob[spos]));
            r_out += itos(x);
            spos += 1;
        }
    }
    r_out += "]";
    r_pos += 4 + key_size + PAD(key_size, 4);
#undef _R
}

_FORCE_INLINE_ void CodeSignRequirements::_parse_hash_string(uint32_t &r_pos, String &r_out, uint32_t p_rq_size) const {
#define _R(x) BSWAP32(*(uint32_t *)(blob.data() + x))

    ERR_FAIL_COND_MSG(r_pos >= p_rq_size, "CodeSign/Requirements: Out of bounds.");
    uint32_t tag_size = _R(r_pos);
    ERR_FAIL_COND_MSG(r_pos + tag_size > p_rq_size, "CodeSign/Requirements: Out of bounds.");
    Vector<uint8_t> data;
    data.resize(tag_size);
    memcpy(data.data(), blob.data() + r_pos + 4, tag_size);
    r_out += "H\"" + StringUtils::hex_encode_buffer(data.data(), data.size()) + "\"";
    r_pos += 4 + tag_size + PAD(tag_size, 4);
#undef _R
}

_FORCE_INLINE_ void CodeSignRequirements::_parse_value(uint32_t &r_pos, String &r_out, uint32_t p_rq_size) const {
#define _R(x) BSWAP32(*(uint32_t *)(blob.data() + x))

    ERR_FAIL_COND_MSG(r_pos >= p_rq_size, "CodeSign/Requirements: Out of bounds.");
    uint32_t key_size = _R(r_pos);
    ERR_FAIL_COND_MSG(r_pos + key_size > p_rq_size, "CodeSign/Requirements: Out of bounds.");
    String key;
    key.resize(key_size);
    memcpy((void *)key.data(), blob.data() + r_pos + 4, key_size);
    r_pos += 4 + key_size + PAD(key_size, 4);
    r_out += "\"" + key + "\"";
#undef _R
}

_FORCE_INLINE_ void CodeSignRequirements::_parse_date(uint32_t &r_pos, String &r_out, uint32_t p_rq_size) const {
#define _R(x) BSWAP32(*(uint32_t *)(blob.data() + x))

    ERR_FAIL_COND_MSG(r_pos >= p_rq_size, "CodeSign/Requirements: Out of bounds.");
    uint32_t date = _R(r_pos);
    time_t t = 978307200 + date;
    struct tm lt;
#ifdef WINDOWS_ENABLED
    gmtime_s(&lt, &t);
#else
    gmtime_r(&t, &lt);
#endif
    r_out += FormatVE("<%04d-%02d-%02d ", (int)(1900 + lt.tm_year), (int)(lt.tm_mon + 1), (int)(lt.tm_mday)) + FormatVE("%02d:%02d:%02d +0000>", (int)(lt.tm_hour), (int)(lt.tm_min), (int)(lt.tm_sec));
#undef _R
}

_FORCE_INLINE_ bool CodeSignRequirements::_parse_match(uint32_t &r_pos, String &r_out, uint32_t p_rq_size) const {
#define _R(x) BSWAP32(*(uint32_t *)(blob.data() + x))

    ERR_FAIL_COND_V_MSG(r_pos >= p_rq_size, false, "CodeSign/Requirements: Out of bounds.");
    uint32_t match = _R(r_pos);
    r_pos += 4;
    switch (match) {
        case 0x00000000: {
            r_out += "exists";
        } break;
        case 0x00000001: {
            r_out += "= ";
            _parse_value(r_pos, r_out, p_rq_size);
        } break;
        case 0x00000002: {
            r_out += "~ ";
            _parse_value(r_pos, r_out, p_rq_size);
        } break;
        case 0x00000003: {
            r_out += "= *";
            _parse_value(r_pos, r_out, p_rq_size);
        } break;
        case 0x00000004: {
            r_out += "= ";
            _parse_value(r_pos, r_out, p_rq_size);
            r_out += "*";
        } break;
        case 0x00000005: {
            r_out += "< ";
            _parse_value(r_pos, r_out, p_rq_size);
        } break;
        case 0x00000006: {
            r_out += "> ";
            _parse_value(r_pos, r_out, p_rq_size);
        } break;
        case 0x00000007: {
            r_out += "<= ";
            _parse_value(r_pos, r_out, p_rq_size);
        } break;
        case 0x00000008: {
            r_out += ">= ";
            _parse_value(r_pos, r_out, p_rq_size);
        } break;
        case 0x00000009: {
            r_out += "= ";
            _parse_date(r_pos, r_out, p_rq_size);
        } break;
        case 0x0000000A: {
            r_out += "< ";
            _parse_date(r_pos, r_out, p_rq_size);
        } break;
        case 0x0000000B: {
            r_out += "> ";
            _parse_date(r_pos, r_out, p_rq_size);
        } break;
        case 0x0000000C: {
            r_out += "<= ";
            _parse_date(r_pos, r_out, p_rq_size);
        } break;
        case 0x0000000D: {
            r_out += ">= ";
            _parse_date(r_pos, r_out, p_rq_size);
        } break;
        case 0x0000000E: {
            r_out += "absent";
        } break;
        default: {
            return false;
        }
    }
    return true;
#undef _R
}

Vector<String> CodeSignRequirements::parse_requirements() const {
#define _R(x) BSWAP32(*(uint32_t *)(blob.data() + x))

    Vector<String> list;

    // Read requirements set header.
    ERR_FAIL_COND_V_MSG(blob.size() < 12, list, "CodeSign/Requirements: Blob is too small.");
    uint32_t magic = _R(0);
    ERR_FAIL_COND_V_MSG(magic != 0xfade0c01, list, "CodeSign/Requirements: Invalid set magic.");
    uint32_t size = _R(4);
    ERR_FAIL_COND_V_MSG(size != (uint32_t)blob.size(), list, "CodeSign/Requirements: Invalid set size.");
    uint32_t count = _R(8);

    for (uint32_t i = 0; i < count; i++) {
        String out;

        // Read requirement header.
        uint32_t rq_type = _R(12 + i * 8);
        uint32_t rq_offset = _R(12 + i * 8 + 4);
        ERR_FAIL_COND_V_MSG(rq_offset + 12 >= (uint32_t)blob.size(), list, "CodeSign/Requirements: Invalid requirement offset.");
        switch (rq_type) {
            case 0x00000001: {
                out += "host => ";
            } break;
            case 0x00000002: {
                out += "guest => ";
            } break;
            case 0x00000003: {
                out += "designated => ";
            } break;
            case 0x00000004: {
                out += "library => ";
            } break;
            case 0x00000005: {
                out += "plugin => ";
            } break;
            default: {
                ERR_FAIL_V_MSG(list, "CodeSign/Requirements: Invalid requirement type.");
            }
        }
        uint32_t rq_magic = _R(rq_offset);
        uint32_t rq_size = _R(rq_offset + 4);
        uint32_t rq_ver = _R(rq_offset + 8);
        uint32_t pos = rq_offset + 12;
        ERR_FAIL_COND_V_MSG(rq_magic != 0xfade0c00, list, "CodeSign/Requirements: Invalid requirement magic.");
        ERR_FAIL_COND_V_MSG(rq_ver != 0x00000001, list, "CodeSign/Requirements: Invalid requirement version.");

        // Read requirement tokens.
        Vector<String> tokens;
        while (pos < rq_offset + rq_size) {
            uint32_t rq_tag = _R(pos);
            pos += 4;
            String token;
            switch (rq_tag) {
                case 0x00000000: {
                    token = "false";
                } break;
                case 0x00000001: {
                    token = "true";
                } break;
                case 0x00000002: {
                    token = "identifier ";
                    _parse_value(pos, token, rq_offset + rq_size);
                } break;
                case 0x00000003: {
                    token = "anchor apple";
                } break;
                case 0x00000004: {
                    _parse_certificate_slot(pos, token, rq_offset + rq_size);
                    token += " ";
                    _parse_hash_string(pos, token, rq_offset + rq_size);
                } break;
                case 0x00000005: {
                    token = "info";
                    _parse_key(pos, token, rq_offset + rq_size);
                    token += " = ";
                    _parse_value(pos, token, rq_offset + rq_size);
                } break;
                case 0x00000006: {
                    token = "and";
                } break;
                case 0x00000007: {
                    token = "or";
                } break;
                case 0x00000008: {
                    token = "cdhash ";
                    _parse_hash_string(pos, token, rq_offset + rq_size);
                } break;
                case 0x00000009: {
                    token = "!";
                } break;
                case 0x0000000A: {
                    token = "info";
                    _parse_key(pos, token, rq_offset + rq_size);
                    token += " ";
                    ERR_FAIL_COND_V_MSG(!_parse_match(pos, token, rq_offset + rq_size), list, "CodeSign/Requirements: Unsupported match suffix.");
                } break;
                case 0x0000000B: {
                    _parse_certificate_slot(pos, token, rq_offset + rq_size);
                    _parse_key(pos, token, rq_offset + rq_size);
                    token += " ";
                    ERR_FAIL_COND_V_MSG(!_parse_match(pos, token, rq_offset + rq_size), list, "CodeSign/Requirements: Unsupported match suffix.");
                } break;
                case 0x0000000C: {
                    _parse_certificate_slot(pos, token, rq_offset + rq_size);
                    token += " trusted";
                } break;
                case 0x0000000D: {
                    token = "anchor trusted";
                } break;
                case 0x0000000E: {
                    _parse_certificate_slot(pos, token, rq_offset + rq_size);
                    _parse_oid_key(pos, token, rq_offset + rq_size);
                    token += " ";
                    ERR_FAIL_COND_V_MSG(!_parse_match(pos, token, rq_offset + rq_size), list, "CodeSign/Requirements: Unsupported match suffix.");
                } break;
                case 0x0000000F: {
                    token = "anchor apple generic";
                } break;
                default: {
                    ERR_FAIL_V_MSG(list, "CodeSign/Requirements: Invalid requirement token.");
                } break;
            }
            tokens.push_back(token);
        }

        // Polish to infix notation (w/o bracket optimization).
        if(!tokens.empty()) {
            for (auto idx = tokens.size()-1; idx!=0; --idx) {
                String &tok(tokens[idx]);
                if (tok == "and") {
                    ERR_FAIL_COND_V_MSG((idx+1)>tokens.size() || (idx+2)>tokens.size(), list, "CodeSign/Requirements: Invalid token sequence.");
                    String token = "(" + tokens[idx+1] + " and " + tokens[idx+2] + ")";
                    tokens.erase_at(idx+2);
                    tokens.erase_at(idx+1);
                    tok = token;
                } else if (tok == "or") {
                    ERR_FAIL_COND_V_MSG((idx+1)>tokens.size() || (idx+2)>tokens.size(), list, "CodeSign/Requirements: Invalid token sequence.");
                    String token = "(" + tokens[idx+1] + " or " + tokens[idx+2] + ")";
                    tokens.erase_at(idx+2);
                    tokens.erase_at(idx+1);
                    tok = token;
                }
            }
        }

        if (tokens.size() == 1) {
            list.push_back(out + tokens.front());
        } else {
            ERR_FAIL_V_MSG(list, "CodeSign/Requirements: Invalid token sequence.");
        }
    }

    return list;
#undef _R
}

Vector<uint8_t> CodeSignRequirements::get_hash_sha1() const {
    Vector<uint8_t> hash;
    hash.resize(0x14);

    CryptoCore::SHA1Context ctx;
    ctx.start();
    ctx.update(blob.data(), blob.size());
    ctx.finish(hash.data());

    return hash;
}

Vector<uint8_t> CodeSignRequirements::get_hash_sha256() const {
    Vector<uint8_t> hash;
    hash.resize(0x20);

    CryptoCore::SHA256Context ctx;
    ctx.start();
    ctx.update(blob.data(), blob.size());
    ctx.finish(hash.data());

    return hash;
}

int CodeSignRequirements::get_size() const {
    return blob.size();
}

void CodeSignRequirements::write_to_file(FileAccess *p_file) const {
    ERR_FAIL_COND_MSG(!p_file, "CodeSign/Requirements: Invalid file handle.");
    p_file->store_buffer(blob.data(), blob.size());
}

/*************************************************************************/
/* CodeSignEntitlementsText                                             */
/*************************************************************************/

CodeSignEntitlementsText::CodeSignEntitlementsText() {
#define _W(a, b, c, d) \
    blob.push_back(a); \
    blob.push_back(b); \
    blob.push_back(c); \
    blob.push_back(d);
    _W(0xFA, 0xDE, 0x71, 0x71); // Text Entitlements set magic.
    _W(0x00, 0x00, 0x00, 0x08); // Length (8 bytes).
#undef _W
}

CodeSignEntitlementsText::CodeSignEntitlementsText(const String &p_string) {
    const auto & utf8 = p_string;
#define _W(a, b, c, d) \
    blob.push_back(a); \
    blob.push_back(b); \
    blob.push_back(c); \
    blob.push_back(d);
    _W(0xFA, 0xDE, 0x71, 0x71); // Text Entitlements set magic.
    for (int i = 3; i >= 0; i--) {
        uint8_t x = ((utf8.length() + 8) >> i * 8) & 0xFF; // Size.
        blob.push_back(x);
    }
    for (int i = 0; i < utf8.length(); i++) { // Write data.
        blob.push_back(utf8[i]);
    }
#undef _W
}

Vector<uint8_t> CodeSignEntitlementsText::get_hash_sha1() const {
    Vector<uint8_t> hash;
    hash.resize(0x14);

    CryptoCore::SHA1Context ctx;
    ctx.start();
    ctx.update(blob.data(), blob.size());
    ctx.finish(hash.data());

    return hash;
}

Vector<uint8_t> CodeSignEntitlementsText::get_hash_sha256() const {
    Vector<uint8_t> hash;
    hash.resize(0x20);

    CryptoCore::SHA256Context ctx;
    ctx.start();
    ctx.update(blob.data(), blob.size());
    ctx.finish(hash.data());

    return hash;
}

int CodeSignEntitlementsText::get_size() const {
    return blob.size();
}

void CodeSignEntitlementsText::write_to_file(FileAccess *p_file) const {
    ERR_FAIL_COND_MSG(!p_file, "CodeSign/EntitlementsText: Invalid file handle.");
    p_file->store_buffer(blob.data(), blob.size());
}

/*************************************************************************/
/* CodeSignEntitlementsBinary                                           */
/*************************************************************************/

CodeSignEntitlementsBinary::CodeSignEntitlementsBinary() {
#define _W(a, b, c, d) \
    blob.push_back(a); \
    blob.push_back(b); \
    blob.push_back(c); \
    blob.push_back(d);
    _W(0xFA, 0xDE, 0x71, 0x72); // Binary Entitlements magic.
    _W(0x00, 0x00, 0x00, 0x08); // Length (8 bytes).
#undef _W
}

CodeSignEntitlementsBinary::CodeSignEntitlementsBinary(const String &p_string) {
    PList pl(p_string);

    Vector<uint8_t> asn1 = pl.save_asn1();
#define _W(a, b, c, d) \
    blob.push_back(a); \
    blob.push_back(b); \
    blob.push_back(c); \
    blob.push_back(d);
    _W(0xFA, 0xDE, 0x71, 0x72); // Binary Entitlements magic.
    uint32_t size = asn1.size() + 8;
    for (int i = 3; i >= 0; i--) {
        uint8_t x = (size >> i * 8) & 0xFF; // Size.
        blob.push_back(x);
    }
    blob.insert(blob.end(),asn1.begin(),asn1.end()); // Write data.
#undef _W
}

Vector<uint8_t> CodeSignEntitlementsBinary::get_hash_sha1() const {
    Vector<uint8_t> hash;
    hash.resize(0x14);

    CryptoCore::SHA1Context ctx;
    ctx.start();
    ctx.update(blob.data(), blob.size());
    ctx.finish(hash.data());

    return hash;
}

Vector<uint8_t> CodeSignEntitlementsBinary::get_hash_sha256() const {
    Vector<uint8_t> hash;
    hash.resize(0x20);

    CryptoCore::SHA256Context ctx;
    ctx.start();
    ctx.update(blob.data(), blob.size());
    ctx.finish(hash.data());

    return hash;
}

int CodeSignEntitlementsBinary::get_size() const {
    return blob.size();
}

void CodeSignEntitlementsBinary::write_to_file(FileAccess *p_file) const {
    ERR_FAIL_COND_MSG(!p_file, "CodeSign/EntitlementsBinary: Invalid file handle.");
    p_file->store_buffer(blob.data(), blob.size());
}

/*************************************************************************/
/* CodeSignCodeDirectory                                                 */
/*************************************************************************/

CodeSignCodeDirectory::CodeSignCodeDirectory() {
#define _W(a, b, c, d) \
    blob.push_back(a); \
    blob.push_back(b); \
    blob.push_back(c); \
    blob.push_back(d);
    _W(0xFA, 0xDE, 0x0C, 0x02); // Code Directory magic.
    _W(0x00, 0x00, 0x00, 0x00); // Size (8 bytes).
#undef _W
}

CodeSignCodeDirectory::CodeSignCodeDirectory(uint8_t p_hash_size, uint8_t p_hash_type, bool p_main, const String &p_id, const String &p_team_id, uint32_t p_page_size, uint64_t p_exe_limit, uint64_t p_code_limit) {
    pages = p_code_limit / (uint64_t(1) << p_page_size);
    remain = p_code_limit % (uint64_t(1) << p_page_size);
    code_slots = pages + (remain > 0 ? 1 : 0);
    special_slots = 7;

    int cd_size = 8 + sizeof(CodeDirectoryHeader) + (code_slots + special_slots) * p_hash_size + p_id.size() + p_team_id.size();
    int cd_off = 8 + sizeof(CodeDirectoryHeader);
#define _W(a, b, c, d) \
    blob.push_back(a); \
    blob.push_back(b); \
    blob.push_back(c); \
    blob.push_back(d);
    _W(0xFA, 0xDE, 0x0C, 0x02); // Code Directory magic.
    for (int i = 3; i >= 0; i--) {
        uint8_t x = (cd_size >> i * 8) & 0xFF; // Size.
        blob.push_back(x);
    }
#undef _W
    blob.resize(cd_size);
    memset(blob.data() + 8, 0x00, cd_size - 8);
    CodeDirectoryHeader *cd = (CodeDirectoryHeader *)(blob.data() + 8);

    bool is_64_cl = (p_code_limit >= std::numeric_limits<uint32_t>::max());

    // Version and options.
    cd->version = BSWAP32(0x20500);
    cd->flags = BSWAP32(SIGNATURE_ADHOC | SIGNATURE_RUNTIME);
    cd->special_slots = BSWAP32(special_slots);
    cd->code_slots = BSWAP32(code_slots);
    if (is_64_cl) {
        cd->code_limit_64 = BSWAP64(p_code_limit);
    } else {
        cd->code_limit = BSWAP32(p_code_limit);
    }
    cd->hash_size = p_hash_size;
    cd->hash_type = p_hash_type;
    cd->page_size = p_page_size;
    cd->exec_seg_base = 0x00;
    cd->exec_seg_limit = BSWAP64(p_exe_limit);
    cd->exec_seg_flags = 0;
    if (p_main) {
        cd->exec_seg_flags |= EXECSEG_MAIN_BINARY;
    }
    cd->exec_seg_flags = BSWAP64(cd->exec_seg_flags);
    uint32_t version = (11 << 16) + (3 << 8) + 0; // Version 11.3.0
    cd->runtime = BSWAP32(version);

    // Copy ID.
    cd->ident_offset = BSWAP32(cd_off);
    memcpy(blob.data() + cd_off, p_id.data(), p_id.size());
    cd_off += p_id.size();

    // Copy Team ID.
    if (p_team_id.length() > 0) {
        cd->team_offset = BSWAP32(cd_off);
        memcpy(blob.data() + cd_off, p_team_id.data(), p_team_id.size());
        cd_off += p_team_id.size();
    } else {
        cd->team_offset = 0;
    }

    // Scatter vector.
    cd->scatter_vector_offset = 0; // Not used.

    // Executable hashes offset.
    cd->hash_offset = BSWAP32(cd_off + special_slots * cd->hash_size);
}

bool CodeSignCodeDirectory::set_hash_in_slot(const Vector<uint8_t> &p_hash, int p_slot) {
    ERR_FAIL_COND_V_MSG((p_slot < -special_slots) || (p_slot >= code_slots), false, FormatVE("CodeSign/CodeDirectory: Invalid hash slot index: %d.", p_slot));
    CodeDirectoryHeader *cd = reinterpret_cast<CodeDirectoryHeader *>(blob.data() + 8);
    for (int i = 0; i < cd->hash_size; i++) {
        blob[BSWAP32(cd->hash_offset) + p_slot * cd->hash_size + i] = p_hash[i];
    }
    return true;
}

int32_t CodeSignCodeDirectory::get_page_count() {
    return pages;
}

int32_t CodeSignCodeDirectory::get_page_remainder() {
    return remain;
}

Vector<uint8_t> CodeSignCodeDirectory::get_hash_sha1() const {
    Vector<uint8_t> hash;
    hash.resize(0x14);

    CryptoCore::SHA1Context ctx;
    ctx.start();
    ctx.update(blob.data(), blob.size());
    ctx.finish(hash.data());

    return hash;
}

Vector<uint8_t> CodeSignCodeDirectory::get_hash_sha256() const {
    Vector<uint8_t> hash;
    hash.resize(0x20);

    CryptoCore::SHA256Context ctx;
    ctx.start();
    ctx.update(blob.data(), blob.size());
    ctx.finish(hash.data());

    return hash;
}

int CodeSignCodeDirectory::get_size() const {
    return blob.size();
}

void CodeSignCodeDirectory::write_to_file(FileAccess *p_file) const {
    ERR_FAIL_COND_MSG(!p_file, "CodeSign/CodeDirectory: Invalid file handle.");
    p_file->store_buffer(blob.data(), blob.size());
}

/*************************************************************************/
/* CodeSignSignature                                                     */
/*************************************************************************/

CodeSignSignature::CodeSignSignature() {
#define _W(a, b, c, d) \
    blob.push_back(a); \
    blob.push_back(b); \
    blob.push_back(c); \
    blob.push_back(d);
    _W(0xFA, 0xDE, 0x0B, 0x01); // Signature magic.
    uint32_t sign_size = 8; // Ad-hoc signature is empty.
    for (int i = 3; i >= 0; i--) {
        uint8_t x = (sign_size >> i * 8) & 0xFF; // Size.
        blob.push_back(x);
    }
#undef _W
}

Vector<uint8_t> CodeSignSignature::get_hash_sha1() const {
    Vector<uint8_t> hash;
    hash.resize(0x14);

    CryptoCore::SHA1Context ctx;
    ctx.start();
    ctx.update(blob.data(), blob.size());
    ctx.finish(hash.data());

    return hash;
}

Vector<uint8_t> CodeSignSignature::get_hash_sha256() const {
    Vector<uint8_t> hash;
    hash.resize(0x20);

    CryptoCore::SHA256Context ctx;
    ctx.start();
    ctx.update(blob.data(), blob.size());
    ctx.finish(hash.data());

    return hash;
}

int CodeSignSignature::get_size() const {
    return blob.size();
}

void CodeSignSignature::write_to_file(FileAccess *p_file) const {
    ERR_FAIL_COND_MSG(!p_file, "CodeSign/Signature: Invalid file handle.");
    p_file->store_buffer(blob.data(), blob.size());
}

/*************************************************************************/
/* CodeSignSuperBlob                                                     */
/*************************************************************************/

bool CodeSignSuperBlob::add_blob(const Ref<CodeSignBlob> &p_blob) {
    if (p_blob) {
        blobs.push_back(p_blob);
        return true;
    } else {
        return false;
    }
}

int CodeSignSuperBlob::get_size() const {
    int size = 12 + blobs.size() * 8;
    for (int i = 0; i < blobs.size(); i++) {
        if (!blobs[i]) {
            return 0;
        }
        size += blobs[i]->get_size();
    }
    return size;
}

void CodeSignSuperBlob::write_to_file(FileAccess *p_file) const {
    ERR_FAIL_COND_MSG(!p_file, "CodeSign/SuperBlob: Invalid file handle.");
    uint32_t size = get_size();
    uint32_t data_offset = 12 + blobs.size() * 8;

    // Write header.
    p_file->store_32(BSWAP32(0xfade0cc0));
    p_file->store_32(BSWAP32(size));
    p_file->store_32(BSWAP32(blobs.size()));

    // Write index.
    for (int i = 0; i < blobs.size(); i++) {
        if (!blobs[i]) {
            return;
        }
        p_file->store_32(BSWAP32(blobs[i]->get_index_type()));
        p_file->store_32(BSWAP32(data_offset));
        data_offset += blobs[i]->get_size();
    }

    // Write blobs.
    for (int i = 0; i < blobs.size(); i++) {
        blobs[i]->write_to_file(p_file);
    }
}

/*************************************************************************/
/* CodeSign                                                              */
/*************************************************************************/

Vector<uint8_t> CodeSign::file_hash_sha1(StringView p_path) {
    Vector<uint8_t> file_hash;
    FileAccessRef f = FileAccess::open(p_path, FileAccess::READ);
    ERR_FAIL_COND_V_MSG(!f, Vector<uint8_t>(), FormatVE("CodeSign: Can't open file: \"%.*s\".", (int)p_path.size(), p_path.data()));

    CryptoCore::SHA1Context ctx;
    ctx.start();

    unsigned char step[4096];
    while (true) {
        uint64_t br = f->get_buffer(step, 4096);
        if (br > 0) {
            ctx.update(step, br);
        }
        if (br < 4096) {
            break;
        }
    }

    file_hash.resize(0x14);
    ctx.finish(file_hash.data());
    return file_hash;
}

Vector<uint8_t> CodeSign::file_hash_sha256(StringView p_path) {
    Vector<uint8_t> file_hash;
    FileAccessRef f = FileAccess::open(p_path, FileAccess::READ);
    ERR_FAIL_COND_V_MSG(!f, Vector<uint8_t>(), FormatVE("CodeSign: Can't open file: \"%.*s\".", (int)p_path.size(), p_path.data()));

    CryptoCore::SHA256Context ctx;
    ctx.start();

    unsigned char step[4096];
    while (true) {
        uint64_t br = f->get_buffer(step, 4096);
        if (br > 0) {
            ctx.update(step, br);
        }
        if (br < 4096) {
            break;
        }
    }

    file_hash.resize(0x20);
    ctx.finish(file_hash.data());
    return file_hash;
}

Error CodeSign::_codesign_file(bool p_use_hardened_runtime, bool p_force, StringView p_info, StringView p_exe_path,  StringView p_bundle_path, StringView p_ent_path, bool p_ios_bundle, String &r_error_msg) {
#define CLEANUP()                                        \
    if (files_to_sign.size() > 1) {                      \
        for (int j = 0; j < files_to_sign.size(); j++) { \
            da->remove(files_to_sign[j]);                \
        }                                                \
    }
    Vector<uint8_t> info_hash1, info_hash2;
    Vector<uint8_t> res_hash1, res_hash2;
    String id;
    String main_exe(p_exe_path);

    print_verbose(FormatVE("CodeSign: Signing executable: %s, bundle: %.*s with entitlements %.*s", main_exe.c_str(), (int)p_bundle_path.size(),
            p_bundle_path.data(), (int)p_ent_path.size(), p_ent_path.data()));

    DirAccess *da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
    if (!da) {
        r_error_msg = TTR("Can't get filesystem access.");
        ERR_FAIL_V_MSG(ERR_CANT_CREATE, "CodeSign: Can't get filesystem access.");
    }

    // Read Info.plist.
    if (!p_info.empty()) {
        print_verbose("CodeSign: Reading bundle info...");
        PList info_plist;
        if (info_plist.load_file(p_info)) {
            info_hash1 = file_hash_sha1(p_info);
            info_hash2 = file_hash_sha256(p_info);
            if (info_hash1.empty() || info_hash2.empty()) {
                r_error_msg = TTR("Failed to get Info.plist hash.");
                ERR_FAIL_V_MSG(FAILED, "CodeSign: Failed to get Info.plist hash.");
            }

            if (info_plist.get_root()->data_type == PList::PLNodeType::PL_NODE_TYPE_DICT && info_plist.get_root()->data_dict.contains("CFBundleExecutable")) {
                main_exe = PathUtils::plus_file(p_exe_path,info_plist.get_root()->data_dict["CFBundleExecutable"]->data_string);
            } else {
                r_error_msg = TTR("Invalid Info.plist, no exe name.");
                ERR_FAIL_V_MSG(FAILED, "CodeSign: Invalid Info.plist, no exe name.");
            }

            if (info_plist.get_root()->data_type == PList::PLNodeType::PL_NODE_TYPE_DICT && info_plist.get_root()->data_dict.contains("CFBundleIdentifier")) {
                id = info_plist.get_root()->data_dict["CFBundleIdentifier"]->data_string;
            } else {
                r_error_msg = TTR("Invalid Info.plist, no bundle id.");
                ERR_FAIL_V_MSG(FAILED, "CodeSign: Invalid Info.plist, no bundle id.");
            }
        } else {
            r_error_msg = TTR("Invalid Info.plist, can't load.");
            ERR_FAIL_V_MSG(FAILED, "CodeSign: Invalid Info.plist, can't load.");
        }
    }

    // Extract fat binary.
    Vector<String> files_to_sign;
    if (LipO::is_lipo(main_exe)) {
        print_verbose("CodeSign: Executable is fat, extracting...");
        String tmp_path_name = PathUtils::plus_file(EditorSettings::get_singleton()->get_cache_dir(),"_lipo");
        Error err = da->make_dir_recursive(tmp_path_name);
        if (err != OK) {
            r_error_msg = FormatVE(TTR("Failed to create \"%s\" subfolder.").asCString(), tmp_path_name.c_str());
            ERR_FAIL_V_MSG(FAILED, FormatVE("CodeSign: Failed to create \"%s\" subfolder.", tmp_path_name.c_str()));
        }
        LipO lip;
        if (lip.open_file(main_exe)) {
            for (int i = 0; i < lip.get_arch_count(); i++) {
                if (!lip.extract_arch(i, PathUtils::plus_file(tmp_path_name,"_exe_" + itos(i)))) {
                    CLEANUP();
                    r_error_msg = TTR("Failed to extract thin binary.");
                    ERR_FAIL_V_MSG(FAILED, "CodeSign: Failed to extract thin binary.");
                }
                files_to_sign.push_back(PathUtils::plus_file(tmp_path_name,"_exe_" + itos(i)));
            }
        }
    } else if (MachO::is_macho(main_exe)) {
        print_verbose("CodeSign: Executable is thin...");
        files_to_sign.push_back(main_exe);
    } else {
        r_error_msg = TTR("Invalid binary format.");
        ERR_FAIL_V_MSG(FAILED, "CodeSign: Invalid binary format.");
    }

    // Check if it's already signed.
    if (!p_force) {
        for (int i = 0; i < files_to_sign.size(); i++) {
            MachO mh;
            mh.open_file(files_to_sign[i]);
            if (mh.is_signed()) {
                CLEANUP();
                r_error_msg = TTR("Already signed!");
                ERR_FAIL_V_MSG(FAILED, "CodeSign: Already signed!");
            }
        }
    }

    // Generate core resources.
    if (!p_bundle_path.empty()) {
        print_verbose("CodeSign: Generating bundle CodeResources...");
        CodeSignCodeResources cr;

        if (p_ios_bundle) {
            cr.add_rule1("^.*");
            cr.add_rule1("^.*\\.lproj/", "optional", 100);
            cr.add_rule1("^.*\\.lproj/locversion.plist$", "omit", 1100);
            cr.add_rule1("^Base\\.lproj/", "", 1010);
            cr.add_rule1("^version.plist$");

            cr.add_rule2(".*\\.dSYM($|/)", "", 11);
            cr.add_rule2("^(.*/)?\\.DS_Store$", "omit", 2000);
            cr.add_rule2("^.*");
            cr.add_rule2("^.*\\.lproj/", "optional", 1000);
            cr.add_rule2("^.*\\.lproj/locversion.plist$", "omit", 1100);
            cr.add_rule2("^Base\\.lproj/", "", 1010);
            cr.add_rule2("^Info\\.plist$", "omit", 20);
            cr.add_rule2("^PkgInfo$", "omit", 20);
            cr.add_rule2("^embedded\\.provisionprofile$", "", 10);
            cr.add_rule2("^version\\.plist$", "", 20);

            cr.add_rule2("^_MASReceipt", "omit", 2000, false);
            cr.add_rule2("^_CodeSignature", "omit", 2000, false);
            cr.add_rule2("^CodeResources", "omit", 2000, false);
        } else {
            cr.add_rule1("^Resources/");
            cr.add_rule1("^Resources/.*\\.lproj/", "optional", 1000);
            cr.add_rule1("^Resources/.*\\.lproj/locversion.plist$", "omit", 1100);
            cr.add_rule1("^Resources/Base\\.lproj/", "", 1010);
            cr.add_rule1("^version.plist$");

            cr.add_rule2(".*\\.dSYM($|/)", "", 11);
            cr.add_rule2("^(.*/)?\\.DS_Store$", "omit", 2000);
            cr.add_rule2("^(Frameworks|SharedFrameworks|PlugIns|Plug-ins|XPCServices|Helpers|MacOS|Library/(Automator|Spotlight|LoginItems))/", "nested", 10);
            cr.add_rule2("^.*");
            cr.add_rule2("^Info\\.plist$", "omit", 20);
            cr.add_rule2("^PkgInfo$", "omit", 20);
            cr.add_rule2("^Resources/", "", 20);
            cr.add_rule2("^Resources/.*\\.lproj/", "optional", 1000);
            cr.add_rule2("^Resources/.*\\.lproj/locversion.plist$", "omit", 1100);
            cr.add_rule2("^Resources/Base\\.lproj/", "", 1010);
            cr.add_rule2("^[^/]+$", "nested", 10);
            cr.add_rule2("^embedded\\.provisionprofile$", "", 10);
            cr.add_rule2("^version\\.plist$", "", 20);
            cr.add_rule2("^_MASReceipt", "omit", 2000, false);
            cr.add_rule2("^_CodeSignature", "omit", 2000, false);
            cr.add_rule2("^CodeResources", "omit", 2000, false);
        }

        if (!cr.add_folder_recursive(p_bundle_path, "", main_exe)) {
            CLEANUP();
            r_error_msg = TTR("Failed to process nested resources.");
            ERR_FAIL_V_MSG(FAILED, "CodeSign: Failed to process nested resources.");
        }
        Error err = da->make_dir_recursive(PathUtils::plus_file(p_bundle_path,"_CodeSignature"));
        if (err != OK) {
            CLEANUP();
            r_error_msg = TTR("Failed to create _CodeSignature subfolder.");
            ERR_FAIL_V_MSG(FAILED, "CodeSign: Failed to create _CodeSignature subfolder.");
        }
        String tgtfile(PathUtils::plus_file(PathUtils::plus_file(p_bundle_path,"_CodeSignature"),"CodeResources"));
        cr.save_to_file(tgtfile);
        res_hash1 = file_hash_sha1(tgtfile);
        res_hash2 = file_hash_sha256(tgtfile);
        if (res_hash1.empty() || res_hash2.empty()) {
            CLEANUP();
            r_error_msg = TTR("Failed to get CodeResources hash.");
            ERR_FAIL_V_MSG(FAILED, "CodeSign: Failed to get CodeResources hash.");
        }
    }

    // Generate common signature structures.
    if (id.empty()) {
        Ref<Crypto> crypto = Ref<Crypto>(Crypto::create());
        PoolByteArray uuid = crypto->generate_random_bytes(16);
        id = (String("a-55554944") /*a-UUID*/ + StringUtils::hex_encode_buffer(uuid.read().ptr(), 16));
    }
    String uuid_str = id;
    print_verbose(FormatVE("CodeSign: Used bundle ID: %s", id.c_str()));

    print_verbose("CodeSign: Processing entitlements...");

    Ref<CodeSignEntitlementsText> cet;
    Ref<CodeSignEntitlementsBinary> ceb;
    if (!p_ent_path.empty()) {
        String entitlements = FileAccess::get_file_as_string(p_ent_path);
        if (entitlements.empty()) {
            CLEANUP();
            r_error_msg = TTR("Invalid entitlements file.");
            ERR_FAIL_V_MSG(FAILED, "CodeSign: Invalid entitlements file.");
        }
        cet = Ref<CodeSignEntitlementsText>(memnew(CodeSignEntitlementsText(entitlements)));
        ceb = Ref<CodeSignEntitlementsBinary>(memnew(CodeSignEntitlementsBinary(entitlements)));
    }

    print_verbose("CodeSign: Generating requirements...");
    Ref<CodeSignRequirements> rq;
    String team_id = "";
    rq = Ref<CodeSignRequirements>(memnew(CodeSignRequirements()));

    // Sign executables.
    for (int i = 0; i < files_to_sign.size(); i++) {
        MachO mh;
        if (!mh.open_file(files_to_sign[i])) {
            CLEANUP();
            r_error_msg = TTR("Invalid executable file.");
            ERR_FAIL_V_MSG(FAILED, "CodeSign: Invalid executable file.");
        }
        print_verbose(FormatVE("CodeSign: Signing executable for cputype: %d ...", mh.get_cputype()));

        print_verbose(("CodeSign: Generating CodeDirectory..."));
        Ref<CodeSignCodeDirectory> cd1 = make_ref_counted<CodeSignCodeDirectory>(0x14, 0x01, true, uuid_str, team_id, 12, mh.get_exe_limit(), mh.get_code_limit());
        Ref<CodeSignCodeDirectory> cd2 = make_ref_counted<CodeSignCodeDirectory>(0x20, 0x02, true, uuid_str, team_id, 12, mh.get_exe_limit(), mh.get_code_limit());
        print_verbose(("CodeSign: Calculating special slot hashes..."));
        if (info_hash2.size() == 0x20) {
            cd2->set_hash_in_slot(info_hash2, CodeSignCodeDirectory::SLOT_INFO_PLIST);
        }
        if (info_hash1.size() == 0x14) {
            cd1->set_hash_in_slot(info_hash1, CodeSignCodeDirectory::SLOT_INFO_PLIST);
        }
        cd1->set_hash_in_slot(rq->get_hash_sha1(), CodeSignCodeDirectory::Slot::SLOT_REQUIREMENTS);
        cd2->set_hash_in_slot(rq->get_hash_sha256(), CodeSignCodeDirectory::Slot::SLOT_REQUIREMENTS);
        if (res_hash2.size() == 0x20) {
            cd2->set_hash_in_slot(res_hash2, CodeSignCodeDirectory::SLOT_RESOURCES);
        }
        if (res_hash1.size() == 0x14) {
            cd1->set_hash_in_slot(res_hash1, CodeSignCodeDirectory::SLOT_RESOURCES);
        }
        if (cet) {
            cd1->set_hash_in_slot(cet->get_hash_sha1(), CodeSignCodeDirectory::Slot::SLOT_ENTITLEMENTS); //Text variant.
            cd2->set_hash_in_slot(cet->get_hash_sha256(), CodeSignCodeDirectory::Slot::SLOT_ENTITLEMENTS);
        }
        if (ceb) {
            cd1->set_hash_in_slot(ceb->get_hash_sha1(), CodeSignCodeDirectory::Slot::SLOT_DER_ENTITLEMENTS); //ASN.1 variant.
            cd2->set_hash_in_slot(ceb->get_hash_sha256(), CodeSignCodeDirectory::Slot::SLOT_DER_ENTITLEMENTS);
        }

        // Calculate signature size.
        int sign_size = 12; // SuperBlob header.
        sign_size += cd1->get_size() + 8;
        sign_size += cd2->get_size() + 8;
        sign_size += rq->get_size() + 8;
        if (cet) {
            sign_size += cet->get_size() + 8;
        }
        if (ceb) {
            sign_size += ceb->get_size() + 8;
        }
        sign_size += 16; // Empty signature size.

        // Alloc/resize signature load command.
        print_verbose(FormatVE("CodeSign: Reallocating space for the signature superblob (%d)...", sign_size));
        if (!mh.set_signature_size(sign_size)) {
            CLEANUP();
            r_error_msg = TTR("Can't resize signature load command.");
            ERR_FAIL_V_MSG(FAILED, "CodeSign: Can't resize signature load command.");
        }

        print_verbose(("CodeSign: Calculating executable code hashes..."));
        // Calculate executable code hashes.
        Vector<uint8_t> buffer;
        Vector<uint8_t> hash1, hash2;
        hash1.resize(0x14);
        hash2.resize(0x20);
        buffer.resize(1 << 12);
        mh.get_file()->seek(0);
        for (int32_t j = 0; j < cd2->get_page_count(); j++) {
            mh.get_file()->get_buffer(buffer.data(), (1 << 12));
            CryptoCore::SHA256Context ctx2;
            ctx2.start();
            ctx2.update(buffer.data(), (1 << 12));
            ctx2.finish(hash2.data());
            cd2->set_hash_in_slot(hash2, j);

            CryptoCore::SHA1Context ctx1;
            ctx1.start();
            ctx1.update(buffer.data(), (1 << 12));
            ctx1.finish(hash1.data());
            cd1->set_hash_in_slot(hash1, j);
        }
        if (cd2->get_page_remainder() > 0) {
            mh.get_file()->get_buffer(buffer.data(), cd2->get_page_remainder());
            CryptoCore::SHA256Context ctx2;
            ctx2.start();
            ctx2.update(buffer.data(), cd2->get_page_remainder());
            ctx2.finish(hash2.data());
            cd2->set_hash_in_slot(hash2, cd2->get_page_count());

            CryptoCore::SHA1Context ctx1;
            ctx1.start();
            ctx1.update(buffer.data(), cd1->get_page_remainder());
            ctx1.finish(hash1.data());
            cd1->set_hash_in_slot(hash1, cd1->get_page_count());
        }

        print_verbose(("CodeSign: Generating signature..."));
        Ref<CodeSignSignature> cs;
        cs = Ref<CodeSignSignature>(memnew(CodeSignSignature()));

        print_verbose(("CodeSign: Writing signature superblob..."));
        // Write signature data to the executable.
        CodeSignSuperBlob sb = CodeSignSuperBlob();
        sb.add_blob(cd2);
        sb.add_blob(cd1);
        sb.add_blob(rq);
        if (cet) {
            sb.add_blob(cet);
        }
        if (ceb) {
            sb.add_blob(ceb);
        }
        sb.add_blob(cs);
        mh.get_file()->seek(mh.get_signature_offset());
        sb.write_to_file(mh.get_file());
    }
    if (files_to_sign.size() > 1) {
        print_verbose(("CodeSign: Rebuilding fat executable..."));
        LipO lip;
        if (!lip.create_file(main_exe, files_to_sign)) {
            CLEANUP();
            r_error_msg = TTR("Failed to create fat binary.");
            ERR_FAIL_V_MSG(FAILED, "CodeSign: Failed to create fat binary.");
        }
        CLEANUP();
    }
    FileAccess::set_unix_permissions(main_exe, 0755); // Restore unix permissions.
    return OK;
#undef CLEANUP
}

Error CodeSign::codesign(bool p_use_hardened_runtime, bool p_force, StringView p_path, StringView p_ent_path, String &r_error_msg) {
    DirAccess *da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
    if (!da) {
        r_error_msg = TTR("Can't get filesystem access.");
        ERR_FAIL_V_MSG(ERR_CANT_CREATE, "CodeSign: Can't get filesystem access.");
    }

    if (da->dir_exists(p_path)) {
        String fmw_ver = "Current"; // Framework version (default).
        String info_path;
        String main_exe;
        String bundle_path;
        bool bundle = false;
        bool ios_bundle = false;
        String basepath(PathUtils::plus_file(p_path,FormatVE("Versions/%s",fmw_ver.c_str())));
        if (da->file_exists(PathUtils::plus_file(p_path,"Contents/Info.plist"))) {
            info_path = PathUtils::plus_file(p_path,"Contents/Info.plist");
            main_exe = PathUtils::plus_file(p_path,"Contents/MacOS");
            bundle_path = PathUtils::plus_file(p_path,"Contents");
            bundle = true;
        } else if (da->file_exists(basepath+"/Resources/Info.plist")) {
            info_path = basepath+"/Resources/Info.plist";
            main_exe = basepath;
            bundle_path = basepath;
            bundle = true;
        } else if (da->file_exists(PathUtils::plus_file(p_path,"Info.plist"))) {
            info_path = PathUtils::plus_file(p_path,"Info.plist");
            main_exe = p_path;
            bundle_path = p_path;
            bundle = true;
            ios_bundle = true;
        }
        if (bundle) {
            return _codesign_file(p_use_hardened_runtime, p_force, info_path, main_exe, bundle_path, p_ent_path, ios_bundle, r_error_msg);
        } else {
            r_error_msg = TTR("Unknown bundle type.");
            ERR_FAIL_V_MSG(FAILED, "CodeSign: Unknown bundle type.");
        }
    } else if (da->file_exists(p_path)) {
        return _codesign_file(p_use_hardened_runtime, p_force, "", p_path, "", p_ent_path, false, r_error_msg);
    } else {
        r_error_msg = TTR("Unknown object type.");
        ERR_FAIL_V_MSG(FAILED, "CodeSign: Unknown object type.");
    }
}
