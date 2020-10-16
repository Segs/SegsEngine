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
#include "core/crypto/crypto_core.h"
#include "core/math/aabb.h"
#include "core/math/basis.h"
#include "core/math/plane.h"
#include "core/math/quat.h"
#include "core/math/transform.h"
#include "core/math/transform_2d.h"
#include "core/math/vector3.h"
#include "core/method_info.h"
#include "core/object.h"
#include "core/rid.h"
#include "core/script_language.h"
#include "core/string.h"
#include "core/string_utils.inl"
#include "core/vector.h"

namespace {
using VariantConstructFunc = void (*)(Variant &, const Variant &);
struct _VariantCall {
    struct ConstantData {
        HashMap<StringName, int> value;
#ifdef DEBUG_ENABLED
        Vector<StringName> value_ordered;
#endif
        HashMap<StringName, Variant> variant_value;
    };
    static_assert (sizeof(ConstantData)==(sizeof(HashMap<StringName,int>)+sizeof(HashMap<StringName,Variant>)+sizeof(Vector<StringName>)));
    static ConstantData *constant_data;

    static void add_constant(VariantType p_type, const StringName &p_constant_name, int p_constant_value) {
        constant_data[static_cast<int8_t>(p_type)].value[p_constant_name] = p_constant_value;
#ifdef DEBUG_ENABLED
        constant_data[static_cast<int8_t>(p_type)].value_ordered.emplace_back(p_constant_name);
#endif
    }

    static void add_variant_constant(
            VariantType p_type, const StringName &p_constant_name, const Variant &p_constant_value) {
        constant_data[static_cast<int8_t>(p_type)].variant_value[p_constant_name] = p_constant_value;
    }
};

_VariantCall::ConstantData *_VariantCall::constant_data = nullptr;

} // namespace

Variant Variant::construct_default(const VariantType p_type) {
    switch (p_type) {
        case VariantType::NIL:
            return Variant();

            // atomic types
        case VariantType::BOOL:
            return Variant(false);
        case VariantType::INT:
            return Variant(0);
        case VariantType::FLOAT:
            return Variant(0.0f);
        case VariantType::STRING:
            return String();

            // math types
        case VariantType::VECTOR2:
            return Vector2(); // 5
        case VariantType::RECT2:
            return Rect2();
        case VariantType::VECTOR3:
            return Vector3();
        case VariantType::TRANSFORM2D:
            return Transform2D();
        case VariantType::PLANE:
            return Plane();
        case VariantType::QUAT:
            return Quat();
        case VariantType::AABB:
            return ::AABB(); // 10
        case VariantType::BASIS:
            return Basis();
        case VariantType::TRANSFORM:
            return Transform();

            // misc types
        case VariantType::COLOR:
            return Color();
        case VariantType::STRING_NAME:
            return StringName();

        case VariantType::NODE_PATH:
            return NodePath(); // 15
        case VariantType::_RID:
            return RID();
        case VariantType::OBJECT:
            return Variant(static_cast<Object *>(nullptr));
        case VariantType::CALLABLE:
            return (Variant)Callable();
        case VariantType::SIGNAL:
            return (Variant)Signal();
        case VariantType::DICTIONARY:
            return Dictionary();
        case VariantType::ARRAY:
            return Array(); // 20
        case VariantType::POOL_BYTE_ARRAY:
            return PoolByteArray();
        case VariantType::POOL_INT_ARRAY:
            return PoolIntArray();
        case VariantType::POOL_REAL_ARRAY:
            return PoolRealArray();
        case VariantType::POOL_STRING_ARRAY:
            return PoolStringArray();
        case VariantType::POOL_VECTOR2_ARRAY:
            return Variant(PoolVector2Array()); // 25
        case VariantType::POOL_VECTOR3_ARRAY:
            return PoolVector3Array();
        case VariantType::POOL_COLOR_ARRAY:
            return PoolColorArray();

        case VariantType::VARIANT_MAX:
        default:
            return Variant();
    }
}

