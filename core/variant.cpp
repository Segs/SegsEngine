/*************************************************************************/
/*  variant.cpp                                                          */
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

#include "core/core_string_names.h"
#include "core/debugger/script_debugger.h"
#include "core/dictionary.h"
#include "core/hashfuncs.h"
#include "core/io/ip_address.h"
#include "core/io/marshalls.h"
#include "core/list.h"
#include "core/math/aabb.h"
#include "core/math/basis.h"
#include "core/math/camera_matrix.h"
#include "core/math/face3.h"
#include "core/math/math_funcs.h"
#include "core/math/plane.h"
#include "core/math/quat.h"
#include "core/math/transform.h"
#include "core/math/transform_2d.h"
#include "core/math/vector3.h"
#include "core/node_path.h"
#include "core/object_db.h"
#include "core/object_rc.h"
#include "core/pool_vector.h"
#include "core/print_string.h"
#include "core/resource.h"
#include "core/rid.h"
#include "core/script_language.h"
#include "core/string.h"
#include "core/string_formatter.h"
#include "core/string_utils.inl"
#include "core/variant_parser.h"

#include "EASTL/sort.h"

template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) eastl::vector<Variant, wrap_allocator>;

const Variant Variant::null_variant;
const Vector<Variant> null_variant_pvec;

const char *Variant::get_type_name(VariantType p_type) {
    switch (p_type) {
        case VariantType::NIL:
            return "Nil";
        // atomic types
        case VariantType::BOOL:
            return "bool";
        case VariantType::INT:
            return "int";
        case VariantType::FLOAT:
            return "float";
        case VariantType::STRING:
            return "String";
        // math types
        case VariantType::VECTOR2:
            return "Vector2";
        case VariantType::RECT2:
            return "Rect2";
        case VariantType::TRANSFORM2D:
            return "Transform2D";
        case VariantType::VECTOR3:
            return "Vector3";
        case VariantType::PLANE:
            return "Plane";
        case VariantType::AABB:
            return "AABB";
        case VariantType::QUAT:
            return "Quat";
        case VariantType::BASIS:
            return "Basis";
        case VariantType::TRANSFORM:
            return "Transform";
        // misc types
        case VariantType::COLOR:
            return "Color";
        case VariantType::STRING_NAME:
            return "StringName";
        case VariantType::_RID:
            return "RID";
        case VariantType::OBJECT:
            return "Object";
        case VariantType::CALLABLE:
            return "Callable";
        case VariantType::SIGNAL:
            return "Signal";

        case VariantType::NODE_PATH:
            return "NodePath";
        case VariantType::DICTIONARY:
            return "Dictionary";
        case VariantType::ARRAY:
            return "Array";
        // arrays
        case VariantType::POOL_BYTE_ARRAY:
            return "PoolByteArray";
        case VariantType::POOL_INT_ARRAY:
            return "PoolIntArray";
        case VariantType::POOL_FLOAT32_ARRAY:
            return "PoolRealArray";
        case VariantType::POOL_STRING_ARRAY:
            return "PoolStringArray";
        case VariantType::POOL_VECTOR2_ARRAY:
            return "PoolVector2Array";
        case VariantType::POOL_VECTOR3_ARRAY:
            return "PoolVector3Array";
        case VariantType::POOL_COLOR_ARRAY:
            return "PoolColorArray";
        default: {
        }
    }
    return "";
}
StringName Variant::interned_type_name(VariantType p_type) {
    switch (p_type) {
        case VariantType::NIL:
            return "Nil";
        // atomic types
        case VariantType::BOOL:
            return "bool";
        case VariantType::INT:
            return "int";
        case VariantType::FLOAT:
            return "float";
        case VariantType::STRING:
            return "String";
        // math types
        case VariantType::VECTOR2:
            return "Vector2";
        case VariantType::RECT2:
            return "Rect2";
        case VariantType::TRANSFORM2D:
            return "Transform2D";
        case VariantType::VECTOR3:
            return "Vector3";
        case VariantType::PLANE:
            return "Plane";
        case VariantType::AABB:
            return "AABB";
        case VariantType::QUAT:
            return "Quat";
        case VariantType::BASIS:
            return "Basis";
        case VariantType::TRANSFORM:
            return "Transform";
        // misc types
        case VariantType::COLOR:
            return "Color";
        case VariantType::STRING_NAME:
            return "StringName";
        case VariantType::_RID:
            return "RID";
        case VariantType::OBJECT:
            return "Object";
        case VariantType::CALLABLE:
            return "Callable";
        case VariantType::SIGNAL:
            return "Signal";
        case VariantType::NODE_PATH:
            return "NodePath";
        case VariantType::DICTIONARY:
            return "Dictionary";
        case VariantType::ARRAY:
            return "Array";
        // arrays
        case VariantType::POOL_BYTE_ARRAY:
            return "PoolByteArray";
        case VariantType::POOL_INT_ARRAY:
            return "PoolIntArray";
        case VariantType::POOL_FLOAT32_ARRAY:
            return "PoolRealArray";
        case VariantType::POOL_STRING_ARRAY:
            return "PoolStringArray";
        case VariantType::POOL_VECTOR2_ARRAY:
            return "PoolVector2Array";
        case VariantType::POOL_VECTOR3_ARRAY:
            return "PoolVector3Array";
        case VariantType::POOL_COLOR_ARRAY:
            return "PoolColorArray";
        default: {
        }
    }
    return StringName();
}

bool Variant::can_convert(VariantType p_type_from, VariantType p_type_to) {
    if (p_type_from == p_type_to) {
        return true;
    }
    if (p_type_to == VariantType::NIL && p_type_from != VariantType::NIL) { // nil can convert to anything
        return true;
    }

    if (p_type_from == VariantType::NIL) {
        return (p_type_to == VariantType::OBJECT);
    }

    const VariantType *valid_types = nullptr;
    const VariantType *invalid_types = nullptr;

    switch (p_type_to) {
        case VariantType::BOOL: {
            static const VariantType valid[] = {
                VariantType::INT,
                VariantType::FLOAT,
                VariantType::STRING,
                VariantType::NIL,
            };

            valid_types = valid;
        } break;
        case VariantType::INT: {
            static const VariantType valid[] = {
                VariantType::BOOL,
                VariantType::FLOAT,
                VariantType::STRING,
                VariantType::NIL,
            };

            valid_types = valid;

        } break;
        case VariantType::FLOAT: {
            static const VariantType valid[] = {
                VariantType::BOOL,
                VariantType::INT,
                VariantType::STRING,
                VariantType::NIL,
            };

            valid_types = valid;

        } break;
        case VariantType::STRING: {
            static const VariantType invalid[] = { VariantType::OBJECT, VariantType::NIL };

            invalid_types = invalid;
        } break;
        case VariantType::TRANSFORM2D: {
            static const VariantType valid[] = { VariantType::TRANSFORM, VariantType::NIL };

            valid_types = valid;
        } break;
        case VariantType::QUAT: {
            static const VariantType valid[] = { VariantType::BASIS, VariantType::NIL };

            valid_types = valid;

        } break;
        case VariantType::BASIS: {
            static const VariantType valid[] = { VariantType::QUAT, VariantType::VECTOR3, VariantType::NIL };

            valid_types = valid;

        } break;
        case VariantType::TRANSFORM: {
            static const VariantType valid[] = { VariantType::TRANSFORM2D, VariantType::QUAT, VariantType::BASIS,
                VariantType::NIL };

            valid_types = valid;

        } break;

        case VariantType::COLOR: {
            static const VariantType valid[] = {
                VariantType::STRING,
                VariantType::INT,
                VariantType::NIL,
            };

            valid_types = valid;

        } break;

        case VariantType::STRING_NAME: {
            static const VariantType valid[] = { VariantType::STRING, VariantType::NIL };

            valid_types = valid;
        } break;

        case VariantType::_RID: {
            static const VariantType valid[] = { VariantType::OBJECT, VariantType::NIL };

            valid_types = valid;
        } break;
        case VariantType::OBJECT: {
            static const VariantType valid[] = { VariantType::NIL };

            valid_types = valid;
        } break;
        case VariantType::NODE_PATH: {
            static const VariantType valid[] = { VariantType::STRING, VariantType::NIL };

            valid_types = valid;
        } break;
        case VariantType::ARRAY: {
            static const VariantType valid[] = { VariantType::POOL_BYTE_ARRAY, VariantType::POOL_INT_ARRAY,
                VariantType::POOL_STRING_ARRAY, VariantType::POOL_FLOAT32_ARRAY, VariantType::POOL_COLOR_ARRAY,
                VariantType::POOL_VECTOR2_ARRAY, VariantType::POOL_VECTOR3_ARRAY, VariantType::NIL };

            valid_types = valid;
        } break;
        // arrays
        case VariantType::POOL_BYTE_ARRAY: {
            static const VariantType valid[] = { VariantType::ARRAY, VariantType::NIL };

            valid_types = valid;
        } break;
        case VariantType::POOL_INT_ARRAY: {
            static const VariantType valid[] = { VariantType::ARRAY, VariantType::NIL };
            valid_types = valid;
        } break;
        case VariantType::POOL_FLOAT32_ARRAY: {
            static const VariantType valid[] = { VariantType::ARRAY, VariantType::NIL };

            valid_types = valid;
        } break;
        case VariantType::POOL_STRING_ARRAY: {
            static const VariantType valid[] = { VariantType::ARRAY, VariantType::NIL };
            valid_types = valid;
        } break;
        case VariantType::POOL_VECTOR2_ARRAY: {
            static const VariantType valid[] = { VariantType::ARRAY, VariantType::NIL };
            valid_types = valid;

        } break;
        case VariantType::POOL_VECTOR3_ARRAY: {
            static const VariantType valid[] = { VariantType::ARRAY, VariantType::NIL };
            valid_types = valid;

        } break;
        case VariantType::POOL_COLOR_ARRAY: {
            static const VariantType valid[] = { VariantType::ARRAY, VariantType::NIL };

            valid_types = valid;

        } break;
        default: {
        }
    }

    if (valid_types) {
        int i = 0;
        while (valid_types[i] != VariantType::NIL) {
            if (p_type_from == valid_types[i]) {
                return true;
            }
            i++;
        }

    } else if (invalid_types) {
        int i = 0;
        while (invalid_types[i] != VariantType::NIL) {
            if (p_type_from == invalid_types[i]) {
                return false;
            }
            i++;
        }

        return true;
    }

    return false;
}

bool Variant::can_convert_strict(VariantType p_type_from, VariantType p_type_to) {
    if (p_type_from == p_type_to) {
        return true;
    }
    if (p_type_to == VariantType::NIL && p_type_from != VariantType::NIL) { // nil can convert to anything
        return true;
    }

    if (p_type_from == VariantType::NIL) {
        return (p_type_to == VariantType::OBJECT);
    }

    const VariantType *valid_types = nullptr;

    switch (p_type_to) {
        case VariantType::BOOL: {
            static const VariantType valid[] = {
                VariantType::INT,
                VariantType::FLOAT,
                // STRING,
                VariantType::NIL,
            };

            valid_types = valid;
        } break;
        case VariantType::INT: {
            static const VariantType valid[] = {
                VariantType::BOOL,
                VariantType::FLOAT,
                // STRING,
                VariantType::NIL,
            };

            valid_types = valid;

        } break;
        case VariantType::FLOAT: {
            static const VariantType valid[] = {
                VariantType::BOOL,
                VariantType::INT,
                // STRING,
                VariantType::NIL,
            };

            valid_types = valid;

        } break;
        case VariantType::STRING: {
            static const VariantType valid[] = { VariantType::NODE_PATH, VariantType::STRING_NAME, VariantType::NIL };

            valid_types = valid;
        } break;
        case VariantType::TRANSFORM2D: {
            static const VariantType valid[] = { VariantType::TRANSFORM, VariantType::NIL };

            valid_types = valid;
        } break;
        case VariantType::QUAT: {
            static const VariantType valid[] = { VariantType::BASIS, VariantType::NIL };

            valid_types = valid;

        } break;
        case VariantType::BASIS: {
            static const VariantType valid[] = { VariantType::QUAT, VariantType::VECTOR3, VariantType::NIL };

            valid_types = valid;

        } break;
        case VariantType::TRANSFORM: {
            static const VariantType valid[] = { VariantType::TRANSFORM2D, VariantType::QUAT, VariantType::BASIS,
                VariantType::NIL };

            valid_types = valid;

        } break;

        case VariantType::COLOR: {
            static const VariantType valid[] = {
                VariantType::STRING,
                VariantType::INT,
                VariantType::NIL,
            };

            valid_types = valid;

        } break;

        case VariantType::_RID: {
            static const VariantType valid[] = { VariantType::OBJECT, VariantType::NIL };

            valid_types = valid;
        } break;
        case VariantType::OBJECT: {
            static const VariantType valid[] = { VariantType::NIL };

            valid_types = valid;
        } break;
        case VariantType::STRING_NAME: {
            static const VariantType valid[] = { VariantType::STRING, VariantType::NIL };

            valid_types = valid;
        } break;
        case VariantType::NODE_PATH: {
            static const VariantType valid[] = { VariantType::STRING, VariantType::NIL };

            valid_types = valid;
        } break;
        case VariantType::ARRAY: {
            static const VariantType valid[] = { VariantType::POOL_BYTE_ARRAY, VariantType::POOL_INT_ARRAY,
                VariantType::POOL_STRING_ARRAY, VariantType::POOL_FLOAT32_ARRAY, VariantType::POOL_COLOR_ARRAY,
                VariantType::POOL_VECTOR2_ARRAY, VariantType::POOL_VECTOR3_ARRAY, VariantType::NIL };

            valid_types = valid;
        } break;
        // arrays
        case VariantType::POOL_BYTE_ARRAY: {
            static const VariantType valid[] = { VariantType::ARRAY, VariantType::NIL };

            valid_types = valid;
        } break;
        case VariantType::POOL_INT_ARRAY: {
            static const VariantType valid[] = { VariantType::ARRAY, VariantType::NIL };
            valid_types = valid;
        } break;
        case VariantType::POOL_FLOAT32_ARRAY: {
            static const VariantType valid[] = { VariantType::ARRAY, VariantType::NIL };

            valid_types = valid;
        } break;
        case VariantType::POOL_STRING_ARRAY: {
            static const VariantType valid[] = { VariantType::ARRAY, VariantType::NIL };
            valid_types = valid;
        } break;
        case VariantType::POOL_VECTOR2_ARRAY: {
            static const VariantType valid[] = { VariantType::ARRAY, VariantType::NIL };
            valid_types = valid;

        } break;
        case VariantType::POOL_VECTOR3_ARRAY: {
            static const VariantType valid[] = { VariantType::ARRAY, VariantType::NIL };
            valid_types = valid;

        } break;
        case VariantType::POOL_COLOR_ARRAY: {
            static const VariantType valid[] = { VariantType::ARRAY, VariantType::NIL };

            valid_types = valid;

        } break;
        default: {
        }
    }

    if (valid_types) {
        int i = 0;
        while (valid_types[i] != VariantType::NIL) {
            if (p_type_from == valid_types[i]) {
                return true;
            }
            i++;
        }
    }

    return false;
}

