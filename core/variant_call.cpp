/*************************************************************************/
/*  variant_call.cpp                                                     */
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

#include "variant.h"

#include "core/color_names.inc"
#include "core/container_tools.h"
#include "core/core_string_names.h"
#include "core/debugger/script_debugger.h"
#include "core/string_utils.h"
#include "core/string_utils.inl"
#include "core/crypto/crypto_core.h"
#include "core/io/compression.h"
#include "core/math/aabb.h"
#include "core/math/basis.h"
#include "core/math/face3.h"
#include "core/math/plane.h"
#include "core/math/quat.h"
#include "core/math/transform.h"
#include "core/math/transform_2d.h"
#include "core/math/vector3.h"
#include "core/math/math_funcs.h"
#include "core/method_bind_interface.h"
#include "core/method_info.h"
#include "core/object.h"
#include "core/object_db.h"
#include "core/object_rc.h"
#include "core/os/os.h"
#include "core/script_language.h"
#include "core/string.h"
#include "core/vector.h"
#include "core/rid.h"
#include "core/container_tools.h"


using String = String;

using VariantFunc = void (*)(Variant &, Variant &, const Variant **);
using VariantConstructFunc = void (*)(Variant &, const Variant **);

namespace {
    struct VariantAutoCaster {
        const Variant &from;
        constexpr VariantAutoCaster(const Variant&src) : from(src) {}
        operator Variant() const{
          return from;
        }

        template<class T>
        operator T() const{
          return from.as<T>();
        }
    };
}
struct _VariantCall {
    struct Arg {
        const char * name;
        VariantType type;
        constexpr Arg(VariantType p_type, const char *p_name) :
            name(p_name),
            type(p_type) { }
        constexpr Arg() : name(nullptr),type(VariantType::NIL) {}
    };
    static void Vector3_dot(Variant &r_ret, Variant &p_self, const Variant **p_args) {

        r_ret = reinterpret_cast<Vector3 *>(p_self._data._mem)->dot(*reinterpret_cast<const Vector3 *>(p_args[0]->_data._mem));
    }

    struct FuncData {

        uint8_t arg_count=0;
        uint8_t def_count=0;
        Variant default_args[5] = {};
        VariantType arg_types[5] = {VariantType::NIL};
        StringView arg_names[5] = {};
        VariantType return_type;

        bool _const;
        bool returns;
        VariantFunc func;

        FuncData() = default;
        FuncData(bool p_const, VariantType p_return, bool p_has_return, VariantFunc p_func,
                std::initializer_list<Variant> p_defaultarg, std::initializer_list<const Arg> p_argtype1) :
                return_type(p_return), _const(p_const), returns(p_has_return), func(p_func) {

            for(const Arg & a : p_argtype1) {
                if(a.name) {
                    arg_types[arg_count] = a.type;
#ifdef DEBUG_ENABLED
                    arg_names[arg_count] = a.name;
#endif
                }
                else
                    break;
                ++arg_count;
            }
            for(const Variant & v : p_defaultarg)
                default_args[def_count++] = v;
        }
        bool verify_arguments(const Variant **p_args, Callable::CallError &r_error) {

            if (arg_count == 0)
                return true;

            const VariantType *tptr = &arg_types[0];

            for (int i = 0; i < arg_count; i++) {

                if (tptr[i] == VariantType::NIL || tptr[i] == p_args[i]->type)
                    continue; // all good
                if (!Variant::can_convert(p_args[i]->type, tptr[i])) {
                    r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
                    r_error.argument = i;
                    r_error.expected = tptr[i];
                    return false;
                }
            }
            return true;
        }

        void call(Variant &r_ret, Variant &p_self, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
#ifdef DEBUG_ENABLED
            if (p_argcount > arg_count) {
                r_error.error = Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
                r_error.argument = arg_count;
                return;
            } else
#endif
                    if (p_argcount < arg_count) {
                int def_argcount = def_count;
#ifdef DEBUG_ENABLED
                if (p_argcount < (arg_count - def_argcount)) {
                    r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
                    r_error.argument = arg_count - def_argcount;
                    return;
                }

#endif
                ERR_FAIL_COND(p_argcount > VARIANT_ARG_MAX);
                const Variant *newargs[VARIANT_ARG_MAX];
                for (int i = 0; i < p_argcount; i++)
                    newargs[i] = p_args[i];
                // fill in any remaining parameters with defaults
                int first_default_arg = arg_count - def_argcount;
                for (int i = p_argcount; i < arg_count; i++)
                    newargs[i] = &default_args[i - first_default_arg];
#ifdef DEBUG_ENABLED
                if (!verify_arguments(newargs, r_error))
                    return;
#endif
                func(r_ret, p_self, newargs);
            } else {
#ifdef DEBUG_ENABLED
                if (!verify_arguments(p_args, r_error))
                    return;
#endif
                func(r_ret, p_self, p_args);
            }
        }
    };

    struct VariantFuncDef {
        VariantType type;
        StaticCString method;
        FuncData func_def;
    };

    struct TypeFunc {
        HashMap<StringName, FuncData> functions;
    };

    static TypeFunc *type_funcs;



    //void addfunc(VariantType p_type, const StringName& p_name,VariantFunc p_func);

    static void make_func_return_variant(VariantType p_type, const StringName &p_name) {

#ifdef DEBUG_ENABLED
        type_funcs[(int)p_type].functions[p_name].returns = true;
#endif
    }
    static void addfunc_span(bool p_const, VariantType p_type, VariantType p_return, bool p_has_return, const StringName &p_name, VariantFunc p_func, const
        std::initializer_list<Variant> p_defaultarg, std::initializer_list<const Arg> p_args) {
        FuncData funcdata(p_const,p_return,p_has_return,p_func,p_defaultarg,p_args);
        type_funcs[(int)p_type].functions[p_name] = funcdata;
    }
    static void addfunc_span(Span<const VariantFuncDef> funcs) {
        for(const VariantFuncDef &fdef : funcs) {
            type_funcs[(int)fdef.type].functions[fdef.method] = fdef.func_def;
        }
    }

    static void addfunc(bool p_const, VariantType p_type, VariantType p_return, bool p_has_return, const StringName &p_name, VariantFunc p_func, const
            std::initializer_list<Variant> p_defaultarg, const Arg &p_argtype1 = Arg(), const Arg &p_argtype2 = Arg(), const Arg &p_argtype3 = Arg(), const Arg &p_argtype4 = Arg(), const Arg &p_argtype5 = Arg()) {

        FuncData funcdata;
        funcdata.func = p_func;
        for(const Variant & v : p_defaultarg)
            funcdata.default_args[funcdata.def_count++] = v;
        funcdata._const = p_const;
        funcdata.returns = p_has_return;
        funcdata.return_type = p_return;
        int idx = 0;
        const Arg *argtypes[5] = {
            &p_argtype1,
            &p_argtype2,
            &p_argtype3,
            &p_argtype4,
            &p_argtype5,
        };
        for(int i=0; i<5; ++i) {
            if (argtypes[i]->name) {
                funcdata.arg_types[i] = argtypes[i]->type;
    #ifdef DEBUG_ENABLED
                funcdata.arg_names[i] = argtypes[i]->name;
    #endif
                idx++;
            } else
                break;
        }
        funcdata.arg_count = idx;
        type_funcs[(int)p_type].functions[p_name] = funcdata;
    }

#define VCALL_LOCALMEM0(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant ** /*p_args*/) { reinterpret_cast<m_type *>(p_self._data._mem)->m_method(); }
#define VCALL_LOCALMEM0R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant ** /*p_args*/) { r_ret = reinterpret_cast<m_type *>(p_self._data._mem)->m_method(); }
#define VCALL_LOCALMEM1(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { reinterpret_cast<m_type *>(p_self._data._mem)->m_method(VariantAutoCaster(*p_args[0])); }
#define VCALL_LOCALMEM1R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { r_ret = reinterpret_cast<m_type *>(p_self._data._mem)->m_method(VariantAutoCaster(*p_args[0])); }
#define VCALL_LOCALMEM2(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { reinterpret_cast<m_type *>(p_self._data._mem)->m_method(VariantAutoCaster(*p_args[0]), VariantAutoCaster(*p_args[1])); }
#define VCALL_LOCALMEM2R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { r_ret = reinterpret_cast<m_type *>(p_self._data._mem)->m_method(VariantAutoCaster(*p_args[0]), VariantAutoCaster(*p_args[1])); }
#define VCALL_LOCALMEM3(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { reinterpret_cast<m_type *>(p_self._data._mem)->m_method(VariantAutoCaster(*p_args[0]), VariantAutoCaster(*p_args[1]), VariantAutoCaster(*p_args[2])); }
#define VCALL_LOCALMEM3R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { r_ret = reinterpret_cast<m_type *>(p_self._data._mem)->m_method(VariantAutoCaster(*p_args[0]), VariantAutoCaster(*p_args[1]), VariantAutoCaster(*p_args[2])); }
#define VCALL_LOCALMEM4(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { reinterpret_cast<m_type *>(p_self._data._mem)->m_method(VariantAutoCaster(*p_args[0]), VariantAutoCaster(*p_args[1]), VariantAutoCaster(*p_args[2]), VariantAutoCaster(*p_args[3])); }
#define VCALL_LOCALMEM4R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { r_ret = reinterpret_cast<m_type *>(p_self._data._mem)->m_method(VariantAutoCaster(*p_args[0]), VariantAutoCaster(*p_args[1]), VariantAutoCaster(*p_args[2]), VariantAutoCaster(*p_args[3])); }
#define VCALL_LOCALMEM5(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { reinterpret_cast<m_type *>(p_self._data._mem)->m_method(VariantAutoCaster(*p_args[0]), VariantAutoCaster(*p_args[1]), VariantAutoCaster(*p_args[2]), VariantAutoCaster(*p_args[3]), VariantAutoCaster(*p_args[4])); }
#define VCALL_LOCALMEM5R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { r_ret = reinterpret_cast<m_type *>(p_self._data._mem)->m_method(VariantAutoCaster(*p_args[0]), VariantAutoCaster(*p_args[1]), VariantAutoCaster(*p_args[2]), VariantAutoCaster(*p_args[3]), VariantAutoCaster(*p_args[4])); }

    // built-in functions of localmem based types
#define VCALL_SU_LOCALMEM0R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant ** /*p_args*/) { \
    r_ret = Variant::from(StringUtils::m_method(*reinterpret_cast<String *>(p_self._data._mem))); }
#define VCALL_SU_LOCALMEM1R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant ** p_args) { \
    r_ret = Variant::from(StringUtils::m_method(*reinterpret_cast<String *>(p_self._data._mem),VariantAutoCaster(*p_args[0]))); }
#define VCALL_SU_LOCALMEM2R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant ** p_args) { \
    r_ret = Variant::from(StringUtils::m_method(*reinterpret_cast<String *>(p_self._data._mem),VariantAutoCaster(*p_args[0]),VariantAutoCaster(*p_args[1]))); }
#define VCALL_SU_LOCALMEM2(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant ** p_args) { \
    StringUtils::m_method(*reinterpret_cast<m_type *>(p_self._data._mem),VariantAutoCaster(*p_args[0]),VariantAutoCaster(*p_args[1])); }

#define VCALL_SU_LOCALMEM3R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant ** p_args) { \
    r_ret = Variant::from(StringUtils::m_method(*reinterpret_cast<m_type *>(p_self._data._mem),VariantAutoCaster(*p_args[0]),VariantAutoCaster(*p_args[1]),VariantAutoCaster(*p_args[2]))); }
#define VCALL_PU_LOCALMEM0R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant ** /*p_args*/) { \
    r_ret = PathUtils::m_method(*reinterpret_cast<m_type *>(p_self._data._mem)); }
