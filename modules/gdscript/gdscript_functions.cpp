/*************************************************************************/
/*  gdscript_functions.cpp                                               */
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

#include "gdscript_functions.h"

#include "gdscript.h"

#include "core/class_db.h"
#include "core/color.h"
#include "core/func_ref.h"
#include "core/io/json.h"
#include "core/io/marshalls.h"
#include "core/io/resource_loader.h"
#include "core/math/aabb.h"
#include "core/math/basis.h"
#include "core/math/face3.h"
#include "core/math/math_funcs.h"
#include "core/math/plane.h"
#include "core/math/quat.h"
#include "core/math/transform.h"
#include "core/math/transform_2d.h"
#include "core/math/vector3.h"
#include "core/method_info.h"
#include "core/object_db.h"
#include "core/os/os.h"
#include "core/pool_vector.h"
#include "core/print_string.h"
#include "core/reference.h"
#include "core/se_string.h"
#include "core/string_formatter.h"
#include "core/string_utils.h"
#include "core/translation_helpers.h"
#include "core/variant_parser.h"
#include "core/vector.h"

using namespace eastl;
const char *GDScriptFunctions::get_func_name(Function p_func) {

    ERR_FAIL_INDEX_V(p_func, FUNC_MAX, "")

    static const char *_names[FUNC_MAX] = {
        "sin",
        "cos",
        "tan",
        "sinh",
        "cosh",
        "tanh",
        "asin",
        "acos",
        "atan",
        "atan2",
        "sqrt",
        "fmod",
        "fposmod",
        "posmod",
        "floor",
        "ceil",
        "round",
        "abs",
        "sign",
        "pow",
        "log",
        "exp",
        "is_nan",
        "is_inf",
        "is_equal_approx",
        "is_zero_approx",
        "ease",
        "decimals",
        "step_decimals",
        "stepify",
        "lerp",
        "lerp_angle",
        "inverse_lerp",
        "range_lerp",
        "smoothstep",
        "move_toward",
        "dectime",
        "randomize",
        "randi",
        "randf",
        "rand_range",
        "seed",
        "rand_seed",
        "deg2rad",
        "rad2deg",
        "linear2db",
        "db2linear",
        "polar2cartesian",
        "cartesian2polar",
        "wrapi",
        "wrapf",
        "max",
        "min",
        "clamp",
        "nearest_po2",
        "weakref",
        "funcref",
        "convert",
        "typeof",
        "type_exists",
        "char",
        "ord",
        "str",
        "print",
        "printt",
        "prints",
        "printerr",
        "printraw",
        "print_debug",
        "push_error",
        "push_warning",
        "var2str",
        "str2var",
        "var2bytes",
        "bytes2var",
        "range",
        "load",
        "inst2dict",
        "dict2inst",
        "validate_json",
        "parse_json",
        "to_json",
        "hash",
        "Color8",
        "ColorN",
        "print_stack",
        "get_stack",
        "instance_from_id",
        "len",
        "is_instance_valid",
    };

    return _names[p_func];
}