bool Variant::deep_equal(const Variant &p_variant, int p_recursion_count) const {
    ERR_FAIL_COND_V_MSG(p_recursion_count > MAX_RECURSION, true, "Max recursion reached");

    // Containers must be handled with recursivity checks
    switch (type) {
        case VariantType::DICTIONARY: {
            if (p_variant.type != VariantType::DICTIONARY) {
                return false;
            }

            const Dictionary v1_as_d = Dictionary(*this);
            const Dictionary v2_as_d = Dictionary(p_variant);

            return v1_as_d.deep_equal(v2_as_d, p_recursion_count + 1);
        }
        case VariantType::ARRAY: {
            if (p_variant.type != VariantType::ARRAY) {
                return false;
            }

            const Array v1_as_a = Array(*this);
            const Array v2_as_a = Array(p_variant);

            return v1_as_a.deep_equal(v2_as_a, p_recursion_count + 1);
        }
        default: {
            return *this == p_variant;
        }
    }
}

bool Variant::operator==(const Variant &p_variant) const {
    if (type != p_variant.type) { // evaluation of operator== needs to be more strict
        return false;
    }
    return evaluate_equal(*this, p_variant);
}

bool Variant::operator!=(const Variant &p_variant) const {
    if (type != p_variant.type) { // evaluation of operator== needs to be more strict
        return true;
    }
    return !evaluate_equal(*this, p_variant);
}

bool Variant::operator<(const Variant &p_variant) const {
    if (type != p_variant.type) { // if types differ, then order by type first
        return type < p_variant.type;
    }
    bool v;
    Variant r;
    evaluate(OP_LESS, *this, p_variant, r, v);
    assert(r.get_type() == VariantType::BOOL);
    return r.as<bool>();
}

bool Variant::is_zero() const {
    switch (type) {
        case VariantType::NIL: {
            return true;
        }

        // atomic types
        case VariantType::BOOL: {
            return !(_data._bool);
        }
        case VariantType::INT: {
            return _data._int == 0;
        }
        case VariantType::FLOAT: {
            return _data._real == 0.0;
        }
        case VariantType::REN_ENT: {
            return _data._int == entt::to_integral(RenderingEntity(entt::null));
        }
        case VariantType::STRING: {
            return reinterpret_cast<const String *>(_data._mem)->empty();
        }

        // math types
        case VariantType::VECTOR2: {
            return *reinterpret_cast<const Vector2 *>(_data._mem) == Vector2();
        }
        case VariantType::RECT2: {
            return *reinterpret_cast<const Rect2 *>(_data._mem) == Rect2();
        }
        case VariantType::TRANSFORM2D: {
            return *_data._transform2d == Transform2D();
        }
        case VariantType::VECTOR3: {
            return *reinterpret_cast<const Vector3 *>(_data._mem) == Vector3();
        }
        case VariantType::PLANE: {
            return *reinterpret_cast<const Plane *>(_data._mem) == Plane();
        }
        /*
        case VariantType::QUAT: {


        } break;*/
        case VariantType::AABB: {
            return *_data._aabb == ::AABB();
        }
        case VariantType::QUAT: {
            return *reinterpret_cast<const Quat *>(_data._mem) == Quat();
        }
        case VariantType::BASIS: {
            return *_data._basis == Basis();
        }
        case VariantType::TRANSFORM: {
            return *_data._transform == Transform();
        }

        // misc types
        case VariantType::COLOR: {
            return *reinterpret_cast<const Color *>(_data._mem) == Color();
        }
        case VariantType::_RID: {
            return *reinterpret_cast<const RID *>(_data._mem) == RID();
        }
        case VariantType::OBJECT: {
            return _UNSAFE_OBJ_PROXY_PTR(*this) == nullptr;
        }
        case VariantType::CALLABLE: {
            return reinterpret_cast<const Callable *>(_data._mem)->is_null();
        }
        case VariantType::SIGNAL: {
            return reinterpret_cast<const Signal *>(_data._mem)->is_null();
        }
        case VariantType::STRING_NAME: {
            return reinterpret_cast<const StringName *>(_data._mem)->empty();
        }
        case VariantType::NODE_PATH: {
            return reinterpret_cast<const NodePath *>(_data._mem)->is_empty();
        }
        case VariantType::DICTIONARY: {
            return reinterpret_cast<const Dictionary *>(_data._mem)->empty();
        }
        case VariantType::ARRAY: {
            return reinterpret_cast<const Array *>(_data._mem)->empty();
        }

        // arrays
        case VariantType::POOL_BYTE_ARRAY: {
            return reinterpret_cast<const PoolVector<uint8_t> *>(_data._mem)->empty();
        }
        case VariantType::POOL_INT_ARRAY: {
            return reinterpret_cast<const PoolVector<int> *>(_data._mem)->empty();
        }
        case VariantType::POOL_FLOAT32_ARRAY: {
            return reinterpret_cast<const PoolVector<real_t> *>(_data._mem)->empty();
        }
        case VariantType::POOL_STRING_ARRAY: {
            return reinterpret_cast<const PoolVector<String> *>(_data._mem)->empty();
        }
        case VariantType::POOL_VECTOR2_ARRAY: {
            return reinterpret_cast<const PoolVector<Vector2> *>(_data._mem)->empty();
        }
        case VariantType::POOL_VECTOR3_ARRAY: {
            return reinterpret_cast<const PoolVector<Vector3> *>(_data._mem)->empty();
        }
        case VariantType::POOL_COLOR_ARRAY: {
            return reinterpret_cast<const PoolVector<Color> *>(_data._mem)->empty();
        }
        default: {
        }
    }

    return false;
}

void Variant::reference(const Variant &p_variant) {
    switch (type) {
        case VariantType::NIL:
        case VariantType::BOOL:
        case VariantType::INT:
        case VariantType::FLOAT:
            break;
        default:
            clear();
    }

    type = p_variant.type;

    switch (p_variant.type) {
        case VariantType::NIL: {
            // none
        } break;

        // atomic types
        case VariantType::BOOL: {
            _data._bool = p_variant._data._bool;
        } break;
        case VariantType::REN_ENT:
        case VariantType::INT: {
            _data._int = p_variant._data._int;
        } break;
        case VariantType::FLOAT: {
            _data._real = p_variant._data._real;
        } break;
        case VariantType::STRING: {
            memnew_placement(_data._mem, String(*reinterpret_cast<const String *>(p_variant._data._mem)));
        } break;

        // math types
        case VariantType::VECTOR2: {
            memnew_placement(_data._mem, Vector2(*reinterpret_cast<const Vector2 *>(p_variant._data._mem)));
        } break;
        case VariantType::RECT2: {
            memnew_placement(_data._mem, Rect2(*reinterpret_cast<const Rect2 *>(p_variant._data._mem)));
        } break;
        case VariantType::TRANSFORM2D: {
            _data._transform2d = memnew(Transform2D(*p_variant._data._transform2d));
        } break;
        case VariantType::VECTOR3: {
            memnew_placement(_data._mem, Vector3(*reinterpret_cast<const Vector3 *>(p_variant._data._mem)));
        } break;
        case VariantType::PLANE: {
            memnew_placement(_data._mem, Plane(*reinterpret_cast<const Plane *>(p_variant._data._mem)));
        } break;

        case VariantType::AABB: {
            _data._aabb = memnew(::AABB(*p_variant._data._aabb));
        } break;
        case VariantType::QUAT: {
            memnew_placement(_data._mem, Quat(*reinterpret_cast<const Quat *>(p_variant._data._mem)));

        } break;
        case VariantType::BASIS: {
            _data._basis = memnew(Basis(*p_variant._data._basis));

        } break;
        case VariantType::TRANSFORM: {
            _data._transform = memnew(Transform(*p_variant._data._transform));
        } break;

        // misc types
        case VariantType::COLOR: {
            memnew_placement(_data._mem, Color(*reinterpret_cast<const Color *>(p_variant._data._mem)));

        } break;
        case VariantType::_RID: {
            memnew_placement(_data._mem, RID(*reinterpret_cast<const RID *>(p_variant._data._mem)));
        } break;
        case VariantType::OBJECT: {
            memnew_placement(_data._mem, ObjData(p_variant._get_obj()));
            if (likely(_get_obj().rc)) {
                _get_obj().rc->increment();
            }
        } break;
        case VariantType::CALLABLE: {
            memnew_placement(_data._mem, Callable(*reinterpret_cast<const Callable *>(p_variant._data._mem)));
        } break;
        case VariantType::SIGNAL: {
            memnew_placement(_data._mem, Signal(*reinterpret_cast<const Signal *>(p_variant._data._mem)));
        } break;

        case VariantType::STRING_NAME: {
            memnew_placement(_data._mem, StringName(*reinterpret_cast<const StringName *>(p_variant._data._mem)));
        } break;

        case VariantType::NODE_PATH: {
            memnew_placement(_data._mem, NodePath(*reinterpret_cast<const NodePath *>(p_variant._data._mem)));

        } break;
        case VariantType::DICTIONARY: {
            memnew_placement(_data._mem, Dictionary(*reinterpret_cast<const Dictionary *>(p_variant._data._mem)));

        } break;
        case VariantType::ARRAY: {
            memnew_placement(_data._mem, Array(*reinterpret_cast<const Array *>(p_variant._data._mem)));

        } break;

        // arrays
        case VariantType::POOL_BYTE_ARRAY: {
            memnew_placement(_data._mem,
                    PoolVector<uint8_t>(*reinterpret_cast<const PoolVector<uint8_t> *>(p_variant._data._mem)));

        } break;
        case VariantType::POOL_INT_ARRAY: {
            memnew_placement(
                    _data._mem, PoolVector<int>(*reinterpret_cast<const PoolVector<int> *>(p_variant._data._mem)));

        } break;
        case VariantType::POOL_FLOAT32_ARRAY: {
            memnew_placement(_data._mem,
                    PoolVector<real_t>(*reinterpret_cast<const PoolVector<real_t> *>(p_variant._data._mem)));

        } break;
        case VariantType::POOL_STRING_ARRAY: {
            memnew_placement(_data._mem,
                    PoolVector<String>(*reinterpret_cast<const PoolVector<String> *>(p_variant._data._mem)));

        } break;
        case VariantType::POOL_VECTOR2_ARRAY: {
            memnew_placement(_data._mem,
                    PoolVector<Vector2>(*reinterpret_cast<const PoolVector<Vector2> *>(p_variant._data._mem)));

        } break;
        case VariantType::POOL_VECTOR3_ARRAY: {
            memnew_placement(_data._mem,
                    PoolVector<Vector3>(*reinterpret_cast<const PoolVector<Vector3> *>(p_variant._data._mem)));

        } break;
        case VariantType::POOL_COLOR_ARRAY: {
            memnew_placement(
                    _data._mem, PoolVector<Color>(*reinterpret_cast<const PoolVector<Color> *>(p_variant._data._mem)));

        } break;
        default: {
        }
    }
}

void Variant::zero() {
    switch (type) {
        case VariantType::NIL:
            break;
        case VariantType::BOOL:
            this->_data._bool = false;
            break;
        case VariantType::INT:
            this->_data._int = 0;
            break;
        case VariantType::REN_ENT:
            _data._int = entt::to_integral(RenderingEntity(entt::null));
            break;
        case VariantType::FLOAT:
            this->_data._real = 0;
            break;
        case VariantType::VECTOR2:
            *reinterpret_cast<Vector2 *>(this->_data._mem) = Vector2();
            break;
        case VariantType::RECT2:
            *reinterpret_cast<Rect2 *>(this->_data._mem) = Rect2();
            break;
        case VariantType::VECTOR3:
            *reinterpret_cast<Vector3 *>(this->_data._mem) = Vector3();
            break;
        case VariantType::PLANE:
            *reinterpret_cast<Plane *>(this->_data._mem) = Plane();
            break;
        case VariantType::QUAT:
            *reinterpret_cast<Quat *>(this->_data._mem) = Quat();
            break;
        case VariantType::COLOR:
            *reinterpret_cast<Color *>(this->_data._mem) = Color();
            break;
        default:
            this->clear();
            break;
    }
}