#define VCALL_PU_LOCALMEM1R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant ** p_args) { \
    r_ret = PathUtils::m_method(*reinterpret_cast<m_type *>(p_self._data._mem),VariantAutoCaster(*p_args[0])); }
#define VCALL_SPU_LOCALMEM1R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant ** p_args) { \
    r_ret = PathUtils::m_method(VariantAutoCaster(*p_args[0])); }

    static void _call_String_casecmp_to(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        r_ret = StringUtils::compare(*reinterpret_cast<String *>(p_self._data._mem),(*p_args[0]).as<String>());
    }
    static void _call_String_nocasecmp_to(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        r_ret = StringUtils::compare(*reinterpret_cast<String *>(p_self._data._mem),(*p_args[0]).as<String>(),StringUtils::CaseInsensitive);
    }
    static void _call_String_length(Variant &r_ret, Variant &p_self, const Variant ** /*p_args*/) {
        r_ret = reinterpret_cast<String *>(p_self._data._mem)->length(); }
    static void _call_String_count(Variant &r_ret, Variant &p_self, const Variant ** p_args) {
        r_ret = Variant(StringUtils::count(*reinterpret_cast<String *>(p_self._data._mem),p_args[0]->as<String>(), VariantAutoCaster(*p_args[1]), VariantAutoCaster(*p_args[2])));
    }
    static void _call_String_countn(Variant &r_ret, Variant &p_self, const Variant ** p_args) {
        r_ret = Variant(StringUtils::countn(*reinterpret_cast<String *>(p_self._data._mem),p_args[0]->as<String>(),VariantAutoCaster(*p_args[1]), VariantAutoCaster(*p_args[2])));
    }
    VCALL_SU_LOCALMEM2R(String, substr)
    static void _call_String_find(Variant &r_ret, Variant &p_self, const Variant ** p_args) {
        r_ret = Variant(StringUtils::find(*reinterpret_cast<String *>(p_self._data._mem),p_args[0]->as<String>(), VariantAutoCaster(*p_args[1])));
    }
    static void _call_String_find_last(Variant &r_ret, Variant &p_self, const Variant ** p_args) {
        r_ret = Variant(StringUtils::find_last(*reinterpret_cast<String *>(p_self._data._mem),p_args[0]->as<String>()));
    }
    static void _call_String_findn(Variant &r_ret, Variant &p_self, const Variant ** p_args) {
        r_ret = Variant(StringUtils::findn(*reinterpret_cast<String *>(p_self._data._mem),p_args[0]->as<String>(), VariantAutoCaster(*p_args[1])));
    }
    static void _call_String_rfind(Variant &r_ret, Variant &p_self, const Variant ** p_args) {
        r_ret = Variant(StringUtils::rfind(*reinterpret_cast<String *>(p_self._data._mem),p_args[0]->as<String>(), VariantAutoCaster(*p_args[1])));
    }
    static void _call_String_rfindn(Variant &r_ret, Variant &p_self, const Variant ** p_args) {
        r_ret = Variant(StringUtils::rfindn(*reinterpret_cast<String *>(p_self._data._mem),p_args[0]->as<String>(),p_args[1]->as<int>()));
    }

    static void _call_String_match(Variant &r_ret, Variant &p_self, const Variant ** p_args) {
        r_ret = Variant(StringUtils::match(*reinterpret_cast<String *>(p_self._data._mem),p_args[0]->as<String>()));
    }
    static void _call_String_matchn(Variant &r_ret, Variant &p_self, const Variant ** p_args) {
        r_ret = Variant(StringUtils::matchn(*reinterpret_cast<String *>(p_self._data._mem),p_args[0]->as<String>()));
    }
    static void _call_String_begins_with(Variant &r_ret, Variant &p_self, const Variant ** p_args) {
        r_ret = Variant(StringUtils::begins_with(*reinterpret_cast<String *>(p_self._data._mem),p_args[0]->as<String>()));
    }
    static void _call_String_ends_with(Variant &r_ret, Variant &p_self, const Variant ** p_args) {
        r_ret = Variant(StringUtils::ends_with(*reinterpret_cast<String *>(p_self._data._mem),p_args[0]->as<String>()));
    }
    static void _call_String_is_subsequence_of(Variant &r_ret, Variant &p_self, const Variant ** p_args) {
        r_ret = Variant(StringUtils::is_subsequence_of(*reinterpret_cast<String *>(p_self._data._mem),p_args[0]->as<String>()));
    }
    static void _call_String_is_subsequence_ofi(Variant &r_ret, Variant &p_self, const Variant ** p_args) {
        r_ret = Variant(StringUtils::is_subsequence_ofi(*reinterpret_cast<String *>(p_self._data._mem),p_args[0]->as<String>()));
    }
//    VCALL_SU_LOCALMEM0R(String, bigrams)
//    VCALL_SU_LOCALMEM1R(String, similarity)