Variant Variant::construct(const VariantType p_type, const Variant &p_arg, Callable::CallError &r_error) {
    r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
    ERR_FAIL_INDEX_V(int(p_type), int(VariantType::VARIANT_MAX), Variant());
    r_error.error = Callable::CallError::CALL_OK;
    if (p_arg.type == p_type) {
        return p_arg; // copy construct
    }
    if (can_convert(p_arg.type, p_type)) {
        // near match construct

        switch (p_type) {
            case VariantType::NIL: {
                return Variant();
            }
            case VariantType::BOOL: {
                return Variant(static_cast<bool>(p_arg));
            }
            case VariantType::INT: {
                return static_cast<int64_t>(p_arg);
            }
            case VariantType::FLOAT: {
                return static_cast<real_t>(p_arg);
            }
            case VariantType::STRING: {
                return static_cast<String>(p_arg);
            }
            case VariantType::VECTOR2: {
                return static_cast<Vector2>(p_arg);
            }
            case VariantType::RECT2:
                return static_cast<Rect2>(p_arg);
            case VariantType::VECTOR3:
                return static_cast<Vector3>(p_arg);
            case VariantType::TRANSFORM2D:
                return static_cast<Transform2D>(p_arg);

            case VariantType::PLANE:
                return static_cast<Plane>(p_arg);
            case VariantType::QUAT:
                return static_cast<Quat>(p_arg);
            case VariantType::AABB:
                return p_arg.as<::AABB>(); // 10
            case VariantType::BASIS:
                return static_cast<Basis>(p_arg);
            case VariantType::TRANSFORM:
                return Transform(static_cast<Transform>(p_arg));

                // misc types
            case VariantType::COLOR:
                return p_arg.type == VariantType::STRING ? Color::html(static_cast<String>(p_arg)) :
                                                           Color::hex(static_cast<uint32_t>(p_arg));
            case VariantType::STRING_NAME:
                return p_arg.as<StringName>();
            case VariantType::NODE_PATH:
                return NodePath(static_cast<NodePath>(p_arg)); // 15
            case VariantType::_RID:
                return static_cast<RID>(p_arg);
            case VariantType::OBJECT:
                return Variant(p_arg.as<Object *>());
            case VariantType::CALLABLE:
                return Variant((Callable)p_arg);
            case VariantType::SIGNAL:
                return Variant((Signal)p_arg);
            case VariantType::DICTIONARY:
                return static_cast<Dictionary>(p_arg);
            case VariantType::ARRAY:
                return static_cast<Array>(p_arg); // 20

                // arrays
            case VariantType::POOL_BYTE_ARRAY:
                return static_cast<PoolByteArray>(p_arg);
            case VariantType::POOL_INT_ARRAY:
                return static_cast<PoolIntArray>(p_arg);
            case VariantType::POOL_REAL_ARRAY:
                return static_cast<PoolRealArray>(p_arg);
            case VariantType::POOL_STRING_ARRAY:
                return static_cast<PoolStringArray>(p_arg);
            case VariantType::POOL_VECTOR2_ARRAY:
                return Variant(static_cast<PoolVector2Array>(p_arg)); // 25
            case VariantType::POOL_VECTOR3_ARRAY:
                return static_cast<PoolVector3Array>(p_arg);
            case VariantType::POOL_COLOR_ARRAY:
                return static_cast<PoolColorArray>(p_arg);
            default:
                return Variant();
        }
    }

    // quat construct from vec3 euler
    if(p_type==VariantType::QUAT && p_arg.type== VariantType::VECTOR3) {
        return Quat(p_arg.as<Vector3>());
    }

    r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD; // no such constructor
    return Variant();
}