void GDScriptFunctions::call(Function p_func, const Variant **p_args, int p_arg_count, Variant &r_ret, Variant::CallError &r_error) {

    r_error.error = Variant::CallError::CALL_OK;
#ifdef DEBUG_ENABLED

#define VALIDATE_ARG_COUNT(m_count)                                        \
    if (p_arg_count < m_count) {                                           \
        r_error.error = Variant::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;  \
        r_error.argument = m_count;                                        \
        r_ret = Variant();                                                 \
        return;                                                            \
    }                                                                      \
    if (p_arg_count > m_count) {                                           \
        r_error.error = Variant::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS; \
        r_error.argument = m_count;                                        \
        r_ret = Variant();                                                 \
        return;                                                            \
    }

#define VALIDATE_ARG_NUM(m_arg)                                          \
    if (!p_args[m_arg]->is_num()) {                                      \
        r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT; \
        r_error.argument = m_arg;                                        \
        r_error.expected = VariantType::REAL;                                \
        r_ret = Variant();                                               \
        return;                                                          \
    }

#else

#define VALIDATE_ARG_COUNT(m_count)
#define VALIDATE_ARG_NUM(m_arg)
#endif

    //using a switch, so the compiler generates a jumptable

    switch (p_func) {

        case MATH_SIN: {
            VALIDATE_ARG_COUNT(1);
            VALIDATE_ARG_NUM(0);
            r_ret = Math::sin((double)*p_args[0]);
        } break;
        case MATH_COS: {
            VALIDATE_ARG_COUNT(1);
            VALIDATE_ARG_NUM(0);
            r_ret = Math::cos((double)*p_args[0]);
        } break;
        case MATH_TAN: {
            VALIDATE_ARG_COUNT(1);
            VALIDATE_ARG_NUM(0);
            r_ret = Math::tan((double)*p_args[0]);
        } break;
        case MATH_SINH: {
            VALIDATE_ARG_COUNT(1);
            VALIDATE_ARG_NUM(0);
            r_ret = Math::sinh((double)*p_args[0]);
        } break;
        case MATH_COSH: {
            VALIDATE_ARG_COUNT(1);
            VALIDATE_ARG_NUM(0);
            r_ret = Math::cosh((double)*p_args[0]);
        } break;
        case MATH_TANH: {
            VALIDATE_ARG_COUNT(1);
            VALIDATE_ARG_NUM(0);
            r_ret = Math::tanh((double)*p_args[0]);
        } break;
        case MATH_ASIN: {
            VALIDATE_ARG_COUNT(1);
            VALIDATE_ARG_NUM(0);
            r_ret = Math::asin((double)*p_args[0]);
        } break;
        case MATH_ACOS: {
            VALIDATE_ARG_COUNT(1);
            VALIDATE_ARG_NUM(0);
            r_ret = Math::acos((double)*p_args[0]);
        } break;
        case MATH_ATAN: {
            VALIDATE_ARG_COUNT(1);
            VALIDATE_ARG_NUM(0);
            r_ret = Math::atan((double)*p_args[0]);
        } break;
        case MATH_ATAN2: {
            VALIDATE_ARG_COUNT(2);
            VALIDATE_ARG_NUM(0);
            VALIDATE_ARG_NUM(1);
            r_ret = Math::atan2((double)*p_args[0], (double)*p_args[1]);
        } break;
        case MATH_SQRT: {
            VALIDATE_ARG_COUNT(1);
            VALIDATE_ARG_NUM(0);
            r_ret = Math::sqrt((double)*p_args[0]);
        } break;
        case MATH_FMOD: {
            VALIDATE_ARG_COUNT(2);
            VALIDATE_ARG_NUM(0);
            VALIDATE_ARG_NUM(1);
            r_ret = Math::fmod((double)*p_args[0], (double)*p_args[1]);
        } break;
        case MATH_FPOSMOD: {
            VALIDATE_ARG_COUNT(2);
            VALIDATE_ARG_NUM(0);
            VALIDATE_ARG_NUM(1);
            r_ret = Math::fposmod((double)*p_args[0], (double)*p_args[1]);
        } break;
        case MATH_POSMOD: {
            VALIDATE_ARG_COUNT(2);
            VALIDATE_ARG_NUM(0);
            VALIDATE_ARG_NUM(1);
            r_ret = Math::posmod((int)*p_args[0], (int)*p_args[1]);
        } break;
        case MATH_FLOOR: {
            VALIDATE_ARG_COUNT(1);
            VALIDATE_ARG_NUM(0);
            r_ret = Math::floor((double)*p_args[0]);
        } break;
        case MATH_CEIL: {
            VALIDATE_ARG_COUNT(1);
            VALIDATE_ARG_NUM(0);
            r_ret = Math::ceil((double)*p_args[0]);
        } break;
        case MATH_ROUND: {
            VALIDATE_ARG_COUNT(1);
            VALIDATE_ARG_NUM(0);
            r_ret = Math::round((double)*p_args[0]);
        } break;
        case MATH_ABS: {
            VALIDATE_ARG_COUNT(1);
            if (p_args[0]->get_type() == VariantType::INT) {

                int64_t i = *p_args[0];
                r_ret = ABS(i);
            } else if (p_args[0]->get_type() == VariantType::REAL) {

                double r = *p_args[0];
                r_ret = Math::abs(r);
            } else {

                r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                r_error.argument = 0;
                r_error.expected = VariantType::REAL;
                r_ret = Variant();
            }
        } break;
        case MATH_SIGN: {
            VALIDATE_ARG_COUNT(1);
            if (p_args[0]->get_type() == VariantType::INT) {

                int64_t i = *p_args[0];
                r_ret = i < 0 ? -1 : (i > 0 ? +1 : 0);
            } else if (p_args[0]->get_type() == VariantType::REAL) {

                real_t r = *p_args[0];
                r_ret = r < 0.0 ? -1.0 : (r > 0.0 ? +1.0 : 0.0);
            } else {

                r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                r_error.argument = 0;
                r_error.expected = VariantType::REAL;
                r_ret = Variant();
            }
        } break;
        case MATH_POW: {
            VALIDATE_ARG_COUNT(2);
            VALIDATE_ARG_NUM(0);
            VALIDATE_ARG_NUM(1);
            r_ret = Math::pow((double)*p_args[0], (double)*p_args[1]);
        } break;
        case MATH_LOG: {
            VALIDATE_ARG_COUNT(1);
            VALIDATE_ARG_NUM(0);
            r_ret = Math::log((double)*p_args[0]);
        } break;
        case MATH_EXP: {
            VALIDATE_ARG_COUNT(1);
            VALIDATE_ARG_NUM(0);
            r_ret = Math::exp((double)*p_args[0]);
        } break;
        case MATH_ISNAN: {
            VALIDATE_ARG_COUNT(1);
            VALIDATE_ARG_NUM(0);
            r_ret = Math::is_nan((double)*p_args[0]);
        } break;
        case MATH_ISINF: {
            VALIDATE_ARG_COUNT(1);
            VALIDATE_ARG_NUM(0);
            r_ret = Math::is_inf((double)*p_args[0]);
        } break;
        case MATH_ISEQUALAPPROX: {
            VALIDATE_ARG_COUNT(2);
            VALIDATE_ARG_NUM(0);
            VALIDATE_ARG_NUM(1);
            r_ret = Math::is_equal_approx((real_t)*p_args[0], (real_t)*p_args[1]);
        } break;
        case MATH_ISZEROAPPROX: {
            VALIDATE_ARG_COUNT(1);
            VALIDATE_ARG_NUM(0);
            r_ret = Math::is_zero_approx((real_t)*p_args[0]);
        } break;
        case MATH_EASE: {
            VALIDATE_ARG_COUNT(2);
            VALIDATE_ARG_NUM(0);
            VALIDATE_ARG_NUM(1);
            r_ret = Math::ease((double)*p_args[0], (double)*p_args[1]);
        } break;
        case MATH_DECIMALS: {
            VALIDATE_ARG_COUNT(1);
            VALIDATE_ARG_NUM(0);
            r_ret = Math::step_decimals((double)*p_args[0]);
            WARN_DEPRECATED_MSG("GDScript method 'decimals' is deprecated and has been renamed to 'step_decimals', please update your code accordingly.");
        } break;
        case MATH_STEP_DECIMALS: {
            VALIDATE_ARG_COUNT(1);
            VALIDATE_ARG_NUM(0);
            r_ret = Math::step_decimals((double)*p_args[0]);
        } break;
        case MATH_STEPIFY: {
            VALIDATE_ARG_COUNT(2);
            VALIDATE_ARG_NUM(0);
            VALIDATE_ARG_NUM(1);
            r_ret = Math::stepify((double)*p_args[0], (double)*p_args[1]);
        } break;
        case MATH_LERP: {
            VALIDATE_ARG_COUNT(3);
            VALIDATE_ARG_NUM(2);
            const double t = (double)*p_args[2];
            switch (p_args[0]->get_type() == p_args[1]->get_type() ? p_args[0]->get_type() : VariantType::REAL) {
                case VariantType::VECTOR2: {
                    r_ret = ((Vector2)*p_args[0]).linear_interpolate((Vector2)*p_args[1], t);
                } break;
                case VariantType::VECTOR3: {
                    r_ret = ((Vector3)*p_args[0]).linear_interpolate((Vector3)*p_args[1], t);
                } break;
                case VariantType::COLOR: {
                    r_ret = ((Color)*p_args[0]).linear_interpolate((Color)*p_args[1], t);
                } break;
                default: {
                    VALIDATE_ARG_NUM(0);
                    VALIDATE_ARG_NUM(1);
                    r_ret = Math::lerp((double)*p_args[0], (double)*p_args[1], t);
                } break;
            }
        } break;
        case MATH_LERP_ANGLE: {
            VALIDATE_ARG_COUNT(3);
            VALIDATE_ARG_NUM(0);
            VALIDATE_ARG_NUM(1);
            VALIDATE_ARG_NUM(2);
            r_ret = Math::lerp_angle((double)*p_args[0], (double)*p_args[1], (double)*p_args[2]);
        } break;
        case MATH_INVERSE_LERP: {
            VALIDATE_ARG_COUNT(3);
            VALIDATE_ARG_NUM(0);
            VALIDATE_ARG_NUM(1);
            VALIDATE_ARG_NUM(2);
            r_ret = Math::inverse_lerp((double)*p_args[0], (double)*p_args[1], (double)*p_args[2]);
        } break;
        case MATH_RANGE_LERP: {
            VALIDATE_ARG_COUNT(5);
            VALIDATE_ARG_NUM(0);
            VALIDATE_ARG_NUM(1);
            VALIDATE_ARG_NUM(2);
            VALIDATE_ARG_NUM(3);
            VALIDATE_ARG_NUM(4);
            r_ret = Math::range_lerp((double)*p_args[0], (double)*p_args[1], (double)*p_args[2], (double)*p_args[3], (double)*p_args[4]);
        } break;
        case MATH_SMOOTHSTEP: {
            VALIDATE_ARG_COUNT(3);
            VALIDATE_ARG_NUM(0);
            VALIDATE_ARG_NUM(1);
            VALIDATE_ARG_NUM(2);
            r_ret = Math::smoothstep((double)*p_args[0], (double)*p_args[1], (double)*p_args[2]);
        } break;
        case MATH_MOVE_TOWARD: {
            VALIDATE_ARG_COUNT(3);
            VALIDATE_ARG_NUM(0);
            VALIDATE_ARG_NUM(1);
            VALIDATE_ARG_NUM(2);
            r_ret = Math::move_toward((double)*p_args[0], (double)*p_args[1], (double)*p_args[2]);
        } break;
        case MATH_DECTIME: {
            VALIDATE_ARG_COUNT(3);
            VALIDATE_ARG_NUM(0);
            VALIDATE_ARG_NUM(1);
            VALIDATE_ARG_NUM(2);
            r_ret = Math::dectime((double)*p_args[0], (double)*p_args[1], (double)*p_args[2]);
        } break;
        case MATH_RANDOMIZE: {
            VALIDATE_ARG_COUNT(0);
            Math::randomize();
            r_ret = Variant();
        } break;
        case MATH_RAND: {
            VALIDATE_ARG_COUNT(0);
            r_ret = Math::rand();
        } break;
        case MATH_RANDF: {
            VALIDATE_ARG_COUNT(0);
            r_ret = Math::randf();
        } break;
        case MATH_RANDOM: {
            VALIDATE_ARG_COUNT(2);
            VALIDATE_ARG_NUM(0);
            VALIDATE_ARG_NUM(1);
            r_ret = Math::random((double)*p_args[0], (double)*p_args[1]);
        } break;
        case MATH_SEED: {
            VALIDATE_ARG_COUNT(1);
            VALIDATE_ARG_NUM(0);
            uint64_t seed = *p_args[0];
            Math::seed(seed);
            r_ret = Variant();
        } break;
        case MATH_RANDSEED: {
            VALIDATE_ARG_COUNT(1);
            VALIDATE_ARG_NUM(0);
            uint64_t seed = *p_args[0];
            int ret = Math::rand_from_seed(&seed);
            Array reta;
            reta.push_back(ret);
            reta.push_back(seed);
            r_ret = reta;

        } break;
        case MATH_DEG2RAD: {
            VALIDATE_ARG_COUNT(1);
            VALIDATE_ARG_NUM(0);
            r_ret = Math::deg2rad((double)*p_args[0]);
        } break;
        case MATH_RAD2DEG: {
            VALIDATE_ARG_COUNT(1);
            VALIDATE_ARG_NUM(0);
            r_ret = Math::rad2deg((double)*p_args[0]);
        } break;
        case MATH_LINEAR2DB: {
            VALIDATE_ARG_COUNT(1);
            VALIDATE_ARG_NUM(0);
            r_ret = Math::linear2db((double)*p_args[0]);
        } break;
        case MATH_DB2LINEAR: {
            VALIDATE_ARG_COUNT(1);
            VALIDATE_ARG_NUM(0);
            r_ret = Math::db2linear((double)*p_args[0]);
        } break;
        case MATH_POLAR2CARTESIAN: {
            VALIDATE_ARG_COUNT(2);
            VALIDATE_ARG_NUM(0);
            VALIDATE_ARG_NUM(1);
            double r = *p_args[0];
            double th = *p_args[1];
            r_ret = Vector2(r * Math::cos(th), r * Math::sin(th));
        } break;
        case MATH_CARTESIAN2POLAR: {
            VALIDATE_ARG_COUNT(2);
            VALIDATE_ARG_NUM(0);
            VALIDATE_ARG_NUM(1);
            double x = *p_args[0];
            double y = *p_args[1];
            r_ret = Vector2(Math::sqrt(x * x + y * y), Math::atan2(y, x));
        } break;
        case MATH_WRAP: {
            VALIDATE_ARG_COUNT(3);
            r_ret = Math::wrapi((int64_t)*p_args[0], (int64_t)*p_args[1], (int64_t)*p_args[2]);
        } break;
        case MATH_WRAPF: {
            VALIDATE_ARG_COUNT(3);
            r_ret = Math::wrapf((double)*p_args[0], (double)*p_args[1], (double)*p_args[2]);
        } break;
        case LOGIC_MAX: {
            VALIDATE_ARG_COUNT(2);
            if (p_args[0]->get_type() == VariantType::INT && p_args[1]->get_type() == VariantType::INT) {

                int64_t a = *p_args[0];
                int64_t b = *p_args[1];
                r_ret = MAX(a, b);
            } else {
                VALIDATE_ARG_NUM(0);
                VALIDATE_ARG_NUM(1);

                real_t a = *p_args[0];
                real_t b = *p_args[1];

                r_ret = MAX(a, b);
            }

        } break;
        case LOGIC_MIN: {
            VALIDATE_ARG_COUNT(2);
            if (p_args[0]->get_type() == VariantType::INT && p_args[1]->get_type() == VariantType::INT) {

                int64_t a = *p_args[0];
                int64_t b = *p_args[1];
                r_ret = MIN(a, b);
            } else {
                VALIDATE_ARG_NUM(0);
                VALIDATE_ARG_NUM(1);

                real_t a = *p_args[0];
                real_t b = *p_args[1];

                r_ret = MIN(a, b);
            }
        } break;
        case LOGIC_CLAMP: {
            VALIDATE_ARG_COUNT(3);
            if (p_args[0]->get_type() == VariantType::INT && p_args[1]->get_type() == VariantType::INT && p_args[2]->get_type() == VariantType::INT) {

                int64_t a = *p_args[0];
                int64_t b = *p_args[1];
                int64_t c = *p_args[2];
                r_ret = CLAMP(a, b, c);
            } else {
                VALIDATE_ARG_NUM(0);
                VALIDATE_ARG_NUM(1);
                VALIDATE_ARG_NUM(2);

                real_t a = *p_args[0];
                real_t b = *p_args[1];
                real_t c = *p_args[2];

                r_ret = CLAMP(a, b, c);
            }
        } break;
        case LOGIC_NEAREST_PO2: {
            VALIDATE_ARG_COUNT(1);
            VALIDATE_ARG_NUM(0);
            int64_t num = *p_args[0];
            r_ret = next_power_of_2(num);
        } break;
        case OBJ_WEAKREF: {
            VALIDATE_ARG_COUNT(1);
            if (p_args[0]->get_type() == VariantType::OBJECT) {
                if (p_args[0]->is_ref()) {
                    Ref<WeakRef> wref(make_ref_counted<WeakRef>());
                    REF r = refFromVariant<RefCounted>(*p_args[0]);
                    if (r) {
                        wref->set_ref(r);
                    }
                    r_ret = wref;
                } else {
                    Ref<WeakRef> wref(make_ref_counted<WeakRef>());
                    Object *obj = *p_args[0];
                    if (obj) {
                        wref->set_obj(obj);
                    }
                    r_ret = wref;
                }
            } else if (p_args[0]->get_type() == VariantType::NIL) {
                r_ret = make_ref_counted<WeakRef>();
            } else {
                r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                r_error.argument = 0;
                r_error.expected = VariantType::OBJECT;
                r_ret = Variant();
                return;
            }
        } break;
        case FUNC_FUNCREF: {
            VALIDATE_ARG_COUNT(2)
            if (p_args[0]->get_type() != VariantType::OBJECT) {

                r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                r_error.argument = 0;
                r_error.expected = VariantType::OBJECT;
                r_ret = Variant();
                return;
            }
            if (p_args[1]->get_type() != VariantType::STRING && p_args[1]->get_type() != VariantType::NODE_PATH) {

                r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                r_error.argument = 1;
                r_error.expected = VariantType::STRING;
                r_ret = Variant();
                return;
            }

            Ref<FuncRef> fr(make_ref_counted<FuncRef>());

            fr->set_instance(*p_args[0]);
            fr->set_function(*p_args[1]);

            r_ret = fr;

        } break;
        case TYPE_CONVERT: {
            VALIDATE_ARG_COUNT(2)
            VALIDATE_ARG_NUM(1)
            int type = *p_args[1];
            if (type < 0 || type >= int8_t(VariantType::VARIANT_MAX)) {

                r_ret = RTR("Invalid type argument to convert(), use TYPE_* constants.");
                r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                r_error.argument = 0;
                r_error.expected = VariantType::INT;
                return;

            } else {

                r_ret = Variant::construct(VariantType(type), p_args, 1, r_error);
            }
        } break;
        case TYPE_OF: {

            VALIDATE_ARG_COUNT(1)
            r_ret = int8_t(p_args[0]->get_type());

        } break;
        case TYPE_EXISTS: {

            VALIDATE_ARG_COUNT(1)
            r_ret = ClassDB::class_exists(*p_args[0]);

        } break;
        case TEXT_CHAR: {
            VALIDATE_ARG_COUNT(1);
            VALIDATE_ARG_NUM(0);
            r_ret = se_string(*p_args[0]);
        } break;
        case TEXT_ORD: {

            VALIDATE_ARG_COUNT(1);

            if (p_args[0]->get_type() != VariantType::STRING) {

                r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                r_error.argument = 0;
                r_error.expected = VariantType::STRING;
                r_ret = Variant();
                return;
            }

            se_string str = p_args[0]->as<se_string>();

            if (str.length() != 1) {

                r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                r_error.argument = 0;
                r_error.expected = VariantType::STRING;
                r_ret = RTR("Expected a string of length 1 (a character).");
                return;
            }

            r_ret = str[0];
        } break;
        case TEXT_STR: {
            if (p_arg_count < 1) {
                r_error.error = Variant::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
                r_error.argument = 1;
                r_ret = Variant();

                return;
            }
            se_string str;
            for (int i = 0; i < p_arg_count; i++) {

                se_string os(p_args[i]->as<se_string>());


                if (i == 0)
                    str = os;
                else
                    str += os;
            }

            r_ret = str;

        } break;
        case TEXT_PRINT: {

            se_string str;
            for (int i = 0; i < p_arg_count; i++) {

                str += p_args[i]->as<se_string>();
            }

            print_line(str);
            r_ret = Variant();

        } break;
        case TEXT_PRINT_TABBED: {

            se_string str;
            for (int i = 0; i < p_arg_count; i++) {

                if (i)
                    str += '\t';
                str += p_args[i]->as<se_string>();

            }

            print_line(str);
            r_ret = Variant();

        } break;
        case TEXT_PRINT_SPACED: {

            se_string str;
            for (int i = 0; i < p_arg_count; i++) {

                if (i)
                    str += ' ';
                str += p_args[i]->as<se_string>();

            }

            print_line(str);
            r_ret = Variant();

        } break;

        case TEXT_PRINTERR: {

            se_string str;
            for (int i = 0; i < p_arg_count; i++) {

                str += p_args[i]->as<se_string>();

            }

            print_error(str);
            r_ret = Variant();

        } break;
        case TEXT_PRINTRAW: {
            se_string str;
            for (int i = 0; i < p_arg_count; i++) {

                str += p_args[i]->as<se_string>();
            }

            OS::get_singleton()->print(str);
            r_ret = Variant();

        } break;
        case TEXT_PRINT_DEBUG: {
            se_string str;
            for (int i = 0; i < p_arg_count; i++) {

                str += p_args[i]->as<se_string>();
            }

            ScriptLanguage *script = GDScriptLanguage::get_singleton();
            if (script->debug_get_stack_level_count() > 0) {
                str += "\n   At: " + script->debug_get_stack_level_source(0) + ":" + ::to_string(script->debug_get_stack_level_line(0)) + ":" + script->debug_get_stack_level_function(0) + "()";
            }

            print_line(str);
            r_ret = Variant();
        } break;
        case PUSH_ERROR: {
            VALIDATE_ARG_COUNT(1);
            if (p_args[0]->get_type() != VariantType::STRING) {
                r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                r_error.argument = 0;
                r_error.expected = VariantType::STRING;
                r_ret = Variant();
                break;
            }

            se_string message = *p_args[0];
            ERR_PRINT(message);
            r_ret = Variant();
        } break;
        case PUSH_WARNING: {
            VALIDATE_ARG_COUNT(1);
            if (p_args[0]->get_type() != VariantType::STRING) {
                r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                r_error.argument = 0;
                r_error.expected = VariantType::STRING;
                r_ret = Variant();
                break;
            }

            se_string message = *p_args[0];
            WARN_PRINTS(message);
            r_ret = Variant();
        } break;
        case VAR_TO_STR: {
            VALIDATE_ARG_COUNT(1);
            se_string vars;
            VariantWriter::write_to_string(*p_args[0], vars);
            r_ret = vars;
        } break;
        case STR_TO_VAR: {
            VALIDATE_ARG_COUNT(1);
            if (p_args[0]->get_type() != VariantType::STRING) {
                r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                r_error.argument = 0;
                r_error.expected = VariantType::STRING;
                r_ret = Variant();
                return;
            }
            r_ret = *p_args[0];

            VariantParser::Stream *ss = VariantParser::get_string_stream(*p_args[0]);
            se_string errs;
            int line;
            (void)VariantParser::parse(ss, r_ret, errs, line);
            VariantParser::release_stream(ss);
        } break;
        case VAR_TO_BYTES: {
            bool full_objects = false;
            if (p_arg_count < 1) {
                r_error.error = Variant::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
                r_error.argument = 1;
                r_ret = Variant();
                return;
            } else if (p_arg_count > 2) {
                r_error.error = Variant::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
                r_error.argument = 2;
                r_ret = Variant();
            } else if (p_arg_count == 2) {
                if (p_args[1]->get_type() != VariantType::BOOL) {
                    r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                    r_error.argument = 1;
                    r_error.expected = VariantType::BOOL;
                    r_ret = Variant();
                    return;
                }
                full_objects = *p_args[1];
            }

            PoolByteArray barr;
            int len;
            Error err = encode_variant(*p_args[0], nullptr, len, full_objects);
            if (err) {
                r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                r_error.argument = 0;
                r_error.expected = VariantType::NIL;
                r_ret = "Unexpected error encoding variable to bytes, likely unserializable type found (Object or RID).";
                return;
            }

            barr.resize(len);
            {
                PoolByteArray::Write w = barr.write();
                encode_variant(*p_args[0], w.ptr(), len, full_objects);
            }
            r_ret = barr;
        } break;
        case BYTES_TO_VAR: {
            bool allow_objects = false;
            if (p_arg_count < 1) {
                r_error.error = Variant::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
                r_error.argument = 1;
                r_ret = Variant();
                return;
            } else if (p_arg_count > 2) {
                r_error.error = Variant::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
                r_error.argument = 2;
                r_ret = Variant();
            } else if (p_arg_count == 2) {
                if (p_args[1]->get_type() != VariantType::BOOL) {
                    r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                    r_error.argument = 1;
                    r_error.expected = VariantType::BOOL;
                    r_ret = Variant();
                    return;
                }
                allow_objects = *p_args[1];
            }

            if (p_args[0]->get_type() != VariantType::POOL_BYTE_ARRAY) {
                r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                r_error.argument = 1;
                r_error.expected = VariantType::POOL_BYTE_ARRAY;
                r_ret = Variant();
                return;
            }

            PoolByteArray varr = *p_args[0];
            Variant ret;
            {
                PoolByteArray::Read r = varr.read();
                Error err = decode_variant(ret, r.ptr(), varr.size(), nullptr, allow_objects);
                if (err != OK) {
                    r_ret = RTR("Not enough bytes for decoding bytes, or invalid format.");
                    r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                    r_error.argument = 0;
                    r_error.expected = VariantType::POOL_BYTE_ARRAY;
                    return;
                }
            }

            r_ret = ret;

        } break;
        case GEN_RANGE: {

            switch (p_arg_count) {

                case 0: {

                    r_error.error = Variant::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
                    r_error.argument = 1;
                    r_ret = Variant();

                } break;
                case 1: {

                    VALIDATE_ARG_NUM(0);
                    int count = *p_args[0];
                    Array arr;
                    if (count <= 0) {
                        r_ret = arr;
                        return;
                    }
                    Error err = arr.resize(count);
                    if (err != OK) {
                        r_error.error = Variant::CallError::CALL_ERROR_INVALID_METHOD;
                        r_ret = Variant();
                        return;
                    }

                    for (int i = 0; i < count; i++) {
                        arr[i] = i;
                    }

                    r_ret = arr;
                } break;
                case 2: {

                    VALIDATE_ARG_NUM(0);
                    VALIDATE_ARG_NUM(1);

                    int from = *p_args[0];
                    int to = *p_args[1];

                    Array arr;
                    if (from >= to) {
                        r_ret = arr;
                        return;
                    }
                    Error err = arr.resize(to - from);
                    if (err != OK) {
                        r_error.error = Variant::CallError::CALL_ERROR_INVALID_METHOD;
                        r_ret = Variant();
                        return;
                    }
                    for (int i = from; i < to; i++)
                        arr[i - from] = i;
                    r_ret = arr;
                } break;
                case 3: {

                    VALIDATE_ARG_NUM(0);
                    VALIDATE_ARG_NUM(1);
                    VALIDATE_ARG_NUM(2);

                    int from = *p_args[0];
                    int to = *p_args[1];
                    int incr = *p_args[2];
                    if (incr == 0) {

                        r_ret = RTR("Step argument is zero!");
                        r_error.error = Variant::CallError::CALL_ERROR_INVALID_METHOD;
                        return;
                    }

                    Array arr;
                    if (from >= to && incr > 0) {
                        r_ret = arr;
                        return;
                    }
                    if (from <= to && incr < 0) {
                        r_ret = arr;
                        return;
                    }

                    //calculate how many
                    int count = 0;
                    if (incr > 0) {

                        count = ((to - from - 1) / incr) + 1;
                    } else {

                        count = ((from - to - 1) / -incr) + 1;
                    }

                    Error err = arr.resize(count);

                    if (err != OK) {
                        r_error.error = Variant::CallError::CALL_ERROR_INVALID_METHOD;
                        r_ret = Variant();
                        return;
                    }

                    if (incr > 0) {
                        int idx = 0;
                        for (int i = from; i < to; i += incr) {
                            arr[idx++] = i;
                        }
                    } else {

                        int idx = 0;
                        for (int i = from; i > to; i += incr) {
                            arr[idx++] = i;
                        }
                    }

                    r_ret = arr;
                } break;
                default: {

                    r_error.error = Variant::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
                    r_error.argument = 3;
                    r_ret = Variant();

                } break;
            }

        } break;
        case RESOURCE_LOAD: {
            VALIDATE_ARG_COUNT(1);
            if (p_args[0]->get_type() != VariantType::STRING) {
                r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                r_error.argument = 0;
                r_error.expected = VariantType::STRING;
                r_ret = Variant();
            } else {
                r_ret = ResourceLoader::load(p_args[0]->as<se_string>());
            }

        } break;
        case INST2DICT: {

            VALIDATE_ARG_COUNT(1);

            if (p_args[0]->get_type() == VariantType::NIL) {
                r_ret = Variant();
            } else if (p_args[0]->get_type() != VariantType::OBJECT) {
                r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                r_error.argument = 0;
                r_ret = Variant();
            } else {

                Object *obj = *p_args[0];
                if (!obj) {
                    r_ret = Variant();

                } else if (!obj->get_script_instance() || obj->get_script_instance()->get_language() != GDScriptLanguage::get_singleton()) {

                    r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                    r_error.argument = 0;
                    r_error.expected = VariantType::DICTIONARY;
                    r_ret = RTR("Not a script with an instance");
                    return;
                } else {

                    GDScriptInstance *ins = static_cast<GDScriptInstance *>(obj->get_script_instance());
                    Ref<GDScript> base = dynamic_ref_cast<GDScript>(ins->get_script());
                    if (not base) {

                        r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                        r_error.argument = 0;
                        r_error.expected = VariantType::DICTIONARY;
                        r_ret = RTR("Not based on a script");
                        return;
                    }

                    GDScript *p = base.get();
                    Vector<StringName> sname;

                    while (p->_owner) {

                        sname.push_back(p->name);
                        p = p->_owner;
                    }
                    sname.invert();

                    if (!PathUtils::is_resource_file(p->path)) {
                        r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                        r_error.argument = 0;
                        r_error.expected = VariantType::DICTIONARY;
                        r_ret = Variant();

                        r_ret = RTR("Not based on a resource file");

                        return;
                    }

                    NodePath cp(sname, Vector<StringName>(), false);

                    Dictionary d;
                    d["@subpath"] = cp;
                    d["@path"] = p->path;

                    p = base.get();

                    while (p) {

                        for (const StringName &E : p->members) {

                            Variant value;
                            if (ins->get(E, value)) {

                                const char *k = E.asCString();
                                if (!d.has(k)) {
                                    d[k] = value;
                                }
                            }
                        }

                        p = p->_base;
                    }

                    r_ret = d;
                }
            }

        } break;
        case DICT2INST: {

            VALIDATE_ARG_COUNT(1);

            if (p_args[0]->get_type() != VariantType::DICTIONARY) {

                r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                r_error.argument = 0;
                r_error.expected = VariantType::DICTIONARY;
                r_ret = Variant();

                return;
            }

            Dictionary d = *p_args[0];

            if (!d.has("@path")) {

                r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                r_error.argument = 0;
                r_error.expected = VariantType::OBJECT;
                r_ret = RTR("Invalid instance dictionary format (missing @path)");

                return;
            }

            Ref<Script> scr = dynamic_ref_cast<Script>(ResourceLoader::load(d["@path"].as<se_string>()));
            if (not scr) {

                r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                r_error.argument = 0;
                r_error.expected = VariantType::OBJECT;
                r_ret = RTR("Invalid instance dictionary format (can't load script at @path)");
                return;
            }

            Ref<GDScript> gdscr = dynamic_ref_cast<GDScript>(scr);

            if (not gdscr) {

                r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                r_error.argument = 0;
                r_error.expected = VariantType::OBJECT;
                r_ret = Variant();
                r_ret = RTR("Invalid instance dictionary format (invalid script at @path)");
                return;
            }

            NodePath sub;
            if (d.has("@subpath")) {
                sub = d["@subpath"];
            }

            for (int i = 0; i < sub.get_name_count(); i++) {

                gdscr = gdscr->subclasses[sub.get_name(i)];
                if (not gdscr) {

                    r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                    r_error.argument = 0;
                    r_error.expected = VariantType::OBJECT;
                    r_ret = Variant();
                    r_ret = RTR("Invalid instance dictionary (invalid subclasses)");
                    return;
                }
            }

            r_ret = gdscr->_new(nullptr, 0, r_error);

            GDScriptInstance *ins = static_cast<GDScriptInstance *>(static_cast<Object *>(r_ret)->get_script_instance());
            Ref<GDScript> gd_ref = dynamic_ref_cast<GDScript>(ins->get_script());

            for (eastl::pair<const StringName,GDScript::MemberInfo> &E : gd_ref->member_indices) {
                if (d.has(E.first)) {
                    ins->members.write[E.second.index] = d[E.first];
                }
            }

        } break;
        case VALIDATE_JSON: {

            VALIDATE_ARG_COUNT(1)

            if (p_args[0]->get_type() != VariantType::STRING) {
                r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                r_error.argument = 0;
                r_error.expected = VariantType::STRING;
                r_ret = Variant();
                return;
            }

            se_string errs;
            int errl;

            Error err = JSON::parse(*p_args[0], r_ret, errs, errl);

            if (err != OK) {
                r_ret = se_string(itos(errl) + ":" + errs);
            } else {
                r_ret = "";
            }

        } break;
        case PARSE_JSON: {

            VALIDATE_ARG_COUNT(1)

            if (p_args[0]->get_type() != VariantType::STRING) {
                r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                r_error.argument = 0;
                r_error.expected = VariantType::STRING;
                r_ret = Variant();
                return;
            }

            se_string errs;
            int errl;

            Error err = JSON::parse(*p_args[0], r_ret, errs, errl);

            if (err != OK) {
                r_ret = Variant();
            }

        } break;
        case TO_JSON: {
            VALIDATE_ARG_COUNT(1)

            r_ret = JSON::print(*p_args[0]);
        } break;
        case HASH: {

            VALIDATE_ARG_COUNT(1);
            r_ret = p_args[0]->hash();

        } break;
        case COLOR8: {

            if (p_arg_count < 3) {
                r_error.error = Variant::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
                r_error.argument = 3;
                r_ret = Variant();

                return;
            }
            if (p_arg_count > 4) {
                r_error.error = Variant::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
                r_error.argument = 4;
                r_ret = Variant();

                return;
            }

            VALIDATE_ARG_NUM(0);
            VALIDATE_ARG_NUM(1);
            VALIDATE_ARG_NUM(2);

            Color color((float)*p_args[0] / 255.0f, (float)*p_args[1] / 255.0f, (float)*p_args[2] / 255.0f);

            if (p_arg_count == 4) {
                VALIDATE_ARG_NUM(3);
                color.a = (float)*p_args[3] / 255.0f;
            }

            r_ret = color;

        } break;
        case COLORN: {

            if (p_arg_count < 1) {
                r_error.error = Variant::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
                r_error.argument = 1;
                r_ret = Variant();
                return;
            }

            if (p_arg_count > 2) {
                r_error.error = Variant::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
                r_error.argument = 2;
                r_ret = Variant();
                return;
            }

            if (p_args[0]->get_type() != VariantType::STRING) {
                r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                r_error.argument = 0;
                r_ret = Variant();
            } else {
                Color color = Color::named(p_args[0]->as<se_string>());
                if (p_arg_count == 2) {
                    VALIDATE_ARG_NUM(1);
                    color.a = *p_args[1];
                }
                r_ret = color;
            }

        } break;

        case PRINT_STACK: {
            VALIDATE_ARG_COUNT(0);

            ScriptLanguage *script = GDScriptLanguage::get_singleton();
            for (int i = 0; i < script->debug_get_stack_level_count(); i++) {

                print_line("Frame " + ::to_string(i) + " - " + script->debug_get_stack_level_source(i) + ":" + ::to_string(script->debug_get_stack_level_line(i)) + " in function '" + script->debug_get_stack_level_function(i) + "'");
            };
        } break;

        case GET_STACK: {
            VALIDATE_ARG_COUNT(0);

            ScriptLanguage *script = GDScriptLanguage::get_singleton();
            Array ret;
            for (int i = 0; i < script->debug_get_stack_level_count(); i++) {

                Dictionary frame;
                frame["source"] = script->debug_get_stack_level_source(i);
                frame["function"] = script->debug_get_stack_level_function(i);
                frame["line"] = script->debug_get_stack_level_line(i);
                ret.push_back(frame);
            };
            r_ret = ret;
        } break;

        case INSTANCE_FROM_ID: {

            VALIDATE_ARG_COUNT(1);
            if (p_args[0]->get_type() != VariantType::INT && p_args[0]->get_type() != VariantType::REAL) {
                r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                r_error.argument = 0;
                r_error.expected = VariantType::INT;
                r_ret = Variant();
                break;
            }

            uint32_t id = *p_args[0];
            //TODO: SEGS: make sure get_instance(id) does not return Reference pointer here
            r_ret = Variant(ObjectDB::get_instance(id));

        } break;
        case LEN: {

            VALIDATE_ARG_COUNT(1);
            switch (p_args[0]->get_type()) {
                case VariantType::STRING: {

                    se_string d = *p_args[0];
                    r_ret = d.length();
                } break;
                case VariantType::DICTIONARY: {

                    Dictionary d = *p_args[0];
                    r_ret = d.size();
                } break;
                case VariantType::ARRAY: {

                    Array d = *p_args[0];
                    r_ret = d.size();
                } break;
                case VariantType::POOL_BYTE_ARRAY: {

                    PoolVector<uint8_t> d = *p_args[0];
                    r_ret = d.size();
                } break;
                case VariantType::POOL_INT_ARRAY: {

                    PoolVector<int> d = *p_args[0];
                    r_ret = d.size();
                } break;
                case VariantType::POOL_REAL_ARRAY: {

                    PoolVector<real_t> d = *p_args[0];
                    r_ret = d.size();
                } break;
                case VariantType::POOL_STRING_ARRAY: {

                    PoolVector<se_string> d = *p_args[0];
                    r_ret = d.size();
                } break;
                case VariantType::POOL_VECTOR2_ARRAY: {

                    PoolVector<Vector2> d = *p_args[0];
                    r_ret = d.size();
                } break;
                case VariantType::POOL_VECTOR3_ARRAY: {

                    PoolVector<Vector3> d = *p_args[0];
                    r_ret = d.size();
                } break;
                case VariantType::POOL_COLOR_ARRAY: {

                    PoolVector<Color> d = *p_args[0];
                    r_ret = d.size();
                } break;
                default: {
                    r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                    r_error.argument = 0;
                    r_error.expected = VariantType::OBJECT;
                    r_ret = Variant();
                    r_ret = RTR("Object can't provide a length.");
                }
            }

        } break;
        case IS_INSTANCE_VALID: {

            VALIDATE_ARG_COUNT(1);
            if (p_args[0]->get_type() != VariantType::OBJECT) {
                r_ret = false;
            } else {
                Object *obj = *p_args[0];
                r_ret = ObjectDB::instance_validate(obj);
            }

        } break;
        case FUNC_MAX: {

            ERR_FAIL();
        } break;
    }
}