//    static void _call_String_format(Variant &r_ret, Variant &p_self, const Variant **p_args) {
//        r_ret = StringUtils::format(*reinterpret_cast<String *>(p_self._data._mem),VariantAutoCaster(*p_args[0]));
//    }
    static void _call_String_replace(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        r_ret = StringUtils::replace(*reinterpret_cast<String *>(p_self._data._mem),p_args[0]->as<String>(), p_args[1]->as<String>());
    }
    static void _call_String_replacen(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        r_ret = StringUtils::replacen(*reinterpret_cast<String *>(p_self._data._mem),p_args[0]->as<String>(), p_args[1]->as<String>());
    }
    VCALL_SU_LOCALMEM1R(String, repeat)
    static void _call_String_insert(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        r_ret = StringUtils::insert(*reinterpret_cast<String *>(p_self._data._mem),p_args[0]->as<int>(), p_args[1]->as<String>());
    }
    VCALL_SU_LOCALMEM0R(String, capitalize)
    static void _call_String_split(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        Vector<StringView> parts(StringUtils::split(*reinterpret_cast<String *>(p_self._data._mem),p_args[0]->as<String>(), p_args[2]->as<bool>()));
        r_ret = Variant::from(parts);
    }
    static void _call_String_rsplit(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        Vector<StringView> parts(StringUtils::rsplit(*reinterpret_cast<String *>(p_self._data._mem),p_args[0]->as<String>(), p_args[2]->as<bool>()));
        r_ret = Variant::from(parts);
    }
    static void _call_String_split_floats(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        r_ret = Variant::from(StringUtils::split_floats(*reinterpret_cast<String *>(p_self._data._mem),p_args[0]->as<String>()));
    }
    VCALL_SU_LOCALMEM0R(String, to_upper)
    VCALL_SU_LOCALMEM0R(String, to_lower)
    VCALL_SU_LOCALMEM1R(String, left)
    VCALL_SU_LOCALMEM1R(String, right)
    VCALL_SU_LOCALMEM0R(String, dedent)
    VCALL_SU_LOCALMEM2R(String, strip_edges)
    VCALL_SU_LOCALMEM0R(String, strip_escapes)
    static void _call_String_lstrip(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        r_ret = StringUtils::lstrip(*reinterpret_cast<String *>(p_self._data._mem),p_args[0]->as<String>());
    }
    static void _call_String_rstrip(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        r_ret = StringUtils::rstrip(*reinterpret_cast<String *>(p_self._data._mem),p_args[0]->as<String>());
    }
    //VCALL_SU_LOCALMEM1R(String, ord_at)
    VCALL_SU_LOCALMEM2(String, erase)
    VCALL_SU_LOCALMEM0R(String, hash)
    static void _call_String_md5_text(Variant &r_ret, Variant &p_self, const Variant **) {
        r_ret = String(StringUtils::md5_text(*reinterpret_cast<String *>(p_self._data._mem))); }
    static void _call_String_sha1_text(Variant &r_ret, Variant &p_self, const Variant **) {
        r_ret = String(StringUtils::sha1_text(*reinterpret_cast<String *>(p_self._data._mem))); }
    static void _call_String_sha256_text(Variant &r_ret, Variant &p_self, const Variant **) {
        r_ret = String(StringUtils::sha256_text(*reinterpret_cast<String *>(p_self._data._mem))); }
    VCALL_SU_LOCALMEM0R(String, md5_buffer)
    VCALL_SU_LOCALMEM0R(String, sha1_buffer)
    VCALL_SU_LOCALMEM0R(String, sha256_buffer)
    static void _call_String_empty(Variant &r_ret, Variant &p_self, const Variant **) {
        r_ret = Variant(reinterpret_cast<String *>(p_self._data._mem)->empty()); }

    VCALL_SPU_LOCALMEM1R(String, humanize_size)
    VCALL_PU_LOCALMEM0R(String, is_abs_path)
    VCALL_PU_LOCALMEM0R(String, is_rel_path)
    static void _call_String_get_extension(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        r_ret = PathUtils::get_extension(*reinterpret_cast<String *>(p_self._data._mem));
    }
    static void _call_String_get_basename(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        r_ret = PathUtils::get_basename(*reinterpret_cast<String *>(p_self._data._mem));
    }
    static void _call_String_plus_file(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        r_ret = PathUtils::plus_file(*reinterpret_cast<String *>(p_self._data._mem),p_args[0]->as<String>());
    }
    static void _call_String_get_base_dir(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        r_ret = PathUtils::get_base_dir(*reinterpret_cast<String *>(p_self._data._mem));
    }
    static void _call_String_get_file(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        r_ret = PathUtils::get_file(*reinterpret_cast<String *>(p_self._data._mem));
    }
    VCALL_SU_LOCALMEM0R(String, xml_escape)
    VCALL_SU_LOCALMEM0R(String, xml_unescape)
    VCALL_SU_LOCALMEM0R(String, http_escape)
    VCALL_SU_LOCALMEM0R(String, http_unescape)
    VCALL_SU_LOCALMEM0R(String, c_escape)
    VCALL_SU_LOCALMEM0R(String, c_unescape)
    VCALL_SU_LOCALMEM0R(String, json_escape)
    VCALL_SU_LOCALMEM0R(String,percent_encode)
    VCALL_SU_LOCALMEM0R(String,percent_decode)
    VCALL_SU_LOCALMEM0R(String, is_valid_identifier)
    VCALL_SU_LOCALMEM0R(String, is_valid_integer)
    VCALL_SU_LOCALMEM0R(String, is_valid_float)
    VCALL_SU_LOCALMEM1R(String, is_valid_hex_number)
    VCALL_SU_LOCALMEM0R(String, is_valid_html_color)
    VCALL_SU_LOCALMEM0R(String, is_valid_ip_address)
    VCALL_SU_LOCALMEM0R(String, is_valid_filename)
    VCALL_SU_LOCALMEM0R(String, to_int)
    VCALL_SU_LOCALMEM0R(String, to_float)
    VCALL_SU_LOCALMEM0R(String, hex_to_int)
    VCALL_SU_LOCALMEM1R(String, pad_decimals)
    VCALL_SU_LOCALMEM1R(String, pad_zeros)
    static void _call_String_trim_prefix(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        r_ret = StringUtils::trim_prefix(*reinterpret_cast<String *>(p_self._data._mem),p_args[0]->as<String>());
    }
    static void _call_String_trim_suffix(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        r_ret = StringUtils::trim_suffix(*reinterpret_cast<String *>(p_self._data._mem),p_args[0]->as<String>());
    }
    static void _call_String_to_ascii(Variant &r_ret, Variant &p_self, const Variant **p_args) {

        String *s = reinterpret_cast<String *>(p_self._data._mem);
        if (s->empty()) {
            r_ret = PoolByteArray();
            return;
        }
        UIString tmp(UIString::fromUtf8(s->data(),s->size()));
        CharString charstr(tmp.toLatin1());

        PoolByteArray retval;
        size_t len = charstr.length();
        retval.resize(len);
        PoolByteArray::Write w = retval.write();
        memcpy(w.ptr(), charstr.data(), len);
        w.release();

        r_ret = retval;
    }

    static void _call_String_to_utf8(Variant &r_ret, Variant &p_self, const Variant **p_args) {

        String *s = reinterpret_cast<String *>(p_self._data._mem);
        if (s->empty()) {
            r_ret = PoolByteArray();
            return;
        }
        PoolByteArray retval;
        size_t len = s->length();
        retval.resize(len);
        PoolByteArray::Write w = retval.write();
        memcpy(w.ptr(), s->data(), len);
        w.release();

        r_ret = retval;
    }

    VCALL_LOCALMEM1R(Vector2, distance_to)
    VCALL_LOCALMEM1R(Vector2, distance_squared_to)
    VCALL_LOCALMEM0R(Vector2, length)
    VCALL_LOCALMEM0R(Vector2, length_squared)
    VCALL_LOCALMEM0R(Vector2, normalized)
    VCALL_LOCALMEM0R(Vector2, is_normalized)
    VCALL_LOCALMEM1R(Vector2, is_equal_approx)
    VCALL_LOCALMEM1R(Vector2, posmod)
    VCALL_LOCALMEM1R(Vector2, posmodv)
    VCALL_LOCALMEM1R(Vector2, project)
    VCALL_LOCALMEM1R(Vector2, angle_to)
    VCALL_LOCALMEM1R(Vector2, angle_to_point)
    VCALL_LOCALMEM1R(Vector2, direction_to)
    VCALL_LOCALMEM2R(Vector2, linear_interpolate)
    VCALL_LOCALMEM2R(Vector2, slerp)
    VCALL_LOCALMEM4R(Vector2, cubic_interpolate)
    VCALL_LOCALMEM2R(Vector2, move_toward)
    VCALL_LOCALMEM1R(Vector2, rotated)
    VCALL_LOCALMEM0R(Vector2, tangent)
    VCALL_LOCALMEM0R(Vector2, floor)
    VCALL_LOCALMEM0R(Vector2, ceil)
    VCALL_LOCALMEM0R(Vector2, round)
    VCALL_LOCALMEM1R(Vector2, snapped)
    VCALL_LOCALMEM0R(Vector2, aspect)
    VCALL_LOCALMEM1R(Vector2, dot)
    VCALL_LOCALMEM1R(Vector2, slide)
    VCALL_LOCALMEM1R(Vector2, bounce)
    VCALL_LOCALMEM1R(Vector2, reflect)
    VCALL_LOCALMEM0R(Vector2, angle)
    VCALL_LOCALMEM1R(Vector2, cross)
    VCALL_LOCALMEM0R(Vector2, abs)
    VCALL_LOCALMEM1R(Vector2, clamped)
    VCALL_LOCALMEM0R(Vector2, sign)

    VCALL_LOCALMEM0R(Rect2, get_area)
    VCALL_LOCALMEM0R(Rect2, has_no_area)
    VCALL_LOCALMEM1R(Rect2, has_point)
    VCALL_LOCALMEM1R(Rect2, is_equal_approx)
    VCALL_LOCALMEM2R(Rect2, intersects)
    VCALL_LOCALMEM1R(Rect2, encloses)
    VCALL_LOCALMEM1R(Rect2, clip)
    VCALL_LOCALMEM1R(Rect2, merge)
    VCALL_LOCALMEM1R(Rect2, expand)
    VCALL_LOCALMEM1R(Rect2, grow)
    VCALL_LOCALMEM2R(Rect2, grow_margin)
    VCALL_LOCALMEM4R(Rect2, grow_individual)
    VCALL_LOCALMEM0R(Rect2, abs)

    VCALL_LOCALMEM0R(Vector3, min_axis)
    VCALL_LOCALMEM0R(Vector3, max_axis)
    VCALL_LOCALMEM1R(Vector3, distance_to)
    VCALL_LOCALMEM1R(Vector3, distance_squared_to)
    VCALL_LOCALMEM0R(Vector3, length)
    VCALL_LOCALMEM0R(Vector3, length_squared)
    VCALL_LOCALMEM0R(Vector3, normalized)
    VCALL_LOCALMEM0R(Vector3, is_normalized)
    VCALL_LOCALMEM1R(Vector3, is_equal_approx)
    VCALL_LOCALMEM0R(Vector3, inverse)
    VCALL_LOCALMEM1R(Vector3, snapped)
    VCALL_LOCALMEM2R(Vector3, rotated)
    VCALL_LOCALMEM2R(Vector3, linear_interpolate)
    VCALL_LOCALMEM2R(Vector3, slerp)
    VCALL_LOCALMEM4R(Vector3, cubic_interpolate)
    VCALL_LOCALMEM2R(Vector3, move_toward)
    VCALL_LOCALMEM1R(Vector3, dot)
    VCALL_LOCALMEM1R(Vector3, cross)
    VCALL_LOCALMEM1R(Vector3, outer)
    VCALL_LOCALMEM0R(Vector3, to_diagonal_matrix)
    VCALL_LOCALMEM0R(Vector3, abs)
    VCALL_LOCALMEM0R(Vector3, floor)
    VCALL_LOCALMEM0R(Vector3, ceil)
    VCALL_LOCALMEM0R(Vector3, round)
    VCALL_LOCALMEM1R(Vector3, posmod)
    VCALL_LOCALMEM1R(Vector3, posmodv)
    VCALL_LOCALMEM1R(Vector3, project)
    VCALL_LOCALMEM1R(Vector3, angle_to)
    VCALL_LOCALMEM1R(Vector3, direction_to)
    VCALL_LOCALMEM1R(Vector3, slide)
    VCALL_LOCALMEM1R(Vector3, bounce)
    VCALL_LOCALMEM1R(Vector3, reflect)
    VCALL_LOCALMEM0R(Vector3, sign)

    VCALL_LOCALMEM0R(Plane, normalized)
    VCALL_LOCALMEM0R(Plane, center)
    VCALL_LOCALMEM0R(Plane, get_any_point)
    VCALL_LOCALMEM1R(Plane, is_equal_approx)
    VCALL_LOCALMEM1R(Plane, is_point_over)
    VCALL_LOCALMEM1R(Plane, distance_to)
    VCALL_LOCALMEM2R(Plane, has_point)
    VCALL_LOCALMEM1R(Plane, project)

    //return vector3 if intersected, nil if not
    static void _call_Plane_intersect_3(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        Vector3 result;
        if (reinterpret_cast<Plane *>(p_self._data._mem)->intersect_3(VariantAutoCaster(*p_args[0]), VariantAutoCaster(*p_args[1]), &result))
            r_ret = result;
        else
            r_ret = Variant();
    }

    static void _call_Plane_intersects_ray(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        Vector3 result;
        if (reinterpret_cast<Plane *>(p_self._data._mem)->intersects_ray(VariantAutoCaster(*p_args[0]), VariantAutoCaster(*p_args[1]), &result))
            r_ret = result;
        else
            r_ret = Variant();
    }

    static void _call_Plane_intersects_segment(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        Vector3 result;
        if (reinterpret_cast<Plane *>(p_self._data._mem)->intersects_segment(VariantAutoCaster(*p_args[0]), VariantAutoCaster(*p_args[1]), &result))
            r_ret = result;
        else
            r_ret = Variant();
    }

    VCALL_LOCALMEM0R(Quat, length)
    VCALL_LOCALMEM0R(Quat, length_squared)
    VCALL_LOCALMEM0R(Quat, normalized)
    VCALL_LOCALMEM0R(Quat, is_normalized)
    VCALL_LOCALMEM1R(Quat, is_equal_approx)
    VCALL_LOCALMEM0R(Quat, inverse)
    VCALL_LOCALMEM1R(Quat, dot)
    VCALL_LOCALMEM1R(Quat, xform)
    VCALL_LOCALMEM2R(Quat, slerp)
    VCALL_LOCALMEM2R(Quat, slerpni)
    VCALL_LOCALMEM4R(Quat, cubic_slerp)
    VCALL_LOCALMEM0R(Quat, get_euler)
    VCALL_LOCALMEM1(Quat, set_euler)
    VCALL_LOCALMEM2(Quat, set_axis_angle)

    VCALL_LOCALMEM0R(Color, to_argb32)
    VCALL_LOCALMEM0R(Color, to_abgr32)
    VCALL_LOCALMEM0R(Color, to_rgba32)
    VCALL_LOCALMEM0R(Color, to_argb64)
    VCALL_LOCALMEM0R(Color, to_abgr64)
    VCALL_LOCALMEM0R(Color, to_rgba64)
    VCALL_LOCALMEM0R(Color, get_v)
    VCALL_LOCALMEM0R(Color, inverted)
    VCALL_LOCALMEM0R(Color, contrasted)
    VCALL_LOCALMEM2R(Color, linear_interpolate)
    VCALL_LOCALMEM1R(Color, blend)
    VCALL_LOCALMEM1R(Color, lightened)
    VCALL_LOCALMEM1R(Color, darkened)
    VCALL_LOCALMEM1R(Color, to_html)
    VCALL_LOCALMEM4R(Color, from_hsv)
    VCALL_LOCALMEM1R(Color, is_equal_approx)

    VCALL_LOCALMEM0R(RID, get_id)

    VCALL_LOCALMEM0R(NodePath, is_absolute)
    VCALL_LOCALMEM0R(NodePath, get_name_count)
    VCALL_LOCALMEM1R(NodePath, get_name)
    VCALL_LOCALMEM0R(NodePath, get_subname_count)
    VCALL_LOCALMEM1R(NodePath, get_subname)
    VCALL_LOCALMEM0R(NodePath, get_concatenated_subnames)
    VCALL_LOCALMEM0R(NodePath, get_as_property_path)
    VCALL_LOCALMEM0R(NodePath, is_empty)

    VCALL_LOCALMEM0R(Dictionary, empty)
    VCALL_LOCALMEM0(Dictionary, clear)
    VCALL_LOCALMEM1R(Dictionary, has)
    VCALL_LOCALMEM1R(Dictionary, has_all)
    VCALL_LOCALMEM1R(Dictionary, erase)
    VCALL_LOCALMEM0R(Dictionary, hash)
    VCALL_LOCALMEM0R(Dictionary, keys)
    VCALL_LOCALMEM0R(Dictionary, values)
    VCALL_LOCALMEM2R(Dictionary, get)

    VCALL_LOCALMEM2(Array, set)
    VCALL_LOCALMEM1R(Array, get)
    VCALL_LOCALMEM0R(Array, empty)
    VCALL_LOCALMEM0(Array, clear)
    VCALL_LOCALMEM0R(Array, hash)
    VCALL_LOCALMEM1(Array, push_back)
    VCALL_LOCALMEM1(Array, push_front)
    VCALL_LOCALMEM0R(Array, pop_back)
    VCALL_LOCALMEM0R(Array, pop_front)
    VCALL_LOCALMEM1(Array, append)
    VCALL_LOCALMEM2(Array, insert)
    VCALL_LOCALMEM0R(Array, front)
    VCALL_LOCALMEM0R(Array, back)
    VCALL_LOCALMEM2R(Array, find)
    VCALL_LOCALMEM2R(Array, rfind)
    VCALL_LOCALMEM1R(Array, find_last)
    VCALL_LOCALMEM1R(Array, count)
    VCALL_LOCALMEM1R(Array, contains)
    VCALL_LOCALMEM1(Array, erase)
    VCALL_LOCALMEM0(Array, sort)
    VCALL_LOCALMEM2(Array, sort_custom)
    VCALL_LOCALMEM0(Array, shuffle)
    VCALL_LOCALMEM2R(Array, bsearch)
    VCALL_LOCALMEM4R(Array, bsearch_custom)
    VCALL_LOCALMEM4R(Array, slice)
    VCALL_LOCALMEM0(Array, invert)
    VCALL_LOCALMEM0R(Array, max)
    VCALL_LOCALMEM0R(Array, min)

    static void _call_PoolByteArray_get_string_from_ascii(Variant &r_ret, Variant &p_self, const Variant **p_args) {

        PoolByteArray *ba = reinterpret_cast<PoolByteArray *>(p_self._data._mem);
        String s;
        if (!ba->empty()) {
            PoolByteArray::Read r = ba->read();
            String cs;
            cs.resize(ba->size() + 1);
            memcpy(cs.data(), r.ptr(), ba->size());
            cs[ba->size()] = 0;

            s = cs.data();
        }
        r_ret = s;
    }

    static void _call_PoolByteArray_get_string_from_utf8(Variant &r_ret, Variant &p_self, const Variant **p_args) {

        PoolByteArray *ba = reinterpret_cast<PoolByteArray *>(p_self._data._mem);
        String s;
        if (ba->size() > 0) {
            PoolByteArray::Read r = ba->read();
            s = String((const char *)r.ptr(), ba->size());
        }
        r_ret = s;
    }

    static void _call_PoolByteArray_compress(Variant &r_ret, Variant &p_self, const Variant **p_args) {

        PoolByteArray *ba = reinterpret_cast<PoolByteArray *>(p_self._data._mem);
        PoolByteArray compressed;
        if (ba->size() > 0) {
            Compression::Mode mode = (Compression::Mode)(int)(VariantAutoCaster(*p_args[0]));

            compressed.resize(Compression::get_max_compressed_buffer_size(ba->size(), mode));
            int result = Compression::compress(compressed.write().ptr(), ba->read().ptr(), ba->size(), mode);

            result = result >= 0 ? result : 0;
            compressed.resize(result);
        }
        r_ret = compressed;
    }

    static void _call_PoolByteArray_decompress(Variant &r_ret, Variant &p_self, const Variant **p_args) {

        PoolByteArray *ba = reinterpret_cast<PoolByteArray *>(p_self._data._mem);
        PoolByteArray decompressed;
        Compression::Mode mode = (Compression::Mode)(int)(VariantAutoCaster(*p_args[1]));

        int buffer_size = (int)(VariantAutoCaster(*p_args[0]));

        if (buffer_size <= 0) {
            r_ret = decompressed;
            ERR_FAIL_MSG("Decompression buffer size must be greater than zero.");
        }

        decompressed.resize(buffer_size);
        int result = Compression::decompress(decompressed.write().ptr(), buffer_size, ba->read().ptr(), ba->size(), mode);

        result = result >= 0 ? result : 0;
        decompressed.resize(result);

        r_ret = decompressed;
    }

    static void _call_PoolByteArray_hex_encode(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        PoolByteArray *ba = reinterpret_cast<PoolByteArray *>(p_self._data._mem);
        if (ba->size() == 0) {
            r_ret = String();
            return;
        }
        PoolByteArray::Read r = ba->read();
        String s(StringUtils::hex_encode_buffer(&r[0], ba->size()));
        r_ret = s;
    }

    VCALL_LOCALMEM0R(PoolByteArray, empty)
    VCALL_LOCALMEM2(PoolByteArray, set)
    VCALL_LOCALMEM1R(PoolByteArray, get)
    VCALL_LOCALMEM1(PoolByteArray, push_back)
    VCALL_LOCALMEM2R(PoolByteArray, insert)
    VCALL_LOCALMEM1(PoolByteArray, append)
    VCALL_LOCALMEM1(PoolByteArray, append_array)
    static void _call_PoolByteArray_invert(Variant &r_ret, Variant &p_self, const Variant ** /*p_args*/) {
        invert(*reinterpret_cast<PoolByteArray *>(p_self._data._mem)); }

    VCALL_LOCALMEM2R(PoolByteArray, subarray)

    VCALL_LOCALMEM0R(PoolIntArray, empty)
    VCALL_LOCALMEM2(PoolIntArray, set)
    VCALL_LOCALMEM1R(PoolIntArray, get)
    VCALL_LOCALMEM1(PoolIntArray, push_back)
    VCALL_LOCALMEM2R(PoolIntArray, insert)
    VCALL_LOCALMEM1(PoolIntArray, append)
    VCALL_LOCALMEM1(PoolIntArray, append_array)
    static void _call_PoolIntArray_invert(Variant &r_ret, Variant &p_self, const Variant ** /*p_args*/) {
        invert(*reinterpret_cast<PoolIntArray *>(p_self._data._mem)); }

    VCALL_LOCALMEM0R(PoolRealArray, empty)
    VCALL_LOCALMEM2(PoolRealArray, set)
    VCALL_LOCALMEM1R(PoolRealArray, get)
    VCALL_LOCALMEM1(PoolRealArray, push_back)
    VCALL_LOCALMEM2R(PoolRealArray, insert)
    VCALL_LOCALMEM1(PoolRealArray, append)
    VCALL_LOCALMEM1(PoolRealArray, append_array)
    static void _call_PoolRealArray_invert(Variant &r_ret, Variant &p_self, const Variant ** /*p_args*/) {
        invert(*reinterpret_cast<PoolRealArray *>(p_self._data._mem)); }

    VCALL_LOCALMEM0R(PoolStringArray, empty)
    VCALL_LOCALMEM2(PoolStringArray, set)
    VCALL_LOCALMEM1R(PoolStringArray, get)
    VCALL_LOCALMEM1(PoolStringArray, push_back)
    VCALL_LOCALMEM2R(PoolStringArray, insert)
    VCALL_LOCALMEM1(PoolStringArray, append)
    VCALL_LOCALMEM1(PoolStringArray, append_array)
    static void _call_PoolStringArray_invert(Variant &r_ret, Variant &p_self, const Variant ** /*p_args*/) {
        invert(*reinterpret_cast<PoolStringArray *>(p_self._data._mem)); }

    static void _call_PoolStringArray_join(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        const PoolStringArray &lhs(*reinterpret_cast<PoolStringArray *>(p_self._data._mem));
        String delimiter(p_args[0]->as<String>());
        String rs;
        int s = lhs.size();
        auto r = lhs.read();
        for (int i = 0; i < s; i++) {
            rs += r[i] + delimiter;
        }
        StringUtils::erase(rs,rs.length() - delimiter.length(), delimiter.length());
        r_ret = rs;
    }

    VCALL_LOCALMEM0R(PoolVector2Array, empty)
    VCALL_LOCALMEM2(PoolVector2Array, set)
    VCALL_LOCALMEM1R(PoolVector2Array, get)
    VCALL_LOCALMEM1(PoolVector2Array, push_back)
    VCALL_LOCALMEM2R(PoolVector2Array, insert)
    VCALL_LOCALMEM1(PoolVector2Array, append)
    VCALL_LOCALMEM1(PoolVector2Array, append_array)
    static void _call_PoolVector2Array_invert(Variant &r_ret, Variant &p_self, const Variant ** /*p_args*/) {
        invert(*reinterpret_cast<PoolVector2Array *>(p_self._data._mem)); }

    VCALL_LOCALMEM0R(PoolVector3Array, empty)
    VCALL_LOCALMEM2(PoolVector3Array, set)
    VCALL_LOCALMEM1R(PoolVector3Array, get)
    VCALL_LOCALMEM1(PoolVector3Array, push_back)
    VCALL_LOCALMEM2R(PoolVector3Array, insert)
    VCALL_LOCALMEM1(PoolVector3Array, append)
    VCALL_LOCALMEM1(PoolVector3Array, append_array)
    static void _call_PoolVector3Array_invert(Variant &r_ret, Variant &p_self, const Variant ** /*p_args*/) {
        invert(*reinterpret_cast<PoolVector3Array *>(p_self._data._mem)); }

    VCALL_LOCALMEM0R(PoolColorArray, empty)
    VCALL_LOCALMEM2(PoolColorArray, set)
    VCALL_LOCALMEM1R(PoolColorArray, get)
    VCALL_LOCALMEM1(PoolColorArray, push_back)
    VCALL_LOCALMEM2R(PoolColorArray, insert)
    VCALL_LOCALMEM1(PoolColorArray, append)
    VCALL_LOCALMEM1(PoolColorArray, append_array)
    static void _call_PoolColorArray_invert(Variant &r_ret, Variant &p_self, const Variant ** /*p_args*/) {
        invert(*reinterpret_cast<PoolColorArray *>(p_self._data._mem)); }