void Variant::get_constants_for_type(VariantType p_type, Vector<StringName> *p_constants) {
    ERR_FAIL_INDEX((int)p_type, (int)VariantType::VARIANT_MAX);

    _VariantCall::ConstantData &cd = _VariantCall::constant_data[static_cast<int>(p_type)];

#ifdef DEBUG_ENABLED
    for (const StringName &E : cd.value_ordered) {
        p_constants->push_back(E);
#else
    for (const auto &E : cd.value) {
        p_constants->emplace_back(E.first);
#endif
    }

    for (eastl::pair<const StringName, Variant> &E : cd.variant_value) {
        p_constants->push_back(E.first);
    }
}

bool Variant::has_constant(VariantType p_type, const StringName &p_value) {
    ERR_FAIL_INDEX_V((int)p_type, (int)VariantType::VARIANT_MAX, false);
    _VariantCall::ConstantData &cd = _VariantCall::constant_data[static_cast<int>(p_type)];
    return cd.value.contains(p_value) || cd.variant_value.contains(p_value);
}

Variant Variant::get_constant_value(VariantType p_type, const StringName &p_value, bool *r_valid) {
    if (r_valid) {
        *r_valid = false;
    }

    ERR_FAIL_INDEX_V((int)p_type, (int)VariantType::VARIANT_MAX, 0);
    _VariantCall::ConstantData &cd = _VariantCall::constant_data[static_cast<int>(p_type)];

    auto E = cd.value.find(p_value);
    if (E == cd.value.end()) {
        auto F = cd.variant_value.find(p_value);
        if (F != cd.variant_value.end()) {
            if (r_valid) {
                *r_valid = true;
            }
            return F->second;
        }
        return -1;
    }
    if (r_valid) {
        *r_valid = true;
    }

    return E->second;
}
void register_variant_methods() {
    _VariantCall::constant_data = memnew_arr(_VariantCall::ConstantData, static_cast<int>(VariantType::VARIANT_MAX));

    /* REGISTER CONSTANTS */

    for (const eastl::pair<const char *const, Color> &color : _named_colors) {
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

    _VariantCall::add_variant_constant(VariantType::TRANSFORM, "IDENTITY", Transform());
    _VariantCall::add_variant_constant(
            VariantType::TRANSFORM, "FLIP_X", Transform(-1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0));
    _VariantCall::add_variant_constant(
            VariantType::TRANSFORM, "FLIP_Y", Transform(1, 0, 0, 0, -1, 0, 0, 0, 1, 0, 0, 0));
    _VariantCall::add_variant_constant(
            VariantType::TRANSFORM, "FLIP_Z", Transform(1, 0, 0, 0, 1, 0, 0, 0, -1, 0, 0, 0));

    _VariantCall::add_variant_constant(VariantType::BASIS, "IDENTITY", Basis());
    _VariantCall::add_variant_constant(VariantType::BASIS, "FLIP_X", Basis(-1, 0, 0, 0, 1, 0, 0, 0, 1));
    _VariantCall::add_variant_constant(VariantType::BASIS, "FLIP_Y", Basis(1, 0, 0, 0, -1, 0, 0, 0, 1));
    _VariantCall::add_variant_constant(VariantType::BASIS, "FLIP_Z", Basis(1, 0, 0, 0, 1, 0, 0, 0, -1));

    _VariantCall::add_variant_constant(VariantType::PLANE, "PLANE_YZ", Plane(Vector3(1, 0, 0), 0));
    _VariantCall::add_variant_constant(VariantType::PLANE, "PLANE_XZ", Plane(Vector3(0, 1, 0), 0));
    _VariantCall::add_variant_constant(VariantType::PLANE, "PLANE_XY", Plane(Vector3(0, 0, 1), 0));

    _VariantCall::add_variant_constant(VariantType::QUAT, "IDENTITY", Quat(0, 0, 0, 1));
}

void unregister_variant_methods() {
    memdelete_arr(_VariantCall::constant_data);
}