bool GDScriptFunctions::is_deterministic(Function p_func) {

    //man i couldn't have chosen a worse function name,
    //way too controversial..

    switch (p_func) {

        case MATH_SIN:
        case MATH_COS:
        case MATH_TAN:
        case MATH_SINH:
        case MATH_COSH:
        case MATH_TANH:
        case MATH_ASIN:
        case MATH_ACOS:
        case MATH_ATAN:
        case MATH_ATAN2:
        case MATH_SQRT:
        case MATH_FMOD:
        case MATH_FPOSMOD:
        case MATH_POSMOD:
        case MATH_FLOOR:
        case MATH_CEIL:
        case MATH_ROUND:
        case MATH_ABS:
        case MATH_SIGN:
        case MATH_POW:
        case MATH_LOG:
        case MATH_EXP:
        case MATH_ISNAN:
        case MATH_ISINF:
        case MATH_EASE:
        case MATH_DECIMALS:
        case MATH_STEP_DECIMALS:
        case MATH_STEPIFY:
        case MATH_LERP:
        case MATH_INVERSE_LERP:
        case MATH_RANGE_LERP:
        case MATH_SMOOTHSTEP:
        case MATH_MOVE_TOWARD:
        case MATH_DECTIME:
        case MATH_DEG2RAD:
        case MATH_RAD2DEG:
        case MATH_LINEAR2DB:
        case MATH_DB2LINEAR:
        case MATH_POLAR2CARTESIAN:
        case MATH_CARTESIAN2POLAR:
        case MATH_WRAP:
        case MATH_WRAPF:
        case LOGIC_MAX:
        case LOGIC_MIN:
        case LOGIC_CLAMP:
        case LOGIC_NEAREST_PO2:
        case TYPE_CONVERT:
        case TYPE_OF:
        case TYPE_EXISTS:
        case TEXT_CHAR:
        case TEXT_ORD:
        case TEXT_STR:
        case COLOR8:
        case LEN:
            // enable for debug only, otherwise not desirable - case GEN_RANGE:
            return true;
        default:
            return false;
    }

    return false;
}

