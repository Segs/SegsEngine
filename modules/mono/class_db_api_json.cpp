/*************************************************************************/
/*  class_db_api_json.cpp                                                */
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

#include "class_db_api_json.h"

#ifdef DEBUG_METHODS_ENABLED

#include "core/io/json.h"
#include "core/os/file_access.h"
#include "core/print_string.h"
#include "core/project_settings.h"
#include "core/version.h"
#include "EASTL/sort.h"

#include "core/method_bind_interface.h"

void class_db_api_to_json(StringView p_output_file, ClassDB::APIType p_api) {
    Dictionary classes_dict;

    List<StringName> names;

    for(const eastl::pair<const StringName, ClassDB::ClassInfo> &k : ClassDB::classes) {

        names.emplace_back(k.first);
    }
    //must be alphabetically sorted for hash to compute

    names.sort(WrapAlphaCompare());

    for (const StringName &E : names) {

        ClassDB::ClassInfo *t = &ClassDB::classes.find(E)->second;
        ERR_FAIL_COND(!t);
        if (t->api != p_api || !t->exposed)
            continue;

        Dictionary class_dict;
        classes_dict[t->name] = class_dict;

        class_dict["inherits"] = t->inherits;

        { //methods

            List<StringName> snames;

            for(const eastl::pair<StringName, MethodBind *> &ck : t->method_map) {

                ERR_CONTINUE(ck.first.empty());

                StringView name(ck.first);
                if (name[0] == '_')
                    continue; // Ignore non-virtual methods that start with an underscore

                snames.push_back(ck.first);
            }

            snames.sort(WrapAlphaCompare());

            Array methods;

            for (const StringName &F : snames) {
                Dictionary method_dict;
                methods.push_back(method_dict);

                MethodBind *mb = t->method_map[F];
                method_dict["name"] = mb->get_name();
                method_dict["argument_count"] = mb->get_argument_count();
                method_dict["return_type"] = mb->get_argument_type(-1);

                Array arguments;
                method_dict["arguments"] = arguments;

                for (int i = 0; i < mb->get_argument_count(); i++) {
                    Dictionary argument_dict;
                    arguments.push_back(argument_dict);
                    const PropertyInfo info = mb->get_argument_info(i);
                    argument_dict["type"] = info.type;
                    argument_dict["name"] = info.name;
                    argument_dict["hint"] = info.hint;
                    argument_dict["hint_string"] = info.hint_string;
                }

                method_dict["default_argument_count"] = mb->get_default_argument_count();

                Array default_arguments;
                method_dict["default_arguments"] = default_arguments;

                for (int i = 0; i < mb->get_default_argument_count(); i++) {
                    Dictionary default_argument_dict;
                    default_arguments.push_back(default_argument_dict);
                    //hash should not change, i hope for tis
                    Variant da = mb->get_default_argument(i);
                    default_argument_dict["value"] = da;
                }

                method_dict["hint_flags"] = mb->get_hint_flags();
            }

            if (!methods.empty()) {
                class_dict["methods"] = methods;
            }
        }

        { //constants

            List<StringName> snames;

            for(const auto &k : t->constant_map) {

                snames.push_back(k.first);
            }

            snames.sort(WrapAlphaCompare());

            Array constants;

            for (const StringName &F : snames) {
                Dictionary constant_dict;
                constants.push_back(constant_dict);

                constant_dict["name"] = F;
                constant_dict["value"] = t->constant_map[F];
            }

            if (!constants.empty()) {
                class_dict["constants"] = constants;
            }
        }

        { //signals

            Vector<StringName> snames;
            auto &signal_map(t->class_signal_map());
            signal_map.keys_into(snames);
            eastl::sort(snames.begin(),snames.end(),WrapAlphaCompare());

            Array used_signals;

            for (const StringName &F : snames) {
                Dictionary signal_dict;
                used_signals.push_back(signal_dict);

                MethodInfo &mi = signal_map[F];
                signal_dict["name"] = F;

                Array arguments;
                signal_dict["arguments"] = arguments;
                for (int i = 0; i < mi.arguments.size(); i++) {
                    Dictionary argument_dict;
                    arguments.push_back(argument_dict);
                    argument_dict["type"] = mi.arguments[i].type;
                }
            }

            if (!used_signals.empty()) {
                class_dict["signals"] = used_signals;
            }
        }

        { //properties

            Vector<StringName> snames;
            t->property_setget.keys_into(snames);
            eastl::sort(snames.begin(),snames.end(),WrapAlphaCompare());

            Array properties;

            for (const StringName &F : snames) {
                Dictionary property_dict;
                properties.push_back(property_dict);

                ClassDB::PropertySetGet *psg = &t->property_setget[F];

                property_dict["name"] = F;
                property_dict["setter"] = psg->setter;
                property_dict["getter"] = psg->getter;
            }

            if (!properties.empty()) {
                class_dict["property_setget"] = properties;
            }
        }

        Array property_list;

        //property list
        for (const PropertyInfo &F : t->property_list) {
            Dictionary property_dict;
            property_list.push_back(property_dict);

            property_dict["name"] = F.name;
            property_dict["type"] = F.type;
            property_dict["hint"] = F.hint;
            property_dict["hint_string"] = F.hint_string;
            property_dict["usage"] = F.usage;
        }

        if (!property_list.empty()) {
            class_dict["property_list"] = property_list;
        }
    }

    FileAccessRef f = FileAccess::open(p_output_file, FileAccess::WRITE);
    ERR_FAIL_COND_MSG(!f, "Cannot open file '" + p_output_file + "'."); 
    f->store_string(JSON::print(classes_dict, /*indent: */ "\t"));
    f->close();

    print_line(String() + "ClassDB API JSON written to: " + ProjectSettings::get_singleton()->globalize_path(p_output_file));
}

#endif // DEBUG_METHODS_ENABLED