#define VCALL_PTR0(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { reinterpret_cast<m_type *>(p_self._data._ptr)->m_method(); }
#define VCALL_PTR0R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { r_ret = reinterpret_cast<m_type *>(p_self._data._ptr)->m_method(); }
#define VCALL_PTR1(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { reinterpret_cast<m_type *>(p_self._data._ptr)->m_method(VariantAutoCaster(*p_args[0])); }
#define VCALL_PTR1R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { r_ret = reinterpret_cast<m_type *>(p_self._data._ptr)->m_method(VariantAutoCaster(*p_args[0])); }
#define VCALL_PTR2(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { reinterpret_cast<m_type *>(p_self._data._ptr)->m_method(VariantAutoCaster(*p_args[0]), VariantAutoCaster(*p_args[1])); }
#define VCALL_PTR2R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { r_ret = reinterpret_cast<m_type *>(p_self._data._ptr)->m_method(VariantAutoCaster(*p_args[0]), VariantAutoCaster(*p_args[1])); }
#define VCALL_PTR3(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { reinterpret_cast<m_type *>(p_self._data._ptr)->m_method(VariantAutoCaster(*p_args[0]), VariantAutoCaster(*p_args[1]), VariantAutoCaster(*p_args[2])); }
#define VCALL_PTR3R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { r_ret = reinterpret_cast<m_type *>(p_self._data._ptr)->m_method(VariantAutoCaster(*p_args[0]), VariantAutoCaster(*p_args[1]), VariantAutoCaster(*p_args[2])); }
#define VCALL_PTR4(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { reinterpret_cast<m_type *>(p_self._data._ptr)->m_method(VariantAutoCaster(*p_args[0]), VariantAutoCaster(*p_args[1]), VariantAutoCaster(*p_args[2]), VariantAutoCaster(*p_args[3])); }
#define VCALL_PTR4R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { r_ret = reinterpret_cast<m_type *>(p_self._data._ptr)->m_method(VariantAutoCaster(*p_args[0]), VariantAutoCaster(*p_args[1]), VariantAutoCaster(*p_args[2]), VariantAutoCaster(*p_args[3])); }
#define VCALL_PTR5(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { reinterpret_cast<m_type *>(p_self._data._ptr)->m_method(VariantAutoCaster(*p_args[0]), VariantAutoCaster(*p_args[1]), VariantAutoCaster(*p_args[2]), VariantAutoCaster(*p_args[3]), VariantAutoCaster(*p_args[4])); }
#define VCALL_PTR5R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { r_ret = reinterpret_cast<m_type *>(p_self._data._ptr)->m_method(VariantAutoCaster(*p_args[0]), VariantAutoCaster(*p_args[1]), VariantAutoCaster(*p_args[2]), VariantAutoCaster(*p_args[3]), VariantAutoCaster(*p_args[4])); }

    VCALL_PTR0R(AABB, abs)
    VCALL_PTR0R(AABB, get_area)
    VCALL_PTR0R(AABB, has_no_area)
    VCALL_PTR0R(AABB, has_no_surface)
    VCALL_PTR1R(AABB, is_equal_approx)
    VCALL_PTR1R(AABB, intersects)
    VCALL_PTR1R(AABB, encloses)
    VCALL_PTR1R(AABB, merge)
    VCALL_PTR1R(AABB, intersection)
    VCALL_PTR1R(AABB, intersects_plane)
    VCALL_PTR2R(AABB, intersects_segment)
    VCALL_PTR1R(AABB, has_point)
    VCALL_PTR1R(AABB, get_support)
    VCALL_PTR0R(AABB, get_longest_axis)
    VCALL_PTR0R(AABB, get_longest_axis_index)
    VCALL_PTR0R(AABB, get_longest_axis_size)
    VCALL_PTR0R(AABB, get_shortest_axis)
    VCALL_PTR0R(AABB, get_shortest_axis_index)
    VCALL_PTR0R(AABB, get_shortest_axis_size)
    VCALL_PTR1R(AABB, expand)
    VCALL_PTR1R(AABB, grow)
    VCALL_PTR1R(AABB, get_endpoint)

    VCALL_PTR0R(Transform2D, inverse)
    VCALL_PTR0R(Transform2D, affine_inverse)
    VCALL_PTR0R(Transform2D, get_rotation)
    VCALL_PTR0R(Transform2D, get_origin)
    VCALL_PTR0R(Transform2D, get_scale)
    VCALL_PTR0R(Transform2D, orthonormalized)
    VCALL_PTR1R(Transform2D, rotated)
    VCALL_PTR1R(Transform2D, scaled)
    VCALL_PTR1R(Transform2D, translated)
    VCALL_PTR2R(Transform2D, interpolate_with)
    VCALL_PTR1R(Transform2D, is_equal_approx)

    static void _call_Transform2D_xform(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        Transform2D *trn = reinterpret_cast<Transform2D *>(p_self._data._ptr);
        switch (p_args[0]->type) {

            case VariantType::VECTOR2: r_ret = trn->xform(p_args[0]->as<Vector2>()); return;
            case VariantType::RECT2: r_ret = trn->xform(p_args[0]->as<Rect2>()); return;
            case VariantType::POOL_VECTOR2_ARRAY:
            {
                PoolVector2Array v = p_args[0]->as<PoolVector2Array>();

                trn->xform(v.write().ptr(),v.size());
                r_ret = Variant(v);
                return;
            }
            default: r_ret = Variant();
        }
    }

    static void _call_Transform2D_xform_inv(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        Transform2D *trn = reinterpret_cast<Transform2D *>(p_self._data._ptr);
        switch (p_args[0]->type) {

            case VariantType::VECTOR2: r_ret = trn->xform_inv(p_args[0]->as<Vector2>()); return;
            case VariantType::RECT2: r_ret = trn->xform_inv(p_args[0]->as<Rect2>()); return;
            case VariantType::POOL_VECTOR2_ARRAY:
            {
                PoolVector2Array v = p_args[0]->as<PoolVector2Array>();

                trn->xform_inv(v.write().ptr(),v.size());
                r_ret = Variant(v);
                return;
            }
            default: r_ret = Variant();
        }
    }

    static void _call_Transform2D_basis_xform(Variant &r_ret, Variant &p_self, const Variant **p_args) {

        switch (p_args[0]->type) {

            case VariantType::VECTOR2: r_ret = reinterpret_cast<Transform2D *>(p_self._data._ptr)->basis_xform(p_args[0]->as<Vector2>()); return;
            default: r_ret = Variant();
        }
    }

    static void _call_Transform2D_basis_xform_inv(Variant &r_ret, Variant &p_self, const Variant **p_args) {

        switch (p_args[0]->type) {

            case VariantType::VECTOR2: r_ret = reinterpret_cast<Transform2D *>(p_self._data._ptr)->basis_xform_inv(p_args[0]->as<Vector2>()); return;
            default: r_ret = Variant();
        }
    }

    VCALL_PTR0R(Basis, inverse)
    VCALL_PTR0R(Basis, transposed)
    VCALL_PTR0R(Basis, determinant)
    VCALL_PTR2R(Basis, rotated)
    VCALL_PTR1R(Basis, scaled)
    VCALL_PTR0R(Basis, get_scale)
    VCALL_PTR0R(Basis, get_euler)
    VCALL_PTR1R(Basis, tdotx)
    VCALL_PTR1R(Basis, tdoty)
    VCALL_PTR1R(Basis, tdotz)
    VCALL_PTR1R(Basis, xform)
    VCALL_PTR1R(Basis, xform_inv)
    VCALL_PTR0R(Basis, get_orthogonal_index)
    VCALL_PTR0R(Basis, orthonormalized)
    VCALL_PTR2R(Basis, slerp)
    VCALL_PTR2R(Basis, is_equal_approx)
    VCALL_PTR0R(Basis, get_rotation_quat)

    VCALL_PTR0R(Transform, inverse)
    VCALL_PTR0R(Transform, affine_inverse)
    VCALL_PTR2R(Transform, rotated)
    VCALL_PTR1R(Transform, scaled)
    VCALL_PTR1R(Transform, translated)
    VCALL_PTR0R(Transform, orthonormalized)
    VCALL_PTR2R(Transform, looking_at)
    VCALL_PTR2R(Transform, interpolate_with)
    VCALL_PTR1R(Transform, is_equal_approx)
    static void _call_Transform_xform(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        Transform *trn = reinterpret_cast<Transform *>(p_self._data._ptr);
        switch (p_args[0]->type) {

            case VariantType::VECTOR3: r_ret = trn->xform(p_args[0]->as<Vector3>()); return;
            case VariantType::PLANE: r_ret = trn->xform(p_args[0]->as<Plane>()); return;
            case VariantType::AABB: r_ret = trn->xform(p_args[0]->as<::AABB>()); return;
            case VariantType::POOL_VECTOR3_ARRAY:
            {
                PoolVector3Array v = p_args[0]->as<PoolVector3Array>();

                trn->xform(v.write().ptr(),v.size());
                r_ret = v;
                return;
            }
            default: r_ret = Variant();
        }
    }

    static void _call_Transform_xform_inv(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        Transform *trn = reinterpret_cast<Transform *>(p_self._data._ptr);

        switch (p_args[0]->type) {

            case VariantType::VECTOR3: r_ret = trn->xform_inv(p_args[0]->as<Vector3>()); return;
            case VariantType::PLANE: r_ret = trn->xform_inv(p_args[0]->as<Plane>()); return;
            case VariantType::AABB: r_ret = trn->xform_inv(p_args[0]->as<::AABB>()); return;
            case VariantType::POOL_VECTOR3_ARRAY:
            {
                PoolVector3Array v = p_args[0]->as<PoolVector3Array>();

                trn->xform_inv(v.write().ptr(),v.size());
                r_ret = v;
                return;
            }
            default: r_ret = Variant();
        }
    }

    /*
    VCALL_PTR0( Transform, invert )
    VCALL_PTR0( Transform, affine_invert )
    VCALL_PTR2( Transform, rotate )
    VCALL_PTR1( Transform, scale )
    VCALL_PTR1( Transform, translate )
    VCALL_PTR0( Transform, orthonormalize ) */

    struct ConstructData {

        int arg_count;
        Vector<VariantType> arg_types;
        Vector<String> arg_names;
        VariantConstructFunc func;
    };

    struct ConstructFunc {

        Vector<ConstructData> constructors;
    };

    static ConstructFunc *construct_funcs;

    static void Vector2_init1(Variant &r_ret, const Variant **p_args) {

        r_ret = Vector2(VariantAutoCaster(*p_args[0]), VariantAutoCaster(*p_args[1]));
    }

    static void Rect2_init1(Variant &r_ret, const Variant **p_args) {

        r_ret = Rect2(VariantAutoCaster(*p_args[0]), VariantAutoCaster(*p_args[1]));
    }

    static void Rect2_init2(Variant &r_ret, const Variant **p_args) {

        r_ret = Rect2(VariantAutoCaster(*p_args[0]), VariantAutoCaster(*p_args[1]), VariantAutoCaster(*p_args[2]), VariantAutoCaster(*p_args[3]));
    }

    static void Transform2D_init2(Variant &r_ret, const Variant **p_args) {

        Transform2D m(p_args[0]->as<float>(), p_args[1]->as<Vector2>());
        r_ret = m;
    }

    static void Transform2D_init3(Variant &r_ret, const Variant **p_args) {

        Transform2D m;
        m[0] = p_args[0]->as<Vector2>();
        m[1] = p_args[1]->as<Vector2>();
        m[2] = p_args[2]->as<Vector2>();
        r_ret = m;
    }

    static void Vector3_init1(Variant &r_ret, const Variant **p_args) {

        r_ret = Vector3(VariantAutoCaster(*p_args[0]), VariantAutoCaster(*p_args[1]), VariantAutoCaster(*p_args[2]));
    }

    static void Plane_init1(Variant &r_ret, const Variant **p_args) {

        r_ret = Plane(p_args[0]->as<float>(), VariantAutoCaster(*p_args[1]), VariantAutoCaster(*p_args[2]), VariantAutoCaster(*p_args[3]));
    }

    static void Plane_init2(Variant &r_ret, const Variant **p_args) {

        r_ret = Plane(VariantAutoCaster(*p_args[0]), VariantAutoCaster(*p_args[1]), VariantAutoCaster(*p_args[2]));
    }

    static void Plane_init3(Variant &r_ret, const Variant **p_args) {

        r_ret = Plane(p_args[0]->as<Vector3>(), p_args[1]->as<real_t>());
    }
    static void Plane_init4(Variant &r_ret, const Variant **p_args) {

        r_ret = Plane(p_args[0]->as<Vector3>(), p_args[1]->as<Vector3>());
    }

    static void Quat_init1(Variant &r_ret, const Variant **p_args) {

        r_ret = Quat(VariantAutoCaster(*p_args[0]), VariantAutoCaster(*p_args[1]), VariantAutoCaster(*p_args[2]), VariantAutoCaster(*p_args[3]));
    }

    static void Quat_init2(Variant &r_ret, const Variant **p_args) {

        r_ret = Quat(p_args[0]->as<Vector3>(), p_args[1]->as<float>());
    }

    static void Quat_init3(Variant &r_ret, const Variant **p_args) {

        r_ret = Quat(p_args[0]->as<Vector3>());
    }

    static void Color_init1(Variant &r_ret, const Variant **p_args) {

        r_ret = Color(VariantAutoCaster(*p_args[0]), VariantAutoCaster(*p_args[1]), VariantAutoCaster(*p_args[2]), VariantAutoCaster(*p_args[3]));
    }

    static void Color_init2(Variant &r_ret, const Variant **p_args) {

        r_ret = Color(VariantAutoCaster(*p_args[0]), VariantAutoCaster(*p_args[1]), VariantAutoCaster(*p_args[2]));
    }

    static void Color_init3(Variant &r_ret, const Variant **p_args) {

        r_ret = Color::html(p_args[0]->as<String>());
    }

    static void Color_init4(Variant &r_ret, const Variant **p_args) {

        r_ret = Color::hex(VariantAutoCaster(*p_args[0]));
    }

    static void AABB_init1(Variant &r_ret, const Variant **p_args) {

        r_ret = ::AABB(VariantAutoCaster(*p_args[0]), VariantAutoCaster(*p_args[1]));
    }

    static void Basis_init1(Variant &r_ret, const Variant **p_args) {

        Basis m;
        m.set_axis(0, VariantAutoCaster(*p_args[0]));
        m.set_axis(1, VariantAutoCaster(*p_args[1]));
        m.set_axis(2, VariantAutoCaster(*p_args[2]));
        r_ret = m;
    }

    static void Basis_init2(Variant &r_ret, const Variant **p_args) {

        r_ret = Basis(p_args[0]->as<Vector3>(), p_args[1]->as<real_t>());
    }

    static void Transform_init1(Variant &r_ret, const Variant **p_args) {

        Transform t;
        t.basis.set_axis(0, VariantAutoCaster(*p_args[0]));
        t.basis.set_axis(1, VariantAutoCaster(*p_args[1]));
        t.basis.set_axis(2, VariantAutoCaster(*p_args[2]));
        t.origin = p_args[3]->as<Vector3>();
        r_ret = t;
    }

    static void Transform_init2(Variant &r_ret, const Variant **p_args) {

        r_ret = Transform(p_args[0]->as<Basis>(), p_args[1]->as<Vector3>());
    }

    static void add_constructor(VariantConstructFunc p_func, const VariantType p_type,
            const char *p_name1 = nullptr, const VariantType p_type1 = VariantType::NIL,
            const char *p_name2 = nullptr, const VariantType p_type2 = VariantType::NIL,
            const char *p_name3 = nullptr, const VariantType p_type3 = VariantType::NIL,
            const char *p_name4 = nullptr, const VariantType p_type4 = VariantType::NIL) {

        ConstructData cd;
        cd.func = p_func;
        cd.arg_count = 0;

        if (nullptr==p_name1)
            goto end;
        cd.arg_count++;
        cd.arg_names.push_back((p_name1));
        cd.arg_types.push_back(p_type1);

        if (nullptr==p_name2)
            goto end;
        cd.arg_count++;
        cd.arg_names.push_back((p_name2));
        cd.arg_types.push_back(p_type2);

        if (nullptr==p_name3)
            goto end;
        cd.arg_count++;
        cd.arg_names.push_back((p_name3));
        cd.arg_types.push_back(p_type3);

        if (nullptr==p_name4)
            goto end;
        cd.arg_count++;
        cd.arg_names.push_back((p_name4));
        cd.arg_types.push_back(p_type4);

    end:

        construct_funcs[(int)p_type].constructors.emplace_back(cd);
    }

    struct ConstantData {

        HashMap<StringName, int> value;
#ifdef DEBUG_ENABLED
        List<StringName> value_ordered;
#endif
        HashMap<StringName, Variant> variant_value;
    };

    static ConstantData *constant_data;

    static void add_constant(VariantType p_type, const StringName &p_constant_name, int p_constant_value) {

        constant_data[int8_t(p_type)].value[p_constant_name] = p_constant_value;
#ifdef DEBUG_ENABLED
        constant_data[int8_t(p_type)].value_ordered.push_back(p_constant_name);
#endif
    }

    static void add_variant_constant(VariantType p_type, const StringName &p_constant_name, const Variant &p_constant_value) {

        constant_data[int8_t(p_type)].variant_value[p_constant_name] = p_constant_value;
    }
};

