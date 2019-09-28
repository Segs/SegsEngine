/*************************************************************************/
/*  variant_call.cpp                                                     */
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

#include "variant.h"

#include "core/color_names.inc"
#include "core/container_tools.h"
#include "core/core_string_names.h"
#include "core/crypto/crypto_core.h"
#include "core/io/compression.h"
#include "core/method_bind_interface.h"
#include "core/method_info.h"
#include "core/object.h"
#include "core/object_db.h"
#include "core/os/os.h"
#include "core/script_language.h"
#include "core/vector.h"
#include "core/rid.h"

using VariantFunc = void (*)(Variant &, Variant &, const Variant **);
using VariantConstructFunc = void (*)(Variant &, const Variant **);

struct _VariantCall {

    static void Vector3_dot(Variant &r_ret, Variant &p_self, const Variant **p_args) {

        r_ret = reinterpret_cast<Vector3 *>(p_self._data._mem)->dot(*reinterpret_cast<const Vector3 *>(p_args[0]->_data._mem));
    }

    struct FuncData {

        int arg_count;
        PODVector<Variant> default_args;
        Vector<VariantType> arg_types;
        Vector<StringName> arg_names;
        VariantType return_type;

        bool _const;
        bool returns;

        VariantFunc func;

        _FORCE_INLINE_ bool verify_arguments(const Variant **p_args, Variant::CallError &r_error) {

            if (arg_count == 0)
                return true;

            const VariantType *tptr = &arg_types[0];

            for (int i = 0; i < arg_count; i++) {

                if (tptr[i] == VariantType::NIL || tptr[i] == p_args[i]->type)
                    continue; // all good
                if (!Variant::can_convert(p_args[i]->type, tptr[i])) {
                    r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                    r_error.argument = i;
                    r_error.expected = tptr[i];
                    return false;
                }
            }
            return true;
        }

        _FORCE_INLINE_ void call(Variant &r_ret, Variant &p_self, const Variant **p_args, int p_argcount, Variant::CallError &r_error) {
#ifdef DEBUG_ENABLED
            if (p_argcount > arg_count) {
                r_error.error = Variant::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
                r_error.argument = arg_count;
                return;
            } else
#endif
                    if (p_argcount < arg_count) {
                int def_argcount = default_args.size();
#ifdef DEBUG_ENABLED
                if (p_argcount < (arg_count - def_argcount)) {
                    r_error.error = Variant::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
                    r_error.argument = arg_count - def_argcount;
                    return;
                }

#endif
                ERR_FAIL_COND(p_argcount > VARIANT_ARG_MAX)
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

    struct TypeFunc {

        Map<StringName, FuncData> functions;
    };

    static TypeFunc *type_funcs;

    struct Arg {
        StringName name;
        VariantType type;
        Arg() { type = VariantType::NIL; }
        Arg(VariantType p_type, const StringName &p_name) :
                name(p_name),
                type(p_type) {
        }
    };

    //void addfunc(VariantType p_type, const StringName& p_name,VariantFunc p_func);

    static void make_func_return_variant(VariantType p_type, const StringName &p_name) {

#ifdef DEBUG_ENABLED
        type_funcs[(int)p_type].functions[p_name].returns = true;
#endif
    }

    static void addfunc(bool p_const, VariantType p_type, VariantType p_return, bool p_has_return, const StringName &p_name, VariantFunc p_func, const
            std::initializer_list<Variant> p_defaultarg, const Arg &p_argtype1 = Arg(), const Arg &p_argtype2 = Arg(), const Arg &p_argtype3 = Arg(), const Arg &p_argtype4 = Arg(), const Arg &p_argtype5 = Arg()) {

        FuncData funcdata;
        funcdata.func = p_func;
        funcdata.default_args = p_defaultarg;
        funcdata._const = p_const;
        funcdata.returns = p_has_return;
        funcdata.return_type = p_return;

        if (p_argtype1.name) {
            funcdata.arg_types.push_back(p_argtype1.type);
#ifdef DEBUG_ENABLED
            funcdata.arg_names.push_back(p_argtype1.name);
#endif

        } else
            goto end;

        if (p_argtype2.name) {
            funcdata.arg_types.push_back(p_argtype2.type);
#ifdef DEBUG_ENABLED
            funcdata.arg_names.push_back(p_argtype2.name);
#endif

        } else
            goto end;

        if (p_argtype3.name) {
            funcdata.arg_types.push_back(p_argtype3.type);
#ifdef DEBUG_ENABLED
            funcdata.arg_names.push_back(p_argtype3.name);
#endif

        } else
            goto end;

        if (p_argtype4.name) {
            funcdata.arg_types.push_back(p_argtype4.type);
#ifdef DEBUG_ENABLED
            funcdata.arg_names.push_back(p_argtype4.name);
#endif
        } else
            goto end;

        if (p_argtype5.name) {
            funcdata.arg_types.push_back(p_argtype5.type);
#ifdef DEBUG_ENABLED
            funcdata.arg_names.push_back(p_argtype5.name);
#endif
        } else
            goto end;

    end:

        funcdata.arg_count = funcdata.arg_types.size();
        type_funcs[(int)p_type].functions[p_name] = funcdata;
    }

#define VCALL_LOCALMEM0(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant ** /*p_args*/) { reinterpret_cast<m_type *>(p_self._data._mem)->m_method(); }
#define VCALL_LOCALMEM0R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant ** /*p_args*/) { r_ret = reinterpret_cast<m_type *>(p_self._data._mem)->m_method(); }
#define VCALL_LOCALMEM1(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { reinterpret_cast<m_type *>(p_self._data._mem)->m_method(*p_args[0]); }
#define VCALL_LOCALMEM1R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { r_ret = reinterpret_cast<m_type *>(p_self._data._mem)->m_method(*p_args[0]); }
#define VCALL_LOCALMEM2(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { reinterpret_cast<m_type *>(p_self._data._mem)->m_method(*p_args[0], *p_args[1]); }
#define VCALL_LOCALMEM2R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { r_ret = reinterpret_cast<m_type *>(p_self._data._mem)->m_method(*p_args[0], *p_args[1]); }
#define VCALL_LOCALMEM3(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { reinterpret_cast<m_type *>(p_self._data._mem)->m_method(*p_args[0], *p_args[1], *p_args[2]); }
#define VCALL_LOCALMEM3R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { r_ret = reinterpret_cast<m_type *>(p_self._data._mem)->m_method(*p_args[0], *p_args[1], *p_args[2]); }
#define VCALL_LOCALMEM4(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { reinterpret_cast<m_type *>(p_self._data._mem)->m_method(*p_args[0], *p_args[1], *p_args[2], *p_args[3]); }
#define VCALL_LOCALMEM4R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { r_ret = reinterpret_cast<m_type *>(p_self._data._mem)->m_method(*p_args[0], *p_args[1], *p_args[2], *p_args[3]); }
#define VCALL_LOCALMEM5(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { reinterpret_cast<m_type *>(p_self._data._mem)->m_method(*p_args[0], *p_args[1], *p_args[2], *p_args[3], *p_args[4]); }
#define VCALL_LOCALMEM5R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { r_ret = reinterpret_cast<m_type *>(p_self._data._mem)->m_method(*p_args[0], *p_args[1], *p_args[2], *p_args[3], *p_args[4]); }

    // built-in functions of localmem based types
#define VCALL_SU_LOCALMEM0R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant ** /*p_args*/) { \
    r_ret = StringUtils::m_method(*reinterpret_cast<m_type *>(p_self._data._mem)); }
#define VCALL_SU_LOCALMEM1R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant ** p_args) { \
    r_ret = StringUtils::m_method(*reinterpret_cast<m_type *>(p_self._data._mem),*p_args[0]); }
#define VCALL_SU_LOCALMEM2R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant ** p_args) { \
    r_ret = StringUtils::m_method(*reinterpret_cast<m_type *>(p_self._data._mem),*p_args[0],*p_args[1]); }
#define VCALL_SU_LOCALMEM2(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant ** p_args) { \
    StringUtils::m_method(*reinterpret_cast<m_type *>(p_self._data._mem),*p_args[0],*p_args[1]); }

#define VCALL_SU_LOCALMEM3R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant ** p_args) { \
    r_ret = StringUtils::m_method(*reinterpret_cast<m_type *>(p_self._data._mem),*p_args[0],*p_args[1],*p_args[2]); }
#define VCALL_PU_LOCALMEM0R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant ** /*p_args*/) { \
    r_ret = PathUtils::m_method(*reinterpret_cast<m_type *>(p_self._data._mem)); }