MethodInfo GDScriptFunctions::get_info(Function p_func) {

#ifdef DEBUG_ENABLED
    //using a switch, so the compiler generates a jumptable

    switch (p_func) {

        case MATH_SIN: {
            MethodInfo mi("sin", PropertyInfo(VariantType::REAL, "s"));
            mi.return_val.type = VariantType::REAL;
            return mi;

        } break;
        case MATH_COS: {
            MethodInfo mi("cos", PropertyInfo(VariantType::REAL, "s"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_TAN: {
            MethodInfo mi("tan", PropertyInfo(VariantType::REAL, "s"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_SINH: {
            MethodInfo mi("sinh", PropertyInfo(VariantType::REAL, "s"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_COSH: {
            MethodInfo mi("cosh", PropertyInfo(VariantType::REAL, "s"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_TANH: {
            MethodInfo mi("tanh", PropertyInfo(VariantType::REAL, "s"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_ASIN: {
            MethodInfo mi("asin", PropertyInfo(VariantType::REAL, "s"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_ACOS: {
            MethodInfo mi("acos", PropertyInfo(VariantType::REAL, "s"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_ATAN: {
            MethodInfo mi("atan", PropertyInfo(VariantType::REAL, "s"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_ATAN2: {
            MethodInfo mi("atan2", PropertyInfo(VariantType::REAL, "y"), PropertyInfo(VariantType::REAL, "x"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_SQRT: {
            MethodInfo mi("sqrt", PropertyInfo(VariantType::REAL, "s"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_FMOD: {
            MethodInfo mi("fmod", PropertyInfo(VariantType::REAL, "a"), PropertyInfo(VariantType::REAL, "b"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_FPOSMOD: {
            MethodInfo mi("fposmod", PropertyInfo(VariantType::REAL, "a"), PropertyInfo(VariantType::REAL, "b"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_POSMOD: {
            MethodInfo mi("posmod", PropertyInfo(VariantType::INT, "a"), PropertyInfo(VariantType::INT, "b"));
            mi.return_val.type = VariantType::INT;
            return mi;
        } break;
        case MATH_FLOOR: {
            MethodInfo mi("floor", PropertyInfo(VariantType::REAL, "s"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_CEIL: {
            MethodInfo mi("ceil", PropertyInfo(VariantType::REAL, "s"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_ROUND: {
            MethodInfo mi("round", PropertyInfo(VariantType::REAL, "s"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_ABS: {
            MethodInfo mi("abs", PropertyInfo(VariantType::REAL, "s"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_SIGN: {
            MethodInfo mi("sign", PropertyInfo(VariantType::REAL, "s"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_POW: {
            MethodInfo mi("pow", PropertyInfo(VariantType::REAL, "base"), PropertyInfo(VariantType::REAL, "exp"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_LOG: {
            MethodInfo mi("log", PropertyInfo(VariantType::REAL, "s"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_EXP: {
            MethodInfo mi("exp", PropertyInfo(VariantType::REAL, "s"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_ISNAN: {
            MethodInfo mi("is_nan", PropertyInfo(VariantType::REAL, "s"));
            mi.return_val.type = VariantType::BOOL;
            return mi;
        } break;
        case MATH_ISINF: {
            MethodInfo mi("is_inf", PropertyInfo(VariantType::REAL, "s"));
            mi.return_val.type = VariantType::BOOL;
            return mi;
        } break;
        case MATH_ISEQUALAPPROX: {
            MethodInfo mi("is_equal_approx", PropertyInfo(VariantType::REAL, "a"), PropertyInfo(VariantType::REAL, "b"));
            mi.return_val.type = VariantType::BOOL;
            return mi;
        } break;
        case MATH_ISZEROAPPROX: {
            MethodInfo mi("is_zero_approx", PropertyInfo(VariantType::REAL, "s"));
            mi.return_val.type = VariantType::BOOL;
            return mi;
        } break;
        case MATH_EASE: {
            MethodInfo mi("ease", PropertyInfo(VariantType::REAL, "s"), PropertyInfo(VariantType::REAL, "curve"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_DECIMALS: {
            MethodInfo mi("decimals", PropertyInfo(VariantType::REAL, "step"));
            mi.return_val.type = VariantType::INT;
            return mi;
        } break;
        case MATH_STEP_DECIMALS: {
            MethodInfo mi("step_decimals", PropertyInfo(VariantType::REAL, "step"));
            mi.return_val.type = VariantType::INT;
            return mi;
        } break;
        case MATH_STEPIFY: {
            MethodInfo mi("stepify", PropertyInfo(VariantType::REAL, "s"), PropertyInfo(VariantType::REAL, "step"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_LERP: {
            MethodInfo mi("lerp", PropertyInfo(VariantType::NIL, "from"), PropertyInfo(VariantType::NIL, "to"), PropertyInfo(VariantType::REAL, "weight"));
            mi.return_val.type = VariantType::NIL;
            mi.return_val.usage |= PROPERTY_USAGE_NIL_IS_VARIANT;
            return mi;
        } break;
        case MATH_LERP_ANGLE: {
            MethodInfo mi("lerp_angle", PropertyInfo(VariantType::REAL, "from"), PropertyInfo(VariantType::REAL, "to"), PropertyInfo(VariantType::REAL, "weight"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_INVERSE_LERP: {
            MethodInfo mi("inverse_lerp", PropertyInfo(VariantType::REAL, "from"), PropertyInfo(VariantType::REAL, "to"), PropertyInfo(VariantType::REAL, "weight"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_RANGE_LERP: {
            MethodInfo mi("range_lerp", PropertyInfo(VariantType::REAL, "value"), PropertyInfo(VariantType::REAL, "istart"), PropertyInfo(VariantType::REAL, "istop"), PropertyInfo(VariantType::REAL, "ostart"), PropertyInfo(VariantType::REAL, "ostop"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_SMOOTHSTEP: {
            MethodInfo mi("smoothstep", PropertyInfo(VariantType::REAL, "from"), PropertyInfo(VariantType::REAL, "to"), PropertyInfo(VariantType::REAL, "weight"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_MOVE_TOWARD: {
            MethodInfo mi("move_toward", PropertyInfo(VariantType::REAL, "from"), PropertyInfo(VariantType::REAL, "to"), PropertyInfo(VariantType::REAL, "delta"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_DECTIME: {
            MethodInfo mi("dectime", PropertyInfo(VariantType::REAL, "value"), PropertyInfo(VariantType::REAL, "amount"), PropertyInfo(VariantType::REAL, "step"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_RANDOMIZE: {
            MethodInfo mi("randomize");
            mi.return_val.type = VariantType::NIL;
            return mi;
        } break;
        case MATH_RAND: {
            MethodInfo mi("randi");
            mi.return_val.type = VariantType::INT;
            return mi;
        } break;
        case MATH_RANDF: {
            MethodInfo mi("randf");
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_RANDOM: {
            MethodInfo mi("rand_range", PropertyInfo(VariantType::REAL, "from"), PropertyInfo(VariantType::REAL, "to"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_SEED: {
            MethodInfo mi("seed", PropertyInfo(VariantType::INT, "seed"));
            mi.return_val.type = VariantType::NIL;
            return mi;
        } break;
        case MATH_RANDSEED: {
            MethodInfo mi("rand_seed", PropertyInfo(VariantType::INT, "seed"));
            mi.return_val.type = VariantType::ARRAY;
            return mi;
        } break;
        case MATH_DEG2RAD: {
            MethodInfo mi("deg2rad", PropertyInfo(VariantType::REAL, "deg"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_RAD2DEG: {
            MethodInfo mi("rad2deg", PropertyInfo(VariantType::REAL, "rad"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_LINEAR2DB: {
            MethodInfo mi("linear2db", PropertyInfo(VariantType::REAL, "nrg"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_DB2LINEAR: {
            MethodInfo mi("db2linear", PropertyInfo(VariantType::REAL, "db"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case MATH_POLAR2CARTESIAN: {
            MethodInfo mi("polar2cartesian", PropertyInfo(VariantType::REAL, "r"), PropertyInfo(VariantType::REAL, "th"));
            mi.return_val.type = VariantType::VECTOR2;
            return mi;
        } break;
        case MATH_CARTESIAN2POLAR: {
            MethodInfo mi("cartesian2polar", PropertyInfo(VariantType::REAL, "x"), PropertyInfo(VariantType::REAL, "y"));
            mi.return_val.type = VariantType::VECTOR2;
            return mi;
        } break;
        case MATH_WRAP: {
            MethodInfo mi("wrapi", PropertyInfo(VariantType::INT, "value"), PropertyInfo(VariantType::INT, "min"), PropertyInfo(VariantType::INT, "max"));
            mi.return_val.type = VariantType::INT;
            return mi;
        } break;
        case MATH_WRAPF: {
            MethodInfo mi("wrapf", PropertyInfo(VariantType::REAL, "value"), PropertyInfo(VariantType::REAL, "min"), PropertyInfo(VariantType::REAL, "max"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case LOGIC_MAX: {
            MethodInfo mi("max", PropertyInfo(VariantType::REAL, "a"), PropertyInfo(VariantType::REAL, "b"));
            mi.return_val.type = VariantType::REAL;
            return mi;

        } break;
        case LOGIC_MIN: {
            MethodInfo mi("min", PropertyInfo(VariantType::REAL, "a"), PropertyInfo(VariantType::REAL, "b"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case LOGIC_CLAMP: {
            MethodInfo mi("clamp", PropertyInfo(VariantType::REAL, "value"), PropertyInfo(VariantType::REAL, "min"), PropertyInfo(VariantType::REAL, "max"));
            mi.return_val.type = VariantType::REAL;
            return mi;
        } break;
        case LOGIC_NEAREST_PO2: {
            MethodInfo mi("nearest_po2", PropertyInfo(VariantType::INT, "value"));
            mi.return_val.type = VariantType::INT;
            return mi;
        } break;
        case OBJ_WEAKREF: {

            MethodInfo mi("weakref", PropertyInfo(VariantType::OBJECT, "obj"));
            mi.return_val.type = VariantType::OBJECT;
            mi.return_val.class_name = "WeakRef";

            return mi;

        } break;
        case FUNC_FUNCREF: {

            MethodInfo mi("funcref", PropertyInfo(VariantType::OBJECT, "instance"), PropertyInfo(VariantType::STRING, "funcname"));
            mi.return_val.type = VariantType::OBJECT;
            mi.return_val.class_name = "FuncRef";
            return mi;

        } break;
        case TYPE_CONVERT: {

            MethodInfo mi("convert", PropertyInfo(VariantType::NIL, "what", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NIL_IS_VARIANT), PropertyInfo(VariantType::INT, "type"));
            mi.return_val.type = VariantType::NIL;
            mi.return_val.usage |= PROPERTY_USAGE_NIL_IS_VARIANT;
            return mi;
        } break;
        case TYPE_OF: {

            MethodInfo mi("typeof", PropertyInfo(VariantType::NIL, "what", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NIL_IS_VARIANT));
            mi.return_val.type = VariantType::INT;
            return mi;

        } break;
        case TYPE_EXISTS: {

            MethodInfo mi("type_exists", PropertyInfo(VariantType::STRING, "type"));
            mi.return_val.type = VariantType::BOOL;
            return mi;

        } break;
        case TEXT_CHAR: {

            MethodInfo mi("char", PropertyInfo(VariantType::INT, "ascii"));
            mi.return_val.type = VariantType::STRING;
            return mi;

        } break;
        case TEXT_ORD: {

            MethodInfo mi("ord", PropertyInfo(VariantType::STRING, "char"));
            mi.return_val.type = VariantType::INT;
            return mi;

        } break;
        case TEXT_STR: {

            MethodInfo mi("str");
            mi.return_val.type = VariantType::STRING;
            mi.flags |= METHOD_FLAG_VARARG;
            return mi;

        } break;
        case TEXT_PRINT: {

            MethodInfo mi("print");
            mi.return_val.type = VariantType::NIL;
            mi.flags |= METHOD_FLAG_VARARG;
            return mi;

        } break;
        case TEXT_PRINT_TABBED: {

            MethodInfo mi("printt");
            mi.return_val.type = VariantType::NIL;
            mi.flags |= METHOD_FLAG_VARARG;
            return mi;

        } break;
        case TEXT_PRINT_SPACED: {

            MethodInfo mi("prints");
            mi.return_val.type = VariantType::NIL;
            mi.flags |= METHOD_FLAG_VARARG;
            return mi;

        } break;
        case TEXT_PRINTERR: {

            MethodInfo mi("printerr");
            mi.return_val.type = VariantType::NIL;
            mi.flags |= METHOD_FLAG_VARARG;
            return mi;

        } break;
        case TEXT_PRINTRAW: {

            MethodInfo mi("printraw");
            mi.return_val.type = VariantType::NIL;
            mi.flags |= METHOD_FLAG_VARARG;
            return mi;

        } break;
        case TEXT_PRINT_DEBUG: {

            MethodInfo mi("print_debug");
            mi.return_val.type = VariantType::NIL;
            mi.flags |= METHOD_FLAG_VARARG;
            return mi;

        } break;
        case PUSH_ERROR: {

            MethodInfo mi(VariantType::NIL, "push_error", PropertyInfo(VariantType::STRING, "message"));
            mi.return_val.type = VariantType::NIL;
            return mi;

        } break;
        case PUSH_WARNING: {

            MethodInfo mi(VariantType::NIL, "push_warning", PropertyInfo(VariantType::STRING, "message"));
            mi.return_val.type = VariantType::NIL;
            return mi;

        } break;
        case VAR_TO_STR: {

            MethodInfo mi("var2str", PropertyInfo(VariantType::NIL, "var", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NIL_IS_VARIANT));
            mi.return_val.type = VariantType::STRING;
            return mi;
        } break;
        case STR_TO_VAR: {

            MethodInfo mi(VariantType::NIL, "str2var", PropertyInfo(VariantType::STRING, "string"));
            mi.return_val.type = VariantType::NIL;
            mi.return_val.usage |= PROPERTY_USAGE_NIL_IS_VARIANT;
            return mi;
        } break;
        case VAR_TO_BYTES: {

            MethodInfo mi("var2bytes", PropertyInfo(VariantType::NIL, "var", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NIL_IS_VARIANT), PropertyInfo(VariantType::BOOL, "full_objects"));
            mi.default_arguments.push_back(false);
            mi.return_val.type = VariantType::POOL_BYTE_ARRAY;
            return mi;
        } break;
        case BYTES_TO_VAR: {

            MethodInfo mi(VariantType::NIL, "bytes2var", PropertyInfo(VariantType::POOL_BYTE_ARRAY, "bytes"), PropertyInfo(VariantType::BOOL, "allow_objects"));
            mi.default_arguments.push_back(false);
            mi.return_val.type = VariantType::NIL;
            mi.return_val.usage |= PROPERTY_USAGE_NIL_IS_VARIANT;
            return mi;
        } break;
        case GEN_RANGE: {

            MethodInfo mi("range");
            mi.return_val.type = VariantType::ARRAY;
            mi.flags |= METHOD_FLAG_VARARG;
            return mi;
        } break;
        case RESOURCE_LOAD: {

            MethodInfo mi("load", PropertyInfo(VariantType::STRING, "path"));
            mi.return_val.type = VariantType::OBJECT;
            mi.return_val.class_name = "Resource";
            return mi;
        } break;
        case INST2DICT: {

            MethodInfo mi("inst2dict", PropertyInfo(VariantType::OBJECT, "inst"));
            mi.return_val.type = VariantType::DICTIONARY;
            return mi;
        } break;
        case DICT2INST: {

            MethodInfo mi("dict2inst", PropertyInfo(VariantType::DICTIONARY, "dict"));
            mi.return_val.type = VariantType::OBJECT;
            return mi;
        } break;
        case VALIDATE_JSON: {

            MethodInfo mi("validate_json", PropertyInfo(VariantType::STRING, "json"));
            mi.return_val.type = VariantType::STRING;
            return mi;
        } break;
        case PARSE_JSON: {

            MethodInfo mi(VariantType::NIL, "parse_json", PropertyInfo(VariantType::STRING, "json"));
            mi.return_val.type = VariantType::NIL;
            mi.return_val.usage |= PROPERTY_USAGE_NIL_IS_VARIANT;
            return mi;
        } break;
        case TO_JSON: {

            MethodInfo mi("to_json", PropertyInfo(VariantType::NIL, "var", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NIL_IS_VARIANT));
            mi.return_val.type = VariantType::STRING;
            return mi;
        } break;
        case HASH: {

            MethodInfo mi("hash", PropertyInfo(VariantType::NIL, "var", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NIL_IS_VARIANT));
            mi.return_val.type = VariantType::INT;
            return mi;
        } break;
        case COLOR8: {

            MethodInfo mi("Color8", PropertyInfo(VariantType::INT, "r8"), PropertyInfo(VariantType::INT, "g8"), PropertyInfo(VariantType::INT, "b8"), PropertyInfo(VariantType::INT, "a8"));
            mi.default_arguments.push_back(255);
            mi.return_val.type = VariantType::COLOR;
            return mi;
        } break;
        case COLORN: {

            MethodInfo mi("ColorN", PropertyInfo(VariantType::STRING, "name"), PropertyInfo(VariantType::REAL, "alpha"));
            mi.default_arguments.push_back(1.0f);
            mi.return_val.type = VariantType::COLOR;
            return mi;
        } break;

        case PRINT_STACK: {
            MethodInfo mi("print_stack");
            mi.return_val.type = VariantType::NIL;
            return mi;
        } break;
        case GET_STACK: {
            MethodInfo mi("get_stack");
            mi.return_val.type = VariantType::ARRAY;
            return mi;
        } break;

        case INSTANCE_FROM_ID: {
            MethodInfo mi("instance_from_id", PropertyInfo(VariantType::INT, "instance_id"));
            mi.return_val.type = VariantType::OBJECT;
            return mi;
        } break;
        case LEN: {
            MethodInfo mi("len", PropertyInfo(VariantType::NIL, "var", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NIL_IS_VARIANT));
            mi.return_val.type = VariantType::INT;
            return mi;
        } break;
        case IS_INSTANCE_VALID: {
            MethodInfo mi("is_instance_valid", PropertyInfo(VariantType::OBJECT, "instance"));
            mi.return_val.type = VariantType::BOOL;
            return mi;
        } break;
        case FUNC_MAX: {

            ERR_FAIL_V(MethodInfo());
        } break;
    }
#endif

    return MethodInfo();
}