void Variant::clear() {
    switch (type) {
        case VariantType::STRING: {
            reinterpret_cast<String *>(_data._mem)->~String();
        } break;
        /*
        // no point, they don't allocate memory
        VariantType::VECTOR3,
        VariantType::PLANE,
        VariantType::QUAT,
        VariantType::COLOR,
        VariantType::VECTOR2,
        VariantType::RECT2
    */
        case VariantType::TRANSFORM2D: {
            memdelete(_data._transform2d);
        } break;
        case VariantType::AABB: {
            memdelete(_data._aabb);
        } break;
        case VariantType::BASIS: {
            memdelete(_data._basis);
        } break;
        case VariantType::TRANSFORM: {
            memdelete(_data._transform);
        } break;

        // misc types
        case VariantType::STRING_NAME: {
            reinterpret_cast<StringName *>(_data._mem)->~StringName();
        } break;

        case VariantType::NODE_PATH: {
            reinterpret_cast<NodePath *>(_data._mem)->~NodePath();
        } break;
        case VariantType::OBJECT: {
            if (likely(_get_obj().rc)) {
                if (unlikely(_get_obj().rc->decrement())) {
                    memdelete(_get_obj().rc);
                }
            } else {
                _get_obj().ref.unref();
            }
        } break;
        case VariantType::CALLABLE: {
            reinterpret_cast<Callable *>(_data._mem)->~Callable();
        } break;
        case VariantType::SIGNAL: {
            reinterpret_cast<Signal *>(_data._mem)->~Signal();
        } break;

        case VariantType::_RID: {
            // not much need probably
            reinterpret_cast<RID *>(_data._mem)->~RID();
        } break;
        case VariantType::DICTIONARY: {
            reinterpret_cast<Dictionary *>(_data._mem)->~Dictionary();
        } break;
        case VariantType::ARRAY: {
            reinterpret_cast<Array *>(_data._mem)->~Array();
        } break;
        // arrays
        case VariantType::POOL_BYTE_ARRAY: {
            reinterpret_cast<PoolVector<uint8_t> *>(_data._mem)->~PoolVector<uint8_t>();
        } break;
        case VariantType::POOL_INT_ARRAY: {
            reinterpret_cast<PoolVector<int> *>(_data._mem)->~PoolVector<int>();
        } break;
        case VariantType::POOL_FLOAT32_ARRAY: {
            reinterpret_cast<PoolVector<real_t> *>(_data._mem)->~PoolVector<real_t>();
        } break;
        case VariantType::POOL_STRING_ARRAY: {
            reinterpret_cast<PoolVector<String> *>(_data._mem)->~PoolVector<String>();
        } break;
        case VariantType::POOL_VECTOR2_ARRAY: {
            reinterpret_cast<PoolVector<Vector2> *>(_data._mem)->~PoolVector<Vector2>();
        } break;
        case VariantType::POOL_VECTOR3_ARRAY: {
            reinterpret_cast<PoolVector<Vector3> *>(_data._mem)->~PoolVector<Vector3>();
        } break;
        case VariantType::POOL_COLOR_ARRAY: {
            reinterpret_cast<PoolVector<Color> *>(_data._mem)->~PoolVector<Color>();
        } break;
        default: {
        } /* not needed */
    }

    type = VariantType::NIL;
}

Variant::operator signed int() const {
    switch (type) {
        case VariantType::NIL:
            return 0;
        case VariantType::BOOL:
            return _data._bool ? 1 : 0;
        case VariantType::INT:
            return _data._int;
        case VariantType::FLOAT:
            return _data._real;
        case VariantType::STRING:
            return StringUtils::to_int((String) * this);
        default: {
            return 0;
        }
    }
}

Variant::operator unsigned int() const {
    switch (type) {
        case VariantType::NIL:
            return 0;
        case VariantType::BOOL:
            return _data._bool ? 1 : 0;
        case VariantType::INT:
            return _data._int;
        case VariantType::FLOAT:
            return _data._real;
        case VariantType::STRING:
            return StringUtils::to_int((String) * this);
        default: {
            return 0;
        }
    }
}

Variant::operator int64_t() const {
    switch (type) {
        case VariantType::NIL:
            return 0;
        case VariantType::BOOL:
            return _data._bool ? 1 : 0;
        case VariantType::INT:
            return _data._int;
        case VariantType::FLOAT:
            return _data._real;
        case VariantType::STRING:
            return StringUtils::to_int64((String) * this);
        default: {
            return 0;
        }
    }
}

/*
template <>
Variant::operator long unsigned int() const {

    switch( type ) {

        case VariantType::NIL: return 0;
        case VariantType::BOOL: return _data._bool ? 1 : 0;
        case VariantType::INT: return _data._int;
        case VariantType::FLOAT: return _data._real;
        case VariantType::STRING: return as<String>().to_int();
        default: {

            return 0;
        }
    }

    return 0;
};
*/

Variant::operator uint64_t() const {
    switch (type) {
        case VariantType::NIL:
            return 0;
        case VariantType::BOOL:
            return _data._bool ? 1 : 0;
        case VariantType::INT:
            return _data._int;
        case VariantType::FLOAT:
            return _data._real;
        case VariantType::STRING:
            return StringUtils::to_int64((String) * this);
        default: {
            return 0;
        }
    }
}

Variant::operator signed short() const {
    switch (type) {
        case VariantType::NIL:
            return 0;
        case VariantType::BOOL:
            return _data._bool ? 1 : 0;
        case VariantType::INT:
            return _data._int;
        case VariantType::FLOAT:
            return _data._real;
        case VariantType::STRING:
            return StringUtils::to_int((String) * this);
        default: {
            return 0;
        }
    }
}

Variant::operator unsigned short() const {
    switch (type) {
        case VariantType::NIL:
            return 0;
        case VariantType::BOOL:
            return _data._bool ? 1 : 0;
        case VariantType::INT:
            return _data._int;
        case VariantType::FLOAT:
            return _data._real;
        case VariantType::STRING:
            return StringUtils::to_int((String) * this);
        default: {
            return 0;
        }
    }
}

Variant::operator signed char() const {
    switch (type) {
        case VariantType::NIL:
            return 0;
        case VariantType::BOOL:
            return _data._bool ? 1 : 0;
        case VariantType::INT:
            return _data._int;
        case VariantType::FLOAT:
            return _data._real;
        case VariantType::STRING:
            return StringUtils::to_int((String) * this);
        default: {
            return 0;
        }
    }
}

Variant::operator unsigned char() const {
    switch (type) {
        case VariantType::NIL:
            return 0;
        case VariantType::BOOL:
            return _data._bool ? 1 : 0;
        case VariantType::INT:
            return _data._int;
        case VariantType::FLOAT:
            return _data._real;
        case VariantType::STRING:
            return StringUtils::to_int((String) * this);
        default: {
            return 0;
        }
    }
}

Variant::operator QChar() const {
    return (uint32_t) * this;
}

Variant::operator float() const {
    switch (type) {
        case VariantType::NIL:
            return 0;
        case VariantType::BOOL:
            return _data._bool ? 1.0 : 0.0;
        case VariantType::INT:
            return (float)_data._int;
        case VariantType::FLOAT:
            return _data._real;
        case VariantType::STRING:
            return StringUtils::to_double((String) * this);
        default: {
            return 0;
        }
    }
}
Variant::operator double() const {
    switch (type) {
        case VariantType::NIL:
            return 0;
        case VariantType::BOOL:
            return _data._bool ? 1.0 : 0.0;
        case VariantType::INT:
            return (double)_data._int;
        case VariantType::FLOAT:
            return _data._real;
        case VariantType::STRING:
            return StringUtils::to_double((String) * this);
        default: {
            return 0;
        }
    }
}
Variant::operator StringName() const {
    if (type == VariantType::STRING_NAME) {
        return *reinterpret_cast<const StringName *>(_data._mem);
    } else if (type == VariantType::STRING) {
        return StringName(*reinterpret_cast<const String *>(_data._mem));
    }

    return StringName();
}
Variant::operator UIString() const {
    Vector<const void *> stack;

    return StringUtils::from_utf8(stringify(stack));
}

Variant::operator String() const {
    Vector<const void *> stack;

    return stringify(stack);
}

Variant::operator StringView() const {
    if (type == VariantType::NIL) {
        return "";
    }

    assert(type == VariantType::STRING || type == VariantType::STRING_NAME);

    if (type == VariantType::STRING_NAME) {
        return StringView(*reinterpret_cast<const StringName *>(_data._mem));
    }

    return StringView(*reinterpret_cast<const String *>(_data._mem));
}

Variant::operator NodePath() const {
    if (type == VariantType::NODE_PATH) {
        return *reinterpret_cast<const NodePath *>(_data._mem);
    }
    if (type == VariantType::STRING) {
        return NodePath((String) * this);
    }
    return NodePath();
}

Variant::operator IP_Address() const {
    if (type == VariantType::POOL_FLOAT32_ARRAY || type == VariantType::POOL_INT_ARRAY ||
            type == VariantType::POOL_BYTE_ARRAY) {
        PoolVector<int> addr = (PoolVector<int>)*this;
        if (addr.size() == 4) {
            return IP_Address(addr.get(0), addr.get(1), addr.get(2), addr.get(3));
        }
    }

    return IP_Address((String) * this);
}

Variant::operator Transform() const {
    if (type == VariantType::TRANSFORM) {
        return *_data._transform;
    }
    if (type == VariantType::BASIS) {
        return Transform(*_data._basis, Vector3());
    }
    if (type == VariantType::QUAT) {
        return Transform(Basis(*reinterpret_cast<const Quat *>(_data._mem)), Vector3());
    }
    if (type == VariantType::TRANSFORM2D) {
        const Transform2D &t = *_data._transform2d;
        Transform m;
        m.basis.elements[0][0] = t.elements[0][0];
        m.basis.elements[1][0] = t.elements[0][1];
        m.basis.elements[0][1] = t.elements[1][0];
        m.basis.elements[1][1] = t.elements[1][1];
        m.origin[0] = t.elements[2][0];
        m.origin[1] = t.elements[2][1];
        return m;
    }
    return Transform();
}

Variant::operator Basis() const {
    if (type == VariantType::BASIS) {
        return *_data._basis;
    }
    if (type == VariantType::QUAT) {
        return *reinterpret_cast<const Quat *>(_data._mem);
    }
    if (type == VariantType::VECTOR3) {
        return Basis(*reinterpret_cast<const Vector3 *>(_data._mem));
    }
    if (type == VariantType::TRANSFORM) { // unexposed in Variant::can_convert?
        return _data._transform->basis;
    }
    return Basis();
}

Variant::operator Quat() const {
    if (type == VariantType::QUAT) {
        return *reinterpret_cast<const Quat *>(_data._mem);
    }
    if (type == VariantType::BASIS) {
        return *_data._basis;
    }
    if (type == VariantType::TRANSFORM) {
        return _data._transform->basis;
    }
    return Quat();
}

Variant::operator GameEntity() const {
    if (type == VariantType::INT) {
        return GE(_data._int);
    } else if (type == VariantType::OBJECT) {
        return _get_obj().rc->instance_id;
    } else if (likely(!_get_obj().ref.is_null())) {
        return _REF_OBJ_PTR(*this)->get_instance_id();
    } else {
        return entt::null;
    }
}

Variant::operator RenderingEntity() const {
    if (type == VariantType::REN_ENT) {
        return RE(_data._int);
    }
    return entt::null;
}

struct _VariantStrPair {
    String key;
    String value;

    bool operator<(const _VariantStrPair &p) const { return key < p.key; }
};