#define VCALL_PU_LOCALMEM1R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant ** p_args) { \
    r_ret = PathUtils::m_method(*reinterpret_cast<m_type *>(p_self._data._mem),*p_args[0]); }

    static void _call_String_casecmp_to(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        r_ret = StringUtils::compare(*reinterpret_cast<String *>(p_self._data._mem),(*p_args[0]));
    }
    static void _call_String_nocasecmp_to(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        r_ret = StringUtils::compare(*reinterpret_cast<String *>(p_self._data._mem),(*p_args[0]),StringUtils::CaseInsensitive);
    }
    VCALL_LOCALMEM0R(String, length)
    VCALL_SU_LOCALMEM3R(String, count)
    VCALL_SU_LOCALMEM3R(String, countn)
    VCALL_SU_LOCALMEM2R(String, substr)
    VCALL_SU_LOCALMEM2R(String, find)
    VCALL_SU_LOCALMEM1R(String, find_last)
    VCALL_SU_LOCALMEM2R(String, findn)
    VCALL_SU_LOCALMEM2R(String, rfind)
    VCALL_SU_LOCALMEM2R(String, rfindn)
    VCALL_SU_LOCALMEM1R(String, match)
    VCALL_SU_LOCALMEM1R(String, matchn)
    VCALL_SU_LOCALMEM1R(String, begins_with)
    VCALL_SU_LOCALMEM1R(String, ends_with)
    VCALL_SU_LOCALMEM1R(String, is_subsequence_of)
    VCALL_SU_LOCALMEM1R(String, is_subsequence_ofi)
    VCALL_SU_LOCALMEM0R(String, bigrams)
    VCALL_SU_LOCALMEM1R(String, similarity)

    static void _call_String_format(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        r_ret = String(
                StringUtils::format(*reinterpret_cast<String *>(p_self._data._mem),*p_args[0]));
    }
    static void _call_String_replace(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        r_ret = StringUtils::replace(*reinterpret_cast<String *>(p_self._data._mem),p_args[0]->as<String>(), String(*p_args[1]));
    }
    VCALL_SU_LOCALMEM2R(String, replacen)
    VCALL_SU_LOCALMEM1R(String, repeat)
    VCALL_SU_LOCALMEM2R(String, insert)
    VCALL_SU_LOCALMEM0R(String, capitalize)
    VCALL_SU_LOCALMEM3R(String, split)
    VCALL_SU_LOCALMEM3R(String, rsplit)
    VCALL_SU_LOCALMEM2R(String, split_floats)
    VCALL_SU_LOCALMEM0R(String, to_upper)
    VCALL_SU_LOCALMEM0R(String, to_lower)
    VCALL_SU_LOCALMEM1R(String, left)
    VCALL_SU_LOCALMEM1R(String, right)
    VCALL_SU_LOCALMEM0R(String, dedent)
    VCALL_SU_LOCALMEM2R(String, strip_edges)
    VCALL_SU_LOCALMEM0R(String, strip_escapes)
    VCALL_SU_LOCALMEM1R(String, lstrip)
    VCALL_SU_LOCALMEM1R(String, rstrip)
    VCALL_PU_LOCALMEM0R(String, get_extension)
    VCALL_PU_LOCALMEM0R(String, get_basename)
    VCALL_PU_LOCALMEM1R(String, plus_file)
    VCALL_SU_LOCALMEM1R(String, ord_at)
    VCALL_SU_LOCALMEM2(String, erase)
    VCALL_SU_LOCALMEM0R(String, hash)
    VCALL_SU_LOCALMEM0R(String, md5_text)
    VCALL_SU_LOCALMEM0R(String, sha1_text)
    VCALL_SU_LOCALMEM0R(String, sha256_text)
    VCALL_SU_LOCALMEM0R(String, md5_buffer)
    VCALL_SU_LOCALMEM0R(String, sha1_buffer)
    VCALL_SU_LOCALMEM0R(String, sha256_buffer)
    VCALL_LOCALMEM0R(String, empty)
    VCALL_PU_LOCALMEM0R(String, is_abs_path)
    VCALL_PU_LOCALMEM0R(String, is_rel_path)
    VCALL_PU_LOCALMEM0R(String, get_base_dir)
    VCALL_PU_LOCALMEM0R(String, get_file)
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
    VCALL_SU_LOCALMEM1R(String, trim_prefix)
    VCALL_SU_LOCALMEM1R(String, trim_suffix)

    static void _call_String_to_ascii(Variant &r_ret, Variant &p_self, const Variant **p_args) {

        String *s = reinterpret_cast<String *>(p_self._data._mem);
        CharString charstr = StringUtils::ascii(*s);

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
        CharString charstr = StringUtils::utf8(*s);

        PoolByteArray retval;
        size_t len = charstr.length();
        retval.resize(len);
        PoolByteArray::Write w = retval.write();
        memcpy(w.ptr(), charstr.data(), len);
        w.release();

        r_ret = retval;
    }

    VCALL_LOCALMEM0R(Vector2, normalized)
    VCALL_LOCALMEM0R(Vector2, length)
    VCALL_LOCALMEM0R(Vector2, length_squared)
    VCALL_LOCALMEM0R(Vector2, is_normalized)
    VCALL_LOCALMEM1R(Vector2, distance_to)
    VCALL_LOCALMEM1R(Vector2, distance_squared_to)
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
    VCALL_LOCALMEM1R(Rect2, intersects)
    VCALL_LOCALMEM1R(Rect2, encloses)
    VCALL_LOCALMEM0R(Rect2, has_no_area)
    VCALL_LOCALMEM1R(Rect2, clip)
    VCALL_LOCALMEM1R(Rect2, merge)
    VCALL_LOCALMEM1R(Rect2, has_point)
    VCALL_LOCALMEM1R(Rect2, grow)
    VCALL_LOCALMEM2R(Rect2, grow_margin)
    VCALL_LOCALMEM4R(Rect2, grow_individual)
    VCALL_LOCALMEM1R(Rect2, expand)
    VCALL_LOCALMEM0R(Rect2, abs)

    VCALL_LOCALMEM0R(Vector3, min_axis)
    VCALL_LOCALMEM0R(Vector3, max_axis)
    VCALL_LOCALMEM0R(Vector3, length)
    VCALL_LOCALMEM0R(Vector3, length_squared)
    VCALL_LOCALMEM0R(Vector3, is_normalized)
    VCALL_LOCALMEM0R(Vector3, normalized)
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
    VCALL_LOCALMEM1R(Vector3, distance_to)
    VCALL_LOCALMEM1R(Vector3, distance_squared_to)
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
    VCALL_LOCALMEM1R(Plane, is_point_over)
    VCALL_LOCALMEM1R(Plane, distance_to)
    VCALL_LOCALMEM2R(Plane, has_point)
    VCALL_LOCALMEM1R(Plane, project)

    //return vector3 if intersected, nil if not
    static void _call_Plane_intersect_3(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        Vector3 result;
        if (reinterpret_cast<Plane *>(p_self._data._mem)->intersect_3(*p_args[0], *p_args[1], &result))
            r_ret = result;
        else
            r_ret = Variant();
    }

    static void _call_Plane_intersects_ray(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        Vector3 result;
        if (reinterpret_cast<Plane *>(p_self._data._mem)->intersects_ray(*p_args[0], *p_args[1], &result))
            r_ret = result;
        else
            r_ret = Variant();
    }

    static void _call_Plane_intersects_segment(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        Vector3 result;
        if (reinterpret_cast<Plane *>(p_self._data._mem)->intersects_segment(*p_args[0], *p_args[1], &result))
            r_ret = result;
        else
            r_ret = Variant();
    }

    VCALL_LOCALMEM0R(Quat, length)
    VCALL_LOCALMEM0R(Quat, length_squared)
    VCALL_LOCALMEM0R(Quat, normalized)
    VCALL_LOCALMEM0R(Quat, is_normalized)
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

    VCALL_LOCALMEM0R(RID, get_id)

    VCALL_LOCALMEM0R(NodePath, is_absolute)
    VCALL_LOCALMEM0R(NodePath, get_name_count)
    VCALL_LOCALMEM1R(NodePath, get_name)
    VCALL_LOCALMEM0R(NodePath, get_subname_count)
    VCALL_LOCALMEM1R(NodePath, get_subname)
    VCALL_LOCALMEM0R(NodePath, get_concatenated_subnames)
    VCALL_LOCALMEM0R(NodePath, get_as_property_path)
    VCALL_LOCALMEM0R(NodePath, is_empty)

    VCALL_LOCALMEM0R(Dictionary, size)
    VCALL_LOCALMEM0R(Dictionary, empty)
    VCALL_LOCALMEM0(Dictionary, clear)
    VCALL_LOCALMEM1R(Dictionary, has)
    VCALL_LOCALMEM1R(Dictionary, has_all)
    VCALL_LOCALMEM1R(Dictionary, erase)
    VCALL_LOCALMEM0R(Dictionary, hash)
    VCALL_LOCALMEM0R(Dictionary, keys)
    VCALL_LOCALMEM0R(Dictionary, values)
    VCALL_LOCALMEM1R(Dictionary, duplicate)
    VCALL_LOCALMEM2R(Dictionary, get)

    VCALL_LOCALMEM2(Array, set)
    VCALL_LOCALMEM1R(Array, get)
    VCALL_LOCALMEM0R(Array, size)
    VCALL_LOCALMEM0R(Array, empty)
    VCALL_LOCALMEM0(Array, clear)
    VCALL_LOCALMEM0R(Array, hash)
    VCALL_LOCALMEM1(Array, push_back)
    VCALL_LOCALMEM1(Array, push_front)
    VCALL_LOCALMEM0R(Array, pop_back)
    VCALL_LOCALMEM0R(Array, pop_front)
    VCALL_LOCALMEM1(Array, append)
    VCALL_LOCALMEM1(Array, resize)
    VCALL_LOCALMEM2(Array, insert)
    VCALL_LOCALMEM1(Array, remove)
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
    VCALL_LOCALMEM1R(Array, duplicate)
    VCALL_LOCALMEM4R(Array, slice)
    VCALL_LOCALMEM0(Array, invert)
    VCALL_LOCALMEM0R(Array, max)
    VCALL_LOCALMEM0R(Array, min)

    static void _call_PoolByteArray_get_string_from_ascii(Variant &r_ret, Variant &p_self, const Variant **p_args) {

        PoolByteArray *ba = reinterpret_cast<PoolByteArray *>(p_self._data._mem);
        String s;
        if (ba->size() >= 0) {
            PoolByteArray::Read r = ba->read();
            CharString cs;
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
        if (ba->size() >= 0) {
            PoolByteArray::Read r = ba->read();
            s = StringUtils::from_utf8((const char *)r.ptr(), ba->size());
        }
        r_ret = s;
    }

    static void _call_PoolByteArray_compress(Variant &r_ret, Variant &p_self, const Variant **p_args) {

        PoolByteArray *ba = reinterpret_cast<PoolByteArray *>(p_self._data._mem);
        PoolByteArray compressed;
        Compression::Mode mode = (Compression::Mode)(int)(*p_args[0]);

        compressed.resize(Compression::get_max_compressed_buffer_size(ba->size(), mode));
        int result = Compression::compress(compressed.write().ptr(), ba->read().ptr(), ba->size(), mode);

        result = result >= 0 ? result : 0;
        compressed.resize(result);

        r_ret = compressed;
    }

    static void _call_PoolByteArray_decompress(Variant &r_ret, Variant &p_self, const Variant **p_args) {

        PoolByteArray *ba = reinterpret_cast<PoolByteArray *>(p_self._data._mem);
        PoolByteArray decompressed;
        Compression::Mode mode = (Compression::Mode)(int)(*p_args[1]);

        int buffer_size = (int)(*p_args[0]);

        if (buffer_size < 0) {
            r_ret = decompressed;
            ERR_FAIL_CMSG("Decompression buffer size is less than zero.")
        }

        decompressed.resize(buffer_size);
        int result = Compression::decompress(decompressed.write().ptr(), buffer_size, ba->read().ptr(), ba->size(), mode);

        result = result >= 0 ? result : 0;
        decompressed.resize(result);

        r_ret = decompressed;
    }

    static void _call_PoolByteArray_hex_encode(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        PoolByteArray *ba = reinterpret_cast<PoolByteArray *>(p_self._data._mem);
        PoolByteArray::Read r = ba->read();
        String s = StringUtils::hex_encode_buffer(&r[0], ba->size());
        r_ret = s;
    }

    VCALL_LOCALMEM0R(PoolByteArray, size)
    VCALL_LOCALMEM2(PoolByteArray, set)
    VCALL_LOCALMEM1R(PoolByteArray, get)
    VCALL_LOCALMEM1(PoolByteArray, push_back)
    VCALL_LOCALMEM1(PoolByteArray, resize)
    VCALL_LOCALMEM2R(PoolByteArray, insert)
    VCALL_LOCALMEM1(PoolByteArray, remove)
    VCALL_LOCALMEM1(PoolByteArray, append)
    VCALL_LOCALMEM1(PoolByteArray, append_array)
    VCALL_LOCALMEM0(PoolByteArray, invert)
    VCALL_LOCALMEM2R(PoolByteArray, subarray)

    VCALL_LOCALMEM0R(PoolIntArray, size)
    VCALL_LOCALMEM2(PoolIntArray, set)
    VCALL_LOCALMEM1R(PoolIntArray, get)
    VCALL_LOCALMEM1(PoolIntArray, push_back)
    VCALL_LOCALMEM1(PoolIntArray, resize)
    VCALL_LOCALMEM2R(PoolIntArray, insert)
    VCALL_LOCALMEM1(PoolIntArray, remove)
    VCALL_LOCALMEM1(PoolIntArray, append)
    VCALL_LOCALMEM1(PoolIntArray, append_array)
    VCALL_LOCALMEM0(PoolIntArray, invert)

    VCALL_LOCALMEM0R(PoolRealArray, size)
    VCALL_LOCALMEM2(PoolRealArray, set)
    VCALL_LOCALMEM1R(PoolRealArray, get)
    VCALL_LOCALMEM1(PoolRealArray, push_back)
    VCALL_LOCALMEM1(PoolRealArray, resize)
    VCALL_LOCALMEM2R(PoolRealArray, insert)
    VCALL_LOCALMEM1(PoolRealArray, remove)
    VCALL_LOCALMEM1(PoolRealArray, append)
    VCALL_LOCALMEM1(PoolRealArray, append_array)
    VCALL_LOCALMEM0(PoolRealArray, invert)

    VCALL_LOCALMEM0R(PoolStringArray, size)
    VCALL_LOCALMEM2(PoolStringArray, set)
    VCALL_LOCALMEM1R(PoolStringArray, get)
    VCALL_LOCALMEM1(PoolStringArray, push_back)
    VCALL_LOCALMEM1(PoolStringArray, resize)
    VCALL_LOCALMEM2R(PoolStringArray, insert)
    VCALL_LOCALMEM1(PoolStringArray, remove)
    VCALL_LOCALMEM1(PoolStringArray, append)
    VCALL_LOCALMEM1(PoolStringArray, append_array)
    VCALL_LOCALMEM0(PoolStringArray, invert)
    VCALL_LOCALMEM1R(PoolStringArray, join)

    VCALL_LOCALMEM0R(PoolVector2Array, size)
    VCALL_LOCALMEM2(PoolVector2Array, set)
    VCALL_LOCALMEM1R(PoolVector2Array, get)
    VCALL_LOCALMEM1(PoolVector2Array, push_back)
    VCALL_LOCALMEM1(PoolVector2Array, resize)
    VCALL_LOCALMEM2R(PoolVector2Array, insert)
    VCALL_LOCALMEM1(PoolVector2Array, remove)
    VCALL_LOCALMEM1(PoolVector2Array, append)
    VCALL_LOCALMEM1(PoolVector2Array, append_array)
    VCALL_LOCALMEM0(PoolVector2Array, invert)

    VCALL_LOCALMEM0R(PoolVector3Array, size)
    VCALL_LOCALMEM2(PoolVector3Array, set)
    VCALL_LOCALMEM1R(PoolVector3Array, get)
    VCALL_LOCALMEM1(PoolVector3Array, push_back)
    VCALL_LOCALMEM1(PoolVector3Array, resize)
    VCALL_LOCALMEM2R(PoolVector3Array, insert)
    VCALL_LOCALMEM1(PoolVector3Array, remove)
    VCALL_LOCALMEM1(PoolVector3Array, append)
    VCALL_LOCALMEM1(PoolVector3Array, append_array)
    VCALL_LOCALMEM0(PoolVector3Array, invert)

    VCALL_LOCALMEM0R(PoolColorArray, size)
    VCALL_LOCALMEM2(PoolColorArray, set)
    VCALL_LOCALMEM1R(PoolColorArray, get)
    VCALL_LOCALMEM1(PoolColorArray, push_back)
    VCALL_LOCALMEM1(PoolColorArray, resize)
    VCALL_LOCALMEM2R(PoolColorArray, insert)
    VCALL_LOCALMEM1(PoolColorArray, remove)
    VCALL_LOCALMEM1(PoolColorArray, append)
    VCALL_LOCALMEM1(PoolColorArray, append_array)
    VCALL_LOCALMEM0(PoolColorArray, invert)

#define VCALL_PTR0(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { reinterpret_cast<m_type *>(p_self._data._ptr)->m_method(); }
#define VCALL_PTR0R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { r_ret = reinterpret_cast<m_type *>(p_self._data._ptr)->m_method(); }
#define VCALL_PTR1(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { reinterpret_cast<m_type *>(p_self._data._ptr)->m_method(*p_args[0]); }
#define VCALL_PTR1R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { r_ret = reinterpret_cast<m_type *>(p_self._data._ptr)->m_method(*p_args[0]); }
#define VCALL_PTR2(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { reinterpret_cast<m_type *>(p_self._data._ptr)->m_method(*p_args[0], *p_args[1]); }
#define VCALL_PTR2R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { r_ret = reinterpret_cast<m_type *>(p_self._data._ptr)->m_method(*p_args[0], *p_args[1]); }
#define VCALL_PTR3(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { reinterpret_cast<m_type *>(p_self._data._ptr)->m_method(*p_args[0], *p_args[1], *p_args[2]); }
#define VCALL_PTR3R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { r_ret = reinterpret_cast<m_type *>(p_self._data._ptr)->m_method(*p_args[0], *p_args[1], *p_args[2]); }
#define VCALL_PTR4(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { reinterpret_cast<m_type *>(p_self._data._ptr)->m_method(*p_args[0], *p_args[1], *p_args[2], *p_args[3]); }
#define VCALL_PTR4R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { r_ret = reinterpret_cast<m_type *>(p_self._data._ptr)->m_method(*p_args[0], *p_args[1], *p_args[2], *p_args[3]); }
#define VCALL_PTR5(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { reinterpret_cast<m_type *>(p_self._data._ptr)->m_method(*p_args[0], *p_args[1], *p_args[2], *p_args[3], *p_args[4]); }
#define VCALL_PTR5R(m_type, m_method) \
    static void _call_##m_type##_##m_method(Variant &r_ret, Variant &p_self, const Variant **p_args) { r_ret = reinterpret_cast<m_type *>(p_self._data._ptr)->m_method(*p_args[0], *p_args[1], *p_args[2], *p_args[3], *p_args[4]); }

    VCALL_PTR0R(AABB, get_area)
    VCALL_PTR0R(AABB, has_no_area)
    VCALL_PTR0R(AABB, has_no_surface)
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

    static void _call_Transform2D_xform(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        Transform2D *trn = reinterpret_cast<Transform2D *>(p_self._data._ptr);
        switch (p_args[0]->type) {

            case VariantType::VECTOR2: r_ret = trn->xform(p_args[0]->operator Vector2()); return;
            case VariantType::RECT2: r_ret = trn->xform(p_args[0]->operator Rect2()); return;
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

            case VariantType::VECTOR2: r_ret = trn->xform_inv(p_args[0]->operator Vector2()); return;
            case VariantType::RECT2: r_ret = trn->xform_inv(p_args[0]->operator Rect2()); return;
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

            case VariantType::VECTOR2: r_ret = reinterpret_cast<Transform2D *>(p_self._data._ptr)->basis_xform(p_args[0]->operator Vector2()); return;
            default: r_ret = Variant();
        }
    }

    static void _call_Transform2D_basis_xform_inv(Variant &r_ret, Variant &p_self, const Variant **p_args) {

        switch (p_args[0]->type) {

            case VariantType::VECTOR2: r_ret = reinterpret_cast<Transform2D *>(p_self._data._ptr)->basis_xform_inv(p_args[0]->operator Vector2()); return;
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

    static void _call_Transform_xform(Variant &r_ret, Variant &p_self, const Variant **p_args) {
        Transform *trn = reinterpret_cast<Transform *>(p_self._data._ptr);
        switch (p_args[0]->type) {

            case VariantType::VECTOR3: r_ret = trn->xform(p_args[0]->operator Vector3()); return;
            case VariantType::PLANE: r_ret = trn->xform(p_args[0]->operator Plane()); return;
            case VariantType::AABB: r_ret = trn->xform(p_args[0]->operator ::AABB()); return;
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

            case VariantType::VECTOR3: r_ret = trn->xform_inv(p_args[0]->operator Vector3()); return;
            case VariantType::PLANE: r_ret = trn->xform_inv(p_args[0]->operator Plane()); return;
            case VariantType::AABB: r_ret = trn->xform_inv(p_args[0]->operator ::AABB()); return;
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

        List<ConstructData> constructors;
    };

    static ConstructFunc *construct_funcs;

    static void Vector2_init1(Variant &r_ret, const Variant **p_args) {

        r_ret = Vector2(*p_args[0], *p_args[1]);
    }

    static void Rect2_init1(Variant &r_ret, const Variant **p_args) {

        r_ret = Rect2(*p_args[0], *p_args[1]);
    }

    static void Rect2_init2(Variant &r_ret, const Variant **p_args) {

        r_ret = Rect2(*p_args[0], *p_args[1], *p_args[2], *p_args[3]);
    }

    static void Transform2D_init2(Variant &r_ret, const Variant **p_args) {

        Transform2D m(*p_args[0], *p_args[1]);
        r_ret = m;
    }

    static void Transform2D_init3(Variant &r_ret, const Variant **p_args) {

        Transform2D m;
        m[0] = *p_args[0];
        m[1] = *p_args[1];
        m[2] = *p_args[2];
        r_ret = m;
    }

    static void Vector3_init1(Variant &r_ret, const Variant **p_args) {

        r_ret = Vector3(*p_args[0], *p_args[1], *p_args[2]);
    }

    static void Plane_init1(Variant &r_ret, const Variant **p_args) {

        r_ret = Plane(*p_args[0], *p_args[1], *p_args[2], *p_args[3]);
    }

    static void Plane_init2(Variant &r_ret, const Variant **p_args) {

        r_ret = Plane(*p_args[0], *p_args[1], *p_args[2]);
    }

    static void Plane_init3(Variant &r_ret, const Variant **p_args) {

        r_ret = Plane(p_args[0]->operator Vector3(), p_args[1]->operator real_t());
    }
    static void Plane_init4(Variant &r_ret, const Variant **p_args) {

        r_ret = Plane(p_args[0]->operator Vector3(), p_args[1]->operator Vector3());
    }

    static void Quat_init1(Variant &r_ret, const Variant **p_args) {

        r_ret = Quat(*p_args[0], *p_args[1], *p_args[2], *p_args[3]);
    }

    static void Quat_init2(Variant &r_ret, const Variant **p_args) {

        r_ret = Quat(((Vector3)(*p_args[0])), ((real_t)(*p_args[1])));
    }

    static void Quat_init3(Variant &r_ret, const Variant **p_args) {

        r_ret = Quat(((Vector3)(*p_args[0])));
    }

    static void Color_init1(Variant &r_ret, const Variant **p_args) {

        r_ret = Color(*p_args[0], *p_args[1], *p_args[2], *p_args[3]);
    }

    static void Color_init2(Variant &r_ret, const Variant **p_args) {

        r_ret = Color(*p_args[0], *p_args[1], *p_args[2]);
    }

    static void Color_init3(Variant &r_ret, const Variant **p_args) {

        r_ret = Color::html(*p_args[0]);
    }

    static void Color_init4(Variant &r_ret, const Variant **p_args) {

        r_ret = Color::hex(*p_args[0]);
    }

    static void AABB_init1(Variant &r_ret, const Variant **p_args) {

        r_ret = ::AABB(*p_args[0], *p_args[1]);
    }

    static void Basis_init1(Variant &r_ret, const Variant **p_args) {

        Basis m;
        m.set_axis(0, *p_args[0]);
        m.set_axis(1, *p_args[1]);
        m.set_axis(2, *p_args[2]);
        r_ret = m;
    }

    static void Basis_init2(Variant &r_ret, const Variant **p_args) {

        r_ret = Basis(p_args[0]->operator Vector3(), p_args[1]->operator real_t());
    }

    static void Transform_init1(Variant &r_ret, const Variant **p_args) {

        Transform t;
        t.basis.set_axis(0, *p_args[0]);
        t.basis.set_axis(1, *p_args[1]);
        t.basis.set_axis(2, *p_args[2]);
        t.origin = *p_args[3];
        r_ret = t;
    }

    static void Transform_init2(Variant &r_ret, const Variant **p_args) {

        r_ret = Transform(p_args[0]->operator Basis(), p_args[1]->operator Vector3());
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
        cd.arg_names.push_back(String(p_name1));
        cd.arg_types.push_back(p_type1);

        if (nullptr==p_name2)
            goto end;
        cd.arg_count++;
        cd.arg_names.push_back(String(p_name2));
        cd.arg_types.push_back(p_type2);

        if (nullptr==p_name3)
            goto end;
        cd.arg_count++;
        cd.arg_names.push_back(String(p_name3));
        cd.arg_types.push_back(p_type3);

        if (nullptr==p_name4)
            goto end;
        cd.arg_count++;
        cd.arg_names.push_back(String(p_name4));
        cd.arg_types.push_back(p_type4);

    end:

        construct_funcs[(int)p_type].constructors.push_back(cd);
    }

    struct ConstantData {

        Map<StringName, int> value;
#ifdef DEBUG_ENABLED
        ListPOD<StringName> value_ordered;
#endif
        Map<StringName, Variant> variant_value;
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

Variant Variant::call(const StringName &p_method, const Variant **p_args, int p_argcount, CallError &r_error) {

    Variant ret;
    call_ptr(p_method, p_args, p_argcount, &ret, r_error);
    return ret;
}

void Variant::call_ptr(const StringName &p_method, const Variant **p_args, int p_argcount, Variant *r_ret, CallError &r_error) {
    Variant ret;

    if (type == VariantType::OBJECT) {
        //call object
        Object *obj = _get_obj().obj;
        if (!obj) {
            r_error.error = CallError::CALL_ERROR_INSTANCE_IS_NULL;
            return;
        }
#ifdef DEBUG_ENABLED
        if (ScriptDebugger::get_singleton() && _get_obj().ref.is_null()) {
            //only if debugging!
            if (!ObjectDB::instance_validate(obj)) {
                r_error.error = CallError::CALL_ERROR_INSTANCE_IS_NULL;
                return;
            }
        }

#endif
        ret = _get_obj().obj->call(p_method, p_args, p_argcount, r_error);

        //else if (type==VariantType::METHOD) {

    } else {

        r_error.error = Variant::CallError::CALL_OK;

        Map<StringName, _VariantCall::FuncData>::iterator E = _VariantCall::type_funcs[(int)type].functions.find(p_method);
#ifdef DEBUG_ENABLED
        if (E==_VariantCall::type_funcs[(int)type].functions.end()) {
            r_error.error = Variant::CallError::CALL_ERROR_INVALID_METHOD;
            return;
        }
#endif
        _VariantCall::FuncData &funcdata = E->second;
        funcdata.call(ret, *this, p_args, p_argcount, r_error);
    }

    if (r_error.error == Variant::CallError::CALL_OK && r_ret)
        *r_ret = ret;
}

#define VCALL(m_type, m_method) _VariantCall::_call_##m_type##_##m_method

Variant Variant::construct(const VariantType p_type, const Variant **p_args, int p_argcount, CallError &r_error, bool p_strict) {

    r_error.error = Variant::CallError::CALL_ERROR_INVALID_METHOD;
    ERR_FAIL_INDEX_V(int(p_type), int(VariantType::VARIANT_MAX), Variant())

    r_error.error = Variant::CallError::CALL_OK;
    if (p_argcount == 0) { //generic construct

        switch (p_type) {
            case VariantType::NIL:
                return Variant();

            // atomic types
            case VariantType::BOOL: return Variant(false);
            case VariantType::INT: return 0;
            case VariantType::REAL: return 0.0f;
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

    } else if (p_argcount > 1) {

        _VariantCall::ConstructFunc &c = _VariantCall::construct_funcs[(int)p_type];

        for (List<_VariantCall::ConstructData>::Element *E = c.constructors.front(); E; E = E->next()) {
            const _VariantCall::ConstructData &cd = E->deref();

            if (cd.arg_count != p_argcount)
                continue;

            //validate parameters
            for (int i = 0; i < cd.arg_count; i++) {
                if (!Variant::can_convert(p_args[i]->type, cd.arg_types[i])) {
                    r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT; //no such constructor
                    r_error.argument = i;
                    r_error.expected = cd.arg_types[i];
                    return Variant();
                }
            }

            Variant v;
            cd.func(v, p_args);
            return v;
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
                return Variant(bool(*p_args[0]));
            }
            case VariantType::INT: {
                return (int64_t(*p_args[0]));
            }
            case VariantType::REAL: {
                return real_t(*p_args[0]);
            }
            case VariantType::STRING: {
                return String(*p_args[0]);
            }
            case VariantType::VECTOR2: {
                return Vector2(*p_args[0]);
            }
            case VariantType::RECT2: return (Rect2(*p_args[0]));
            case VariantType::VECTOR3: return (Vector3(*p_args[0]));
            case VariantType::PLANE: return (Plane(*p_args[0]));
            case VariantType::QUAT: return (p_args[0]->operator Quat());
            case VariantType::AABB:
                return (::AABB(*p_args[0])); // 10
            case VariantType::BASIS: return (Basis(p_args[0]->operator Basis()));
            case VariantType::TRANSFORM:
                return (Transform(p_args[0]->operator Transform()));

            // misc types
            case VariantType::COLOR: return p_args[0]->type == VariantType::STRING ? Color::html(*p_args[0]) : Color::hex(*p_args[0]);
            case VariantType::NODE_PATH:
                return (NodePath(p_args[0]->operator NodePath())); // 15
            case VariantType::_RID: return (RID(*p_args[0]));
            case VariantType::OBJECT: return Variant((Object *)(p_args[0]->operator Object *()));
            case VariantType::DICTIONARY: return p_args[0]->operator Dictionary();
            case VariantType::ARRAY:
                return p_args[0]->operator Array(); // 20

            // arrays
            case VariantType::POOL_BYTE_ARRAY: return (PoolByteArray(*p_args[0]));
            case VariantType::POOL_INT_ARRAY: return (PoolIntArray(*p_args[0]));
            case VariantType::POOL_REAL_ARRAY: return (PoolRealArray(*p_args[0]));
            case VariantType::POOL_STRING_ARRAY: return (PoolStringArray(*p_args[0]));
            case VariantType::POOL_VECTOR2_ARRAY:
                return Variant(PoolVector2Array(*p_args[0])); // 25
            case VariantType::POOL_VECTOR3_ARRAY: return (PoolVector3Array(*p_args[0]));
            case VariantType::POOL_COLOR_ARRAY: return (PoolColorArray(*p_args[0]));
            default: return Variant();
        }
    }
    r_error.error = Variant::CallError::CALL_ERROR_INVALID_METHOD; //no such constructor
    return Variant();
}

bool Variant::has_method(const StringName &p_method) const {

    if (type == VariantType::OBJECT) {
        Object *obj = operator Object *();
        if (!obj)
            return false;
#ifdef DEBUG_ENABLED
        if (ScriptDebugger::get_singleton()) {
            if (ObjectDB::instance_validate(obj)) {
#endif
                return obj->has_method(p_method);
#ifdef DEBUG_ENABLED
            }
        }
#endif
    }

    const _VariantCall::TypeFunc &tf = _VariantCall::type_funcs[(int)type];
    return tf.functions.contains(p_method);
}

Vector<VariantType> Variant::get_method_argument_types(VariantType p_type, const StringName &p_method) {

    const _VariantCall::TypeFunc &tf = _VariantCall::type_funcs[(int)p_type];

    const Map<StringName, _VariantCall::FuncData>::const_iterator E = tf.functions.find(p_method);
    if (E==tf.functions.end())
        return Vector<VariantType>();

    return E->second.arg_types;
}

bool Variant::is_method_const(VariantType p_type, const StringName &p_method) {

    const _VariantCall::TypeFunc &tf = _VariantCall::type_funcs[(int)p_type];

    const Map<StringName, _VariantCall::FuncData>::const_iterator E = tf.functions.find(p_method);
    if (E==tf.functions.end())
        return false;

    return E->second._const;
}

Vector<StringName> Variant::get_method_argument_names(VariantType p_type, const StringName &p_method) {

    const _VariantCall::TypeFunc &tf = _VariantCall::type_funcs[(int)p_type];

    const Map<StringName, _VariantCall::FuncData>::const_iterator E = tf.functions.find(p_method);
    if (E==tf.functions.end())
        return Vector<StringName>();

    return E->second.arg_names;
}

VariantType Variant::get_method_return_type(VariantType p_type, const StringName &p_method, bool *r_has_return) {

    const _VariantCall::TypeFunc &tf = _VariantCall::type_funcs[(int)p_type];

    const Map<StringName, _VariantCall::FuncData>::const_iterator E = tf.functions.find(p_method);
    if (E==tf.functions.end())
        return VariantType::NIL;

    if (r_has_return)
        *r_has_return = E->second.returns;

    return E->second.return_type;
}
static const PODVector<Variant> s_empty;

const PODVector<Variant> &Variant::get_method_default_arguments(VariantType p_type, const StringName &p_method) {
    const _VariantCall::TypeFunc &tf = _VariantCall::type_funcs[(int)p_type];

    const Map<StringName, _VariantCall::FuncData>::const_iterator E = tf.functions.find(p_method);
    if (E==tf.functions.end())
        return s_empty;

    return E->second.default_args;
}

void Variant::get_method_list(PODVector<MethodInfo> *p_list) const {

    const _VariantCall::TypeFunc &tf = _VariantCall::type_funcs[(int)type];

    for (const eastl::pair<const StringName,_VariantCall::FuncData> &E : tf.functions) {

        const _VariantCall::FuncData &fd = E.second;

        MethodInfo mi;
        mi.name = E.first;

        if (fd._const) {
            mi.flags |= METHOD_FLAG_CONST;
        }

        for (int i = 0; i < fd.arg_types.size(); i++) {

            PropertyInfo pi;
            pi.type = fd.arg_types[i];
#ifdef DEBUG_ENABLED
            pi.name = fd.arg_names[i];
#endif
            mi.arguments.push_back(pi);
        }

        mi.default_arguments = fd.default_args;
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

void Variant::get_constructor_list(VariantType p_type, PODVector<MethodInfo> *p_list) {

    ERR_FAIL_INDEX(int(p_type), int(VariantType::VARIANT_MAX))

    //custom constructors
    for (const List<_VariantCall::ConstructData>::Element *E = _VariantCall::construct_funcs[(int)p_type].constructors.front(); E; E = E->next()) {

        const _VariantCall::ConstructData &cd = E->deref();
        MethodInfo mi;
        mi.name = Variant::get_type_name(p_type);
        mi.return_val.type = p_type;
        for (int i = 0; i < cd.arg_count; i++) {

            PropertyInfo pi;
            pi.name = cd.arg_names[i];
            pi.type = cd.arg_types[i];
            mi.arguments.push_back(pi);
        }
        p_list->push_back(mi);
    }
    //default constructors
    for (int i = 0; i < int(VariantType::VARIANT_MAX); i++) {
        if (i == int(p_type))
            continue;
        if (!Variant::can_convert(VariantType(i), p_type))
            continue;

        MethodInfo mi;
        mi.name = Variant::get_type_name(p_type);
        PropertyInfo pi;
        pi.name = "from";
        pi.type = VariantType(i);
        mi.arguments.push_back(pi);
        mi.return_val.type = p_type;
        p_list->push_back(mi);
    }
}

void Variant::get_constants_for_type(VariantType p_type, ListPOD<StringName> *p_constants) {

    ERR_FAIL_INDEX((int)p_type, (int)VariantType::VARIANT_MAX)

    _VariantCall::ConstantData &cd = _VariantCall::constant_data[(int)p_type];

#ifdef DEBUG_ENABLED
    for (const StringName &E : cd.value_ordered) {

        p_constants->push_back(E);
#else
    for (Map<StringName, int>::Element *E = cd.value.front(); E; E = E->next()) {

        p_constants->push_back(E.first);
#endif
    }

    for (eastl::pair<const StringName,Variant> &E : cd.variant_value) {

        p_constants->push_back(E.first);
    }
}

bool Variant::has_constant(VariantType p_type, const StringName &p_value) {

    ERR_FAIL_INDEX_V((int)p_type, (int)VariantType::VARIANT_MAX, false)
    _VariantCall::ConstantData &cd = _VariantCall::constant_data[(int)p_type];
    return cd.value.contains(p_value) || cd.variant_value.contains(p_value);
}

Variant Variant::get_constant_value(VariantType p_type, const StringName &p_value, bool *r_valid) {

    if (r_valid)
        *r_valid = false;

    ERR_FAIL_INDEX_V((int)p_type, (int)VariantType::VARIANT_MAX, 0)
    _VariantCall::ConstantData &cd = _VariantCall::constant_data[(int)p_type];

    Map<StringName, int>::iterator E = cd.value.find(p_value);
    if (E==cd.value.end()) {
        Map<StringName, Variant>::iterator F = cd.variant_value.find(p_value);
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
    ADDFUNC1R(STRING, INT, String, casecmp_to, STRING, "to")
    ADDFUNC1R(STRING, INT, String, nocasecmp_to, STRING, "to")
    ADDFUNC0R(STRING, INT, String, length)
    ADDFUNC2R(STRING, STRING, String, substr, INT, "from", INT, "len", {-1})

    ADDFUNC2R(STRING, INT, String, find, STRING, "what", INT, "from", {0})
    ADDFUNC3R(STRING, INT, String, count, STRING, "what", INT, "from", INT, "to", 0, 0)
    ADDFUNC3R(STRING, INT, String, countn, STRING, "what", INT, "from", INT, "to", 0, 0)

    ADDFUNC1R(STRING, INT, String, find_last, STRING, "what")
    ADDFUNC2R(STRING, INT, String, findn, STRING, "what", INT, "from", {0})
    ADDFUNC2R(STRING, INT, String, rfind, STRING, "what", INT, "from", {-1})
    ADDFUNC2R(STRING, INT, String, rfindn, STRING, "what", INT, "from", {-1})
    ADDFUNC1R(STRING, BOOL, String, match, STRING, "expr")
    ADDFUNC1R(STRING, BOOL, String, matchn, STRING, "expr")
    ADDFUNC1R(STRING, BOOL, String, begins_with, STRING, "text")
    ADDFUNC1R(STRING, BOOL, String, ends_with, STRING, "text")
    ADDFUNC1R(STRING, BOOL, String, is_subsequence_of, STRING, "text")
    ADDFUNC1R(STRING, BOOL, String, is_subsequence_ofi, STRING, "text")
    ADDFUNC0R(STRING, POOL_STRING_ARRAY, String, bigrams)
    ADDFUNC1R(STRING, REAL, String, similarity, STRING, "text")

    ADDFUNC1R(STRING, STRING, String, format, NIL, "values")
    ADDFUNC2R(STRING, STRING, String, replace, STRING, "what", STRING, "forwhat")
    ADDFUNC2R(STRING, STRING, String, replacen, STRING, "what", STRING, "forwhat")
    ADDFUNC1R(STRING, STRING, String, repeat, INT, "count", varray())
    ADDFUNC2R(STRING, STRING, String, insert, INT, "position", STRING, "what")
    ADDFUNC0R(STRING, STRING, String, capitalize)
    ADDFUNC3R(STRING, POOL_STRING_ARRAY, String, split, STRING, "delimiter", BOOL, "allow_empty", INT, "maxsplit", true, 0)
    ADDFUNC3R(STRING, POOL_STRING_ARRAY, String, rsplit, STRING, "delimiter", BOOL, "allow_empty", INT, "maxsplit", true, 0)
    ADDFUNC2R(STRING, POOL_REAL_ARRAY, String, split_floats, STRING, "delimiter", BOOL, "allow_empty", {true})

    ADDFUNC0R(STRING, STRING, String, to_upper)
    ADDFUNC0R(STRING, STRING, String, to_lower)

    ADDFUNC1R(STRING, STRING, String, left, INT, "position")
    ADDFUNC1R(STRING, STRING, String, right, INT, "position")
    ADDFUNC2R(STRING, STRING, String, strip_edges, BOOL, "left", BOOL, "right", true, true)
    ADDFUNC0R(STRING, STRING, String, strip_escapes)
    ADDFUNC1R(STRING, STRING, String, lstrip, STRING, "chars")
    ADDFUNC1R(STRING, STRING, String, rstrip, STRING, "chars")
    ADDFUNC0R(STRING, STRING, String, get_extension)
    ADDFUNC0R(STRING, STRING, String, get_basename)
    ADDFUNC1R(STRING, STRING, String, plus_file, STRING, "file")
    ADDFUNC1R(STRING, INT, String, ord_at, INT, "at")
    ADDFUNC0R(STRING, STRING, String, dedent)
    ADDFUNC2(STRING, NIL, String, erase, INT, "position", INT, "chars")
    ADDFUNC0R(STRING, INT, String, hash)
    ADDFUNC0R(STRING, STRING, String, md5_text)
    ADDFUNC0R(STRING, STRING, String, sha1_text)
    ADDFUNC0R(STRING, STRING, String, sha256_text)
    ADDFUNC0R(STRING, POOL_BYTE_ARRAY, String, md5_buffer)
    ADDFUNC0R(STRING, POOL_BYTE_ARRAY, String, sha1_buffer)
    ADDFUNC0R(STRING, POOL_BYTE_ARRAY, String, sha256_buffer)
    ADDFUNC0R(STRING, BOOL, String, empty)
    ADDFUNC0R(STRING, BOOL, String, is_abs_path)
    ADDFUNC0R(STRING, BOOL, String, is_rel_path)
    ADDFUNC0R(STRING, STRING, String, get_base_dir)
    ADDFUNC0R(STRING, STRING, String, get_file)
    ADDFUNC0R(STRING, STRING, String, xml_escape)
    ADDFUNC0R(STRING, STRING, String, xml_unescape)
    ADDFUNC0R(STRING, STRING, String, http_escape)
    ADDFUNC0R(STRING, STRING, String, http_unescape)
    ADDFUNC0R(STRING, STRING, String, c_escape)
    ADDFUNC0R(STRING, STRING, String, c_unescape)
    ADDFUNC0R(STRING, STRING, String, json_escape)
    ADDFUNC0R(STRING, STRING, String, percent_encode)
    ADDFUNC0R(STRING, STRING, String, percent_decode)
    ADDFUNC0R(STRING, BOOL, String, is_valid_identifier)
    ADDFUNC0R(STRING, BOOL, String, is_valid_integer)
    ADDFUNC0R(STRING, BOOL, String, is_valid_float)
    ADDFUNC1R(STRING, BOOL, String, is_valid_hex_number, BOOL, "with_prefix", false)
    ADDFUNC0R(STRING, BOOL, String, is_valid_html_color)
    ADDFUNC0R(STRING, BOOL, String, is_valid_ip_address)
    ADDFUNC0R(STRING, BOOL, String, is_valid_filename)
    ADDFUNC0R(STRING, INT, String, to_int)
    ADDFUNC0R(STRING, REAL, String, to_float)
    ADDFUNC0R(STRING, INT, String, hex_to_int)
    ADDFUNC1R(STRING, STRING, String, pad_decimals, INT, "digits")
    ADDFUNC1R(STRING, STRING, String, pad_zeros, INT, "digits")
    ADDFUNC1R(STRING, STRING, String, trim_prefix, STRING, "prefix")
    ADDFUNC1R(STRING, STRING, String, trim_suffix, STRING, "suffix")

    ADDFUNC0R(STRING, POOL_BYTE_ARRAY, String, to_ascii)
    ADDFUNC0R(STRING, POOL_BYTE_ARRAY, String, to_utf8)

    ADDFUNC0R(VECTOR2, VECTOR2, Vector2, normalized)
    ADDFUNC0R(VECTOR2, REAL, Vector2, length)
    ADDFUNC0R(VECTOR2, REAL, Vector2, angle)
    ADDFUNC0R(VECTOR2, REAL, Vector2, length_squared)
    ADDFUNC0R(VECTOR2, BOOL, Vector2, is_normalized)
    ADDFUNC1R(VECTOR2, VECTOR2, Vector2, direction_to, VECTOR2, "b")
    ADDFUNC1R(VECTOR2, REAL, Vector2, distance_to, VECTOR2, "to")
    ADDFUNC1R(VECTOR2, REAL, Vector2, distance_squared_to, VECTOR2, "to")
    ADDFUNC1R(VECTOR2, VECTOR2, Vector2, posmod, REAL, "mod")
    ADDFUNC1R(VECTOR2, VECTOR2, Vector2, posmodv, VECTOR2, "modv")
    ADDFUNC1R(VECTOR2, VECTOR2, Vector2, project, VECTOR2, "b")
    ADDFUNC1R(VECTOR2, REAL, Vector2, angle_to, VECTOR2, "to")
    ADDFUNC1R(VECTOR2, REAL, Vector2, angle_to_point, VECTOR2, "to")
    ADDFUNC2R(VECTOR2, VECTOR2, Vector2, linear_interpolate, VECTOR2, "b", REAL, "t")
    ADDFUNC2R(VECTOR2, VECTOR2, Vector2, slerp, VECTOR2, "b", REAL, "t")
    ADDFUNC4R(VECTOR2, VECTOR2, Vector2, cubic_interpolate, VECTOR2, "b", VECTOR2, "pre_a", VECTOR2, "post_b", REAL, "t")
    ADDFUNC2R(VECTOR2, VECTOR2, Vector2, move_toward, VECTOR2, "to", REAL, "delta")
    ADDFUNC1R(VECTOR2, VECTOR2, Vector2, rotated, REAL, "phi")
    ADDFUNC0R(VECTOR2, VECTOR2, Vector2, tangent)
    ADDFUNC0R(VECTOR2, VECTOR2, Vector2, floor)
    ADDFUNC0R(VECTOR2, VECTOR2, Vector2, ceil)
    ADDFUNC0R(VECTOR2, VECTOR2, Vector2, round)
    ADDFUNC1R(VECTOR2, VECTOR2, Vector2, snapped, VECTOR2, "by")
    ADDFUNC0R(VECTOR2, REAL, Vector2, aspect)
    ADDFUNC1R(VECTOR2, REAL, Vector2, dot, VECTOR2, "with")
    ADDFUNC1R(VECTOR2, VECTOR2, Vector2, slide, VECTOR2, "n")
    ADDFUNC1R(VECTOR2, VECTOR2, Vector2, bounce, VECTOR2, "n")
    ADDFUNC1R(VECTOR2, VECTOR2, Vector2, reflect, VECTOR2, "n")
    ADDFUNC1R(VECTOR2, REAL, Vector2, cross, VECTOR2, "with")
    ADDFUNC0R(VECTOR2, VECTOR2, Vector2, abs)
    ADDFUNC1R(VECTOR2, VECTOR2, Vector2, clamped, REAL, "length")
    ADDFUNC0R(VECTOR2, VECTOR2, Vector2, sign)

    ADDFUNC0R(RECT2, REAL, Rect2, get_area)
    ADDFUNC1R(RECT2, BOOL, Rect2, intersects, RECT2, "b")
    ADDFUNC1R(RECT2, BOOL, Rect2, encloses, RECT2, "b")
    ADDFUNC0R(RECT2, BOOL, Rect2, has_no_area)
    ADDFUNC1R(RECT2, RECT2, Rect2, clip, RECT2, "b")
    ADDFUNC1R(RECT2, RECT2, Rect2, merge, RECT2, "b")
    ADDFUNC1R(RECT2, BOOL, Rect2, has_point, VECTOR2, "point")
    ADDFUNC1R(RECT2, RECT2, Rect2, grow, REAL, "by")
    ADDFUNC2R(RECT2, RECT2, Rect2, grow_margin, INT, "margin", REAL, "by")
    ADDFUNC4R(RECT2, RECT2, Rect2, grow_individual, REAL, "left", REAL, "top", REAL, "right", REAL, " bottom")
    ADDFUNC1R(RECT2, RECT2, Rect2, expand, VECTOR2, "to")
    ADDFUNC0R(RECT2, RECT2, Rect2, abs)

    ADDFUNC0R(VECTOR3, INT, Vector3, min_axis)
    ADDFUNC0R(VECTOR3, INT, Vector3, max_axis)
    ADDFUNC0R(VECTOR3, REAL, Vector3, length)
    ADDFUNC0R(VECTOR3, REAL, Vector3, length_squared)
    ADDFUNC0R(VECTOR3, BOOL, Vector3, is_normalized)
    ADDFUNC0R(VECTOR3, VECTOR3, Vector3, normalized)
    ADDFUNC0R(VECTOR3, VECTOR3, Vector3, inverse)
    ADDFUNC1R(VECTOR3, VECTOR3, Vector3, snapped, VECTOR3, "by")
    ADDFUNC2R(VECTOR3, VECTOR3, Vector3, rotated, VECTOR3, "axis", REAL, "phi")
    ADDFUNC2R(VECTOR3, VECTOR3, Vector3, linear_interpolate, VECTOR3, "b", REAL, "t")
    ADDFUNC2R(VECTOR3, VECTOR3, Vector3, slerp, VECTOR3, "b", REAL, "t")
    ADDFUNC4R(VECTOR3, VECTOR3, Vector3, cubic_interpolate, VECTOR3, "b", VECTOR3, "pre_a", VECTOR3, "post_b", REAL, "t")
    ADDFUNC1R(VECTOR3, VECTOR3, Vector3, direction_to, VECTOR3, "b")
    ADDFUNC2R(VECTOR3, VECTOR3, Vector3, move_toward, VECTOR3, "to", REAL, "delta")
    ADDFUNC1R(VECTOR3, REAL, Vector3, dot, VECTOR3, "b")
    ADDFUNC1R(VECTOR3, VECTOR3, Vector3, cross, VECTOR3, "b")
    ADDFUNC1R(VECTOR3, BASIS, Vector3, outer, VECTOR3, "b")
    ADDFUNC0R(VECTOR3, BASIS, Vector3, to_diagonal_matrix)
    ADDFUNC0R(VECTOR3, VECTOR3, Vector3, abs)
    ADDFUNC0R(VECTOR3, VECTOR3, Vector3, floor)
    ADDFUNC0R(VECTOR3, VECTOR3, Vector3, ceil)
    ADDFUNC0R(VECTOR3, VECTOR3, Vector3, round)
    ADDFUNC1R(VECTOR3, REAL, Vector3, distance_to, VECTOR3, "b")
    ADDFUNC1R(VECTOR3, REAL, Vector3, distance_squared_to, VECTOR3, "b")
    ADDFUNC1R(VECTOR3, VECTOR3, Vector3, posmod, REAL, "mod")
    ADDFUNC1R(VECTOR3, VECTOR3, Vector3, posmodv, VECTOR3, "modv")
    ADDFUNC1R(VECTOR3, VECTOR3, Vector3, project, VECTOR3, "b")
    ADDFUNC1R(VECTOR3, REAL, Vector3, angle_to, VECTOR3, "to")
    ADDFUNC1R(VECTOR3, VECTOR3, Vector3, slide, VECTOR3, "n")
    ADDFUNC1R(VECTOR3, VECTOR3, Vector3, bounce, VECTOR3, "n")
    ADDFUNC1R(VECTOR3, VECTOR3, Vector3, reflect, VECTOR3, "n")
    ADDFUNC0R(VECTOR3, VECTOR3, Vector3, sign)

    ADDFUNC0R(PLANE, PLANE, Plane, normalized)
    ADDFUNC0R(PLANE, VECTOR3, Plane, center)
    ADDFUNC0R(PLANE, VECTOR3, Plane, get_any_point)
    ADDFUNC1R(PLANE, BOOL, Plane, is_point_over, VECTOR3, "point")
    ADDFUNC1R(PLANE, REAL, Plane, distance_to, VECTOR3, "point")
    ADDFUNC2R(PLANE, BOOL, Plane, has_point, VECTOR3, "point", REAL, "epsilon", CMP_EPSILON)
    ADDFUNC1R(PLANE, VECTOR3, Plane, project, VECTOR3, "point")
    ADDFUNC2R(PLANE, VECTOR3, Plane, intersect_3, PLANE, "b", PLANE, "c")
    ADDFUNC2R(PLANE, VECTOR3, Plane, intersects_ray, VECTOR3, "from", VECTOR3, "dir")
    ADDFUNC2R(PLANE, VECTOR3, Plane, intersects_segment, VECTOR3, "begin", VECTOR3, "end")

    ADDFUNC0R(QUAT, REAL, Quat, length)
    ADDFUNC0R(QUAT, REAL, Quat, length_squared)
    ADDFUNC0R(QUAT, QUAT, Quat, normalized)
    ADDFUNC0R(QUAT, BOOL, Quat, is_normalized)
    ADDFUNC0R(QUAT, QUAT, Quat, inverse)
    ADDFUNC1R(QUAT, REAL, Quat, dot, QUAT, "b")
    ADDFUNC1R(QUAT, VECTOR3, Quat, xform, VECTOR3, "v")
    ADDFUNC2R(QUAT, QUAT, Quat, slerp, QUAT, "b", REAL, "t")
    ADDFUNC2R(QUAT, QUAT, Quat, slerpni, QUAT, "b", REAL, "t")
    ADDFUNC4R(QUAT, QUAT, Quat, cubic_slerp, QUAT, "b", QUAT, "pre_a", QUAT, "post_b", REAL, "t")
    ADDFUNC0R(QUAT, VECTOR3, Quat, get_euler)
    ADDFUNC1(QUAT, NIL, Quat, set_euler, VECTOR3, "euler")
    ADDFUNC2(QUAT, NIL, Quat, set_axis_angle, VECTOR3, "axis", REAL, "angle")

    ADDFUNC0R(COLOR, INT, Color, to_argb32)
    ADDFUNC0R(COLOR, INT, Color, to_abgr32)
    ADDFUNC0R(COLOR, INT, Color, to_rgba32)
    ADDFUNC0R(COLOR, INT, Color, to_argb64)
    ADDFUNC0R(COLOR, INT, Color, to_abgr64)
    ADDFUNC0R(COLOR, INT, Color, to_rgba64)
    ADDFUNC0R(COLOR, REAL, Color, get_v)
    ADDFUNC0R(COLOR, COLOR, Color, inverted)
    ADDFUNC0R(COLOR, COLOR, Color, contrasted)
    ADDFUNC2R(COLOR, COLOR, Color, linear_interpolate, COLOR, "b", REAL, "t")
    ADDFUNC1R(COLOR, COLOR, Color, blend, COLOR, "over")
    ADDFUNC1R(COLOR, COLOR, Color, lightened, REAL, "amount")
    ADDFUNC1R(COLOR, COLOR, Color, darkened, REAL, "amount")
    ADDFUNC1R(COLOR, STRING, Color, to_html, BOOL, "with_alpha", {true})
    ADDFUNC4R(COLOR, COLOR, Color, from_hsv, REAL, "h", REAL, "s", REAL, "v", REAL, "a", {1.0})

    ADDFUNC0R(_RID, INT, RID, get_id)

    ADDFUNC0R(NODE_PATH, BOOL, NodePath, is_absolute)
    ADDFUNC0R(NODE_PATH, INT, NodePath, get_name_count)
    ADDFUNC1R(NODE_PATH, STRING, NodePath, get_name, INT, "idx")
    ADDFUNC0R(NODE_PATH, INT, NodePath, get_subname_count)
    ADDFUNC1R(NODE_PATH, STRING, NodePath, get_subname, INT, "idx")
    ADDFUNC0R(NODE_PATH, STRING, NodePath, get_concatenated_subnames)
    ADDFUNC0R(NODE_PATH, NODE_PATH, NodePath, get_as_property_path)
    ADDFUNC0R(NODE_PATH, BOOL, NodePath, is_empty)

    ADDFUNC0R(DICTIONARY, INT, Dictionary, size)
    ADDFUNC0R(DICTIONARY, BOOL, Dictionary, empty)
    ADDFUNC0NC(DICTIONARY, NIL, Dictionary, clear)
    ADDFUNC1R(DICTIONARY, BOOL, Dictionary, has, NIL, "key")
    ADDFUNC1R(DICTIONARY, BOOL, Dictionary, has_all, ARRAY, "keys")
    ADDFUNC1R(DICTIONARY, BOOL, Dictionary, erase, NIL, "key")
    ADDFUNC0R(DICTIONARY, INT, Dictionary, hash)
    ADDFUNC0R(DICTIONARY, ARRAY, Dictionary, keys)
    ADDFUNC0R(DICTIONARY, ARRAY, Dictionary, values)
    ADDFUNC1R(DICTIONARY, DICTIONARY, Dictionary, duplicate, BOOL, "deep", false)
    ADDFUNC2R(DICTIONARY, NIL, Dictionary, get, NIL, "key", NIL, "default", Variant())

    ADDFUNC0R(ARRAY, INT, Array, size)
    ADDFUNC0R(ARRAY, BOOL, Array, empty)
    ADDFUNC0NC(ARRAY, NIL, Array, clear)
    ADDFUNC0R(ARRAY, INT, Array, hash)
    ADDFUNC1NC(ARRAY, NIL, Array, push_back, NIL, "value")
    ADDFUNC1NC(ARRAY, NIL, Array, push_front, NIL, "value")
    ADDFUNC1NC(ARRAY, NIL, Array, append, NIL, "value")
    ADDFUNC1NC(ARRAY, NIL, Array, resize, INT, "size")
    ADDFUNC2NC(ARRAY, NIL, Array, insert, INT, "position", NIL, "value")
    ADDFUNC1NC(ARRAY, NIL, Array, remove, INT, "position")
    ADDFUNC1NC(ARRAY, NIL, Array, erase, NIL, "value")
    ADDFUNC0R(ARRAY, NIL, Array, front)
    ADDFUNC0R(ARRAY, NIL, Array, back)
    ADDFUNC2R(ARRAY, INT, Array, find, NIL, "what", INT, "from", {0})
    ADDFUNC2R(ARRAY, INT, Array, rfind, NIL, "what", INT, "from", {-1})
    ADDFUNC1R(ARRAY, INT, Array, find_last, NIL, "value")
    ADDFUNC1R(ARRAY, INT, Array, count, NIL, "value")
    ADDFUNC1R(ARRAY, BOOL, Array, contains, NIL, "value")
    ADDFUNC0RNC(ARRAY, NIL, Array, pop_back)
    ADDFUNC0RNC(ARRAY, NIL, Array, pop_front)
    ADDFUNC0NC(ARRAY, NIL, Array, sort)
    ADDFUNC2NC(ARRAY, NIL, Array, sort_custom, OBJECT, "obj", STRING, "func")
    ADDFUNC0NC(ARRAY, NIL, Array, shuffle)
    ADDFUNC2R(ARRAY, INT, Array, bsearch, NIL, "value", BOOL, "before", true)
    ADDFUNC4R(ARRAY, INT, Array, bsearch_custom, NIL, "value", OBJECT, "obj", STRING, "func", BOOL, "before", true)
    ADDFUNC0NC(ARRAY, NIL, Array, invert)
    ADDFUNC1R(ARRAY, ARRAY, Array, duplicate, BOOL, "deep", false)
    ADDFUNC4R(ARRAY, ARRAY, Array, slice, INT, "begin", INT, "end", INT, "step", BOOL, "deep", 1, false)
    ADDFUNC0R(ARRAY, NIL, Array, max)
    ADDFUNC0R(ARRAY, NIL, Array, min)

    ADDFUNC0R(POOL_BYTE_ARRAY, INT, PoolByteArray, size)
    ADDFUNC2(POOL_BYTE_ARRAY, NIL, PoolByteArray, set, INT, "idx", INT, "byte")
    ADDFUNC1(POOL_BYTE_ARRAY, NIL, PoolByteArray, push_back, INT, "byte")
    ADDFUNC1(POOL_BYTE_ARRAY, NIL, PoolByteArray, append, INT, "byte")
    ADDFUNC1(POOL_BYTE_ARRAY, NIL, PoolByteArray, append_array, POOL_BYTE_ARRAY, "array")
    ADDFUNC1(POOL_BYTE_ARRAY, NIL, PoolByteArray, remove, INT, "idx")
    ADDFUNC2R(POOL_BYTE_ARRAY, INT, PoolByteArray, insert, INT, "idx", INT, "byte")
    ADDFUNC1(POOL_BYTE_ARRAY, NIL, PoolByteArray, resize, INT, "idx")
    ADDFUNC0(POOL_BYTE_ARRAY, NIL, PoolByteArray, invert)
    ADDFUNC2R(POOL_BYTE_ARRAY, POOL_BYTE_ARRAY, PoolByteArray, subarray, INT, "from", INT, "to")

    ADDFUNC0R(POOL_BYTE_ARRAY, STRING, PoolByteArray, get_string_from_ascii)
    ADDFUNC0R(POOL_BYTE_ARRAY, STRING, PoolByteArray, get_string_from_utf8)
    ADDFUNC0R(POOL_BYTE_ARRAY, STRING, PoolByteArray, hex_encode)
    ADDFUNC1R(POOL_BYTE_ARRAY, POOL_BYTE_ARRAY, PoolByteArray, compress, INT, "compression_mode", {0})
    ADDFUNC2R(POOL_BYTE_ARRAY, POOL_BYTE_ARRAY, PoolByteArray, decompress, INT, "buffer_size", INT, "compression_mode", {0})

    ADDFUNC0R(POOL_INT_ARRAY, INT, PoolIntArray, size)
    ADDFUNC2(POOL_INT_ARRAY, NIL, PoolIntArray, set, INT, "idx", INT, "integer")
    ADDFUNC1(POOL_INT_ARRAY, NIL, PoolIntArray, push_back, INT, "integer")
    ADDFUNC1(POOL_INT_ARRAY, NIL, PoolIntArray, append, INT, "integer")
    ADDFUNC1(POOL_INT_ARRAY, NIL, PoolIntArray, append_array, POOL_INT_ARRAY, "array")
    ADDFUNC1(POOL_INT_ARRAY, NIL, PoolIntArray, remove, INT, "idx")
    ADDFUNC2R(POOL_INT_ARRAY, INT, PoolIntArray, insert, INT, "idx", INT, "integer")
    ADDFUNC1(POOL_INT_ARRAY, NIL, PoolIntArray, resize, INT, "idx")
    ADDFUNC0(POOL_INT_ARRAY, NIL, PoolIntArray, invert)

    ADDFUNC0R(POOL_REAL_ARRAY, INT, PoolRealArray, size)
    ADDFUNC2(POOL_REAL_ARRAY, NIL, PoolRealArray, set, INT, "idx", REAL, "value")
    ADDFUNC1(POOL_REAL_ARRAY, NIL, PoolRealArray, push_back, REAL, "value")
    ADDFUNC1(POOL_REAL_ARRAY, NIL, PoolRealArray, append, REAL, "value")
    ADDFUNC1(POOL_REAL_ARRAY, NIL, PoolRealArray, append_array, POOL_REAL_ARRAY, "array")
    ADDFUNC1(POOL_REAL_ARRAY, NIL, PoolRealArray, remove, INT, "idx")
    ADDFUNC2R(POOL_REAL_ARRAY, INT, PoolRealArray, insert, INT, "idx", REAL, "value")
    ADDFUNC1(POOL_REAL_ARRAY, NIL, PoolRealArray, resize, INT, "idx")
    ADDFUNC0(POOL_REAL_ARRAY, NIL, PoolRealArray, invert)

    ADDFUNC0R(POOL_STRING_ARRAY, INT, PoolStringArray, size)
    ADDFUNC2(POOL_STRING_ARRAY, NIL, PoolStringArray, set, INT, "idx", STRING, "string")
    ADDFUNC1(POOL_STRING_ARRAY, NIL, PoolStringArray, push_back, STRING, "string")
    ADDFUNC1(POOL_STRING_ARRAY, NIL, PoolStringArray, append, STRING, "string")
    ADDFUNC1(POOL_STRING_ARRAY, NIL, PoolStringArray, append_array, POOL_STRING_ARRAY, "array")
    ADDFUNC1(POOL_STRING_ARRAY, NIL, PoolStringArray, remove, INT, "idx")
    ADDFUNC2R(POOL_STRING_ARRAY, INT, PoolStringArray, insert, INT, "idx", STRING, "string")
    ADDFUNC1(POOL_STRING_ARRAY, NIL, PoolStringArray, resize, INT, "idx")
    ADDFUNC0(POOL_STRING_ARRAY, NIL, PoolStringArray, invert)
    ADDFUNC1(POOL_STRING_ARRAY, STRING, PoolStringArray, join, STRING, "delimiter")

    ADDFUNC0R(POOL_VECTOR2_ARRAY, INT, PoolVector2Array, size)
    ADDFUNC2(POOL_VECTOR2_ARRAY, NIL, PoolVector2Array, set, INT, "idx", VECTOR2, "vector2")
    ADDFUNC1(POOL_VECTOR2_ARRAY, NIL, PoolVector2Array, push_back, VECTOR2, "vector2")
    ADDFUNC1(POOL_VECTOR2_ARRAY, NIL, PoolVector2Array, append, VECTOR2, "vector2")
    ADDFUNC1(POOL_VECTOR2_ARRAY, NIL, PoolVector2Array, append_array, POOL_VECTOR2_ARRAY, "array")
    ADDFUNC1(POOL_VECTOR2_ARRAY, NIL, PoolVector2Array, remove, INT, "idx")
    ADDFUNC2R(POOL_VECTOR2_ARRAY, INT, PoolVector2Array, insert, INT, "idx", VECTOR2, "vector2")
    ADDFUNC1(POOL_VECTOR2_ARRAY, NIL, PoolVector2Array, resize, INT, "idx")
    ADDFUNC0(POOL_VECTOR2_ARRAY, NIL, PoolVector2Array, invert)

    ADDFUNC0R(POOL_VECTOR3_ARRAY, INT, PoolVector3Array, size)
    ADDFUNC2(POOL_VECTOR3_ARRAY, NIL, PoolVector3Array, set, INT, "idx", VECTOR3, "vector3")
    ADDFUNC1(POOL_VECTOR3_ARRAY, NIL, PoolVector3Array, push_back, VECTOR3, "vector3")
    ADDFUNC1(POOL_VECTOR3_ARRAY, NIL, PoolVector3Array, append, VECTOR3, "vector3")
    ADDFUNC1(POOL_VECTOR3_ARRAY, NIL, PoolVector3Array, append_array, POOL_VECTOR3_ARRAY, "array")
    ADDFUNC1(POOL_VECTOR3_ARRAY, NIL, PoolVector3Array, remove, INT, "idx")
    ADDFUNC2R(POOL_VECTOR3_ARRAY, INT, PoolVector3Array, insert, INT, "idx", VECTOR3, "vector3")
    ADDFUNC1(POOL_VECTOR3_ARRAY, NIL, PoolVector3Array, resize, INT, "idx")
    ADDFUNC0(POOL_VECTOR3_ARRAY, NIL, PoolVector3Array, invert)

    ADDFUNC0R(POOL_COLOR_ARRAY, INT, PoolColorArray, size)
    ADDFUNC2(POOL_COLOR_ARRAY, NIL, PoolColorArray, set, INT, "idx", COLOR, "color")
    ADDFUNC1(POOL_COLOR_ARRAY, NIL, PoolColorArray, push_back, COLOR, "color")
    ADDFUNC1(POOL_COLOR_ARRAY, NIL, PoolColorArray, append, COLOR, "color")
    ADDFUNC1(POOL_COLOR_ARRAY, NIL, PoolColorArray, append_array, POOL_COLOR_ARRAY, "array")
    ADDFUNC1(POOL_COLOR_ARRAY, NIL, PoolColorArray, remove, INT, "idx")
    ADDFUNC2R(POOL_COLOR_ARRAY, INT, PoolColorArray, insert, INT, "idx", COLOR, "color")
    ADDFUNC1(POOL_COLOR_ARRAY, NIL, PoolColorArray, resize, INT, "idx")
    ADDFUNC0(POOL_COLOR_ARRAY, NIL, PoolColorArray, invert)

    //pointerbased

    ADDFUNC0R(AABB, REAL, AABB, get_area)
    ADDFUNC0R(AABB, BOOL, AABB, has_no_area)
    ADDFUNC0R(AABB, BOOL, AABB, has_no_surface)
    ADDFUNC1R(AABB, BOOL, AABB, intersects, AABB, "with")
    ADDFUNC1R(AABB, BOOL, AABB, encloses, AABB, "with")
    ADDFUNC1R(AABB, AABB, AABB, merge, AABB, "with")
    ADDFUNC1R(AABB, AABB, AABB, intersection, AABB, "with")
    ADDFUNC1R(AABB, BOOL, AABB, intersects_plane, PLANE, "plane")
    ADDFUNC2R(AABB, BOOL, AABB, intersects_segment, VECTOR3, "from", VECTOR3, "to")
    ADDFUNC1R(AABB, BOOL, AABB, has_point, VECTOR3, "point")
    ADDFUNC1R(AABB, VECTOR3, AABB, get_support, VECTOR3, "dir")
    ADDFUNC0R(AABB, VECTOR3, AABB, get_longest_axis)
    ADDFUNC0R(AABB, INT, AABB, get_longest_axis_index)
    ADDFUNC0R(AABB, REAL, AABB, get_longest_axis_size)
    ADDFUNC0R(AABB, VECTOR3, AABB, get_shortest_axis)
    ADDFUNC0R(AABB, INT, AABB, get_shortest_axis_index)
    ADDFUNC0R(AABB, REAL, AABB, get_shortest_axis_size)
    ADDFUNC1R(AABB, AABB, AABB, expand, VECTOR3, "to_point")
    ADDFUNC1R(AABB, AABB, AABB, grow, REAL, "by")
    ADDFUNC1R(AABB, VECTOR3, AABB, get_endpoint, INT, "idx")

    ADDFUNC0R(TRANSFORM2D, TRANSFORM2D, Transform2D, inverse)
    ADDFUNC0R(TRANSFORM2D, TRANSFORM2D, Transform2D, affine_inverse)
    ADDFUNC0R(TRANSFORM2D, REAL, Transform2D, get_rotation)
    ADDFUNC0R(TRANSFORM2D, VECTOR2, Transform2D, get_origin)
    ADDFUNC0R(TRANSFORM2D, VECTOR2, Transform2D, get_scale)
    ADDFUNC0R(TRANSFORM2D, TRANSFORM2D, Transform2D, orthonormalized)
    ADDFUNC1R(TRANSFORM2D, TRANSFORM2D, Transform2D, rotated, REAL, "phi")
    ADDFUNC1R(TRANSFORM2D, TRANSFORM2D, Transform2D, scaled, VECTOR2, "scale")
    ADDFUNC1R(TRANSFORM2D, TRANSFORM2D, Transform2D, translated, VECTOR2, "offset")
    ADDFUNC1R(TRANSFORM2D, NIL, Transform2D, xform, NIL, "v")
    ADDFUNC1R(TRANSFORM2D, NIL, Transform2D, xform_inv, NIL, "v")
    ADDFUNC1R(TRANSFORM2D, VECTOR2, Transform2D, basis_xform, VECTOR2, "v")
    ADDFUNC1R(TRANSFORM2D, VECTOR2, Transform2D, basis_xform_inv, VECTOR2, "v")
    ADDFUNC2R(TRANSFORM2D, TRANSFORM2D, Transform2D, interpolate_with, TRANSFORM2D, "transform", REAL, "weight")

    ADDFUNC0R(BASIS, BASIS, Basis, inverse)
    ADDFUNC0R(BASIS, BASIS, Basis, transposed)
    ADDFUNC0R(BASIS, BASIS, Basis, orthonormalized)
    ADDFUNC0R(BASIS, REAL, Basis, determinant)
    ADDFUNC2R(BASIS, BASIS, Basis, rotated, VECTOR3, "axis", REAL, "phi")
    ADDFUNC1R(BASIS, BASIS, Basis, scaled, VECTOR3, "scale")
    ADDFUNC0R(BASIS, VECTOR3, Basis, get_scale)
    ADDFUNC0R(BASIS, VECTOR3, Basis, get_euler)
    ADDFUNC1R(BASIS, REAL, Basis, tdotx, VECTOR3, "with")
    ADDFUNC1R(BASIS, REAL, Basis, tdoty, VECTOR3, "with")
    ADDFUNC1R(BASIS, REAL, Basis, tdotz, VECTOR3, "with")
    ADDFUNC1R(BASIS, VECTOR3, Basis, xform, VECTOR3, "v")
    ADDFUNC1R(BASIS, VECTOR3, Basis, xform_inv, VECTOR3, "v")
    ADDFUNC0R(BASIS, INT, Basis, get_orthogonal_index)
    ADDFUNC2R(BASIS, BASIS, Basis, slerp, BASIS, "b", REAL, "t")
    ADDFUNC2R(BASIS, BOOL, Basis, is_equal_approx, BASIS, "b", REAL, "epsilon", CMP_EPSILON)
    ADDFUNC0R(BASIS, QUAT, Basis, get_rotation_quat)

    ADDFUNC0R(TRANSFORM, TRANSFORM, Transform, inverse)
    ADDFUNC0R(TRANSFORM, TRANSFORM, Transform, affine_inverse)
    ADDFUNC0R(TRANSFORM, TRANSFORM, Transform, orthonormalized)
    ADDFUNC2R(TRANSFORM, TRANSFORM, Transform, rotated, VECTOR3, "axis", REAL, "phi")
    ADDFUNC1R(TRANSFORM, TRANSFORM, Transform, scaled, VECTOR3, "scale")
    ADDFUNC1R(TRANSFORM, TRANSFORM, Transform, translated, VECTOR3, "ofs")
    ADDFUNC2R(TRANSFORM, TRANSFORM, Transform, looking_at, VECTOR3, "target", VECTOR3, "up")
    ADDFUNC2R(TRANSFORM, TRANSFORM, Transform, interpolate_with, TRANSFORM, "transform", REAL, "weight")
    ADDFUNC1R(TRANSFORM, NIL, Transform, xform, NIL, "v")
    ADDFUNC1R(TRANSFORM, NIL, Transform, xform_inv, NIL, "v")

    /* REGISTER CONSTRUCTORS */

    _VariantCall::add_constructor(_VariantCall::Vector2_init1, VariantType::VECTOR2, "x", VariantType::REAL, "y", VariantType::REAL);

    _VariantCall::add_constructor(_VariantCall::Rect2_init1, VariantType::RECT2, "position", VariantType::VECTOR2, "size", VariantType::VECTOR2);
    _VariantCall::add_constructor(_VariantCall::Rect2_init2, VariantType::RECT2, "x", VariantType::REAL, "y", VariantType::REAL, "width", VariantType::REAL, "height", VariantType::REAL);

    _VariantCall::add_constructor(_VariantCall::Transform2D_init2, VariantType::TRANSFORM2D, "rotation", VariantType::REAL, "position", VariantType::VECTOR2);
    _VariantCall::add_constructor(_VariantCall::Transform2D_init3, VariantType::TRANSFORM2D, "x_axis", VariantType::VECTOR2, "y_axis", VariantType::VECTOR2, "origin", VariantType::VECTOR2);

    _VariantCall::add_constructor(_VariantCall::Vector3_init1, VariantType::VECTOR3, "x", VariantType::REAL, "y", VariantType::REAL, "z", VariantType::REAL);

    _VariantCall::add_constructor(_VariantCall::Plane_init1, VariantType::PLANE, "a", VariantType::REAL, "b", VariantType::REAL, "c", VariantType::REAL, "d", VariantType::REAL);
    _VariantCall::add_constructor(_VariantCall::Plane_init2, VariantType::PLANE, "v1", VariantType::VECTOR3, "v2", VariantType::VECTOR3, "v3", VariantType::VECTOR3);
    _VariantCall::add_constructor(_VariantCall::Plane_init3, VariantType::PLANE, "normal", VariantType::VECTOR3, "d", VariantType::REAL);

    _VariantCall::add_constructor(_VariantCall::Quat_init1, VariantType::QUAT, "x", VariantType::REAL, "y", VariantType::REAL, "z", VariantType::REAL, "w", VariantType::REAL);
    _VariantCall::add_constructor(_VariantCall::Quat_init2, VariantType::QUAT, "axis", VariantType::VECTOR3, "angle", VariantType::REAL);
    _VariantCall::add_constructor(_VariantCall::Quat_init3, VariantType::QUAT, "euler", VariantType::VECTOR3);

    _VariantCall::add_constructor(_VariantCall::Color_init1, VariantType::COLOR, "r", VariantType::REAL, "g", VariantType::REAL, "b", VariantType::REAL, "a", VariantType::REAL);
    _VariantCall::add_constructor(_VariantCall::Color_init2, VariantType::COLOR, "r", VariantType::REAL, "g", VariantType::REAL, "b", VariantType::REAL);

    _VariantCall::add_constructor(_VariantCall::AABB_init1, VariantType::AABB, "position", VariantType::VECTOR3, "size", VariantType::VECTOR3);

    _VariantCall::add_constructor(_VariantCall::Basis_init1, VariantType::BASIS, "x_axis", VariantType::VECTOR3, "y_axis", VariantType::VECTOR3, "z_axis", VariantType::VECTOR3);
    _VariantCall::add_constructor(_VariantCall::Basis_init2, VariantType::BASIS, "axis", VariantType::VECTOR3, "phi", VariantType::REAL);

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