_VariantCall::TypeFunc *_VariantCall::type_funcs = nullptr;
_VariantCall::ConstructFunc *_VariantCall::construct_funcs = nullptr;
_VariantCall::ConstantData *_VariantCall::constant_data = nullptr;

/*Variant Variant::call(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {

    Variant ret;
    if (type == VariantType::OBJECT) {
        //call object
        Object* obj = _OBJ_PTR(*this);
        if (!obj) {
#ifdef DEBUG_ENABLED
            if (ScriptDebugger::get_singleton() && _get_obj().rc && !gObjectDB().get_instance(_get_obj().rc->instance_id)) {
                WARN_PRINT("Attempted call on a deleted object.");
            }
#endif
            r_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
            return ret;
        }

        ret = obj->call(p_method, p_args, p_argcount, r_error);

        //else if (type==VariantType::METHOD) {

    }
    else {

        r_error.error = Callable::CallError::CALL_OK;

        auto E = _VariantCall::type_funcs[(int)type].functions.find(p_method);
#ifdef DEBUG_ENABLED
        if (E == _VariantCall::type_funcs[(int)type].functions.end()) {
            r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
            return ret;
        }
#endif
        _VariantCall::FuncData& funcdata = E->second;
        funcdata.call(ret, *this, p_args, p_argcount, r_error);
    }

    if (r_error.error != Callable::CallError::CALL_OK)
        return Variant();
  return ret;
}*/

