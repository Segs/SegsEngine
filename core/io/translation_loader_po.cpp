/*************************************************************************/
/*  translation_loader_po.cpp                                            */
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

#include "translation_loader_po.h"

#include "core/list.h"
#include "core/os/file_access.h"
#include "core/translation.h"

RES TranslationLoaderPO::load_translation(FileAccess *f, Error *r_error, const String &p_path) {

    enum Status {

        STATUS_NONE,
        STATUS_READING_ID,
        STATUS_READING_STRING,
    };

    Status status = STATUS_NONE;

    String msg_id;
    String msg_str;
    String config;

    if (r_error)
        *r_error = ERR_FILE_CORRUPT;

    Ref<Translation> translation(make_ref_counted<Translation>());
    int line = 1;
    bool skip_this = false;
    bool skip_next = false;
    bool is_eof = false;

    while (!is_eof) {

        String l = StringUtils::strip_edges(f->get_line());
        is_eof = f->eof_reached();

        // If we reached last line and it's not a content line, break, otherwise let processing that last loop
        if (is_eof && l.empty()) {

            if (status == STATUS_READING_ID) {
                memdelete(f);
                ERR_FAIL_V_MSG(RES(), p_path + ":" + itos(line) + " Unexpected EOF while reading 'msgid' at file: ")
            } else {
                break;
            }
        }

        if (StringUtils::begins_with(l,"msgid")) {

            if (status == STATUS_READING_ID) {

                memdelete(f);
                ERR_FAIL_V_MSG(RES(), p_path + ":" + itos(line) + " Unexpected 'msgid', was expecting 'msgstr' while parsing: ")
            }

            if (!msg_id.empty()) {
                if (!skip_this)
                    translation->add_message(StringName(msg_id), StringName(msg_str));
            } else if (config.empty())
                config = msg_str;

            l = StringUtils::strip_edges(StringUtils::substr(l,5, l.length()));
            status = STATUS_READING_ID;
            msg_id = "";
            msg_str = "";
            skip_this = skip_next;
            skip_next = false;
        }

        if (StringUtils::begins_with(l,"msgstr")) {

            if (status != STATUS_READING_ID) {

                memdelete(f);
                ERR_FAIL_V_MSG(RES(), p_path + ":" + itos(line) + " Unexpected 'msgstr', was expecting 'msgid' while parsing: ")
            }

            l = StringUtils::strip_edges(StringUtils::substr(l,6, l.length()));
            status = STATUS_READING_STRING;
        }

        if (l.empty() || StringUtils::begins_with(l,"#")) {
            if (StringUtils::contains(l,"fuzzy")) {
                skip_next = true;
            }
            line++;
            continue; //nothing to read or comment
        }

        ERR_FAIL_COND_V_MSG(!StringUtils::begins_with(l,"\"") || status == STATUS_NONE, RES(), p_path + ":" + itos(line) + " Invalid line '" + l + "' while parsing: ")

        l = StringUtils::substr(l,1, l.length());
        //find final quote
        int end_pos = -1;
        for (int i = 0; i < l.length(); i++) {

            if (l[i] == '"' && (i == 0 || l[i - 1] != '\\')) {
                end_pos = i;
                break;
            }
        }

        ERR_FAIL_COND_V_MSG(end_pos == -1, RES(), p_path + ":" + itos(line) + " Expected '\"' at end of message while parsing file: ")

        l = StringUtils::substr(l,0, end_pos);
        l = StringUtils::c_unescape(l);

        if (status == STATUS_READING_ID)
            msg_id += l;
        else
            msg_str += l;

        line++;
    }

    f->close();
    memdelete(f);

    if (status == STATUS_READING_STRING) {

        if (!msg_id.empty()) {
            if (!skip_this)
                translation->add_message(StringName(msg_id), StringName(msg_str));
        } else if (config.empty())
            config = msg_str;
    }

    ERR_FAIL_COND_V_MSG(config.empty(), RES(), "No config found in file: " + p_path + ".")

    Vector<String> configs = StringUtils::split(config,"\n");
    for (int i = 0; i < configs.size(); i++) {

        String c =StringUtils::strip_edges( configs[i]);
        int p = StringUtils::find(c,":");
        if (p == -1)
            continue;
        String prop = StringUtils::strip_edges(StringUtils::substr(c,0, p));
        String value = StringUtils::strip_edges(StringUtils::substr(c,p + 1, c.length()));

        if (prop == "X-Language" || prop == "Language") {
            translation->set_locale(value);
        }
    }

    if (r_error)
        *r_error = OK;

    return translation;
}

RES TranslationLoaderPO::load(const String &p_path, const String &p_original_path, Error *r_error) {

    if (r_error)
        *r_error = ERR_CANT_OPEN;

    FileAccess *f = FileAccess::open(p_path, FileAccess::READ);
    ERR_FAIL_COND_V_MSG(!f, RES(), "Cannot open file '" + p_path + "'.")

    return load_translation(f, r_error);
}

void TranslationLoaderPO::get_recognized_extensions(ListPOD<String> *p_extensions) const {

    p_extensions->push_back("po");
    //p_extensions->push_back("mo"); //mo in the future...
}
bool TranslationLoaderPO::handles_type(const String &p_type) const {

    return (p_type == "Translation");
}

String TranslationLoaderPO::get_resource_type(const String &p_path) const {

    if (StringUtils::to_lower(PathUtils::get_extension(p_path)) == "po")
        return "Translation";
    return "";
}