String Variant::stringify(Vector<const void *> &stack) const {
    switch (type) {
        case VariantType::NIL:
            return ("Null");
        case VariantType::BOOL:
            return (_data._bool ? "True" : "False");
        case VariantType::INT:
            return itos(_data._int);
        case VariantType::FLOAT:
            return rtos(_data._real);
        case VariantType::STRING:
            return (*reinterpret_cast<const String *>(_data._mem));
        case VariantType::STRING_NAME:
            return (*reinterpret_cast<const StringName *>(_data._mem)).asCString();
        case VariantType::VECTOR2:
            return "(" + (String)as<Vector2>() + ")";
        case VariantType::RECT2:
            return "(" + (String)as<Rect2>() + ")";
        case VariantType::TRANSFORM2D: {
            Transform2D mat32 = as<Transform2D>();
            return "(" + Variant(mat32.elements[0]).as<String>() + ", " + Variant(mat32.elements[1]).as<String>() +
                   ", " + Variant(mat32.elements[2]).as<String>() + ")";
        }
        case VariantType::VECTOR3:
            return "(" + (String)as<Vector3>() + ")";
        case VariantType::PLANE:
            return as<Plane>();
        // case VariantType::QUAT:
        case VariantType::AABB:
            return as<AABB>();
        case VariantType::QUAT:
            return "(" + (String)as<Quat>() + ")";
        case VariantType::BASIS: {
            Basis mat3 = as<Basis>();

            String mtx("(");
            for (int i = 0; i < 3; i++) {
                if (i != 0) {
                    mtx += (", ");
                }

                mtx += ("(");

                for (int j = 0; j < 3; j++) {
                    if (j != 0) {
                        mtx += (", ");
                    }

                    mtx += Variant(mat3.elements[i][j]).as<String>();
                }

                mtx += (")");
            }

            return mtx + ")";
        }
        case VariantType::TRANSFORM:
            return as<Transform>();
        case VariantType::NODE_PATH:
            return ((String)as<NodePath>());
        case VariantType::COLOR: {
            Color z(as<Color>());
            return FormatVE("%f,%f,%f,%f", z.r, z.g, z.b, z.a);
        }
        case VariantType::DICTIONARY: {
            const Dictionary &d = *reinterpret_cast<const Dictionary *>(_data._mem);
            if (stack.contains(d.id())) {
                return ("{...}");
            }

            stack.push_back(d.id());

            // const String *K=NULL;
            String str("{");
            auto keys(d.get_key_list());

            Vector<_VariantStrPair> pairs;

            for (auto &E : keys) {
                _VariantStrPair sp;
                sp.key = E;
                sp.value = d[E].stringify(stack);

                pairs.push_back(sp);
            }
            eastl::sort(pairs.begin(), pairs.end());

            for (int i = 0; i < pairs.size(); i++) {
                if (i > 0) {
                    str += (", ");
                }
                str += pairs[i].key + ":" + pairs[i].value;
            }
            str += ("}");

            return str;
        }
        case VariantType::POOL_VECTOR2_ARRAY: {
            PoolVector<Vector2> vec = as<PoolVector<Vector2>>();
            String str("[");
            for (int i = 0; i < vec.size(); i++) {
                if (i > 0) {
                    str += (", ");
                }
                str += Variant(vec[i]).as<String>();
            }
            str += ("]");
            return str;
        }
        case VariantType::POOL_VECTOR3_ARRAY: {
            PoolVector<Vector3> vec = as<PoolVector<Vector3>>();
            String str("[");
            for (int i = 0; i < vec.size(); i++) {
                if (i > 0) {
                    str += ", ";
                }
                str = str + Variant(vec[i]).as<String>();
            }
            str += ("]");
            return str;
        }
        case VariantType::POOL_STRING_ARRAY: {
            PoolVector<String> vec = as<PoolVector<String>>();
            String str("[");
            for (int i = 0; i < vec.size(); i++) {
                if (i > 0) {
                    str += ", ";
                }
                str = str + vec[i];
            }
            str += "]";
            return str;
        }
        case VariantType::POOL_INT_ARRAY: {
            PoolVector<int> vec = as<PoolVector<int>>();
            String str("[");
            for (int i = 0; i < vec.size(); i++) {
                if (i > 0) {
                    str += ", ";
                }
                str = str + itos(vec[i]);
            }
            str += "]";
            return str;
        }
        case VariantType::POOL_FLOAT32_ARRAY: {
            PoolVector<real_t> vec = as<PoolVector<real_t>>();
            String str("[");
            for (int i = 0; i < vec.size(); i++) {
                if (i > 0) {
                    str += (", ");
                }
                str = str + rtos(vec[i]);
            }
            str += ("]");
            return str;
        }
        case VariantType::ARRAY: {
            Array arr = as<Array>();
            if (stack.find(arr.id())) {
                return ("[...]");
            }
            stack.push_back(arr.id());

            String str("[");
            for (int i = 0; i < arr.size(); i++) {
                if (i) {
                    str += (", ");
                }

                str += arr[i].stringify(stack);
            }

            str += ("]");
            return str;
        }
        case VariantType::OBJECT: {
            Object *obj = _OBJ_PTR(*this);
        if (likely(obj)) {
                return obj->to_string();
        } else {
            if (_get_obj().rc) {
                return "[Deleted Object]";
            }
            return "[Object:null]";
        }
        }
        case VariantType::CALLABLE: {
            const Callable &c = *reinterpret_cast<const Callable *>(_data._mem);
            return (String)c;
        }
        case VariantType::SIGNAL: {
            const Signal &s = *reinterpret_cast<const Signal *>(_data._mem);
            return (String)s;
        }
        case VariantType::_RID: {
            const RID &s = *reinterpret_cast<const RID *>(_data._mem);
            return "RID(" + itos(s.get_id()) + ")";
        }
        default: {
            return "[" + String(get_type_name(type)) + "]";
        }
    }

}

Variant::operator Vector2() const {
    if (type == VariantType::VECTOR2) {
        return *reinterpret_cast<const Vector2 *>(_data._mem);
    }
    if (type == VariantType::VECTOR3) {
        return Vector2(
                reinterpret_cast<const Vector3 *>(_data._mem)->x, reinterpret_cast<const Vector3 *>(_data._mem)->y);
    }
    return Vector2();
}

Variant::operator Rect2() const {
    if (type == VariantType::RECT2) {
        return *reinterpret_cast<const Rect2 *>(_data._mem);
    }
    return Rect2();
}

Variant::operator Vector3() const {
    if (type == VariantType::VECTOR3) {
        return *reinterpret_cast<const Vector3 *>(_data._mem);
    }
    if (type == VariantType::VECTOR2) {
        return Vector3(reinterpret_cast<const Vector2 *>(_data._mem)->x,
                reinterpret_cast<const Vector2 *>(_data._mem)->y, 0.0);
    }
    return Vector3();
}

Variant::operator Plane() const {
    if (type == VariantType::PLANE) {
        return *reinterpret_cast<const Plane *>(_data._mem);
    }
    return Plane();
}

Variant::operator ::AABB() const {
    if (type == VariantType::AABB) {
        return *_data._aabb;
    }
    return ::AABB();
}

Variant::operator Transform2D() const {
    if (type == VariantType::TRANSFORM2D) {
        return *_data._transform2d;
    }
    if (type == VariantType::TRANSFORM) {
        const Transform &t = *_data._transform;
        Transform2D m;
        m.elements[0][0] = t.basis.elements[0][0];
        m.elements[0][1] = t.basis.elements[1][0];
        m.elements[1][0] = t.basis.elements[0][1];
        m.elements[1][1] = t.basis.elements[1][1];
        m.elements[2][0] = t.origin[0];
        m.elements[2][1] = t.origin[1];
        return m;
    }
    return Transform2D();
}

Variant::operator Color() const {
    if (type == VariantType::COLOR) {
        return *reinterpret_cast<const Color *>(_data._mem);
    }
    if (type == VariantType::STRING) {
        return Color::html((String) * this);
    }
    if (type == VariantType::INT) {
        return Color::hex(_data._int);
    }
    return Color();
}

Variant::operator RefPtr() const {
    if (type == VariantType::OBJECT) {
        return _get_obj().ref;
    }
    return RefPtr();
}

Variant::operator RID() const {
    if (type == VariantType::_RID) {
        return *reinterpret_cast<const RID *>(_data._mem);
    }
    if (type != VariantType::OBJECT) {
        return RID();
    }
    if (!_get_obj().ref.is_null()) {
        return _get_obj().ref.get_phys_rid();
    }
    Object *obj = likely(_get_obj().rc) ? _get_obj().rc->get_ptr() : nullptr;
    if (unlikely(!obj)) {
        if (_get_obj().rc) {
            ERR_PRINT("Attempted get RID on a deleted object.");
        }
        return RID();
    }

    Callable::CallError ce;
    Variant ret = obj->call(CoreStringNames::get_singleton()->get_rid, nullptr, 0, ce);
    if (ce.error == Callable::CallError::CALL_OK && ret.get_type() == VariantType::_RID) {
        return ret.as<RID>();
    }
    return RID();
}



Variant::operator Object *() const {
    if (type == VariantType::OBJECT) {
        return _OBJ_PTR(*this);
    }
    return nullptr;
}

Variant::operator Callable() const {
    if (type == VariantType::CALLABLE) {
        return *reinterpret_cast<const Callable *>(_data._mem);
    }
    return Callable();
}

Variant::operator Signal() const {
    if (type == VariantType::SIGNAL) {
        return *reinterpret_cast<const Signal *>(_data._mem);
    }
    return Signal();
}

Variant::operator Dictionary() const {
    if (type == VariantType::DICTIONARY) {
        return *reinterpret_cast<const Dictionary *>(_data._mem);
    }
    return Dictionary();
}

template <class DA, class SA> DA _convert_array(const SA &p_array) {
    DA da;
    da.resize(p_array.size());

    for (int i = 0; i < p_array.size(); i++) {
        if constexpr (eastl::is_same_v<Variant, typename DA::ValueType>) {
            da.set(i, p_array.get(i));
        } else {
            da.set(i, Variant(p_array.get(i)).as<typename DA::ValueType>());
        }
    }

    return da;
}

template <class DA> DA _convert_array_from_variant(const Variant &p_variant) {
    switch (p_variant.get_type()) {
        case VariantType::ARRAY: {
            return _convert_array<DA, Array>(p_variant.as<Array>());
        }
        case VariantType::POOL_BYTE_ARRAY: {
            return _convert_array<DA, PoolVector<uint8_t>>(p_variant.as<PoolVector<uint8_t>>());
        }
        case VariantType::POOL_INT_ARRAY: {
            return _convert_array<DA, PoolVector<int>>(p_variant.as<PoolVector<int>>());
        }
        case VariantType::POOL_FLOAT32_ARRAY: {
            return _convert_array<DA, PoolVector<real_t>>(p_variant.as<PoolVector<real_t>>());
        }
        case VariantType::POOL_STRING_ARRAY: {
            return _convert_array<DA, PoolVector<String>>(p_variant.as<PoolVector<String>>());
        }
        case VariantType::POOL_VECTOR2_ARRAY: {
            return _convert_array<DA, PoolVector<Vector2>>(p_variant.as<PoolVector<Vector2>>());
        }
        case VariantType::POOL_VECTOR3_ARRAY: {
            return _convert_array<DA, PoolVector<Vector3>>(p_variant.as<PoolVector<Vector3>>());
        }
        case VariantType::POOL_COLOR_ARRAY: {
            return _convert_array<DA, PoolVector<Color>>(p_variant.as<PoolVector<Color>>());
        }
        default: {
            return DA();
        }
    }
}

Variant::operator Vector<Variant>() const {
    return ((Array) * this).vals();
}

Variant::operator Array() const {
    if (type == VariantType::ARRAY) {
        return *reinterpret_cast<const Array *>(_data._mem);
    }
    return _convert_array_from_variant<Array>(*this);
}

Variant::operator PoolVector<uint8_t>() const {
    if (type == VariantType::POOL_BYTE_ARRAY) {
        return *reinterpret_cast<const PoolVector<uint8_t> *>(_data._mem);
    }
    return _convert_array_from_variant<PoolVector<uint8_t>>(*this);
}

Variant::operator PoolVector<int>() const {
    if (type == VariantType::POOL_INT_ARRAY) {
        return *reinterpret_cast<const PoolVector<int> *>(_data._mem);
    }
    return _convert_array_from_variant<PoolVector<int>>(*this);
}

Variant::operator PoolVector<real_t>() const {
    if (type == VariantType::POOL_FLOAT32_ARRAY) {
        return *reinterpret_cast<const PoolVector<real_t> *>(_data._mem);
    }
    return _convert_array_from_variant<PoolVector<real_t>>(*this);
}

Variant::operator PoolVector<String>() const {
    if (type == VariantType::POOL_STRING_ARRAY) {
        return *reinterpret_cast<const PoolVector<String> *>(_data._mem);
    }
    return _convert_array_from_variant<PoolVector<String>>(*this);
}

Variant::operator Vector<String>() const {
    Vector<String> res;
    PoolVector<String> tmp;
    if (type == VariantType::POOL_STRING_ARRAY) {
        tmp = *reinterpret_cast<const PoolVector<String> *>(_data._mem);
    } else {
        tmp = _convert_array_from_variant<PoolVector<String>>(*this);
    }
    res.reserve(tmp.size());
    for (int i = 0, fin = tmp.size(); i < fin; ++i) {
        res.emplace_back(tmp[i]);
    }
    return res;
}

Variant::operator Vector<uint8_t>() const {
    PoolVector<uint8_t> tmp;
    if (type == VariantType::POOL_BYTE_ARRAY) {
        tmp = *reinterpret_cast<const PoolVector<uint8_t> *>(_data._mem);
    } else {
        tmp = _convert_array_from_variant<PoolVector<uint8_t>>(*this);
    }
    auto rddata(tmp.read());
    return Vector<uint8_t>(rddata.ptr(), rddata.ptr() + tmp.size());
}

Variant::operator Vector<int>() const {
    PoolVector<int> tmp;
    if (type == VariantType::POOL_INT_ARRAY) {
        tmp = *reinterpret_cast<const PoolVector<int> *>(_data._mem);
    } else {
        WARN_PRINT("Inefficient code, converting non int-array Variant to array");
        tmp = _convert_array_from_variant<PoolVector<int>>(*this);
    }
    auto rd(tmp.read());
    return Vector<int>(rd.ptr(), rd.ptr() + tmp.size());
}

Variant::operator Vector<float>() const {
    PoolVector<float> tmp;
    if (type == VariantType::POOL_FLOAT32_ARRAY) {
        tmp = *reinterpret_cast<const PoolVector<float> *>(_data._mem);
    } else {
        WARN_PRINT("Inefficient code, converting non int-array Variant to array");
        tmp = _convert_array_from_variant<PoolVector<float>>(*this);
    }
    auto rd(tmp.read());
    return Vector<float>(rd.ptr(), rd.ptr() + tmp.size());
}

Variant::operator Vector<Color>() const {
    PoolVector<Color> tmp;
    if (type == VariantType::POOL_COLOR_ARRAY) {
        tmp = *reinterpret_cast<const PoolVector<Color> *>(_data._mem);
    } else {
        WARN_PRINT("Inefficient code, converting non Color-array Variant to array");
        tmp = _convert_array_from_variant<PoolVector<Color>>(*this);
    }
    auto as_span(tmp.toSpan());
    return Vector<Color>(as_span.begin(),as_span.end());
}

Variant::operator Vector<Vector2>() const {
    PoolVector<Vector2> tmp;
    if (type == VariantType::POOL_VECTOR2_ARRAY) {
        tmp = *reinterpret_cast<const PoolVector<Vector2> *>(_data._mem);
    } else {
        WARN_PRINT("Inefficient code, converting non Vector2-array Variant to array");
        tmp = _convert_array_from_variant<PoolVector<Vector2>>(*this);
    }
    auto as_span(tmp.toSpan());
    return Vector<Vector2>(as_span.begin(),as_span.end());
}