#define VCALL(m_type, m_method) _VariantCall::_call_##m_type##_##m_method

Variant Variant::construct(const VariantType p_type, const Variant **p_args, int p_argcount, Callable::CallError &r_error, bool p_strict) {

    r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
    ERR_FAIL_INDEX_V(int(p_type), int(VariantType::VARIANT_MAX), Variant());

    r_error.error = Callable::CallError::CALL_OK;
    if (p_argcount == 0) { //generic construct

        switch (p_type) {
            case VariantType::NIL:
                return Variant();

            // atomic types
            case VariantType::BOOL: return Variant(false);
            case VariantType::INT: return 0;
            case VariantType::FLOAT: return 0.0f;
            case VariantType::STRING:
                return String();

            // math types
            case VariantType::VECTOR2:
                return Vector2(); // 5
            case VariantType::RECT2: return Rect2();
            case VariantType::VECTOR3: return Vector3();
            case VariantType::TRANSFORM2D: return Transform2D();
            case VariantType::PLANE: return Plane();
            case VariantType::QUAT: return Quat();
            case VariantType::AABB:
                return ::AABB(); // 10
            case VariantType::BASIS: return Basis();
            case VariantType::TRANSFORM:
                return Transform();

            // misc types
            case VariantType::COLOR: return Color();
            case VariantType::NODE_PATH:
                return NodePath(); // 15
            case VariantType::_RID: return RID();
            case VariantType::OBJECT: return Variant((Object *)nullptr);
            case VariantType::DICTIONARY: return Dictionary();
            case VariantType::ARRAY:
                return Array(); // 20
            case VariantType::POOL_BYTE_ARRAY: return PoolByteArray();
            case VariantType::POOL_INT_ARRAY: return PoolIntArray();
            case VariantType::POOL_REAL_ARRAY: return PoolRealArray();
            case VariantType::POOL_STRING_ARRAY: return PoolStringArray();
            case VariantType::POOL_VECTOR2_ARRAY:
                return Variant(PoolVector2Array()); // 25
            case VariantType::POOL_VECTOR3_ARRAY: return PoolVector3Array();
            case VariantType::POOL_COLOR_ARRAY: return PoolColorArray();
            default: return Variant();
        }

    } else if (p_argcount == 1 && p_args[0]->type == p_type) {
        return *p_args[0]; //copy construct
    } else if (p_argcount == 1 && (!p_strict || Variant::can_convert(p_args[0]->type, p_type))) {
        //near match construct

        switch (p_type) {
            case VariantType::NIL: {

                return Variant();
            }
            case VariantType::BOOL: {
                return Variant(p_args[0]->as<bool>());
            }
            case VariantType::INT: {
                return p_args[0]->as<int64_t>();
            }
            case VariantType::FLOAT: {
                return p_args[0]->as<real_t>();
            }
            case VariantType::STRING: {
                return p_args[0]->as<String>();
            }
            case VariantType::VECTOR2: {
                return p_args[0]->as<Vector2>();
            }
            case VariantType::RECT2: return (p_args[0]->as<Rect2>());
            case VariantType::VECTOR3: return (p_args[0]->as<Vector3>());
            case VariantType::TRANSFORM2D:
                return (Transform2D(p_args[0]->as<Transform2D>()));

            case VariantType::PLANE: return (p_args[0]->as<Plane>());
            case VariantType::QUAT: return (p_args[0]->as<Quat>());
            case VariantType::AABB:
                return (p_args[0]->as<::AABB>()); // 10
            case VariantType::BASIS: return (Basis(p_args[0]->as<Basis>()));
            case VariantType::TRANSFORM:
                return (Transform(p_args[0]->as<Transform>()));

            // misc types
            case VariantType::COLOR: return p_args[0]->type == VariantType::STRING ? Color::html(p_args[0]->as<String>()) : Color::hex(VariantAutoCaster(*p_args[0]));
            case VariantType::NODE_PATH:
                return (NodePath(p_args[0]->as<NodePath>())); // 15
            case VariantType::_RID: return (p_args[0]->as<RID>());
            case VariantType::OBJECT: return Variant((Object *)(p_args[0]->as<Object *>()));
            case VariantType::DICTIONARY: return p_args[0]->as<Dictionary>();
            case VariantType::ARRAY:
                return p_args[0]->as<Array>(); // 20

            // arrays
            case VariantType::POOL_BYTE_ARRAY: return (p_args[0]->as<PoolByteArray>());
            case VariantType::POOL_INT_ARRAY: return (p_args[0]->as<PoolIntArray>());
            case VariantType::POOL_REAL_ARRAY: return (p_args[0]->as<PoolRealArray>());
            case VariantType::POOL_STRING_ARRAY: return (p_args[0]->as<PoolStringArray>());
            case VariantType::POOL_VECTOR2_ARRAY:
                return Variant(p_args[0]->as<PoolVector2Array>()); // 25
            case VariantType::POOL_VECTOR3_ARRAY: return (p_args[0]->as<PoolVector3Array>());
            case VariantType::POOL_COLOR_ARRAY: return (p_args[0]->as<PoolColorArray>());
            default: return Variant();
        }
    } else if (p_argcount >= 1) {

        _VariantCall::ConstructFunc &c = _VariantCall::construct_funcs[int(p_type)];

        for (const _VariantCall::ConstructData &cd : c.constructors) {
            if (cd.arg_count != p_argcount)
                continue;

            //validate parameters
            for (int i = 0; i < cd.arg_count; i++) {
                if (!Variant::can_convert(p_args[i]->type, cd.arg_types[i])) {
                    r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT; //no such constructor
                    r_error.argument = i;
                    r_error.expected = cd.arg_types[i];
                    return Variant();
                }
            }

            Variant v;
            cd.func(v, p_args);
            return v;
        }
    }
    r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD; //no such constructor
    return Variant();
}

/*
bool Variant::has_method(const StringName &p_method) const {

    if (type == VariantType::OBJECT) {
        Object *obj = _OBJ_PTR(*this);
        if (!obj) {
#ifdef DEBUG_ENABLED
            if (ScriptDebugger::get_singleton() && _get_obj().rc && !gObjectDB().get_instance(_get_obj().rc->instance_id)) {
                WARN_PRINT("Attempted method check on a deleted object.");
            }
#endif
            return false;
        }
        return obj->has_method(p_method);
    }

    const _VariantCall::TypeFunc &tf = _VariantCall::type_funcs[(int)type];
    return tf.functions.contains(p_method);
}

Span<const VariantType> Variant::get_method_argument_types(VariantType p_type, const StringName &p_method) {

    const _VariantCall::TypeFunc &tf = _VariantCall::type_funcs[(int)p_type];

    auto E = tf.functions.find(p_method);
    if (E==tf.functions.end())
        return {};

    return E->second.arg_types;
}

bool Variant::is_method_const(VariantType p_type, const StringName &p_method) {

    const _VariantCall::TypeFunc &tf = _VariantCall::type_funcs[(int)p_type];

    auto E = tf.functions.find(p_method);
    if (E==tf.functions.end())
        return false;

    return E->second._const;
}
*/

/*
Span<const StringView> Variant::get_method_argument_names(VariantType p_type, const StringName &p_method) {

    const _VariantCall::TypeFunc &tf = _VariantCall::type_funcs[(int)p_type];

    auto E = tf.functions.find(p_method);
    if (E==tf.functions.end())
        return {};

    return Span<const StringView>(E->second.arg_names, ptrdiff_t(E->second.arg_count));
}

VariantType Variant::get_method_return_type(VariantType p_type, const StringName &p_method, bool *r_has_return) {

    const _VariantCall::TypeFunc &tf = _VariantCall::type_funcs[(int)p_type];

    auto E = tf.functions.find(p_method);
    if (E==tf.functions.end())
        return VariantType::NIL;

    if (r_has_return)
        *r_has_return = E->second.returns;

    return E->second.return_type;
}
static const Vector<Variant> s_empty;

Span<const Variant> Variant::get_method_default_arguments(VariantType p_type, const StringName &p_method) {
    const _VariantCall::TypeFunc &tf = _VariantCall::type_funcs[(int)p_type];

    auto E = tf.functions.find(p_method);
    if (E==tf.functions.end())
        return {};

    return E->second.default_args;
}

void Variant::get_method_list(Vector<MethodInfo> *p_list) const {

    const _VariantCall::TypeFunc &tf = _VariantCall::type_funcs[(int)type];

    for (const eastl::pair<const StringName,_VariantCall::FuncData> &E : tf.functions) {

        const _VariantCall::FuncData &fd = E.second;

        MethodInfo mi;
        mi.name = E.first;

        if (fd._const) {
            mi.flags |= METHOD_FLAG_CONST;
        }

        for (int i = 0; i < fd.arg_count; i++) {

            PropertyInfo pi;
            pi.type = fd.arg_types[i];
#ifdef DEBUG_ENABLED
            pi.name = StaticCString(fd.arg_names[i].data(),true);
#endif
            mi.arguments.emplace_back(eastl::move(pi));
        }

        mi.default_arguments.assign(fd.default_args, fd.default_args+fd.def_count);
        PropertyInfo ret;
#ifdef DEBUG_ENABLED
        ret.type = fd.return_type;
        if (fd.returns)
            ret.name = "ret";
        mi.return_val = ret;
#endif

        p_list->push_back(mi);
    }
}
*/

