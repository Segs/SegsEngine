/*************************************************************************/
/*  bindings_generator.h                                                 */
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

#include "common.h"

#include "core/hash_map.h"
#include "core/map.h"
//#include "core/class_db.h"
//#include "core/string_builder.h"
//#include "editor/doc/doc_data.h"
//#include "core/doc_support/doc_data.h"
//#include "core/reflection_support/reflection_data.h"

struct GeneratorContext;
struct TypeInterface;
struct ConstantInterface;
struct EnumInterface;
struct PropertyInterface;
struct MethodInterface;
struct StringBuilder;

class DocData;
namespace DocContents {
class ClassDoc;
};
class GenError {

};

class BindingsGenerator {
    friend struct GeneratorContext;
public:

    struct InternalCall {
        String name;
        String im_type_out; // Return type for the C# method declaration. Also used as companion of [unique_siq]
        String im_sig; // Signature for the C# method declaration
        String unique_sig; // Unique signature to avoid duplicates in containers
        bool editor_only=false;

        InternalCall() {}

        InternalCall(const String &p_name, const String &p_im_type_out, const String &p_im_sig = String(), const String &p_unique_sig = String()) {
            name = p_name;
            im_type_out = p_im_type_out;
            im_sig = p_im_sig;
            unique_sig = p_unique_sig;
        }

//        InternalCall(APIType api_type, const String &p_name, const String &p_im_type_out, const String &p_im_sig = String(), const String &p_unique_sig = String()) {
//            name = p_name;
//            im_type_out = p_im_type_out;
//            im_sig = p_im_sig;
//            unique_sig = p_unique_sig;
//            editor_only = api_type == APIType::Editor;
//        }

        bool operator==(const InternalCall &p_a) const {
            return p_a.unique_sig == unique_sig;
        }
    };

    bool log_print_enabled;
    bool initialized;

    HashMap<String,InternalCall> method_icalls;
    Map<const MethodInterface *, const InternalCall *> method_icalls_map;

    Vector<const InternalCall *> generated_icall_funcs;

//    List<InternalCall> core_custom_icalls;
//    List<InternalCall> editor_custom_icalls;

    Map<String, Vector<String> > blacklisted_methods;


    void _initialize_blacklisted_methods();

    bool has_named_icall(const String &p_name, const Vector<InternalCall> &p_list) {
        for (const InternalCall &E : p_list) {
            if (E.name == p_name)
                return true;
        }
        return false;
    }

    String bbcode_to_xml(StringView p_bbcode, const TypeInterface *p_itype, DocData *doc);

    int _determine_enum_prefix(const EnumInterface &p_ienum);
    void _apply_prefix_to_enum_constants(EnumInterface &p_ienum, int p_prefix_length);

    void _generate_method_icalls(const TypeInterface &p_itype);

    bool _populate_object_type_interfaces();
    void _populate_builtin_type_interfaces();

    void _populate_global_constants();

    GenError _generate_cs_property(const TypeInterface &p_itype, const PropertyInterface &p_iprop, StringBuilder &p_output);
    GenError _generate_cs_method(const TypeInterface &p_itype, const MethodInterface &p_imethod, int &p_method_bind_count, StringBuilder &p_output);

    GenError _generate_glue_method(const TypeInterface &p_itype, const MethodInterface &p_imethod, StringBuilder &p_output);

    void _initialize(DocData *docs);

public:
    GenError generate_cs_core_project(StringView p_proj_dir, GeneratorContext &ctx, DocData *doc);
    GenError generate_cs_editor_project(const String &p_proj_dir, GeneratorContext &ctx);
    GenError generate_cs_api(StringView p_output_dir, GeneratorContext &ctx, DocData *doc);
    GenError generate_glue(StringView p_output_dir, GeneratorContext &ctx);

    bool is_log_print_enabled() const { return log_print_enabled; }
    void set_log_print_enabled(bool p_enabled) { log_print_enabled = p_enabled; }

    bool is_initialized() const { return initialized; }

    static uint32_t get_version();

    static void handle_cmdline_args(const Vector<String> &p_cmdline_args);


    BindingsGenerator(DocData* docs) :
            log_print_enabled(true),
            initialized(false) {
        _initialize(docs);
    }
private:
    GenError generate_cs_type_docs(const TypeInterface &itype, const DocContents::ClassDoc *class_doc, StringBuilder &output);
    void generate_cs_type_doc_summary(const TypeInterface &itype, const DocContents::ClassDoc *class_doc, StringBuilder &output);
};


struct GeneratorContext {
    String  m_cs_namespace; // namespace used by all generated bindings
    String m_globals_class; // a class where all non-enum globals will be put
    String m_native_calls_class; // this class will contain all internal call wrappers
    String m_assembly_name;
    // Hash code of the 'user' side of this binding.
    uint64_t cs_side_hash;
    // Engine/module internal api hash
    uint64_t api_hash;

    Vector<BindingsGenerator::InternalCall> custom_icalls;

};