Variant::operator Vector<Vector3>() const {
    PoolVector<Vector3> tmp;
    if (type == VariantType::POOL_VECTOR3_ARRAY) {
        tmp = *reinterpret_cast<const PoolVector<Vector3> *>(_data._mem);
    } else {
        WARN_PRINT("Inefficient code, converting non Vector3-array Variant to array");
        tmp = _convert_array_from_variant<PoolVector<Vector3>>(*this);
    }
    auto as_span(tmp.toSpan());
    return Vector<Vector3>(as_span.begin(),as_span.end());
}

Variant::operator Span<const uint8_t>() const {
    ERR_FAIL_COND_V(type != VariantType::POOL_BYTE_ARRAY, Span<const uint8_t>());

    auto tmp = reinterpret_cast<const PoolVector<uint8_t> *>(_data._mem);
    return Span<const uint8_t>(tmp->read().ptr(), tmp->size());
}

Variant::operator Span<const int>() const {
    ERR_FAIL_COND_V(type != VariantType::POOL_INT_ARRAY, Span<const int>());

    auto tmp = reinterpret_cast<const PoolVector<int> *>(_data._mem);
    return Span<const int>(tmp->read().ptr(), tmp->size());
}

Variant::operator Span<const float>() const {
    ERR_FAIL_COND_V(type != VariantType::POOL_FLOAT32_ARRAY, Span<const float>());

    auto tmp = reinterpret_cast<const PoolVector<float> *>(_data._mem);
    return Span<const float>(tmp->read().ptr(), tmp->size());
}

Variant::operator Span<const Vector2>() const {
    ERR_FAIL_COND_V(type != VariantType::POOL_VECTOR2_ARRAY, Span<const Vector2>());

    auto tmp = reinterpret_cast<const PoolVector<Vector2> *>(_data._mem);
    return Span<const Vector2>(tmp->read().ptr(), tmp->size());
}

Variant::operator Span<const Vector3>() const {
    ERR_FAIL_COND_V(type != VariantType::POOL_VECTOR3_ARRAY, Span<const Vector3>());

    auto tmp = reinterpret_cast<const PoolVector<Vector3> *>(_data._mem);
    return Span<const Vector3>(tmp->read().ptr(), tmp->size());
}

Variant::operator Span<const Color>() const {
    ERR_FAIL_COND_V(type != VariantType::POOL_COLOR_ARRAY, Span<const Color>());

    auto tmp = reinterpret_cast<const PoolVector<Color> *>(_data._mem);
    return Span<const Color>(tmp->read().ptr(), tmp->size());
}
Variant::operator PoolVector<Vector3>() const {
    if (type == VariantType::POOL_VECTOR3_ARRAY) {
        return *reinterpret_cast<const PoolVector<Vector3> *>(_data._mem);
    }
    return _convert_array_from_variant<PoolVector<Vector3>>(*this);
}

Variant::operator PoolVector<Vector2>() const {
    if (type == VariantType::POOL_VECTOR2_ARRAY) {
        return *reinterpret_cast<const PoolVector<Vector2> *>(_data._mem);
    }
    return _convert_array_from_variant<PoolVector<Vector2>>(*this);
}

Variant::operator PoolVector<Color>() const {
    if (type == VariantType::POOL_COLOR_ARRAY) {
        return *reinterpret_cast<const PoolVector<Color> *>(_data._mem);
    }
    return _convert_array_from_variant<PoolVector<Color>>(*this);
}

/* helpers */

Variant::operator PoolVector<RID>() const {
    Array va = (Array) * this;
    PoolVector<RID> rids;
    rids.resize(va.size());
    auto wr(rids.write());
    for (int i = 0; i < rids.size(); i++) {
        wr[i] = va[i].as<RID>();
    }
    return rids;
}

Variant::operator Vector<RID>() const {
    Array va = (Array) * this;
    Vector<RID> rids;
    rids.resize(va.size());
    for (int i = 0; i < rids.size(); i++) {
        rids[i] = va[i].as<RID>();
    }
    return rids;
}
Variant::operator PoolVector<Plane>() const {
    Array va = (Array) * this;
    PoolVector<Plane> planes;
    int va_size = va.size();
    if (va_size == 0) {
        return planes;
    }

    planes.resize(va_size);
    PoolVector<Plane>::Write w = planes.write();

    for (int i = 0; i < va_size; i++) {
        w[i] = va[i].as<Plane>();
    }

    return planes;
}

Variant::operator PoolVector<Face3>() const {
    PoolVector<Vector3> va = (PoolVector<Vector3>)*this;
    PoolVector<Face3> faces;
    int va_size = va.size();
    if (va_size == 0) {
        return faces;
    }

    faces.resize(va_size / 3);
    PoolVector<Face3>::Write w = faces.write();
    PoolVector<Vector3>::Read r = va.read();

    for (int i = 0; i < va_size; i++) {
        w[i / 3].vertex[i % 3] = r[i];
    }

    return faces;
}

Variant::operator Vector<Plane>() const {
    Array va = (Array) * this;
    Vector<Plane> planes;
    int va_size = va.size();
    if (va_size == 0) {
        return planes;
    }

    planes.resize(va_size);

    for (int i = 0; i < va_size; i++) {
        planes[i] = va[i].as<Plane>();
    }

    return planes;
}

Variant::Variant(QChar p_char) {
    type = VariantType::INT;
    _data._int = p_char.unicode();
}

Variant::Variant(StringName p_string) {
    type = VariantType::STRING_NAME;
    memnew_placement(_data._mem, StringName(eastl::move(p_string)));
}

Variant::Variant(StringView p_string) {
    type = VariantType::STRING;
    memnew_placement(_data._mem, String(p_string));
}
Variant::Variant(const char *p_string) {
    static_assert(sizeof(_data._mem) >= sizeof(String));

    type = VariantType::STRING;
    memnew_placement(_data._mem, String(p_string ? p_string : ""));
}
// Variant::Variant(const String &p_string) {

//    type = VariantType::STRING;
//    memnew_placement(_data._mem, String(StringUtils::to_utf8(p_string).data()));
//}

Variant::Variant(const String &p_string) {
    type = VariantType::STRING;
    memnew_placement(_data._mem, String(p_string));
}
Variant::Variant(const CharType *p_wstring) {
    type = VariantType::STRING;
    memnew_placement(_data._mem, String(StringUtils::to_utf8(UIString(p_wstring)).data()));
}
Variant::Variant(const Vector3 &p_vector3) {
    type = VariantType::VECTOR3;
    memnew_placement(_data._mem, Vector3(p_vector3));
}
Variant::Variant(const Vector2 &p_vector2) {
    type = VariantType::VECTOR2;
    memnew_placement(_data._mem, Vector2(p_vector2));
}
Variant::Variant(const Rect2 &p_rect2) {
    type = VariantType::RECT2;
    memnew_placement(_data._mem, Rect2(p_rect2));
}

Variant::Variant(const Plane &p_plane) {
    type = VariantType::PLANE;
    memnew_placement(_data._mem, Plane(p_plane));
}
Variant::Variant(const ::AABB &p_aabb) {
    type = VariantType::AABB;
    _data._aabb = memnew(::AABB(p_aabb));
}

Variant::Variant(const Basis &p_matrix) {
    type = VariantType::BASIS;
    _data._basis = memnew(Basis(p_matrix));
}

Variant::Variant(const Quat &p_quat) {
    type = VariantType::QUAT;
    memnew_placement(_data._mem, Quat(p_quat));
}
Variant::Variant(const Transform &p_transform) {
    type = VariantType::TRANSFORM;
    _data._transform = memnew(Transform(p_transform));
}

Variant::Variant(const Transform2D &p_transform) {
    type = VariantType::TRANSFORM2D;
    _data._transform2d = memnew(Transform2D(p_transform));
}
Variant::Variant(const Color &p_color) {
    type = VariantType::COLOR;
    memnew_placement(_data._mem, Color(p_color));
}

Variant::Variant(const NodePath &p_node_path) {
    static_assert(sizeof(_data._mem) >= sizeof(NodePath));
    type = VariantType::NODE_PATH;
    memnew_placement(_data._mem, NodePath(p_node_path));
}

Variant::Variant(const RefPtr &p_resource) {
    type = VariantType::OBJECT;
    memnew_placement(_data._mem, ObjData);
    _get_obj().rc = nullptr;
    _get_obj().ref = p_resource;
}

Variant::Variant(const RID &p_rid) {
    type = VariantType::_RID;
    memnew_placement(_data._mem, RID(p_rid));
}

Variant::Variant(const Object *p_object) {
    type = VariantType::OBJECT;
    Object *obj = const_cast<Object *>(p_object);

    memnew_placement(_data._mem, ObjData);
    RefCounted *ref = object_cast<RefCounted>(obj);
    if (unlikely(ref)) {
        *reinterpret_cast<Ref<RefCounted> *>(_get_obj().ref.get()) = Ref<RefCounted>(ref);
        _get_obj().rc = nullptr;
    } else {
        _get_obj().rc = likely(obj) ? obj->_use_rc() : nullptr;
    }
}


Variant::Variant(const Callable &p_callable) {
    type = VariantType::CALLABLE;
    memnew_placement(_data._mem, Callable(p_callable));
}

Variant::Variant(const Signal &p_signal) {
    type = VariantType::SIGNAL;
    memnew_placement(_data._mem, Signal(p_signal));
}

Variant::Variant(const Dictionary &p_dictionary) {
    type = VariantType::DICTIONARY;
    memnew_placement(_data._mem, Dictionary(p_dictionary));
}
Variant::Variant(Dictionary &&p_dictionary) noexcept {
    type = VariantType::DICTIONARY;
    memnew_placement(_data._mem, Dictionary(eastl::move(p_dictionary)));
}
Variant::Variant(const Array &p_array) {
    static_assert(sizeof(_data._mem) >= sizeof(Array));

    type = VariantType::ARRAY;
    memnew_placement(_data._mem, Array(p_array));
}
Variant::Variant(Array &&p_array) noexcept {
    type = VariantType::ARRAY;
    memnew_placement(_data._mem, Array(eastl::move(p_array)));
}
Variant::Variant(const PoolVector<Plane> &p_array) {
    type = VariantType::ARRAY;

    Array *plane_array = memnew_placement(_data._mem, Array);

    plane_array->resize(p_array.size());

    for (int i = 0; i < p_array.size(); i++) {
        plane_array->operator[](i) = Variant(p_array[i]);
    }
}

constexpr VariantType getBulitinArrayType(const PoolVector<uint8_t> &) {
    return VariantType::POOL_BYTE_ARRAY;
}
constexpr VariantType getBulitinArrayType(const PoolVector<int> &) {
    return VariantType::POOL_INT_ARRAY;
}
constexpr VariantType getBulitinArrayType(const PoolVector<float> &) {
    return VariantType::POOL_FLOAT32_ARRAY;
}
constexpr VariantType getBulitinArrayType(const PoolVector<String> &) {
    return VariantType::POOL_STRING_ARRAY;
}
constexpr VariantType getBulitinArrayType(const PoolVector<Vector2> &) {
    return VariantType::POOL_VECTOR2_ARRAY;
}
constexpr VariantType getBulitinArrayType(const PoolVector<Vector3> &) {
    return VariantType::POOL_VECTOR3_ARRAY;
}
constexpr VariantType getBulitinArrayType(const PoolVector<Color> &) {
    return VariantType::POOL_COLOR_ARRAY;
}

Variant::Variant(const Vector<float> &from) {
    static_assert(sizeof(_data._mem) >= sizeof(PoolVector<float>));
    PoolVector<float> *plane_array = memnew_placement(_data._mem, PoolVector<float>);
    type = getBulitinArrayType(*plane_array);

    int len = from.size();
    plane_array->resize(len);
    auto w = plane_array->write();
    eastl::copy(from.begin(), from.end(), w.ptr());
}
Variant::Variant(const Vector<Vector3> &from) {
    static_assert(sizeof(_data._mem) >= sizeof(PoolVector<Vector3>));
    auto plane_array = memnew_placement(_data._mem, PoolVector<Vector3>);
    type = getBulitinArrayType(*plane_array);

    int len = from.size();
    plane_array->resize(len);
    auto w = plane_array->write();
    eastl::copy(from.begin(), from.end(), w.ptr());
}
Variant::Variant(const Vector<Face3> &from) {
    static_assert(sizeof(_data._mem) >= sizeof(PoolVector<Vector3>));
    auto plane_array = memnew_placement(_data._mem, PoolVector<Vector3>);
    type = getBulitinArrayType(*plane_array);

    int len = from.size();
    plane_array->resize(len * 3);
    auto w = plane_array->write();
    for (int i = 0; i < len; ++i) {
        w[i * 3 + 0] = from[i].vertex[0];
        w[i * 3 + 1] = from[i].vertex[1];
        w[i * 3 + 2] = from[i].vertex[2];
    }
}
Variant::Variant(const Vector<Vector2> &from) {
    static_assert(sizeof(_data._mem) >= sizeof(PoolVector<Vector2>));
    auto plane_array = memnew_placement(_data._mem, PoolVector<Vector2>);
    type = getBulitinArrayType(*plane_array);

    int len = from.size();
    plane_array->resize(len);
    auto w = plane_array->write();
    eastl::copy(from.begin(), from.end(), w.ptr());
}
Variant::Variant(const Vector<Color> &from) {
    static_assert(sizeof(_data._mem) >= sizeof(PoolVector<Color>));
    auto plane_array = memnew_placement(_data._mem, PoolVector<Color>);
    type = getBulitinArrayType(*plane_array);

    int len = from.size();
    plane_array->resize(len);
    auto w = plane_array->write();
    eastl::copy(from.begin(), from.end(), w.ptr());
}
Variant::Variant(const Vector<Plane> &from) {
    type = VariantType::ARRAY;

    Array *plane_array = memnew_placement(_data._mem, Array);

    plane_array->resize(from.size());
    int i = 0;
    for (const Plane &p : from) {
        plane_array->operator[](i++) = Variant(p);
    }
}
template <class T> Variant Variant::fromVector(Span<const T> p_array) {
    Variant res;
    res.type = VariantType::ARRAY;

    Array *plane_array = memnew_placement(res._data._mem, Array);

    plane_array->resize(p_array.size());

    for (int i = 0; i < p_array.size(); i++) {
        plane_array->operator[](i) = Variant(p_array[i]);
    }
    return res;
}
template <class T> Variant Variant::fromVectorBuiltin(Span<const T> p_array) {
    static_assert(sizeof(_data) >= sizeof(PoolVector<T>));

    Variant res;
    PoolVector<T> *plane_array = memnew_placement(res._data._mem, PoolVector<T>);
    res.type = getBulitinArrayType(*plane_array);

    int len = p_array.size();
    plane_array->resize(len);
    typename PoolVector<T>::Write w = plane_array->write();
    const T *r = p_array.data();

    for (int i = 0; i < len; i++) {
        w[i] = r[i];
    }
    return res;
}
template <> Variant Variant::fromVector(Span<const Variant> p_array) {
    Variant res;
    res.type = VariantType::ARRAY;

    memnew_placement(res._data._mem, Array(eastl::move(Vector<Variant>(p_array.begin(), p_array.end()))));
    return res;
}