void Variant::get_constructor_list(VariantType p_type, Vector<MethodInfo> *p_list) {

    ERR_FAIL_INDEX(int(p_type), int(VariantType::VARIANT_MAX));

    //custom constructors
    for (const _VariantCall::ConstructData &cd : _VariantCall::construct_funcs[(int)p_type].constructors) {

        MethodInfo mi;
        mi.name = Variant::interned_type_name(p_type);
        mi.return_val.type = p_type;
        for (int i = 0; i < cd.arg_count; i++) {

            PropertyInfo pi;
            pi.name = StringName(cd.arg_names[i]);
            pi.type = cd.arg_types[i];
            mi.arguments.emplace_back(eastl::move(pi));
        }
        p_list->push_back(mi);
    }
    //default constructors
    for (int i = 0; i < int(VariantType::VARIANT_MAX); i++) {
        if (i == int(p_type))
            continue;
        if (!Variant::can_convert(VariantType(i), p_type))
            continue;

        MethodInfo mi(p_type);
        mi.name = eastl::move(Variant::interned_type_name(p_type));
        PropertyInfo pi;
        pi.name = "from";
        pi.type = VariantType(i);
        mi.arguments.emplace_back(eastl::move(pi));
        p_list->emplace_back(eastl::move(mi));
    }
}

