/*************************************************************************/
/*  translation_loader_po.cpp                                            */
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

#include "translation_loader_po.h"

#include "core/list.h"
#include "core/os/file_access.h"
#include "core/translation.h"
#include "core/string_utils.inl"
#include "core/string_formatter.h"

using namespace eastl;

RES TranslationLoaderPO::load_translation(FileAccess *f, bool p_use_context, Error *r_error) {
    if (r_error) {
        *r_error = ERR_FILE_CORRUPT;
    }

    const String path = f->get_path();
    Ref<Translation> translation;
    if (p_use_context) {
        translation = Ref<Translation>(memnew(ContextTranslation));
    } else {
        translation = make_ref_counted<Translation>();
    }
    String config;

    uint32_t magic = f->get_32();
    if (magic == 0x950412de) {
        // Load binary MO file.

        uint16_t version_maj = f->get_16();
        uint16_t version_min = f->get_16();
        if (version_maj > 1) {
            ERR_FAIL_V_MSG(RES(), FormatVE("Unsupported MO file %s, version %d.%d.", path.c_str(), version_maj, version_min));
        }

        uint32_t num_strings = f->get_32();
        uint32_t id_table_offset = f->get_32();
        uint32_t trans_table_offset = f->get_32();

        // Read string tables.
        for (uint32_t i = 0; i < num_strings; i++) {
            String msg_id;
            String msg_context;

            // Read id strings and context.
            {
                FixedVector<uint8_t,256,true> data;
                f->seek(id_table_offset + i * 8);
                uint32_t str_start = 0;
                uint32_t str_len = f->get_32();
                uint32_t str_offset = f->get_32();

                data.resize(str_len + 1);
                f->seek(str_offset);
                f->get_buffer(data.data(), str_len);
                data[str_len] = 0;

                for (uint32_t j = 0; j < str_len + 1; j++) {
                    if (data[j] == 0x04) {
                        msg_context = String((const char *)data.data(), j);
                        str_start = j + 1;
                    }
                    else if (data[j] == 0x00) {
                        msg_id = String((const char *)(data.data() + str_start), j - str_start);
                        break;
                    }
                }
            }

            // Read translated strings.
            {
                Vector<uint8_t> data;
                f->seek(trans_table_offset + i * 8);
                uint32_t str_len = f->get_32();
                uint32_t str_offset = f->get_32();

                data.resize(str_len + 1);
                f->seek(str_offset);
                f->get_buffer(data.data(), str_len);
                data[str_len] = 0;

                if (msg_id.empty()) {
                    config = String((const char *)data.data(), str_len);
                } else {
                    for (uint32_t j = 0; j < str_len + 1; j++) {
                        if (data[j] == 0x00) {
                            translation->add_context_message(StringName(msg_id),
                                    StringName(StringView((const char *)data.data(), j)), StringName(msg_context));
                            break;
                        }
                    }
                }
            }
        }

        memdelete(f);
    } else {
        // Try to load as text PO file.
        f->seek(0);
        enum Status {

            STATUS_NONE,
            STATUS_READING_ID,
            STATUS_READING_STRING,
            STATUS_READING_CONTEXT,
        };

        Status status = STATUS_NONE;

        String msg_id;
        String msg_str;
        String msg_context;

        if (r_error)
            *r_error = ERR_FILE_CORRUPT;

        int line = 1;
        bool entered_context = false;
        bool skip_this = false;
        bool skip_next = false;
        bool is_eof = false;

        while (!is_eof) {
            String l(StringUtils::strip_edges(f->get_line()));
            is_eof = f->eof_reached();

            // If we reached last line and it's not a content line, break, otherwise let processing that last loop
            if (is_eof && l.empty()) {
                if (status == STATUS_READING_ID || status == STATUS_READING_CONTEXT) {
                    memdelete(f);
                    ERR_FAIL_V_MSG(RES(), path + ":" + ::to_string(line) + " Unexpected EOF while reading PO file.");
                } else {
                    break;
                }
            }
            if (StringUtils::begins_with(l, "msgctxt")) {
                if (status != STATUS_READING_STRING) {
                    memdelete(f);
                    ERR_FAIL_V_MSG(RES(),
                            "Unexpected 'msgctxt', was expecting 'msgstr' before 'msgctxt' while parsing: " + path +
                                    ":" + itos(line));
                }

                // In PO file, "msgctxt" appears before "msgid". If we encounter a "msgctxt", we add what we have read
                // and set "entered_context" to true to prevent adding twice.
                if (!skip_this && msg_id != "") {
                    translation->add_context_message(StringName(msg_id), StringName(msg_str), StringName(msg_context));
                }
                msg_context = "";
                l = StringUtils::strip_edges(StringView(l).substr(7, l.length()));
                status = STATUS_READING_CONTEXT;
                entered_context = true;
            }
            if (StringUtils::begins_with(l, "msgid")) {
                if (status == STATUS_READING_ID) {
                    memdelete(f);
                    ERR_FAIL_V_MSG(RES(), path + ":" + ::to_string(line) +
                                                  " Unexpected 'msgid', was expecting 'msgstr' while parsing: ");
                }

                if (!msg_id.empty()) {
                    if (!skip_this && !entered_context)
                        translation->add_context_message(
                                StringName(msg_id), StringName(msg_str), StringName(msg_context));
                } else if (msg_context.empty())
                    msg_context = msg_str;

                l = StringUtils::strip_edges(StringUtils::substr(l, 5, l.length()));
                status = STATUS_READING_ID;
                // If we did not encounter msgctxt, we reset context to empty to reset it.
                if (!entered_context) {
                    msg_context = "";
                }
                msg_id = "";
                msg_str = "";
                skip_this = skip_next;
                skip_next = false;
                entered_context = false;
            }

            if (StringUtils::begins_with(l, "msgstr")) {
                if (status != STATUS_READING_ID) {
                    memdelete(f);
                    ERR_FAIL_V_MSG(RES(),
                            path + ":" + ::to_string(line) +
                                    " Unexpected 'msgstr', was expecting 'msgid' before 'msgstr' while parsing: ");
                }

                l = StringUtils::strip_edges(StringUtils::substr(l, 6, l.length()));
                status = STATUS_READING_STRING;
            }

            if (l.empty() || StringUtils::begins_with(l, "#")) {
                if (StringUtils::contains(l, "fuzzy")) {
                    skip_next = true;
                }
                line++;
                continue; // nothing to read or comment
            }

            ERR_FAIL_COND_V_MSG(!StringUtils::begins_with(l, "\"") || status == STATUS_NONE, RES(),
                    String(path) + ":" + ::to_string(line) + " Invalid line '" + l + "' while parsing: ");

            l = StringUtils::substr(l, 1, l.length());
            // Find final quote, ignoring escaped ones (\").
            // The escape_next logic is necessary to properly parse things like \\"
            // where the blackslash is the one being escaped, not the quote.
            // find final quote
            int end_pos = -1;
            bool escape_next = false;
            for (size_t i = 0; i < l.length(); i++) {
                if (l[i] == '\\' && !escape_next) {
                    escape_next = true;
                    continue;
                }

                if (l[i] == '"' && !escape_next) {
                    end_pos = i;
                    break;
                }

                escape_next = false;
            }

            ERR_FAIL_COND_V_MSG(end_pos == -1, RES(),
                    path + ":" + ::to_string(line) + " Expected '\"' at end of message while parsing file: ");

            l = StringUtils::substr(l, 0, end_pos);
            l = StringUtils::c_unescape(l);

            if (status == STATUS_READING_ID) {
                msg_id += l;
            } else if (status == STATUS_READING_STRING) {
                msg_str += l;
            } else if (status == STATUS_READING_CONTEXT) {
                msg_context += l;
            }

            line++;
        }

        memdelete(f);

        if (status == STATUS_READING_STRING) {
            if (!msg_id.empty()) {
                if (!skip_this)
                    translation->add_context_message(StringName(msg_id), StringName(msg_str), StringName(msg_context));
            } else if (msg_context.empty()) {
                msg_context = msg_str;
            }
        }
    }
    ERR_FAIL_COND_V_MSG(config.empty(), RES(), "No config found in file: " + path + ".");

    Vector<StringView> configs = StringUtils::split(config,'\n');
    for (StringView cf : configs) {

        StringView c =StringUtils::strip_edges( cf );
        auto p = StringUtils::find(c,":");
        if (p == String::npos)
            continue;
        StringView prop = StringUtils::strip_edges(StringUtils::substr(c,0, p));
        StringView value = StringUtils::strip_edges(StringUtils::substr(c,p + 1, c.length()));

        if (prop == "X-Language"_sv || prop == "Language"_sv) {
            translation->set_locale(value);
        }
    }

    if (r_error)
        *r_error = OK;

    return translation;

}

RES TranslationLoaderPO::load(StringView p_path, StringView p_original_path, Error *r_error, bool p_no_subresource_cache) {

    if (r_error)
        *r_error = ERR_CANT_OPEN;

    FileAccess *f = FileAccess::open(p_path, FileAccess::READ);
    ERR_FAIL_COND_V_MSG(!f, RES(), "Cannot open file '" + String(p_path) + "'.");

    return load_translation(f, false,r_error);
}

void TranslationLoaderPO::get_recognized_extensions(Vector<String> &p_extensions) const {

    p_extensions.push_back("po");
    p_extensions.push_back("mo");
}
bool TranslationLoaderPO::handles_type(StringView p_type) const {

    return (p_type == StringView("Translation"));
}

String TranslationLoaderPO::get_resource_type(StringView p_path) const {

    String ext = StringUtils::to_lower(PathUtils::get_extension(p_path));
    if (ext == "po" || ext == "mo")
        return "Translation";
    return String();
}