template <> Variant Variant::from(const Frustum &p_array) {
    return fromVector<Plane>(p_array);
}

template <> Variant Variant::from(const PoolVector<RID> &p_array) {
    return fromVector<RID>({ p_array.read().ptr(), size_t(p_array.size()) });
}

template <> Variant Variant::from(const Span<const Vector2> &p_array) {
    return fromVectorBuiltin<Vector2>(p_array);
}

template <> Variant Variant::from(const Span<const Vector3> &p_array) {
    return fromVectorBuiltin<Vector3>(p_array);
}

template <> Variant Variant::from(const Vector<String> &p_array) {
    return fromVectorBuiltin<String>(p_array);
}
template <> Variant Variant::move_from(Vector<Variant> &&p_array) {
    return Array(eastl::move(p_array));
}

template <> Variant Variant::from(const Vector<Variant> &p_array) {
    Array res;
    res.push_back(p_array.data(), p_array.size());
    return res;
}

template <> Variant Variant::from(const Vector<StringView> &p_array) {
    Variant res;
    PoolVector<String> *plane_array = memnew_placement(res._data._mem, PoolVector<String>);
    res.type = getBulitinArrayType(*plane_array);

    int len = p_array.size();
    plane_array->resize(len);
    typename PoolVector<String>::Write w = plane_array->write();
    const StringView *r = p_array.data();

    for (int i = 0; i < len; i++) {
        w[i] = r[i];
    }
    return res;
}
template <> Variant Variant::from(const Vector<StringName> &p_array) {
    Variant res;
    PoolVector<String> *plane_array = memnew_placement(res._data._mem, PoolVector<String>);
    res.type = getBulitinArrayType(*plane_array);

    int len = p_array.size();
    plane_array->resize(len);
    typename PoolVector<String>::Write w = plane_array->write();
    const StringName *r = p_array.data();

    for (int i = 0; i < len; i++) {
        w[i] = r[i];
    }
    return res;
}
Variant::Variant(const PoolVector<uint8_t> &p_raw_array) {
    static_assert(sizeof(_data._mem) >= sizeof(PoolVector<uint8_t>));

    type = VariantType::POOL_BYTE_ARRAY;
    static_assert(sizeof(PoolVector<uint8_t>) <= sizeof(_data));
    memnew_placement(_data._mem, PoolVector<uint8_t>(p_raw_array));
}
Variant::Variant(const Vector<uint8_t> &p_raw_array) {
    type = VariantType::POOL_BYTE_ARRAY;
    PoolVector<uint8_t> to_add;
    to_add.resize(p_raw_array.size());
    if (!p_raw_array.empty()) {
        memcpy(to_add.write().ptr(), p_raw_array.data(), p_raw_array.size());
    }
    static_assert(sizeof(PoolVector<uint8_t>) <= sizeof(_data));
    memnew_placement(_data._mem, PoolVector<uint8_t>(to_add));
}
Variant::Variant(const PoolVector<int> &p_int_array) {
    type = VariantType::POOL_INT_ARRAY;
    static_assert(sizeof(PoolVector<int>) <= sizeof(_data));
    memnew_placement(_data._mem, PoolVector<int>(p_int_array));
}
Variant::Variant(const Vector<int> &p_int_array) {
    type = VariantType::POOL_INT_ARRAY;
    PoolVector<int> to_add;
    to_add.resize(p_int_array.size());
    if (!p_int_array.empty()) {
        memcpy(to_add.write().ptr(), p_int_array.data(), p_int_array.size() * sizeof(int));
    }
    static_assert(sizeof(PoolVector<uint8_t>) <= sizeof(_data));
    memnew_placement(_data._mem, PoolVector<int>(to_add));
}
Variant::Variant(const PoolVector<real_t> &p_real_array) {
    type = VariantType::POOL_FLOAT32_ARRAY;
    memnew_placement(_data._mem, PoolVector<real_t>(p_real_array));
}
Variant::Variant(const PoolVector<UIString> &p_string_array) {
    static_assert(sizeof(_data._mem) >= sizeof(PoolVector<String>));

    type = VariantType::POOL_STRING_ARRAY;
    memnew_placement(_data._mem, PoolVector<String>{});
    PoolVector<String> &tgt(*reinterpret_cast<PoolVector<String> *>(_data._mem));
    tgt.resize(p_string_array.size());
    auto wr = tgt.write();
    for (int i = 0; i < p_string_array.size(); ++i) {
        wr.ptr()[i] = StringUtils::to_utf8(p_string_array[i]);
    }
}
Variant::Variant(const PoolVector<String> &p_string_array) {
    type = VariantType::POOL_STRING_ARRAY;
    memnew_placement(_data._mem, PoolVector<String>(p_string_array));
}
Variant::Variant(const PoolVector<Vector3> &p_vector3_array) {
    type = VariantType::POOL_VECTOR3_ARRAY;
    memnew_placement(_data._mem, PoolVector<Vector3>(p_vector3_array));
}

Variant::Variant(const PoolVector<Vector2> &p_vector2_array) {
    type = VariantType::POOL_VECTOR2_ARRAY;
    memnew_placement(_data._mem, PoolVector<Vector2>(p_vector2_array));
}
Variant::Variant(const PoolVector<Color> &p_color_array) {
    type = VariantType::POOL_COLOR_ARRAY;
    memnew_placement(_data._mem, PoolVector<Color>(p_color_array));
}

Variant::Variant(const PoolVector<Face3> &p_face_array) {
    PoolVector<Vector3> vertices;
    int face_count = p_face_array.size();
    vertices.resize(face_count * 3);

    if (face_count) {
        PoolVector<Face3>::Read r = p_face_array.read();
        PoolVector<Vector3>::Write w = vertices.write();

        for (int i = 0; i < face_count; i++) {
            for (int j = 0; j < 3; j++) {
                w[i * 3 + j] = r[i].vertex[j];
            }
        }
    }

    type = VariantType::NIL;

    *this = vertices;
}

Variant &Variant::operator=(const Variant &p_variant) {
    if (unlikely(this == &p_variant)) {
        return *this;
    }

    if (unlikely(type != p_variant.type)) {
        reference(p_variant);
        return *this;
    }

    switch (p_variant.type) {
        case VariantType::NIL: {
            // none
        } break;

        // atomic types
        case VariantType::BOOL: {
            _data._bool = p_variant._data._bool;
        } break;
        case VariantType::INT: {
            _data._int = p_variant._data._int;
        } break;
        case VariantType::FLOAT: {
            _data._real = p_variant._data._real;
        } break;
        case VariantType::REN_ENT: {
            _data._int = p_variant._data._int;
        } break;
        case VariantType::STRING: {
            *reinterpret_cast<String *>(_data._mem) = *reinterpret_cast<const String *>(p_variant._data._mem);
        } break;

        // math types
        case VariantType::VECTOR2: {
            *reinterpret_cast<Vector2 *>(_data._mem) = *reinterpret_cast<const Vector2 *>(p_variant._data._mem);
        } break;
        case VariantType::RECT2: {
            *reinterpret_cast<Rect2 *>(_data._mem) = *reinterpret_cast<const Rect2 *>(p_variant._data._mem);
        } break;
        case VariantType::TRANSFORM2D: {
            *_data._transform2d = *(p_variant._data._transform2d);
        } break;
        case VariantType::VECTOR3: {
            *reinterpret_cast<Vector3 *>(_data._mem) = *reinterpret_cast<const Vector3 *>(p_variant._data._mem);
        } break;
        case VariantType::PLANE: {
            *reinterpret_cast<Plane *>(_data._mem) = *reinterpret_cast<const Plane *>(p_variant._data._mem);
        } break;

        case VariantType::AABB: {
            *_data._aabb = *(p_variant._data._aabb);
        } break;
        case VariantType::QUAT: {
            *reinterpret_cast<Quat *>(_data._mem) = *reinterpret_cast<const Quat *>(p_variant._data._mem);
        } break;
        case VariantType::BASIS: {
            *_data._basis = *(p_variant._data._basis);
        } break;
        case VariantType::TRANSFORM: {
            *_data._transform = *(p_variant._data._transform);
        } break;

        // misc types
        case VariantType::COLOR: {
            *reinterpret_cast<Color *>(_data._mem) = *reinterpret_cast<const Color *>(p_variant._data._mem);
        } break;
        case VariantType::_RID: {
            *reinterpret_cast<RID *>(_data._mem) = *reinterpret_cast<const RID *>(p_variant._data._mem);
        } break;
        case VariantType::OBJECT: {
            if (likely(_get_obj().rc)) {
                if (unlikely(_get_obj().rc->decrement())) {
                    memdelete(_get_obj().rc);
                }
            }
            *reinterpret_cast<ObjData *>(_data._mem) = p_variant._get_obj();
            if (likely(_get_obj().rc)) {
                _get_obj().rc->increment();
            }
        } break;
        case VariantType::CALLABLE: {
            *reinterpret_cast<Callable *>(_data._mem) = *reinterpret_cast<const Callable *>(p_variant._data._mem);
        } break;
        case VariantType::SIGNAL: {
            *reinterpret_cast<Signal *>(_data._mem) = *reinterpret_cast<const Signal *>(p_variant._data._mem);
        } break;
        case VariantType::STRING_NAME: {
            *reinterpret_cast<StringName *>(_data._mem) = *reinterpret_cast<const StringName *>(p_variant._data._mem);
        } break;
        case VariantType::NODE_PATH: {
            *reinterpret_cast<NodePath *>(_data._mem) = *reinterpret_cast<const NodePath *>(p_variant._data._mem);
        } break;
        case VariantType::DICTIONARY: {
            *reinterpret_cast<Dictionary *>(_data._mem) = *reinterpret_cast<const Dictionary *>(p_variant._data._mem);
        } break;
        case VariantType::ARRAY: {
            *reinterpret_cast<Array *>(_data._mem) = *reinterpret_cast<const Array *>(p_variant._data._mem);
        } break;

        // arrays
        case VariantType::POOL_BYTE_ARRAY: {
            *reinterpret_cast<PoolVector<uint8_t> *>(_data._mem) =
                    *reinterpret_cast<const PoolVector<uint8_t> *>(p_variant._data._mem);
        } break;
        case VariantType::POOL_INT_ARRAY: {
            *reinterpret_cast<PoolVector<int> *>(_data._mem) =
                    *reinterpret_cast<const PoolVector<int> *>(p_variant._data._mem);
        } break;
        case VariantType::POOL_FLOAT32_ARRAY: {
            *reinterpret_cast<PoolVector<real_t> *>(_data._mem) =
                    *reinterpret_cast<const PoolVector<real_t> *>(p_variant._data._mem);
        } break;
        case VariantType::POOL_STRING_ARRAY: {
            *reinterpret_cast<PoolVector<String> *>(_data._mem) =
                    *reinterpret_cast<const PoolVector<String> *>(p_variant._data._mem);
        } break;
        case VariantType::POOL_VECTOR2_ARRAY: {
            *reinterpret_cast<PoolVector<Vector2> *>(_data._mem) =
                    *reinterpret_cast<const PoolVector<Vector2> *>(p_variant._data._mem);
        } break;
        case VariantType::POOL_VECTOR3_ARRAY: {
            *reinterpret_cast<PoolVector<Vector3> *>(_data._mem) =
                    *reinterpret_cast<const PoolVector<Vector3> *>(p_variant._data._mem);
        } break;
        case VariantType::POOL_COLOR_ARRAY: {
            *reinterpret_cast<PoolVector<Color> *>(_data._mem) =
                    *reinterpret_cast<const PoolVector<Color> *>(p_variant._data._mem);
        } break;
        default: {
        }
    }
    return *this;
}

Variant::Variant(const IP_Address &p_address) {
    type = VariantType::STRING;
    memnew_placement(_data._mem, String(p_address));
}

Variant::Variant(const Variant &p_variant) {
    type = VariantType::NIL;
    reference(p_variant);
}

/*
Variant::~Variant() {

    clear();
}*/