void Variant::get_constants_for_type(VariantType p_type, Vector<StringName> *p_constants) {

    ERR_FAIL_INDEX((int)p_type, (int)VariantType::VARIANT_MAX);

    _VariantCall::ConstantData &cd = _VariantCall::constant_data[(int)p_type];

#ifdef DEBUG_ENABLED
    for (const StringName &E : cd.value_ordered) {

        p_constants->push_back(E);
#else
    for (const auto &E : cd.value) {
        p_constants->push_back(E.first);
#endif
    }

    for (eastl::pair<const StringName,Variant> &E : cd.variant_value) {

        p_constants->push_back(E.first);
    }
}

bool Variant::has_constant(VariantType p_type, const StringName &p_value) {

    ERR_FAIL_INDEX_V((int)p_type, (int)VariantType::VARIANT_MAX, false);
    _VariantCall::ConstantData &cd = _VariantCall::constant_data[(int)p_type];
    return cd.value.contains(p_value) || cd.variant_value.contains(p_value);
}

Variant Variant::get_constant_value(VariantType p_type, const StringName &p_value, bool *r_valid) {

    if (r_valid)
        *r_valid = false;

    ERR_FAIL_INDEX_V((int)p_type, (int)VariantType::VARIANT_MAX, 0);
    _VariantCall::ConstantData &cd = _VariantCall::constant_data[(int)p_type];

    auto E = cd.value.find(p_value);
    if (E==cd.value.end()) {
        HashMap<StringName, Variant>::iterator F = cd.variant_value.find(p_value);
        if (F!=cd.variant_value.end()) {
            if (r_valid)
                *r_valid = true;
            return F->second;
        }
        return -1;
    }
    if (r_valid)
        *r_valid = true;

    return E->second;
}
void register_variant_methods() {

    _VariantCall::type_funcs = memnew_arr(_VariantCall::TypeFunc, int(VariantType::VARIANT_MAX));

    _VariantCall::construct_funcs = memnew_arr(_VariantCall::ConstructFunc, int(VariantType::VARIANT_MAX));
    _VariantCall::constant_data = memnew_arr(_VariantCall::ConstantData, int(VariantType::VARIANT_MAX));

#define ADDFUNCSR(m_vtype, m_ret, m_class, m_method,m_arg_span, ...) \
    _VariantCall::addfunc_span(true, VariantType::m_vtype, VariantType::m_ret, true, StringName(#m_method), VCALL(m_class, m_method), {__VA_ARGS__},m_arg_span);

#define ADDFUNC0R(m_vtype, m_ret, m_class, m_method) \
    _VariantCall::addfunc(true, VariantType::m_vtype, VariantType::m_ret, true, StringName(#m_method), VCALL(m_class, m_method), {});
#define ADDFUNC1R(m_vtype, m_ret, m_class, m_method, m_arg1, m_argname1, ...) \
    _VariantCall::addfunc(true, VariantType::m_vtype, VariantType::m_ret, true, StringName(#m_method), VCALL(m_class, m_method), {__VA_ARGS__}, _VariantCall::Arg(VariantType::m_arg1, m_argname1));
#define ADDFUNC2R(m_vtype, m_ret, m_class, m_method, m_arg1, m_argname1, m_arg2, m_argname2, ...) \
    _VariantCall::addfunc(true, VariantType::m_vtype, VariantType::m_ret, true, StringName(#m_method), VCALL(m_class, m_method), {__VA_ARGS__}, _VariantCall::Arg(VariantType::m_arg1, m_argname1), _VariantCall::Arg(VariantType::m_arg2, (m_argname2)));
#define ADDFUNC3R(m_vtype, m_ret, m_class, m_method, m_arg1, m_argname1, m_arg2, m_argname2, m_arg3, m_argname3, ...) \
    _VariantCall::addfunc(true, VariantType::m_vtype, VariantType::m_ret, true, StringName(#m_method), VCALL(m_class, m_method), {__VA_ARGS__}, _VariantCall::Arg(VariantType::m_arg1, m_argname1), _VariantCall::Arg(VariantType::m_arg2, (m_argname2)), _VariantCall::Arg(VariantType::m_arg3, (m_argname3)));
#define ADDFUNC4R(m_vtype, m_ret, m_class, m_method, m_arg1, m_argname1, m_arg2, m_argname2, m_arg3, m_argname3, m_arg4, m_argname4, ...) \
    _VariantCall::addfunc(true, VariantType::m_vtype, VariantType::m_ret, true, StringName(#m_method), VCALL(m_class, m_method), {__VA_ARGS__}, _VariantCall::Arg(VariantType::m_arg1, m_argname1), _VariantCall::Arg(VariantType::m_arg2, (m_argname2)), _VariantCall::Arg(VariantType::m_arg3, (m_argname3)), _VariantCall::Arg(VariantType::m_arg4, (m_argname4)));

#define ADDFUNC0RNC(m_vtype, m_ret, m_class, m_method) \
    _VariantCall::addfunc(false, VariantType::m_vtype, VariantType::m_ret, true, StringName(#m_method), VCALL(m_class, m_method), {});
#define ADDFUNC1RNC(m_vtype, m_ret, m_class, m_method, m_arg1, m_argname1, ...) \
    _VariantCall::addfunc(false, VariantType::m_vtype, VariantType::m_ret, true, StringName(#m_method), VCALL(m_class, m_method), {__VA_ARGS__}, _VariantCall::Arg(VariantType::m_arg1, (m_argname1)));
#define ADDFUNC2RNC(m_vtype, m_ret, m_class, m_method, m_arg1, m_argname1, m_arg2, m_argname2, ...) \
    _VariantCall::addfunc(false, VariantType::m_vtype, VariantType::m_ret, true, StringName(#m_method), VCALL(m_class, m_method), {__VA_ARGS__}, _VariantCall::Arg(VariantType::m_arg1, (m_argname1)), _VariantCall::Arg(VariantType::m_arg2, (m_argname2)));
#define ADDFUNC3RNC(m_vtype, m_ret, m_class, m_method, m_arg1, m_argname1, m_arg2, m_argname2, m_arg3, m_argname3, ...) \
    _VariantCall::addfunc(false, VariantType::m_vtype, VariantType::m_ret, true, StringName(#m_method), VCALL(m_class, m_method), {__VA_ARGS__}, _VariantCall::Arg(VariantType::m_arg1, (m_argname1)), _VariantCall::Arg(VariantType::m_arg2, (m_argname2)), _VariantCall::Arg(VariantType::m_arg3, (m_argname3)));
#define ADDFUNC4RNC(m_vtype, m_ret, m_class, m_method, m_arg1, m_argname1, m_arg2, m_argname2, m_arg3, m_argname3, m_arg4, m_argname4, ...) \
    _VariantCall::addfunc(false, VariantType::m_vtype, VariantType::m_ret, true, StringName(#m_method), VCALL(m_class, m_method), {__VA_ARGS__}, _VariantCall::Arg(VariantType::m_arg1, (m_argname1)), _VariantCall::Arg(VariantType::m_arg2, (m_argname2)), _VariantCall::Arg(VariantType::m_arg3, (m_argname3)), _VariantCall::Arg(VariantType::m_arg4, (m_argname4)));

#define ADDFUNC0(m_vtype, m_ret, m_class, m_method) \
    _VariantCall::addfunc(true, VariantType::m_vtype, VariantType::m_ret, false, StringName(#m_method), VCALL(m_class, m_method), {});
#define ADDFUNC1(m_vtype, m_ret, m_class, m_method, m_arg1, m_argname1, ...) \
    _VariantCall::addfunc(true, VariantType::m_vtype, VariantType::m_ret, false, StringName(#m_method), VCALL(m_class, m_method), {__VA_ARGS__}, _VariantCall::Arg(VariantType::m_arg1, (m_argname1)));
#define ADDFUNC2(m_vtype, m_ret, m_class, m_method, m_arg1, m_argname1, m_arg2, m_argname2, ...) \
    _VariantCall::addfunc(true, VariantType::m_vtype, VariantType::m_ret, false, StringName(#m_method), VCALL(m_class, m_method), {__VA_ARGS__}, _VariantCall::Arg(VariantType::m_arg1, (m_argname1)), _VariantCall::Arg(VariantType::m_arg2, (m_argname2)));
#define ADDFUNC3(m_vtype, m_ret, m_class, m_method, m_arg1, m_argname1, m_arg2, m_argname2, m_arg3, m_argname3, ...) \
    _VariantCall::addfunc(true, VariantType::m_vtype, VariantType::m_ret, false, StringName(#m_method), VCALL(m_class, m_method), {__VA_ARGS__}, _VariantCall::Arg(VariantType::m_arg1, (m_argname1)), _VariantCall::Arg(VariantType::m_arg2, (m_argname2)), _VariantCall::Arg(VariantType::m_arg3, (m_argname3)));
#define ADDFUNC4(m_vtype, m_ret, m_class, m_method, m_arg1, m_argname1, m_arg2, m_argname2, m_arg3, m_argname3, m_arg4, m_argname4, ...) \
    _VariantCall::addfunc(true, VariantType::m_vtype, VariantType::m_ret, false, StringName(#m_method), VCALL(m_class, m_method), {__VA_ARGS__}, _VariantCall::Arg(VariantType::m_arg1, (m_argname1)), _VariantCall::Arg(VariantType::m_arg2, (m_argname2)), _VariantCall::Arg(VariantType::m_arg3, (m_argname3)), _VariantCall::Arg(VariantType::m_arg4, (m_argname4)));

#define ADDFUNC0NC(m_vtype, m_ret, m_class, m_method) \
    _VariantCall::addfunc(false, VariantType::m_vtype, VariantType::m_ret, false, StringName(#m_method), VCALL(m_class, m_method), {});
#define ADDFUNC1NC(m_vtype, m_ret, m_class, m_method, m_arg1, m_argname1, ...) \
    _VariantCall::addfunc(false, VariantType::m_vtype, VariantType::m_ret, false, StringName(#m_method), VCALL(m_class, m_method), {__VA_ARGS__}, _VariantCall::Arg(VariantType::m_arg1, (m_argname1)));
#define ADDFUNC2NC(m_vtype, m_ret, m_class, m_method, m_arg1, m_argname1, m_arg2, m_argname2, ...) \
    _VariantCall::addfunc(false, VariantType::m_vtype, VariantType::m_ret, false, StringName(#m_method), VCALL(m_class, m_method), {__VA_ARGS__}, _VariantCall::Arg(VariantType::m_arg1, (m_argname1)), _VariantCall::Arg(VariantType::m_arg2, (m_argname2)));
#define ADDFUNC3NC(m_vtype, m_ret, m_class, m_method, m_arg1, m_argname1, m_arg2, m_argname2, m_arg3, m_argname3, ...) \
    _VariantCall::addfunc(false, VariantType::m_vtype, VariantType::m_ret, false, StringName(#m_method), VCALL(m_class, m_method), {__VA_ARGS__}, _VariantCall::Arg(VariantType::m_arg1, (m_argname1)), _VariantCall::Arg(VariantType::m_arg2, (m_argname2)), _VariantCall::Arg(VariantType::m_arg3, (m_argname3)));
#define ADDFUNC4NC(m_vtype, m_ret, m_class, m_method, m_arg1, m_argname1, m_arg2, m_argname2, m_arg3, m_argname3, m_arg4, m_argname4, ...) \
    _VariantCall::addfunc(false, VariantType::m_vtype, VariantType::m_ret, false, StringName(#m_method), VCALL(m_class, m_method), {__VA_ARGS__}, _VariantCall::Arg(VariantType::m_arg1, (m_argname1)), _VariantCall::Arg(VariantType::m_arg2, (m_argname2)), _VariantCall::Arg(VariantType::m_arg3, (m_argname3)), _VariantCall::Arg(VariantType::m_arg4, (m_argname4)));



    /* STRING */

//    ADDFUNC0R(STRING, POOL_STRING_ARRAY, String, bigrams)
//    ADDFUNC1R(STRING, REAL, String, similarity, STRING, "text")


    /* REGISTER CONSTRUCTORS */

    _VariantCall::add_constructor(_VariantCall::Vector2_init1, VariantType::VECTOR2, "x", VariantType::FLOAT, "y", VariantType::FLOAT);

    _VariantCall::add_constructor(_VariantCall::Rect2_init1, VariantType::RECT2, "position", VariantType::VECTOR2, "size", VariantType::VECTOR2);
    _VariantCall::add_constructor(_VariantCall::Rect2_init2, VariantType::RECT2, "x", VariantType::FLOAT, "y", VariantType::FLOAT, "width", VariantType::FLOAT, "height", VariantType::FLOAT);

    _VariantCall::add_constructor(_VariantCall::Transform2D_init2, VariantType::TRANSFORM2D, "rotation", VariantType::FLOAT, "position", VariantType::VECTOR2);
    _VariantCall::add_constructor(_VariantCall::Transform2D_init3, VariantType::TRANSFORM2D, "x_axis", VariantType::VECTOR2, "y_axis", VariantType::VECTOR2, "origin", VariantType::VECTOR2);

    _VariantCall::add_constructor(_VariantCall::Vector3_init1, VariantType::VECTOR3, "x", VariantType::FLOAT, "y", VariantType::FLOAT, "z", VariantType::FLOAT);

    _VariantCall::add_constructor(_VariantCall::Plane_init1, VariantType::PLANE, "a", VariantType::FLOAT, "b", VariantType::FLOAT, "c", VariantType::FLOAT, "d", VariantType::FLOAT);
    _VariantCall::add_constructor(_VariantCall::Plane_init2, VariantType::PLANE, "v1", VariantType::VECTOR3, "v2", VariantType::VECTOR3, "v3", VariantType::VECTOR3);
    _VariantCall::add_constructor(_VariantCall::Plane_init3, VariantType::PLANE, "normal", VariantType::VECTOR3, "d", VariantType::FLOAT);

    _VariantCall::add_constructor(_VariantCall::Quat_init1, VariantType::QUAT, "x", VariantType::FLOAT, "y", VariantType::FLOAT, "z", VariantType::FLOAT, "w", VariantType::FLOAT);
    _VariantCall::add_constructor(_VariantCall::Quat_init2, VariantType::QUAT, "axis", VariantType::VECTOR3, "angle", VariantType::FLOAT);
    _VariantCall::add_constructor(_VariantCall::Quat_init3, VariantType::QUAT, "euler", VariantType::VECTOR3);

    _VariantCall::add_constructor(_VariantCall::Color_init1, VariantType::COLOR, "r", VariantType::FLOAT, "g", VariantType::FLOAT, "b", VariantType::FLOAT, "a", VariantType::FLOAT);
    _VariantCall::add_constructor(_VariantCall::Color_init2, VariantType::COLOR, "r", VariantType::FLOAT, "g", VariantType::FLOAT, "b", VariantType::FLOAT);

    _VariantCall::add_constructor(_VariantCall::AABB_init1, VariantType::AABB, "position", VariantType::VECTOR3, "size", VariantType::VECTOR3);

    _VariantCall::add_constructor(_VariantCall::Basis_init1, VariantType::BASIS, "x_axis", VariantType::VECTOR3, "y_axis", VariantType::VECTOR3, "z_axis", VariantType::VECTOR3);
    _VariantCall::add_constructor(_VariantCall::Basis_init2, VariantType::BASIS, "axis", VariantType::VECTOR3, "phi", VariantType::FLOAT);

    _VariantCall::add_constructor(_VariantCall::Transform_init1, VariantType::TRANSFORM, "x_axis", VariantType::VECTOR3, "y_axis", VariantType::VECTOR3, "z_axis", VariantType::VECTOR3, "origin", VariantType::VECTOR3);
    _VariantCall::add_constructor(_VariantCall::Transform_init2, VariantType::TRANSFORM, "basis", VariantType::BASIS, "origin", VariantType::VECTOR3);

    /* REGISTER CONSTANTS */

    for (const eastl::pair<const char *const,Color> &color : _named_colors) {
        _VariantCall::add_variant_constant(VariantType::COLOR, StringName(color.first), color.second);
    }

    _VariantCall::add_constant(VariantType::VECTOR3, "AXIS_X", Vector3::AXIS_X);
    _VariantCall::add_constant(VariantType::VECTOR3, "AXIS_Y", Vector3::AXIS_Y);
    _VariantCall::add_constant(VariantType::VECTOR3, "AXIS_Z", Vector3::AXIS_Z);

    _VariantCall::add_variant_constant(VariantType::VECTOR3, "ZERO", Vector3(0, 0, 0));
    _VariantCall::add_variant_constant(VariantType::VECTOR3, "ONE", Vector3(1, 1, 1));
    _VariantCall::add_variant_constant(VariantType::VECTOR3, "INF", Vector3(Math_INF, Math_INF, Math_INF));
    _VariantCall::add_variant_constant(VariantType::VECTOR3, "LEFT", Vector3(-1, 0, 0));
    _VariantCall::add_variant_constant(VariantType::VECTOR3, "RIGHT", Vector3(1, 0, 0));
    _VariantCall::add_variant_constant(VariantType::VECTOR3, "UP", Vector3(0, 1, 0));
    _VariantCall::add_variant_constant(VariantType::VECTOR3, "DOWN", Vector3(0, -1, 0));
    _VariantCall::add_variant_constant(VariantType::VECTOR3, "FORWARD", Vector3(0, 0, -1));
    _VariantCall::add_variant_constant(VariantType::VECTOR3, "BACK", Vector3(0, 0, 1));
    _VariantCall::add_constant(VariantType::VECTOR2, "AXIS_X", Vector2::AXIS_X);
    _VariantCall::add_constant(VariantType::VECTOR2, "AXIS_Y", Vector2::AXIS_Y);

    _VariantCall::add_variant_constant(VariantType::VECTOR2, "ZERO", Vector2(0, 0));
    _VariantCall::add_variant_constant(VariantType::VECTOR2, "ONE", Vector2(1, 1));
    _VariantCall::add_variant_constant(VariantType::VECTOR2, "INF", Vector2(Math_INF, Math_INF));
    _VariantCall::add_variant_constant(VariantType::VECTOR2, "LEFT", Vector2(-1, 0));
    _VariantCall::add_variant_constant(VariantType::VECTOR2, "RIGHT", Vector2(1, 0));
    _VariantCall::add_variant_constant(VariantType::VECTOR2, "UP", Vector2(0, -1));
    _VariantCall::add_variant_constant(VariantType::VECTOR2, "DOWN", Vector2(0, 1));

    _VariantCall::add_variant_constant(VariantType::TRANSFORM2D, "IDENTITY", Transform2D());
    _VariantCall::add_variant_constant(VariantType::TRANSFORM2D, "FLIP_X", Transform2D(-1, 0, 0, 1, 0, 0));
    _VariantCall::add_variant_constant(VariantType::TRANSFORM2D, "FLIP_Y", Transform2D(1, 0, 0, -1, 0, 0));

    Transform identity_transform = Transform();
    Transform flip_x_transform = Transform(-1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0);
    Transform flip_y_transform = Transform(1, 0, 0, 0, -1, 0, 0, 0, 1, 0, 0, 0);
    Transform flip_z_transform = Transform(1, 0, 0, 0, 1, 0, 0, 0, -1, 0, 0, 0);
    _VariantCall::add_variant_constant(VariantType::TRANSFORM, "IDENTITY", identity_transform);
    _VariantCall::add_variant_constant(VariantType::TRANSFORM, "FLIP_X", flip_x_transform);
    _VariantCall::add_variant_constant(VariantType::TRANSFORM, "FLIP_Y", flip_y_transform);
    _VariantCall::add_variant_constant(VariantType::TRANSFORM, "FLIP_Z", flip_z_transform);

    Basis identity_basis = Basis();
    Basis flip_x_basis = Basis(-1, 0, 0, 0, 1, 0, 0, 0, 1);
    Basis flip_y_basis = Basis(1, 0, 0, 0, -1, 0, 0, 0, 1);
    Basis flip_z_basis = Basis(1, 0, 0, 0, 1, 0, 0, 0, -1);
    _VariantCall::add_variant_constant(VariantType::BASIS, "IDENTITY", identity_basis);
    _VariantCall::add_variant_constant(VariantType::BASIS, "FLIP_X", flip_x_basis);
    _VariantCall::add_variant_constant(VariantType::BASIS, "FLIP_Y", flip_y_basis);
    _VariantCall::add_variant_constant(VariantType::BASIS, "FLIP_Z", flip_z_basis);

    _VariantCall::add_variant_constant(VariantType::PLANE, "PLANE_YZ", Plane(Vector3(1, 0, 0), 0));
    _VariantCall::add_variant_constant(VariantType::PLANE, "PLANE_XZ", Plane(Vector3(0, 1, 0), 0));
    _VariantCall::add_variant_constant(VariantType::PLANE, "PLANE_XY", Plane(Vector3(0, 0, 1), 0));

    _VariantCall::add_variant_constant(VariantType::QUAT, "IDENTITY", Quat(0, 0, 0, 1));
}

void unregister_variant_methods() {

    memdelete_arr(_VariantCall::type_funcs);
    memdelete_arr(_VariantCall::construct_funcs);
    memdelete_arr(_VariantCall::constant_data);
}
