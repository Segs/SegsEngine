/*************************************************************************/
/*  doc_dump.cpp                                                         */
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

#include "doc_dump.h"

#include "core/os/file_access.h"
#include "core/version.h"
#include "core/string_utils.h"
#include "core/math/transform.h"
#include "scene/main/node.h"
#include "core/method_bind_interface.h"

#include "EASTL/sort.h"

extern void _write_string(FileAccess *f, int p_tablevel, se_string_view p_string);

struct _ConstantSort {

    se_string name;
    int value;
    bool operator<(const _ConstantSort &p_c) const {

        se_string_view left_a = not name.contains("_") ? name : StringUtils::substr(name,0, StringUtils::find(name,"_"));
        se_string_view left_b = StringUtils::find(p_c.name,"_") == se_string::npos ? p_c.name : StringUtils::substr(p_c.name,0, StringUtils::find(p_c.name,"_"));
        if (left_a == left_b)
            return value < p_c.value;
        else
            return left_a < left_b;
    }
};

static se_string _escape_string(se_string_view p_str) {

    se_string ret(p_str);
    ret = StringUtils::replace(ret,"&", "&amp;");
    ret = StringUtils::replace(ret,"<", "&gt;");
    ret = StringUtils::replace(ret,">", "&lt;");
    ret = StringUtils::replace(ret,"'", "&apos;");
    ret = StringUtils::replace(ret,"\"", "&quot;");
    for (char i = 1; i < 32; i++) {

        char chr[2] = { i, 0 };
        ret = StringUtils::replace(ret,chr, "&#" + StringUtils::num(i) + ";");
    }
    return ret;
}
void DocDump::dump(se_string_view p_file) {

    PODVector<StringName> class_list;
    ClassDB::get_class_list(&class_list);
    eastl::sort(class_list.begin(),class_list.end(),WrapAlphaCompare());

    FileAccess *f = FileAccess::open(p_file, FileAccess::WRITE);

    _write_string(f, 0, R"(<?xml version="1.0" encoding="UTF-8" ?>)");
    _write_string(f, 0, se_string("<doc version=\"") + VERSION_NUMBER + "\" name=\"Engine Types\">");

    for(int i=0,fin=class_list.size(); i<fin; ++i) {

        StringName name(class_list[i]);

        se_string header = se_string("<class name=\"") + name + "\"";
        StringName inherits = ClassDB::get_parent_class(name);
        if (!inherits.empty())
            header += se_string(" inherits=\"") + inherits + "\"";
        StringName category = ClassDB::get_category(name);
        if (category.empty())
            category = "Core";
        header += se_string(" category=\"") + category + "\"";
        header += '>';
        _write_string(f, 0, header);
        _write_string(f, 1, "<brief_description>");
        _write_string(f, 1, "</brief_description>");
        _write_string(f, 1, "<description>");
        _write_string(f, 1, "</description>");
        _write_string(f, 1, "<methods>");

        PODVector<MethodInfo> method_list;
        ClassDB::get_method_list(name, &method_list, true);
        eastl::sort(method_list.begin(),method_list.end());

        for (const MethodInfo &E : method_list) {
            if (E.name.empty() || E.name.asCString()[0] == '_')
                continue; //hidden

            MethodBind *m = ClassDB::get_method(name, E.name);

            se_string qualifiers;
            if (E.flags & METHOD_FLAG_CONST)
                qualifiers += "qualifiers=\"const\"";

            _write_string(f, 2, "<method name=\"" + _escape_string(E.name) + "\" " + qualifiers + " >");

            for (int i = -1; i < E.arguments.size(); i++) {

                PropertyInfo arginfo;

                if (i == -1) {

                    arginfo = E.return_val;
                    se_string type_name = arginfo.hint == PROPERTY_HINT_RESOURCE_TYPE ? arginfo.hint_string : Variant::get_type_name(arginfo.type);

                    if (arginfo.type == VariantType::NIL)
                        continue;
                    _write_string(f, 3, "<return type=\"" + type_name + "\">");
                } else {

                    arginfo = E.arguments[i];

                    se_string type_name;

                    if (arginfo.hint == PROPERTY_HINT_RESOURCE_TYPE)
                        type_name = arginfo.hint_string;
                    else if (arginfo.type == VariantType::NIL)
                        type_name = "Variant";
                    else
                        type_name = Variant::get_type_name(arginfo.type);

                    if (m && m->has_default_argument(i)) {
                        Variant default_arg = m->get_default_argument(i);
                        se_string default_arg_text = _escape_string(m->get_default_argument(i).as<se_string>());

                        switch (default_arg.get_type()) {

                            case VariantType::NIL:
                                default_arg_text = "NULL";
                                break;
                            // atomic types
                            case VariantType::BOOL:
                                if (bool(default_arg))
                                    default_arg_text = "true";
                                else
                                    default_arg_text = "false";
                                break;
                            case VariantType::INT:
                            case VariantType::REAL:
                                //keep it
                                break;
                            case VariantType::STRING:
                            case VariantType::NODE_PATH:
                                default_arg_text = "\"" + default_arg_text + "\"";
                                break;
                            case VariantType::TRANSFORM:
                                if (default_arg.as<Transform>() == Transform()) {
                                    default_arg_text = "";
                                }

                                default_arg_text = se_string(Variant::get_type_name(default_arg.get_type())) + "(" + default_arg_text + ")";
                                break;

                            case VariantType::VECTOR2:
                            case VariantType::RECT2:
                            case VariantType::VECTOR3:
                            case VariantType::PLANE:
                            case VariantType::QUAT:
                            case VariantType::AABB:
                            case VariantType::BASIS:
                            case VariantType::COLOR:
                            case VariantType::POOL_BYTE_ARRAY:
                            case VariantType::POOL_INT_ARRAY:
                            case VariantType::POOL_REAL_ARRAY:
                            case VariantType::POOL_STRING_ARRAY:
                            case VariantType::POOL_VECTOR3_ARRAY:
                            case VariantType::POOL_COLOR_ARRAY:
                                default_arg_text = se_string(Variant::get_type_name(default_arg.get_type())) + "(" + default_arg_text + ")";
                                break;
                            case VariantType::OBJECT:
                            case VariantType::DICTIONARY: // 20
                            case VariantType::ARRAY:
                            case VariantType::_RID:

                            default: {
                            }
                        }

                        _write_string(f, 3, "<argument index=\"" + itos(i) + "\" name=\"" + _escape_string(arginfo.name) + "\" type=\"" + type_name + "\" default=\"" + _escape_string(default_arg_text) + "\">");
                    } else
                        _write_string(f, 3, "<argument index=\"" + itos(i) + "\" name=\"" + arginfo.name + "\" type=\"" + type_name + "\">");
                }

                se_string hint;
                switch (arginfo.hint) {
                    case PROPERTY_HINT_DIR: hint = "A directory."; break;
                case PROPERTY_HINT_RANGE:
                    hint = se_string("Range - min: ") + StringUtils::get_slice(arginfo.hint_string,",", 0) + " max: " + StringUtils::get_slice(arginfo.hint_string,",", 1) +
                           " step: " + StringUtils::get_slice(arginfo.hint_string,",", 2);
                    break;
                    case PROPERTY_HINT_ENUM:
                        hint = "Values: ";
                        for (int j = 0; j < StringUtils::get_slice_count(arginfo.hint_string,','); j++) {
                            if (j > 0) hint += ", ";
                            hint += se_string(StringUtils::get_slice(arginfo.hint_string,",", j)) + "=" + itos(j);
                        }
                        break;
                    case PROPERTY_HINT_LENGTH: hint = "Length: " + arginfo.hint_string; break;
                    case PROPERTY_HINT_FLAGS:
                        hint = "Values: ";
                        for (int j = 0; j < StringUtils::get_slice_count(arginfo.hint_string,','); j++) {
                            if (j > 0) hint += ", ";
                            hint += se_string(StringUtils::get_slice(arginfo.hint_string,",", j)) + "=" + itos((uint64_t)1 << j);
                        }
                        break;
                    case PROPERTY_HINT_FILE: hint = "A file:"; break;
                    default: {
                    }
                        //case PROPERTY_HINT_RESOURCE_TYPE: hint="Type: "+arginfo.hint_string; break;
                }
                if (!hint.empty())
                    _write_string(f, 4, hint);

                _write_string(f, 3, i == -1 ? "</return>" : "</argument>");
            }

            _write_string(f, 3, "<description>");
            _write_string(f, 3, "</description>");

            _write_string(f, 2, "</method>");
        }

        _write_string(f, 1, "</methods>");

        ListPOD<MethodInfo> signal_list;
        ClassDB::get_signal_list(name, &signal_list, true);

        if (!signal_list.empty()) {

            _write_string(f, 1, "<signals>");
            for (const MethodInfo &EV : signal_list) {

                _write_string(f, 2, se_string("<signal name=\"") + EV.name + "\">");
                for (size_t i = 0; i < EV.arguments.size(); i++) {
                    PropertyInfo arginfo = EV.arguments[i];
                    _write_string(f, 3, "<argument index=\"" + itos(i) + "\" name=\"" + arginfo.name + "\" type=\"" + Variant::get_type_name(arginfo.type) + "\">");
                    _write_string(f, 3, "</argument>");
                }
                _write_string(f, 3, "<description>");
                _write_string(f, 3, "</description>");

                _write_string(f, 2, "</signal>");
            }

            _write_string(f, 1, "</signals>");
        }

        _write_string(f, 1, "<constants>");

        ListPOD<se_string> constant_list;
        ClassDB::get_integer_constant_list(name, &constant_list, true);

        /* constants are sorted in a special way */

        List<_ConstantSort> constant_sort;

        for (const se_string &E : constant_list) {
            _ConstantSort cs;
            cs.name = E;
            cs.value = ClassDB::get_integer_constant(name, StringName(E));
            constant_sort.push_back(cs);
        }

        constant_sort.sort();

        for (List<_ConstantSort>::Element *E = constant_sort.front(); E; E = E->next()) {

            _write_string(f, 2, "<constant name=\"" + E->deref().name + "\" value=\"" + itos(E->deref().value) + "\">");
            _write_string(f, 2, "</constant>");
        }

        _write_string(f, 1, "</constants>");
        _write_string(f, 0, "</class>");

    }

    _write_string(f, 0, "</doc>");
    f->close();
    memdelete(f);
}