uint32_t Variant::hash() const {
    switch (type) {
        case VariantType::NIL: {
            return 0;
        }
        case VariantType::BOOL: {
            return _data._bool ? 1 : 0;
        }
        case VariantType::INT: {
            return _data._int;
        }
        case VariantType::FLOAT: {
            return hash_djb2_one_float(_data._real);
        }
        case VariantType::REN_ENT: {
            return _data._int;
        }
        case VariantType::STRING: {
            return StringUtils::hash(*reinterpret_cast<const String *>(_data._mem));
        }

        // math types
        case VariantType::VECTOR2: {
            uint32_t hash = hash_djb2_one_float(reinterpret_cast<const Vector2 *>(_data._mem)->x);
            return hash_djb2_one_float(reinterpret_cast<const Vector2 *>(_data._mem)->y, hash);
        }
        case VariantType::RECT2: {
            uint32_t hash = hash_djb2_one_float(reinterpret_cast<const Rect2 *>(_data._mem)->position.x);
            hash = hash_djb2_one_float(reinterpret_cast<const Rect2 *>(_data._mem)->position.y, hash);
            hash = hash_djb2_one_float(reinterpret_cast<const Rect2 *>(_data._mem)->size.x, hash);
            return hash_djb2_one_float(reinterpret_cast<const Rect2 *>(_data._mem)->size.y, hash);
        }
        case VariantType::TRANSFORM2D: {
            uint32_t hash = 5831;
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 2; j++) {
                    hash = hash_djb2_one_float(_data._transform2d->elements[i][j], hash);
                }
            }

            return hash;
        }
        case VariantType::VECTOR3: {
            uint32_t hash = hash_djb2_one_float(reinterpret_cast<const Vector3 *>(_data._mem)->x);
            hash = hash_djb2_one_float(reinterpret_cast<const Vector3 *>(_data._mem)->y, hash);
            return hash_djb2_one_float(reinterpret_cast<const Vector3 *>(_data._mem)->z, hash);
        }
        case VariantType::PLANE: {
            uint32_t hash = hash_djb2_one_float(reinterpret_cast<const Plane *>(_data._mem)->normal.x);
            hash = hash_djb2_one_float(reinterpret_cast<const Plane *>(_data._mem)->normal.y, hash);
            hash = hash_djb2_one_float(reinterpret_cast<const Plane *>(_data._mem)->normal.z, hash);
            return hash_djb2_one_float(reinterpret_cast<const Plane *>(_data._mem)->d, hash);
        }
        /*
            case VariantType::QUAT: {


            } break;*/
        case VariantType::AABB: {
            uint32_t hash = 5831;
            for (int i = 0; i < 3; i++) {
                hash = hash_djb2_one_float(_data._aabb->position[i], hash);
                hash = hash_djb2_one_float(_data._aabb->size[i], hash);
            }

            return hash;
        }
        case VariantType::QUAT: {
            uint32_t hash = hash_djb2_one_float(reinterpret_cast<const Quat *>(_data._mem)->x);
            hash = hash_djb2_one_float(reinterpret_cast<const Quat *>(_data._mem)->y, hash);
            hash = hash_djb2_one_float(reinterpret_cast<const Quat *>(_data._mem)->z, hash);
            return hash_djb2_one_float(reinterpret_cast<const Quat *>(_data._mem)->w, hash);

        }
        case VariantType::BASIS: {
            uint32_t hash = 5831;
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 3; j++) {
                    hash = hash_djb2_one_float(_data._basis->elements[i][j], hash);
                }
            }

            return hash;
        }
        case VariantType::TRANSFORM: {
            uint32_t hash = 5831;
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 3; j++) {
                    hash = hash_djb2_one_float(_data._transform->basis.elements[i][j], hash);
                }
                hash = hash_djb2_one_float(_data._transform->origin[i], hash);
            }

            return hash;
        }

        // misc types
        case VariantType::COLOR: {
            uint32_t hash = hash_djb2_one_float(reinterpret_cast<const Color *>(_data._mem)->r);
            hash = hash_djb2_one_float(reinterpret_cast<const Color *>(_data._mem)->g, hash);
            hash = hash_djb2_one_float(reinterpret_cast<const Color *>(_data._mem)->b, hash);
            return hash_djb2_one_float(reinterpret_cast<const Color *>(_data._mem)->a, hash);
        }
        case VariantType::_RID: {
            return hash_djb2_one_64(reinterpret_cast<const RID *>(_data._mem)->get_id());
        }
        case VariantType::OBJECT: {
            return hash_djb2_one_64(make_uint64_t(_UNSAFE_OBJ_PROXY_PTR(*this)));
        }
        case VariantType::STRING_NAME: {
            return reinterpret_cast<const StringName *>(_data._mem)->hash();
        }

        case VariantType::NODE_PATH: {
            return reinterpret_cast<const NodePath *>(_data._mem)->hash();
        }
        case VariantType::DICTIONARY: {
            return reinterpret_cast<const Dictionary *>(_data._mem)->hash();
        }
        case VariantType::CALLABLE: {
            return reinterpret_cast<const Callable *>(_data._mem)->hash();
        }
        case VariantType::SIGNAL: {
            const Signal &s = *reinterpret_cast<const Signal *>(_data._mem);
            uint32_t hash = s.get_name().hash();
            return hash_djb2_one_64(entt::to_integral(s.get_object_id()), hash);
        }
        case VariantType::ARRAY: {
            const Array &arr = *reinterpret_cast<const Array *>(_data._mem);
            return arr.hash();
        }
        case VariantType::POOL_BYTE_ARRAY: {
            const PoolVector<uint8_t> &arr = *reinterpret_cast<const PoolVector<uint8_t> *>(_data._mem);
            int len = arr.size();
            if (likely(len)) {
                PoolVector<uint8_t>::Read r = arr.read();
                return hash_djb2_buffer((uint8_t *)&r[0], len);
            }
            return hash_djb2_one_64(0);
        }
        case VariantType::POOL_INT_ARRAY: {
            const PoolVector<int> &arr = *reinterpret_cast<const PoolVector<int> *>(_data._mem);
            int len = arr.size();
            if (likely(len)) {
                PoolVector<int>::Read r = arr.read();
                return hash_djb2_buffer((uint8_t *)&r[0], len * sizeof(int));
            }
            return hash_djb2_one_64(0);
        }
        case VariantType::POOL_FLOAT32_ARRAY: {
            const PoolVector<real_t> &arr = *reinterpret_cast<const PoolVector<real_t> *>(_data._mem);
            int len = arr.size();

            if (likely(len)) {
                PoolVector<real_t>::Read r = arr.read();
                return hash_djb2_buffer((uint8_t *)&r[0], len * sizeof(real_t));
            }
            return hash_djb2_one_float(0.0);
        }
        case VariantType::POOL_STRING_ARRAY: {
            uint32_t hash = 5831;
            const PoolVector<String> &arr = *reinterpret_cast<const PoolVector<String> *>(_data._mem);
            int len = arr.size();

            if (likely(len)) {
                PoolVector<String>::Read r = arr.read();

                for (int i = 0; i < len; i++) {
                    hash = hash_djb2_one_32(StringUtils::hash(r[i]), hash);
                }
            }

            return hash;
        }
        case VariantType::POOL_VECTOR2_ARRAY: {
            uint32_t hash = 5831;
            const PoolVector<Vector2> &arr = *reinterpret_cast<const PoolVector<Vector2> *>(_data._mem);
            int len = arr.size();

            if (likely(len)) {
                PoolVector<Vector2>::Read r = arr.read();

                for (int i = 0; i < len; i++) {
                    hash = hash_djb2_one_float(r[i].x, hash);
                    hash = hash_djb2_one_float(r[i].y, hash);
                }
            }

            return hash;
        }
        case VariantType::POOL_VECTOR3_ARRAY: {
            uint32_t hash = 5831;
            const PoolVector<Vector3> &arr = *reinterpret_cast<const PoolVector<Vector3> *>(_data._mem);
            int len = arr.size();

            if (likely(len)) {
                PoolVector<Vector3>::Read r = arr.read();

                for (int i = 0; i < len; i++) {
                    hash = hash_djb2_one_float(r[i].x, hash);
                    hash = hash_djb2_one_float(r[i].y, hash);
                    hash = hash_djb2_one_float(r[i].z, hash);
                }
            }

            return hash;
        }
        case VariantType::POOL_COLOR_ARRAY: {
            uint32_t hash = 5831;
            const PoolVector<Color> &arr = *reinterpret_cast<const PoolVector<Color> *>(_data._mem);
            int len = arr.size();

            if (likely(len)) {
                PoolVector<Color>::Read r = arr.read();

                for (int i = 0; i < len; i++) {
                    hash = hash_djb2_one_float(r[i].r, hash);
                    hash = hash_djb2_one_float(r[i].g, hash);
                    hash = hash_djb2_one_float(r[i].b, hash);
                    hash = hash_djb2_one_float(r[i].a, hash);
                }
            }

            return hash;
        }
        default: {
        }
    }

    return 0;
}
namespace {
template <class T> bool hash_compare_helper(T p_lhs, T p_rhs);
template <> bool hash_compare_helper<float>(float p_lhs, float p_rhs) {
    return (p_lhs == p_rhs) || (Math::is_nan(p_lhs) && Math::is_nan(p_rhs));
}
template <> bool hash_compare_helper<Vector2>(Vector2 p_lhs, Vector2 p_rhs) {
    return hash_compare_helper(p_lhs.x, p_rhs.x) && hash_compare_helper((p_lhs).y, (p_rhs).y);
}
template <> bool hash_compare_helper<Vector3>(Vector3 p_lhs, Vector3 p_rhs) {
    return hash_compare_helper(p_lhs.x, p_rhs.x) && hash_compare_helper((p_lhs).y, (p_rhs).y) &&
           hash_compare_helper((p_lhs).z, (p_rhs).z);
}
template <> bool hash_compare_helper<Quat>(Quat p_lhs, Quat p_rhs) {
    return hash_compare_helper(p_lhs.x, p_rhs.x) && hash_compare_helper((p_lhs).y, (p_rhs).y) &&
           hash_compare_helper((p_lhs).z, (p_rhs).z) && hash_compare_helper((p_lhs).w, (p_rhs).w);
}
template <> bool hash_compare_helper<Color>(Color p_lhs, Color p_rhs) {
    return hash_compare_helper(p_lhs.r, p_rhs.r) && hash_compare_helper((p_lhs).g, (p_rhs).g) &&
           hash_compare_helper((p_lhs).b, (p_rhs).b) && hash_compare_helper((p_lhs).a, (p_rhs).a);
}
} // end of anonymous namespace

#define hash_compare_pool_array(p_lhs, p_rhs, p_type, p_compare_func)                                                  \
    const PoolVector<p_type> &l = *reinterpret_cast<const PoolVector<p_type> *>(p_lhs);                                \
    const PoolVector<p_type> &r = *reinterpret_cast<const PoolVector<p_type> *>(p_rhs);                                \
                                                                                                                       \
    if (l.size() != r.size())                                                                                          \
        return false;                                                                                                  \
                                                                                                                       \
    PoolVector<p_type>::Read lr = l.read();                                                                            \
    PoolVector<p_type>::Read rr = r.read();                                                                            \
                                                                                                                       \
    for (int i = 0; i < l.size(); ++i) {                                                                               \
        if (!p_compare_func((lr[i]), (rr[i])))                                                                         \
            return false;                                                                                              \
    }                                                                                                                  \
                                                                                                                       \
    return true

static bool can_compare(VariantType a,VariantType b)
{
    if( (a == VariantType::STRING || a == VariantType::STRING_NAME) &&
        (b == VariantType::STRING || b == VariantType::STRING_NAME) )
        return true;
    return a==b;
}

bool Variant::hash_compare(const Variant &p_variant) const {
    if (!can_compare(type,p_variant.type)) {
        return false;
    }

    switch (type) {
        case VariantType::INT:
        case VariantType::REN_ENT: {
            return _data._int == p_variant._data._int;
        }
        case VariantType::FLOAT: {
            return hash_compare_helper<float>(_data._real, p_variant._data._real);
        }

        case VariantType::VECTOR2: {
            const Vector2 *l = reinterpret_cast<const Vector2 *>(_data._mem);
            const Vector2 *r = reinterpret_cast<const Vector2 *>(p_variant._data._mem);

            return hash_compare_helper(*l, *r);
        }

        case VariantType::RECT2: {
            const Rect2 *l = reinterpret_cast<const Rect2 *>(_data._mem);
            const Rect2 *r = reinterpret_cast<const Rect2 *>(p_variant._data._mem);

            return (hash_compare_helper(l->position, r->position)) && (hash_compare_helper(l->size, r->size));
        }

        case VariantType::TRANSFORM2D: {
            Transform2D *l = _data._transform2d;
            Transform2D *r = p_variant._data._transform2d;

            for (int i = 0; i < 3; i++) {
                if (!(hash_compare_helper(l->elements[i], r->elements[i]))) {
                    return false;
                }
            }

            return true;
        }

        case VariantType::VECTOR3: {
            const Vector3 *l = reinterpret_cast<const Vector3 *>(_data._mem);
            const Vector3 *r = reinterpret_cast<const Vector3 *>(p_variant._data._mem);

            return hash_compare_helper(*l, *r);
        }

        case VariantType::PLANE: {
            const Plane *l = reinterpret_cast<const Plane *>(_data._mem);
            const Plane *r = reinterpret_cast<const Plane *>(p_variant._data._mem);

            return (hash_compare_helper(l->normal, r->normal)) && (hash_compare_helper(l->d, r->d));
        }

        case VariantType::AABB: {
            const ::AABB *l = _data._aabb;
            const ::AABB *r = p_variant._data._aabb;

            return (hash_compare_helper(l->position, r->position) && (hash_compare_helper(l->size, r->size)));
        }

        case VariantType::QUAT: {
            const Quat *l = reinterpret_cast<const Quat *>(_data._mem);
            const Quat *r = reinterpret_cast<const Quat *>(p_variant._data._mem);

            return hash_compare_helper(*l, *r);
        }

        case VariantType::BASIS: {
            const Basis *l = _data._basis;
            const Basis *r = p_variant._data._basis;

            for (int i = 0; i < 3; i++) {
                if (!(hash_compare_helper(l->elements[i], r->elements[i]))) {
                    return false;
                }
            }

            return true;
        }

        case VariantType::TRANSFORM: {
            const Transform *l = _data._transform;
            const Transform *r = p_variant._data._transform;

            for (int i = 0; i < 3; i++) {
                if (!(hash_compare_helper(l->basis.elements[i], r->basis.elements[i]))) {
                    return false;
                }
            }

            return hash_compare_helper(l->origin, r->origin);
        }

        case VariantType::COLOR: {
            const Color *l = reinterpret_cast<const Color *>(_data._mem);
            const Color *r = reinterpret_cast<const Color *>(p_variant._data._mem);

            return hash_compare_helper(*l, *r);
        }

        case VariantType::ARRAY: {
            const Array &l = *(reinterpret_cast<const Array *>(_data._mem));
            const Array &r = *(reinterpret_cast<const Array *>(p_variant._data._mem));

            if (l.size() != r.size()) {
                return false;
            }

            for (int i = 0; i < l.size(); ++i) {
                if (!l[i].hash_compare(r[i])) {
                    return false;
                }
            }

            return true;
        }

        case VariantType::POOL_FLOAT32_ARRAY: {
            hash_compare_pool_array(_data._mem, p_variant._data._mem, real_t, hash_compare_helper);
        } break;

        case VariantType::POOL_VECTOR2_ARRAY: {
            hash_compare_pool_array(_data._mem, p_variant._data._mem, Vector2, hash_compare_helper);
        } break;

        case VariantType::POOL_VECTOR3_ARRAY: {
            hash_compare_pool_array(_data._mem, p_variant._data._mem, Vector3, hash_compare_helper);
        } break;

        case VariantType::POOL_COLOR_ARRAY: {
            hash_compare_pool_array(_data._mem, p_variant._data._mem, Color, hash_compare_helper);
        } break;

        default:
            return evaluate_equal(*this, p_variant);
    }

    return false;
}

bool Variant::is_ref() const {
    return type == VariantType::OBJECT && !_get_obj().ref.is_null();
}

String Variant::get_construct_string() const {
    String vars;
    VariantWriter::write_to_string(*this, vars);

    return vars;
}

String Variant::get_call_error_text(Object *p_base, const StringName &p_method, const Variant **p_argptrs,
        int p_argcount, const Callable::CallError &ce) {
    String err_text;

    if (ce.error == Callable::CallError::CALL_ERROR_INVALID_ARGUMENT) {
        int errorarg = ce.argument;
        if (p_argptrs) {
            err_text = "Cannot convert argument " + itos(errorarg + 1) + " from " +
                       Variant::get_type_name(p_argptrs[errorarg]->get_type()) + " to " +
                       Variant::get_type_name(ce.expected) + ".";
        } else {
            err_text = "Cannot convert argument " + itos(errorarg + 1) + " from [missing argptr, type unknown] to " +
                       Variant::get_type_name(ce.expected) + ".";
        }
    } else if (ce.error == Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS) {
        err_text = "Method expected " + itos(ce.argument) + " arguments, but called with " + itos(p_argcount) + ".";
    } else if (ce.error == Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS) {
        err_text = "Method expected " + itos(ce.argument) + " arguments, but called with " + itos(p_argcount) + ".";
    } else if (ce.error == Callable::CallError::CALL_ERROR_INVALID_METHOD) {
        err_text = "Method not found.";
    } else if (ce.error == Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL) {
        err_text = "Instance is null";
    } else if (ce.error == Callable::CallError::CALL_OK) {
        return "Call OK";
    }

    String class_name(p_base->get_class());
    Ref<Script> script = refFromRefPtr<Script>(p_base->get_script());
    if (script && PathUtils::is_resource_file(script->get_path())) {
        class_name += "(" + String(PathUtils::get_file(script->get_path())) + ")";
    }
    return "'" + class_name + "::" + String(p_method) + "': " + err_text;
}

String Variant::get_callable_error_text(
        const Callable &p_callable, const Variant **p_argptrs, int p_argcount, const Callable::CallError &ce) {
    String err_text;

    if (ce.error == Callable::CallError::CALL_ERROR_INVALID_ARGUMENT) {
        int errorarg = ce.argument;
        if (p_argptrs) {
            err_text = "Cannot convert argument " + itos(errorarg + 1) + " from " +
                       Variant::get_type_name(p_argptrs[errorarg]->get_type()) + " to " +
                       Variant::get_type_name(VariantType(ce.expected)) + ".";
        } else {
            err_text = "Cannot convert argument " + itos(errorarg + 1) + " from [missing argptr, type unknown] to " +
                       Variant::get_type_name(VariantType(ce.expected)) + ".";
        }
    } else if (ce.error == Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS) {
        err_text = "Method expected " + itos(ce.argument) + " arguments, but called with " + itos(p_argcount) + ".";
    } else if (ce.error == Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS) {
        err_text = "Method expected " + itos(ce.argument) + " arguments, but called with " + itos(p_argcount) + ".";
    } else if (ce.error == Callable::CallError::CALL_ERROR_INVALID_METHOD) {
        err_text = "Method not found.";
    } else if (ce.error == Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL) {
        err_text = "Instance is null";
    } else if (ce.error == Callable::CallError::CALL_OK) {
        return "Call OK";
    }

    return String(p_callable) + " : " + err_text;
}

// String vformat(StringView p_text, const Variant &p1, const Variant &p2, const Variant &p3, const Variant &p4, const
// Variant &p5) {

//    Array args;
//    if (p1.get_type() != VariantType::NIL) {

//        args.push_back(p1);

//        if (p2.get_type() != VariantType::NIL) {

//            args.push_back(p2);

//            if (p3.get_type() != VariantType::NIL) {

//                args.push_back(p3);

//                if (p4.get_type() != VariantType::NIL) {

//                    args.push_back(p4);

//                    if (p5.get_type() != VariantType::NIL) {

//                        args.push_back(p5);
//                    }
//                }
//            }
//        }
//    }

//    bool error = false;
//    String fmt = StringUtils::sprintf(p_text,args, &error);

//    ERR_FAIL_COND_V(error, String());

//    return fmt;
//}

void fill_with_all_variant_types(
        const char *nillname, char (&s)[7 + (longest_variant_type_name + 1) * int(VariantType::VARIANT_MAX)]) {
    assert(strlen(nillname) <= 7);

    int write_idx = sprintf(s, "%s", nillname);
    for (int i = 1; i < int(VariantType::VARIANT_MAX); i++) {
        write_idx += sprintf(s + write_idx, ",%s", Variant::get_type_name(VariantType(i)));
    }
}

Vector<Variant> varray(std::initializer_list<Variant> v) {
    return v;
}

void VariantOps::resize(Variant &arg, int new_size) {
    switch (arg.get_type()) {
        case VariantType::ARRAY: {
            reinterpret_cast<Array *>(arg._data._mem)->resize(new_size);
            break;
        }
        case VariantType::POOL_BYTE_ARRAY: {
            reinterpret_cast<PoolVector<uint8_t> *>(arg._data._mem)->resize(new_size);
            break;
        }
        case VariantType::POOL_INT_ARRAY: {
            reinterpret_cast<PoolVector<int> *>(arg._data._mem)->resize(new_size);
            break;
        }
        case VariantType::POOL_FLOAT32_ARRAY: {
            reinterpret_cast<PoolVector<real_t> *>(arg._data._mem)->resize(new_size);
            break;
        }
        case VariantType::POOL_STRING_ARRAY: {
            reinterpret_cast<PoolVector<String> *>(arg._data._mem)->resize(new_size);
            break;
        }
        case VariantType::POOL_VECTOR2_ARRAY: {
            reinterpret_cast<PoolVector<Vector2> *>(arg._data._mem)->resize(new_size);
            break;
        }
        case VariantType::POOL_VECTOR3_ARRAY: {
            reinterpret_cast<PoolVector<Vector3> *>(arg._data._mem)->resize(new_size);
            break;
        }
        case VariantType::POOL_COLOR_ARRAY: {
            reinterpret_cast<PoolVector<Color> *>(arg._data._mem)->resize(new_size);
            break;
        }
        default:
            ERR_PRINT("Unhandled variant operation");
    }
}

int VariantOps::size(const Variant &arg) {
    switch (arg.get_type()) {
            // TODO: consider removing dictionary and string from handled `size` queries
        case VariantType::DICTIONARY: {
            return reinterpret_cast<const Dictionary *>(arg._data._mem)->size();
        }
        case VariantType::STRING: {
            return reinterpret_cast<const String *>(arg._data._mem)->size();
        }

        case VariantType::ARRAY: {
            return reinterpret_cast<const Array *>(arg._data._mem)->size();
        }
        case VariantType::POOL_BYTE_ARRAY: {
            return reinterpret_cast<const PoolVector<uint8_t> *>(arg._data._mem)->size();
        }
        case VariantType::POOL_INT_ARRAY: {
            return reinterpret_cast<const PoolVector<int> *>(arg._data._mem)->size();
        }
        case VariantType::POOL_FLOAT32_ARRAY: {
            return reinterpret_cast<const PoolVector<real_t> *>(arg._data._mem)->size();
        }
        case VariantType::POOL_STRING_ARRAY: {
            return reinterpret_cast<const PoolVector<String> *>(arg._data._mem)->size();
        }
        case VariantType::POOL_VECTOR2_ARRAY: {
            return reinterpret_cast<const PoolVector<Vector2> *>(arg._data._mem)->size();
        }
        case VariantType::POOL_VECTOR3_ARRAY: {
            return reinterpret_cast<const PoolVector<Vector3> *>(arg._data._mem)->size();
        }
        case VariantType::POOL_COLOR_ARRAY: {
            return reinterpret_cast<const PoolVector<Color> *>(arg._data._mem)->size();
        }
        default:
            ERR_PRINT("Unhandled variant operation");
    }
    return -1;
}

Variant VariantOps::duplicate(const Variant &arg, bool deep) {
    switch (arg.get_type()) {
            // TODO: consider removing dictionary and string from handled `size` queries
        case VariantType::DICTIONARY: {
            return reinterpret_cast<const Dictionary *>(arg._data._mem)->duplicate(deep);
        }
        case VariantType::ARRAY: {
            return reinterpret_cast<const Array *>(arg._data._mem)->duplicate(deep);
        }
        default:
            ERR_PRINT("Unhandled variant operation");
    }
    return -1;
}

void VariantOps::remove(Variant &arg, int idx) {
    switch (arg.get_type()) {
        case VariantType::ARRAY: {
            reinterpret_cast<Array *>(arg._data._mem)->remove(idx);
            break;
        }
        case VariantType::POOL_BYTE_ARRAY: {
            reinterpret_cast<PoolVector<uint8_t> *>(arg._data._mem)->remove(idx);
            break;
        }
        case VariantType::POOL_INT_ARRAY: {
            reinterpret_cast<PoolVector<int> *>(arg._data._mem)->remove(idx);
            break;
        }
        case VariantType::POOL_FLOAT32_ARRAY: {
            reinterpret_cast<PoolVector<real_t> *>(arg._data._mem)->remove(idx);
            break;
        }
        case VariantType::POOL_STRING_ARRAY: {
            reinterpret_cast<PoolVector<String> *>(arg._data._mem)->remove(idx);
            break;
        }
        case VariantType::POOL_VECTOR2_ARRAY: {
            reinterpret_cast<PoolVector<Vector2> *>(arg._data._mem)->remove(idx);
            break;
        }
        case VariantType::POOL_VECTOR3_ARRAY: {
            reinterpret_cast<PoolVector<Vector3> *>(arg._data._mem)->remove(idx);
            break;
        }
        case VariantType::POOL_COLOR_ARRAY: {
            reinterpret_cast<PoolVector<Color> *>(arg._data._mem)->remove(idx);
            break;
        }
        default:
            ERR_PRINT("Unhandled variant operation");
    }
}
void VariantOps::insert(Variant &arg, int idx, Variant &&val) {
    switch (arg.get_type()) {
        case VariantType::ARRAY: {
            reinterpret_cast<Array *>(arg._data._mem)->insert(idx,eastl::move(val));
            break;
        }
        case VariantType::POOL_BYTE_ARRAY: {
            reinterpret_cast<PoolVector<uint8_t> *>(arg._data._mem)->insert(idx,val.as<uint8_t>());
            break;
        }
        case VariantType::POOL_INT_ARRAY: {
            reinterpret_cast<PoolVector<int> *>(arg._data._mem)->insert(idx,val.as<int>());
            break;
        }
        case VariantType::POOL_FLOAT32_ARRAY: {
            reinterpret_cast<PoolVector<real_t> *>(arg._data._mem)->insert(idx,val.as<real_t>());
            break;
        }
        case VariantType::POOL_STRING_ARRAY: {
            reinterpret_cast<PoolVector<String> *>(arg._data._mem)->insert(idx,val.as<String>());
            break;
        }
        case VariantType::POOL_VECTOR2_ARRAY: {
            reinterpret_cast<PoolVector<Vector2> *>(arg._data._mem)->insert(idx,val.as<Vector2>());
            break;
        }
        case VariantType::POOL_VECTOR3_ARRAY: {
            reinterpret_cast<PoolVector<Vector3> *>(arg._data._mem)->insert(idx,val.as<Vector3>());
            break;
        }
        case VariantType::POOL_COLOR_ARRAY: {
            reinterpret_cast<PoolVector<Color> *>(arg._data._mem)->insert(idx,val.as<Color>());
            break;
        }
        default:
            ERR_PRINT("Unhandled variant operation");
    }
}
