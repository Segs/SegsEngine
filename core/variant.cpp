/*************************************************************************/
/*  variant.cpp                                                          */
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

#include "core/core_string_names.h"
#include "core/hashfuncs.h"
#include "core/node_path.h"
#include "core/dictionary.h"
#include "core/io/marshalls.h"
#include "core/io/ip_address.h"
#include "core/math/aabb.h"
#include "core/math/basis.h"
#include "core/math/face3.h"
#include "core/math/plane.h"
#include "core/math/quat.h"
#include "core/math/transform.h"
#include "core/math/transform_2d.h"
#include "core/math/vector3.h"
#include "core/math/math_funcs.h"
#include "core/object_db.h"
#include "core/pool_vector.h"
#include "core/print_string.h"
#include "core/resource.h"
#include "core/string_formatter.h"
#include "core/variant_parser.h"
#include "core/script_language.h"
#include "scene/gui/control.h"
#include "scene/main/node.h"


template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) eastl::vector<Variant,wrap_allocator>;

const Variant Variant::null_variant;

const char *Variant::get_type_name(VariantType p_type) {

    switch (p_type) {
        case VariantType::NIL: return "Nil";
        // atomic types
        case VariantType::BOOL: return "bool";
        case VariantType::INT: return "int";
        case VariantType::REAL: return "float";
        case VariantType::STRING: return "String";
        // math types
        case VariantType::VECTOR2: return "Vector2";
        case VariantType::RECT2: return "Rect2";
        case VariantType::TRANSFORM2D: return "Transform2D";
        case VariantType::VECTOR3: return "Vector3";
        case VariantType::PLANE: return "Plane";
        case VariantType::AABB: return "AABB";
        case VariantType::QUAT: return "Quat";
        case VariantType::BASIS: return "Basis";
        case VariantType::TRANSFORM: return "Transform";
        // misc types
        case VariantType::COLOR: return "Color";
        case VariantType::_RID: return "RID";
        case VariantType::OBJECT: return "Object";
        case VariantType::NODE_PATH: return "NodePath";
        case VariantType::DICTIONARY: return "Dictionary";
        case VariantType::ARRAY: return "Array";
        // arrays
        case VariantType::POOL_BYTE_ARRAY: return "PoolByteArray";
        case VariantType::POOL_INT_ARRAY: return "PoolIntArray";
        case VariantType::POOL_REAL_ARRAY: return "PoolRealArray";
        case VariantType::POOL_STRING_ARRAY: return "PoolStringArray";
        case VariantType::POOL_VECTOR2_ARRAY: return "PoolVector2Array";
        case VariantType::POOL_VECTOR3_ARRAY: return "PoolVector3Array";
        case VariantType::POOL_COLOR_ARRAY: return "PoolColorArray";
        default: { }
    }
    return "";
}
StringName Variant::interned_type_name(VariantType p_type) {

    switch (p_type) {
        case VariantType::NIL: return "Nil";
        // atomic types
        case VariantType::BOOL: return "bool";
        case VariantType::INT: return "int";
        case VariantType::REAL: return "float";
        case VariantType::STRING: return "String";
        // math types
        case VariantType::VECTOR2: return "Vector2";
        case VariantType::RECT2: return "Rect2";
        case VariantType::TRANSFORM2D: return "Transform2D";
        case VariantType::VECTOR3: return "Vector3";
        case VariantType::PLANE: return "Plane";
        case VariantType::AABB: return "AABB";
        case VariantType::QUAT: return "Quat";
        case VariantType::BASIS: return "Basis";
        case VariantType::TRANSFORM: return "Transform";
        // misc types
        case VariantType::COLOR: return "Color";
        case VariantType::_RID: return "RID";
        case VariantType::OBJECT: return "Object";
        case VariantType::NODE_PATH: return "NodePath";
        case VariantType::DICTIONARY: return "Dictionary";
        case VariantType::ARRAY: return "Array";
        // arrays
        case VariantType::POOL_BYTE_ARRAY: return "PoolByteArray";
        case VariantType::POOL_INT_ARRAY: return "PoolIntArray";
        case VariantType::POOL_REAL_ARRAY: return "PoolRealArray";
        case VariantType::POOL_STRING_ARRAY: return "PoolStringArray";
        case VariantType::POOL_VECTOR2_ARRAY: return "PoolVector2Array";
        case VariantType::POOL_VECTOR3_ARRAY: return "PoolVector3Array";
        case VariantType::POOL_COLOR_ARRAY: return "PoolColorArray";
        default: { }
    }
    return "";
}

bool Variant::can_convert(VariantType p_type_from, VariantType p_type_to) {

    if (p_type_from == p_type_to)
        return true;
    if (p_type_to == VariantType::NIL && p_type_from != VariantType::NIL) //nil can convert to anything
        return true;

    if (p_type_from == VariantType::NIL) {
        return (p_type_to == VariantType::OBJECT);
    }

    const VariantType *valid_types = nullptr;
    const VariantType *invalid_types = nullptr;

    switch (p_type_to) {
        case VariantType::BOOL: {

            static const VariantType valid[] = {
                VariantType::INT,
                VariantType::REAL,
                VariantType::STRING,
                VariantType::NIL,
            };

            valid_types = valid;
        } break;
        case VariantType::INT: {

            static const VariantType valid[] = {
                VariantType::BOOL,
                VariantType::REAL,
                VariantType::STRING,
                VariantType::NIL,
            };

            valid_types = valid;

        } break;
        case VariantType::REAL: {

            static const VariantType valid[] = {
                VariantType::BOOL,
                VariantType::INT,
                VariantType::STRING,
                VariantType::NIL,
            };

            valid_types = valid;

        } break;
        case VariantType::STRING: {

            static const VariantType invalid[] = {
                VariantType::OBJECT,
                VariantType::NIL
            };

            invalid_types = invalid;
        } break;
        case VariantType::TRANSFORM2D: {

            static const VariantType valid[] = {
                VariantType::TRANSFORM,
                VariantType::NIL
            };

            valid_types = valid;
        } break;
        case VariantType::QUAT: {

            static const VariantType valid[] = {
                VariantType::BASIS,
                VariantType::NIL
            };

            valid_types = valid;

        } break;
        case VariantType::BASIS: {

            static const VariantType valid[] = {
                VariantType::QUAT,
                VariantType::VECTOR3,
                VariantType::NIL
            };

            valid_types = valid;

        } break;
        case VariantType::TRANSFORM: {

            static const VariantType valid[] = {
                VariantType::TRANSFORM2D,
                VariantType::QUAT,
                VariantType::BASIS,
                VariantType::NIL
            };

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

            static const VariantType valid[] = {
                VariantType::OBJECT,
                VariantType::NIL
            };

            valid_types = valid;
        } break;
        case VariantType::OBJECT: {

            static const VariantType valid[] = {
                VariantType::NIL
            };

            valid_types = valid;
        } break;
        case VariantType::NODE_PATH: {

            static const VariantType valid[] = {
                VariantType::STRING,
                VariantType::NIL
            };

            valid_types = valid;
        } break;
        case VariantType::ARRAY: {

            static const VariantType valid[] = {
                VariantType::POOL_BYTE_ARRAY,
                VariantType::POOL_INT_ARRAY,
                VariantType::POOL_STRING_ARRAY,
                VariantType::POOL_REAL_ARRAY,
                VariantType::POOL_COLOR_ARRAY,
                VariantType::POOL_VECTOR2_ARRAY,
                VariantType::POOL_VECTOR3_ARRAY,
                VariantType::NIL
            };

            valid_types = valid;
        } break;
        // arrays
        case VariantType::POOL_BYTE_ARRAY: {

            static const VariantType valid[] = {
                VariantType::ARRAY,
                VariantType::NIL
            };

            valid_types = valid;
        } break;
        case VariantType::POOL_INT_ARRAY: {

            static const VariantType valid[] = {
                VariantType::ARRAY,
                VariantType::NIL
            };
            valid_types = valid;
        } break;
        case VariantType::POOL_REAL_ARRAY: {

            static const VariantType valid[] = {
                VariantType::ARRAY,
                VariantType::NIL
            };

            valid_types = valid;
        } break;
        case VariantType::POOL_STRING_ARRAY: {

            static const VariantType valid[] = {
                VariantType::ARRAY,
                VariantType::NIL
            };
            valid_types = valid;
        } break;
        case VariantType::POOL_VECTOR2_ARRAY: {

            static const VariantType valid[] = {
                VariantType::ARRAY,
                VariantType::NIL
            };
            valid_types = valid;

        } break;
        case VariantType::POOL_VECTOR3_ARRAY: {

            static const VariantType valid[] = {
                VariantType::ARRAY,
                VariantType::NIL
            };
            valid_types = valid;

        } break;
        case VariantType::POOL_COLOR_ARRAY: {

            static const VariantType valid[] = {
                VariantType::ARRAY,
                VariantType::NIL
            };

            valid_types = valid;

        } break;
        default: {
        }
    }

    if (valid_types) {

        int i = 0;
        while (valid_types[i] != VariantType::NIL) {

            if (p_type_from == valid_types[i])
                return true;
            i++;
        }

    } else if (invalid_types) {

        int i = 0;
        while (invalid_types[i] != VariantType::NIL) {

            if (p_type_from == invalid_types[i])
                return false;
            i++;
        }

        return true;
    }

    return false;
}

bool Variant::can_convert_strict(VariantType p_type_from, VariantType p_type_to) {

    if (p_type_from == p_type_to)
        return true;
    if (p_type_to == VariantType::NIL && p_type_from != VariantType::NIL) //nil can convert to anything
        return true;

    if (p_type_from == VariantType::NIL) {
        return (p_type_to == VariantType::OBJECT);
    }

    const VariantType *valid_types = nullptr;

    switch (p_type_to) {
        case VariantType::BOOL: {

            static const VariantType valid[] = {
                VariantType::INT,
                VariantType::REAL,
                //STRING,
                VariantType::NIL,
            };

            valid_types = valid;
        } break;
        case VariantType::INT: {

            static const VariantType valid[] = {
                VariantType::BOOL,
                VariantType::REAL,
                //STRING,
                VariantType::NIL,
            };

            valid_types = valid;

        } break;
        case VariantType::REAL: {

            static const VariantType valid[] = {
                VariantType::BOOL,
                VariantType::INT,
                //STRING,
                VariantType::NIL,
            };

            valid_types = valid;

        } break;
        case VariantType::STRING: {

            static const VariantType valid[] = {
                VariantType::NODE_PATH,
                VariantType::NIL
            };

            valid_types = valid;
        } break;
        case VariantType::TRANSFORM2D: {

            static const VariantType valid[] = {
                VariantType::TRANSFORM,
                VariantType::NIL
            };

            valid_types = valid;
        } break;
        case VariantType::QUAT: {

            static const VariantType valid[] = {
                VariantType::BASIS,
                VariantType::NIL
            };

            valid_types = valid;

        } break;
        case VariantType::BASIS: {

            static const VariantType valid[] = {
                VariantType::QUAT,
                VariantType::VECTOR3,
                VariantType::NIL
            };

            valid_types = valid;

        } break;
        case VariantType::TRANSFORM: {

            static const VariantType valid[] = {
                VariantType::TRANSFORM2D,
                VariantType::QUAT,
                VariantType::BASIS,
                VariantType::NIL
            };

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

            static const VariantType valid[] = {
                VariantType::OBJECT,
                VariantType::NIL
            };

            valid_types = valid;
        } break;
        case VariantType::OBJECT: {

            static const VariantType valid[] = {
                VariantType::NIL
            };

            valid_types = valid;
        } break;
        case VariantType::NODE_PATH: {

            static const VariantType valid[] = {
                VariantType::STRING,
                VariantType::NIL
            };

            valid_types = valid;
        } break;
        case VariantType::ARRAY: {

            static const VariantType valid[] = {
                VariantType::POOL_BYTE_ARRAY,
                VariantType::POOL_INT_ARRAY,
                VariantType::POOL_STRING_ARRAY,
                VariantType::POOL_REAL_ARRAY,
                VariantType::POOL_COLOR_ARRAY,
                VariantType::POOL_VECTOR2_ARRAY,
                VariantType::POOL_VECTOR3_ARRAY,
                VariantType::NIL
            };

            valid_types = valid;
        } break;
        // arrays
        case VariantType::POOL_BYTE_ARRAY: {

            static const VariantType valid[] = {
                VariantType::ARRAY,
                VariantType::NIL
            };

            valid_types = valid;
        } break;
        case VariantType::POOL_INT_ARRAY: {

            static const VariantType valid[] = {
                VariantType::ARRAY,
                VariantType::NIL
            };
            valid_types = valid;
        } break;
        case VariantType::POOL_REAL_ARRAY: {

            static const VariantType valid[] = {
                VariantType::ARRAY,
                VariantType::NIL
            };

            valid_types = valid;
        } break;
        case VariantType::POOL_STRING_ARRAY: {

            static const VariantType valid[] = {
                VariantType::ARRAY,
                VariantType::NIL
            };
            valid_types = valid;
        } break;
        case VariantType::POOL_VECTOR2_ARRAY: {

            static const VariantType valid[] = {
                VariantType::ARRAY,
                VariantType::NIL
            };
            valid_types = valid;

        } break;
        case VariantType::POOL_VECTOR3_ARRAY: {

            static const VariantType valid[] = {
                VariantType::ARRAY,
                VariantType::NIL
            };
            valid_types = valid;

        } break;
        case VariantType::POOL_COLOR_ARRAY: {

            static const VariantType valid[] = {
                VariantType::ARRAY,
                VariantType::NIL
            };

            valid_types = valid;

        } break;
        default: {
        }
    }

    if (valid_types) {

        int i = 0;
        while (valid_types[i] != VariantType::NIL) {

            if (p_type_from == valid_types[i])
                return true;
            i++;
        }
    }

    return false;
}

bool Variant::operator==(const Variant &p_variant) const {

    if (type != p_variant.type) //evaluation of operator== needs to be more strict
        return false;
    bool v;
    Variant r;
    evaluate(OP_EQUAL, *this, p_variant, r, v);
    return r.as<bool>();
}

bool Variant::operator!=(const Variant &p_variant) const {

    if (type != p_variant.type) //evaluation of operator== needs to be more strict
        return true;
    bool v;
    Variant r;
    evaluate(OP_NOT_EQUAL, *this, p_variant, r, v);
    return r.as<bool>();
}

bool Variant::operator<(const Variant &p_variant) const {
    if (type != p_variant.type) //if types differ, then order by type first
        return type < p_variant.type;
    bool v;
    Variant r;
    evaluate(OP_LESS, *this, p_variant, r, v);
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
        case VariantType::REAL: {

            return _data._real == 0.0;

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
            return _get_obj().obj == nullptr;
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

            return reinterpret_cast<const PoolVector<uint8_t> *>(_data._mem)->size() == 0;

        }
        case VariantType::POOL_INT_ARRAY: {

            return reinterpret_cast<const PoolVector<int> *>(_data._mem)->size() == 0;

        }
        case VariantType::POOL_REAL_ARRAY: {

            return reinterpret_cast<const PoolVector<real_t> *>(_data._mem)->size() == 0;

        }
        case VariantType::POOL_STRING_ARRAY: {

            return reinterpret_cast<const PoolVector<String> *>(_data._mem)->size() == 0;

        }
        case VariantType::POOL_VECTOR2_ARRAY: {

            return reinterpret_cast<const PoolVector<Vector2> *>(_data._mem)->size() == 0;

        }
        case VariantType::POOL_VECTOR3_ARRAY: {

            return reinterpret_cast<const PoolVector<Vector3> *>(_data._mem)->size() == 0;

        }
        case VariantType::POOL_COLOR_ARRAY: {

            return reinterpret_cast<const PoolVector<Color> *>(_data._mem)->size() == 0;

        }
        default: {
        }
    }

    return false;
}

bool Variant::is_one() const {

    switch (type) {
        case VariantType::NIL: {

            return true;
        }

        // atomic types
        case VariantType::BOOL: {

            return _data._bool;
        }
        case VariantType::INT: {

            return _data._int == 1;

        }
        case VariantType::REAL: {

            return _data._real == 1.0;

        }
        case VariantType::VECTOR2: {

            return *reinterpret_cast<const Vector2 *>(_data._mem) == Vector2(1, 1);

        }
        case VariantType::RECT2: {

            return *reinterpret_cast<const Rect2 *>(_data._mem) == Rect2(1, 1, 1, 1);

        }
        case VariantType::VECTOR3: {

            return *reinterpret_cast<const Vector3 *>(_data._mem) == Vector3(1, 1, 1);

        }
        case VariantType::PLANE: {

            return *reinterpret_cast<const Plane *>(_data._mem) == Plane(1, 1, 1, 1);

        }
        case VariantType::COLOR: {

            return *reinterpret_cast<const Color *>(_data._mem) == Color(1, 1, 1, 1);

        }

        default: {
            return !is_zero();
        }
    }

    return false;
}

void Variant::reference(const Variant &p_variant) {

    switch (type) {
        case VariantType::NIL:
        case VariantType::BOOL:
        case VariantType::INT:
        case VariantType::REAL:
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
        case VariantType::INT: {

            _data._int = p_variant._data._int;
        } break;
        case VariantType::REAL: {

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

            memnew_placement(_data._mem, PoolVector<uint8_t>(*reinterpret_cast<const PoolVector<uint8_t> *>(p_variant._data._mem)));

        } break;
        case VariantType::POOL_INT_ARRAY: {

            memnew_placement(_data._mem, PoolVector<int>(*reinterpret_cast<const PoolVector<int> *>(p_variant._data._mem)));

        } break;
        case VariantType::POOL_REAL_ARRAY: {

            memnew_placement(_data._mem, PoolVector<real_t>(*reinterpret_cast<const PoolVector<real_t> *>(p_variant._data._mem)));

        } break;
        case VariantType::POOL_STRING_ARRAY: {

            memnew_placement(_data._mem, PoolVector<String>(*reinterpret_cast<const PoolVector<String> *>(p_variant._data._mem)));

        } break;
        case VariantType::POOL_VECTOR2_ARRAY: {

            memnew_placement(_data._mem, PoolVector<Vector2>(*reinterpret_cast<const PoolVector<Vector2> *>(p_variant._data._mem)));

        } break;
        case VariantType::POOL_VECTOR3_ARRAY: {

            memnew_placement(_data._mem, PoolVector<Vector3>(*reinterpret_cast<const PoolVector<Vector3> *>(p_variant._data._mem)));

        } break;
        case VariantType::POOL_COLOR_ARRAY: {

            memnew_placement(_data._mem, PoolVector<Color>(*reinterpret_cast<const PoolVector<Color> *>(p_variant._data._mem)));

        } break;
        default: {
        }
    }
}

void Variant::zero() {
    switch (type) {
        case VariantType::NIL: break;
        case VariantType::BOOL: this->_data._bool = false; break;
        case VariantType::INT: this->_data._int = 0; break;
        case VariantType::REAL: this->_data._real = 0; break;
        case VariantType::VECTOR2: *reinterpret_cast<Vector2 *>(this->_data._mem) = Vector2(); break;
        case VariantType::RECT2: *reinterpret_cast<Rect2 *>(this->_data._mem) = Rect2(); break;
        case VariantType::VECTOR3: *reinterpret_cast<Vector3 *>(this->_data._mem) = Vector3(); break;
        case VariantType::PLANE: *reinterpret_cast<Plane *>(this->_data._mem) = Plane(); break;
        case VariantType::QUAT: *reinterpret_cast<Quat *>(this->_data._mem) = Quat(); break;
        case VariantType::COLOR: *reinterpret_cast<Color *>(this->_data._mem) = Color(); break;
        default: this->clear(); break;
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
        case VariantType::NODE_PATH: {

            reinterpret_cast<NodePath *>(_data._mem)->~NodePath();
        } break;
        case VariantType::OBJECT: {

            _get_obj().obj = nullptr;
            _get_obj().ref.unref();
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
        case VariantType::POOL_REAL_ARRAY: {

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

        case VariantType::NIL: return 0;
        case VariantType::BOOL: return _data._bool ? 1 : 0;
        case VariantType::INT: return _data._int;
        case VariantType::REAL: return _data._real;
        case VariantType::STRING: return StringUtils::to_int(as<String>());
        default: {

            return 0;
        }
    }
}
Variant::operator unsigned int() const {

    switch (type) {

        case VariantType::NIL: return 0;
        case VariantType::BOOL: return _data._bool ? 1 : 0;
        case VariantType::INT: return _data._int;
        case VariantType::REAL: return _data._real;
        case VariantType::STRING: return StringUtils::to_int(as<String>());
        default: {

            return 0;
        }
    }
}

Variant::operator int64_t() const {

    switch (type) {

        case VariantType::NIL: return 0;
        case VariantType::BOOL: return _data._bool ? 1 : 0;
        case VariantType::INT: return _data._int;
        case VariantType::REAL: return _data._real;
        case VariantType::STRING: return StringUtils::to_int64(as<String>());
        default: {

            return 0;
        }
    }
}

/*
Variant::operator long unsigned int() const {

    switch( type ) {

        case VariantType::NIL: return 0;
        case VariantType::BOOL: return _data._bool ? 1 : 0;
        case VariantType::INT: return _data._int;
        case VariantType::REAL: return _data._real;
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

        case VariantType::NIL: return 0;
        case VariantType::BOOL: return _data._bool ? 1 : 0;
        case VariantType::INT: return _data._int;
        case VariantType::REAL: return _data._real;
        case VariantType::STRING: return StringUtils::to_int64(as<String>());
        default: {

            return 0;
        }
    }
}

Variant::operator signed short() const {

    switch (type) {

        case VariantType::NIL: return 0;
        case VariantType::BOOL: return _data._bool ? 1 : 0;
        case VariantType::INT: return _data._int;
        case VariantType::REAL: return _data._real;
        case VariantType::STRING: return StringUtils::to_int(as<String>());
        default: {

            return 0;
        }
    }
}
Variant::operator unsigned short() const {

    switch (type) {

        case VariantType::NIL: return 0;
        case VariantType::BOOL: return _data._bool ? 1 : 0;
        case VariantType::INT: return _data._int;
        case VariantType::REAL: return _data._real;
        case VariantType::STRING: return StringUtils::to_int(as<String>());
        default: {

            return 0;
        }
    }
}
Variant::operator signed char() const {

    switch (type) {

        case VariantType::NIL: return 0;
        case VariantType::BOOL: return _data._bool ? 1 : 0;
        case VariantType::INT: return _data._int;
        case VariantType::REAL: return _data._real;
        case VariantType::STRING: return StringUtils::to_int(as<String>());
        default: {

            return 0;
        }
    }
}
Variant::operator unsigned char() const {

    switch (type) {

        case VariantType::NIL: return 0;
        case VariantType::BOOL: return _data._bool ? 1 : 0;
        case VariantType::INT: return _data._int;
        case VariantType::REAL: return _data._real;
        case VariantType::STRING: return StringUtils::to_int(as<String>());
        default: {

            return 0;
        }
    }
}
template<>
QChar Variant::as<QChar>() const {

    return operator unsigned int();
}

template <>
float Variant::as<float>() const {

    switch (type) {

        case VariantType::NIL: return 0;
        case VariantType::BOOL: return _data._bool ? 1.0 : 0.0;
        case VariantType::INT: return (float)_data._int;
        case VariantType::REAL: return _data._real;
        case VariantType::STRING: return StringUtils::to_double(as<String>());
        default: {

            return 0;
        }
    }
}
template <> double Variant::as<double>() const {

    switch (type) {

        case VariantType::NIL: return 0;
        case VariantType::BOOL: return _data._bool ? 1.0 : 0.0;
        case VariantType::INT: return (double)_data._int;
        case VariantType::REAL: return _data._real;
        case VariantType::STRING: return StringUtils::to_double( as<String>() );
        default: {

            return 0;
        }
    }
}

template<>
String Variant::as<String>() const {
    List<const void *> stack;

    return stringify(stack);
}
template<>
NodePath Variant::as<NodePath>() const {
    if (type == VariantType::NODE_PATH)
        return *reinterpret_cast<const NodePath *>(_data._mem);
    if (type == VariantType::STRING)
        return NodePath(as<String>());
    return NodePath();
}
template<>
StringName Variant::as<StringName>() const {

    if (type == VariantType::NODE_PATH) {
        return reinterpret_cast<const NodePath *>(_data._mem)->get_sname();
    }
    return StringName(as<String>());
}
template<>
IP_Address Variant::as<IP_Address>() const {

    if (type == VariantType::POOL_REAL_ARRAY || type == VariantType::POOL_INT_ARRAY || type == VariantType::POOL_BYTE_ARRAY) {

        PoolVector<int> addr = operator PoolVector<int>();
        if (addr.size() == 4) {
            return IP_Address(addr.get(0), addr.get(1), addr.get(2), addr.get(3));
        }
    }

    return IP_Address(as<String>());
}
template<>
Transform Variant::as<Transform>() const {
    if (type == VariantType::TRANSFORM)
        return *_data._transform;
    if (type == VariantType::BASIS)
        return Transform(*_data._basis, Vector3());
    if (type == VariantType::QUAT)
        return Transform(Basis(*reinterpret_cast<const Quat *>(_data._mem)), Vector3());
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
template<>
Basis Variant::as<Basis>() const {
    if (type == VariantType::BASIS)
        return *_data._basis;
    if (type == VariantType::QUAT)
        return *reinterpret_cast<const Quat *>(_data._mem);
    if (type == VariantType::VECTOR3) {
        return Basis(*reinterpret_cast<const Vector3 *>(_data._mem));
    }
    if (type == VariantType::TRANSFORM) // unexposed in Variant::can_convert?
        return _data._transform->basis;
    return Basis();
}
template<>
Quat Variant::as<Quat>() const {
    if (type == VariantType::QUAT)
        return *reinterpret_cast<const Quat *>(_data._mem);
    if (type == VariantType::BASIS)
        return *_data._basis;
    if (type == VariantType::TRANSFORM)
        return _data._transform->basis;
    return Quat();
}

struct _VariantStrPair {

    String key;
    String value;

    bool operator<(const _VariantStrPair &p) const {

        return key < p.key;
    }
};


String Variant::stringify(List<const void *> &stack) const {
    switch (type) {

        case VariantType::NIL: return "Null";
        case VariantType::BOOL: return _data._bool ? "True" : "False";
        case VariantType::INT: return itos(_data._int);
        case VariantType::REAL: return rtos(_data._real);
        case VariantType::STRING: return *reinterpret_cast<const String *>(_data._mem);
        case VariantType::VECTOR2: return "(" + operator Vector2() + ")";
        case VariantType::RECT2: return "(" + operator Rect2() + ")";
        case VariantType::TRANSFORM2D: {

            Transform2D mat32 = operator Transform2D();
            return "(" + Variant(mat32.elements[0]).as<String>() + ", " + Variant(mat32.elements[1]).as<String>() + ", " + Variant(mat32.elements[2]).as<String>() + ")";
        } break;
        case VariantType::VECTOR3: return "(" + operator Vector3() + ")";
        case VariantType::PLANE:
            return operator Plane();
        //case VariantType::QUAT:
        case VariantType::AABB: return operator ::AABB();
        case VariantType::QUAT: return "(" + operator Quat() + ")";
        case VariantType::BASIS: {

            Basis mat3 = operator Basis();

            String mtx("(");
            for (int i = 0; i < 3; i++) {

                if (i != 0)
                    mtx += ", ";

                mtx += "(";

                for (int j = 0; j < 3; j++) {

                    if (j != 0)
                        mtx += ", ";

                    mtx += Variant(mat3.elements[i][j]).as<String>();
                }

                mtx += ")";
            }

            return mtx + ")";
        }
        case VariantType::TRANSFORM: return operator Transform();
        case VariantType::NODE_PATH: return (String)as<NodePath>();
        case VariantType::COLOR:  {
            Color z(as<Color>());
            return FormatV("%f,%f,%f,%f",z.r,z.g,z.b,z.a);
        }
        case VariantType::DICTIONARY: {

            const Dictionary &d = *reinterpret_cast<const Dictionary *>(_data._mem);
            if (stack.find(d.id())) {
                return "{...}";
            }

            stack.push_back(d.id());

            //const String *K=NULL;
            String str("{");
            ListPOD<Variant> keys;
            d.get_key_list(&keys);

            Vector<_VariantStrPair> pairs;

            for(Variant &E : keys ) {

                _VariantStrPair sp;
                sp.key = E.stringify(stack);
                sp.value = d[E].stringify(stack);

                pairs.push_back(sp);
            }

            pairs.sort();

            for (int i = 0; i < pairs.size(); i++) {
                if (i > 0)
                    str += ", ";
                str += pairs[i].key + ":" + pairs[i].value;
            }
            str += "}";

            return str;
        }
        case VariantType::POOL_VECTOR2_ARRAY: {

            PoolVector<Vector2> vec = operator PoolVector<Vector2>();
            String str("[");
            for (int i = 0; i < vec.size(); i++) {

                if (i > 0)
                    str += ", ";
                str = str + Variant(vec[i]).as<String>();
            }
            str += "]";
            return str;
        }
        case VariantType::POOL_VECTOR3_ARRAY: {

            PoolVector<Vector3> vec = operator PoolVector<Vector3>();
            String str("[");
            for (int i = 0; i < vec.size(); i++) {

                if (i > 0)
                    str += ", ";
                str = str + Variant(vec[i]).as<String>();
            }
            str += "]";
            return str;
        }
        case VariantType::POOL_STRING_ARRAY: {

            PoolVector<String> vec = operator PoolVector<String>();
            String str("[");
            for (int i = 0; i < vec.size(); i++) {

                if (i > 0)
                    str += ", ";
                str = str + vec[i];
            }
            str += "]";
            return str;
        }
        case VariantType::POOL_INT_ARRAY: {

            PoolVector<int> vec = operator PoolVector<int>();
            String str("[");
            for (int i = 0; i < vec.size(); i++) {

                if (i > 0)
                    str += ", ";
                str = str + itos(vec[i]);
            }
            str += "]";
            return str;
        }
        case VariantType::POOL_REAL_ARRAY: {

            PoolVector<real_t> vec = operator PoolVector<real_t>();
            String str("[");
            for (int i = 0; i < vec.size(); i++) {

                if (i > 0)
                    str += ", ";
                str = str + rtos(vec[i]);
            }
            str += "]";
            return str;
        } break;
        case VariantType::ARRAY: {

            Array arr = operator Array();
            if (stack.find(arr.id())) {
                return "[...]";
            }
            stack.push_back(arr.id());

            String str("[");
            for (int i = 0; i < arr.size(); i++) {
                if (i)
                    str += ", ";

                str += arr[i].stringify(stack);
            }

            str += "]";
            return str;

        }
        case VariantType::OBJECT: {

            if (_get_obj().obj) {
#ifdef DEBUG_ENABLED
                if (ScriptDebugger::get_singleton() && _get_obj().ref.is_null()) {
                    //only if debugging!
                    if (!ObjectDB::instance_validate(_get_obj().obj)) {
                        return "[Deleted Object]";
                    }
                }
#endif
                return _get_obj().obj->to_string();
            }
            return "[Object:null]";

        }
        default: {
            return "[" + String(get_type_name(type)) + "]";
        }
    }

    return "";
}

Variant::operator Vector2() const {

    if (type == VariantType::VECTOR2)
        return *reinterpret_cast<const Vector2 *>(_data._mem);
    if (type == VariantType::VECTOR3)
        return Vector2(reinterpret_cast<const Vector3 *>(_data._mem)->x, reinterpret_cast<const Vector3 *>(_data._mem)->y);
    return Vector2();
}
Variant::operator Rect2() const {

    if (type == VariantType::RECT2)
        return *reinterpret_cast<const Rect2 *>(_data._mem);
    return Rect2();
}

Variant::operator Vector3() const {

    if (type == VariantType::VECTOR3)
        return *reinterpret_cast<const Vector3 *>(_data._mem);
    if (type == VariantType::VECTOR2)
        return Vector3(reinterpret_cast<const Vector2 *>(_data._mem)->x, reinterpret_cast<const Vector2 *>(_data._mem)->y, 0.0);
    return Vector3();
}
Variant::operator Plane() const {

    if (type == VariantType::PLANE)
        return *reinterpret_cast<const Plane *>(_data._mem);
    return Plane();
}
Variant::operator ::AABB() const {

    if (type == VariantType::AABB)
        return *_data._aabb;
    return ::AABB();
}

Variant::operator Basis() const {
    return as<Basis>();
}

Variant::operator Quat() const {

    return as<Quat>();
}

Variant::operator Transform() const {
    return as<Transform>();
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

    if (type == VariantType::COLOR)
        return *reinterpret_cast<const Color *>(_data._mem);
    if (type == VariantType::STRING)
        return Color::html(as<String>());
    if (type == VariantType::INT)
        return Color::hex(operator int());
    return Color();
}

Variant::operator NodePath() const {
    return as<NodePath>();
}

Variant::operator RefPtr() const {

    if (type == VariantType::OBJECT)
        return _get_obj().ref;
    return RefPtr();
}

Variant::operator RID() const {

    if (type == VariantType::_RID)
        return *reinterpret_cast<const RID *>(_data._mem);
    if (type == VariantType::OBJECT && !_get_obj().ref.is_null()) {
        return _get_obj().ref.get_rid();
    }
    if (type == VariantType::OBJECT && _get_obj().obj) {
#ifdef DEBUG_ENABLED
        if (ScriptDebugger::get_singleton()) {
            ERR_FAIL_COND_V_MSG(!ObjectDB::instance_validate(_get_obj().obj), RID(), "Invalid pointer (object was deleted).")
        }
#endif
        Variant::CallError ce;
        Variant ret = _get_obj().obj->call(CoreStringNames::get_singleton()->get_rid, nullptr, 0, ce);
        if (ce.error == Variant::CallError::CALL_OK && ret.get_type() == VariantType::_RID) {
            return ret;
        }
        return RID();
    }
    return RID();
}

Variant::operator Object *() const {

    if (type == VariantType::OBJECT)
        return _get_obj().obj;
    return nullptr;
}
Variant::operator Node *() const {

    if (type == VariantType::OBJECT)
        return object_cast<Node>(_get_obj().obj);
    return nullptr;
}
Variant::operator Control *() const {

    if (type == VariantType::OBJECT)
        return object_cast<Control>(_get_obj().obj);
    return nullptr;
}

Variant::operator Dictionary() const {

    if (type == VariantType::DICTIONARY)
        return *reinterpret_cast<const Dictionary *>(_data._mem);
    return Dictionary();
}

template <class DA, class SA>
DA _convert_array(const SA &p_array) {

    DA da;
    da.resize(p_array.size());

    for (int i = 0; i < p_array.size(); i++) {

        da.set(i, Variant(p_array.get(i)).as<typename DA::ValueType>());
    }

    return da;
}

template <class DA>
DA _convert_array_from_variant(const Variant &p_variant) {

    switch (p_variant.get_type()) {

        case VariantType::ARRAY: {
            return _convert_array<DA, Array>(p_variant.operator Array());
        }
        case VariantType::POOL_BYTE_ARRAY: {
            return _convert_array<DA, PoolVector<uint8_t> >(p_variant.operator PoolVector<uint8_t>());
        }
        case VariantType::POOL_INT_ARRAY: {
            return _convert_array<DA, PoolVector<int> >(p_variant.operator PoolVector<int>());
        }
        case VariantType::POOL_REAL_ARRAY: {
            return _convert_array<DA, PoolVector<real_t> >(p_variant.operator PoolVector<real_t>());
        }
        case VariantType::POOL_STRING_ARRAY: {
            return _convert_array<DA, PoolVector<String> >(p_variant.operator PoolVector<String>());
        }
        case VariantType::POOL_VECTOR2_ARRAY: {
            return _convert_array<DA, PoolVector<Vector2> >(p_variant.operator PoolVector<Vector2>());
        }
        case VariantType::POOL_VECTOR3_ARRAY: {
            return _convert_array<DA, PoolVector<Vector3> >(p_variant.operator PoolVector<Vector3>());
        }
        case VariantType::POOL_COLOR_ARRAY: {
            return _convert_array<DA, PoolVector<Color> >(p_variant.operator PoolVector<Color>());
        }
        default: {
            return DA();
        }
    }
}

Variant::operator Array() const {

    if (type == VariantType::ARRAY)
        return *reinterpret_cast<const Array *>(_data._mem);
    return _convert_array_from_variant<Array>(*this);
}

Variant::operator PoolVector<uint8_t>() const {

    if (type == VariantType::POOL_BYTE_ARRAY)
        return *reinterpret_cast<const PoolVector<uint8_t> *>(_data._mem);
    return _convert_array_from_variant<PoolVector<uint8_t> >(*this);
}
Variant::operator PoolVector<int>() const {

    if (type == VariantType::POOL_INT_ARRAY)
        return *reinterpret_cast<const PoolVector<int> *>(_data._mem);
    return _convert_array_from_variant<PoolVector<int> >(*this);
}
Variant::operator PoolVector<real_t>() const {

    if (type == VariantType::POOL_REAL_ARRAY)
        return *reinterpret_cast<const PoolVector<real_t> *>(_data._mem);
    return _convert_array_from_variant<PoolVector<real_t> >(*this);
}

Variant::operator PoolVector<String>() const {

    if (type == VariantType::POOL_STRING_ARRAY)
        return *reinterpret_cast<const PoolVector<String> *>(_data._mem);
    return _convert_array_from_variant<PoolVector<String> >(*this);
}
Variant::operator PoolVector<Vector3>() const {

    if (type == VariantType::POOL_VECTOR3_ARRAY)
        return *reinterpret_cast<const PoolVector<Vector3> *>(_data._mem);
    return _convert_array_from_variant<PoolVector<Vector3> >(*this);
}
Variant::operator PoolVector<Vector2>() const {

    if (type == VariantType::POOL_VECTOR2_ARRAY)
        return *reinterpret_cast<const PoolVector<Vector2> *>(_data._mem);
    return _convert_array_from_variant<PoolVector<Vector2> >(*this);
}

Variant::operator PoolVector<Color>() const {

    if (type == VariantType::POOL_COLOR_ARRAY)
        return *reinterpret_cast<const PoolVector<Color> *>(_data._mem);
    return _convert_array_from_variant<PoolVector<Color> >(*this);
}

/* helpers */

Variant::operator Vector<RID>() const {

    Array va = operator Array();
    Vector<RID> rids;
    rids.resize(va.size());
    for (int i = 0; i < rids.size(); i++)
        rids.write[i] = va[i];
    return rids;
}

Variant::operator Vector<Vector2>() const {

    PoolVector<Vector2> from = operator PoolVector<Vector2>();
    Vector<Vector2> to;
    int len = from.size();
    if (len == 0)
        return Vector<Vector2>();
    to.resize(len);
    PoolVector<Vector2>::Read r = from.read();
    Vector2 *w = to.ptrw();
    for (int i = 0; i < len; i++) {

        w[i] = r[i];
    }
    return to;
}

Variant::operator PoolVector<Plane>() const {

    Array va = operator Array();
    PoolVector<Plane> planes;
    int va_size = va.size();
    if (va_size == 0)
        return planes;

    planes.resize(va_size);
    PoolVector<Plane>::Write w = planes.write();

    for (int i = 0; i < va_size; i++)
        w[i] = va[i];

    return planes;
}

Variant::operator PoolVector<Face3>() const {

    PoolVector<Vector3> va = operator PoolVector<Vector3>();
    PoolVector<Face3> faces;
    int va_size = va.size();
    if (va_size == 0)
        return faces;

    faces.resize(va_size / 3);
    PoolVector<Face3>::Write w = faces.write();
    PoolVector<Vector3>::Read r = va.read();

    for (int i = 0; i < va_size; i++)
        w[i / 3].vertex[i % 3] = r[i];

    return faces;
}

Variant::operator Vector<Plane>() const {

    Array va = operator Array();
    Vector<Plane> planes;
    int va_size = va.size();
    if (va_size == 0)
        return planes;

    planes.resize(va_size);

    for (int i = 0; i < va_size; i++)
        planes.write[i] = va[i];

    return planes;
}

Variant::operator Vector<Variant>() const {

    Array from = operator Array();
    Vector<Variant> to;
    int len = from.size();
    to.resize(len);
    for (int i = 0; i < len; i++) {

        to.write[i] = from[i];
    }
    return to;
}

Variant::operator Vector<uint8_t>() const {

    PoolVector<uint8_t> from = operator PoolVector<uint8_t>();
    Vector<uint8_t> to;
    int len = from.size();
    to.resize(len);
    for (int i = 0; i < len; i++) {

        to.write[i] = from[i];
    }
    return to;
}
Variant::operator Vector<int>() const {

    PoolVector<int> from = operator PoolVector<int>();
    Vector<int> to;
    int len = from.size();
    to.resize(len);
    for (int i = 0; i < len; i++) {

        to.write[i] = from[i];
    }
    return to;
}
Variant::operator Vector<real_t>() const {

    PoolVector<real_t> from = operator PoolVector<real_t>();
    Vector<real_t> to;
    int len = from.size();
    to.resize(len);
    for (int i = 0; i < len; i++) {

        to.write[i] = from[i];
    }
    return to;
}

Variant::operator Vector<String>() const {

    PoolVector<String> from = operator PoolVector<String>();
    Vector<String> to;
    int len = from.size();
    to.resize(len);
    for (int i = 0; i < len; i++) {

        to.write[i] = from[i];
    }
    return to;
}
Variant::operator Vector<StringName>() const {

    PoolVector<String> from = operator PoolVector<String>();
    Vector<StringName> to;
    int len = from.size();
    to.resize(len);
    for (int i = 0; i < len; i++) {

        to.write[i] = StringName(from[i]);
    }
    return to;
}

Variant::operator Vector<Vector3>() const {

    PoolVector<Vector3> from = operator PoolVector<Vector3>();
    Vector<Vector3> to;
    int len = from.size();
    if (len == 0)
        return Vector<Vector3>();
    to.resize(len);
    PoolVector<Vector3>::Read r = from.read();
    Vector3 *w = to.ptrw();
    for (int i = 0; i < len; i++) {

        w[i] = r[i];
    }
    return to;
}
Variant::operator Vector<Color>() const {

    PoolVector<Color> from = operator PoolVector<Color>();
    Vector<Color> to;
    int len = from.size();
    if (len == 0)
        return Vector<Color>();
    to.resize(len);
    PoolVector<Color>::Read r = from.read();
    Color *w = to.ptrw();
    for (int i = 0; i < len; i++) {

        w[i] = r[i];
    }
    return to;
}

Variant::operator Margin() const {

    return (Margin) operator int();
}
Variant::operator Orientation() const {

    return (Orientation) operator int();
}
Variant::operator IP_Address() const {
    return as<IP_Address>();
}

Variant::Variant(QChar p_char) {

    type = VariantType::INT;
    _data._int = p_char.unicode();
}

Variant::Variant(const StringName &p_string) {

    type = VariantType::STRING;
    memnew_placement(_data._mem, String(p_string));
}
Variant::Variant(const String &p_string) {

    type = VariantType::STRING;
    memnew_placement(_data._mem, String(p_string));
}

Variant::Variant(const char *const p_cstring) {

    type = VariantType::STRING;
    memnew_placement(_data._mem, String((const char *)p_cstring));
}

Variant::Variant(const CharType *p_wstring) {

    type = VariantType::STRING;
    memnew_placement(_data._mem, String(p_wstring));
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

    type = VariantType::NODE_PATH;
    memnew_placement(_data._mem, NodePath(p_node_path));
}

Variant::Variant(const RefPtr &p_resource) {

    type = VariantType::OBJECT;
    memnew_placement(_data._mem, ObjData);
    REF *ref = reinterpret_cast<REF *>(p_resource.get_data());
    _get_obj().obj = ref->get();
    _get_obj().ref = p_resource;
}

Variant::Variant(const RID &p_rid) {

    type = VariantType::_RID;
    memnew_placement(_data._mem, RID(p_rid));
}

Variant::Variant(const Object *p_object) {

    type = VariantType::OBJECT;
#if DEBUG_VARIANT_OBJECT_CONSTRUCTOR
    assert(!p_object || !ObjectNS::cast_to<RefCounted>(p_object));
#endif
    memnew_placement(_data._mem, ObjData);
    _get_obj().obj = const_cast<Object *>(p_object);
}

Variant::Variant(const Dictionary &p_dictionary) {

    type = VariantType::DICTIONARY;
    memnew_placement(_data._mem, Dictionary(p_dictionary));
}

Variant::Variant(const Array &p_array) {

    type = VariantType::ARRAY;
    memnew_placement(_data._mem, Array(p_array));
}

Variant::Variant(const PoolVector<Plane> &p_array) {

    type = VariantType::ARRAY;

    Array *plane_array = memnew_placement(_data._mem, Array);

    plane_array->resize(p_array.size());

    for (int i = 0; i < p_array.size(); i++) {

        plane_array->operator[](i) = Variant(p_array[i]);
    }
}

Variant::Variant(const Vector<Plane> &p_array) {

    type = VariantType::ARRAY;

    Array *plane_array = memnew_placement(_data._mem, Array);

    plane_array->resize(p_array.size());

    for (int i = 0; i < p_array.size(); i++) {

        plane_array->operator[](i) = Variant(p_array[i]);
    }
}

Variant::Variant(const Vector<RID> &p_array) {

    type = VariantType::ARRAY;

    Array *rid_array = memnew_placement(_data._mem, Array);

    rid_array->resize(p_array.size());

    for (int i = 0; i < p_array.size(); i++) {

        rid_array->set(i, Variant(p_array[i]));
    }
}

Variant::Variant(const Vector<Vector2> &p_array) {

    type = VariantType::NIL;
    PoolVector<Vector2> v;
    int len = p_array.size();
    if (len > 0) {
        v.resize(len);
        PoolVector<Vector2>::Write w = v.write();
        const Vector2 *r = p_array.ptr();

        for (int i = 0; i < len; i++)
            w[i] = r[i];
    }
    *this = Variant(v);
}

Variant::Variant(const PoolVector<uint8_t> &p_raw_array) {

    type = VariantType::POOL_BYTE_ARRAY;
    static_assert (sizeof(PoolVector<uint8_t>)<=sizeof(_data));
    memnew_placement(_data._mem, PoolVector<uint8_t>(p_raw_array));
}
Variant::Variant(const PoolVector<int> &p_int_array) {

    type = VariantType::POOL_INT_ARRAY;
    static_assert (sizeof(PoolVector<int>)<=sizeof(_data));
    memnew_placement(_data._mem, PoolVector<int>(p_int_array));
}
Variant::Variant(const PoolVector<real_t> &p_real_array) {

    type = VariantType::POOL_REAL_ARRAY;
    memnew_placement(_data._mem, PoolVector<real_t>(p_real_array));
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

            for (int j = 0; j < 3; j++)
                w[i * 3 + j] = r[i].vertex[j];
        }
    }

    type = VariantType::NIL;

    *this = vertices;
}

/* helpers */

Variant::Variant(const Vector<Variant> &p_array) {

    type = VariantType::NIL;
    Array v;
    int len = p_array.size();
    v.resize(len);
    for (int i = 0; i < len; i++)
        v.set(i, p_array[i]);
    *this = v;
}

Variant::Variant(const Vector<uint8_t> &p_array) {

    type = VariantType::NIL;
    PoolVector<uint8_t> v;
    int len = p_array.size();
    v.resize(len);
    for (int i = 0; i < len; i++)
        v.set(i, p_array[i]);
    *this = v;
}

Variant::Variant(const Vector<int> &p_array) {

    type = VariantType::NIL;
    PoolVector<int> v;
    int len = p_array.size();
    v.resize(len);
    for (int i = 0; i < len; i++)
        v.set(i, p_array[i]);
    *this = v;
}

Variant::Variant(const Vector<real_t> &p_array) {

    type = VariantType::NIL;
    PoolVector<real_t> v;
    int len = p_array.size();
    v.resize(len);
    for (int i = 0; i < len; i++)
        v.set(i, p_array[i]);
    *this = v;
}

Variant::Variant(const Vector<String> &p_array) {

    type = VariantType::NIL;
    PoolVector<String> v;
    int len = p_array.size();
    v.resize(len);
    for (int i = 0; i < len; i++)
        v.set(i, p_array[i]);
    *this = v;
}

Variant::Variant(const Vector<StringName> &p_array) {

    type = VariantType::NIL;
    PoolVector<String> v;
    int len = p_array.size();
    v.resize(len);
    for (int i = 0; i < len; i++)
        v.set(i, p_array[i]);
    *this = v;
}

Variant::Variant(const Vector<Vector3> &p_array) {

    type = VariantType::NIL;
    PoolVector<Vector3> v;
    int len = p_array.size();
    if (len > 0) {
        v.resize(len);
        PoolVector<Vector3>::Write w = v.write();
        const Vector3 *r = p_array.ptr();

        for (int i = 0; i < len; i++)
            w[i] = r[i];
    }
    *this = v;
}

Variant::Variant(const Vector<Color> &p_array) {

    type = VariantType::NIL;
    PoolVector<Color> v;
    int len = p_array.size();
    v.resize(len);
    for (int i = 0; i < len; i++)
        v.set(i, p_array[i]);
    *this = v;
}

Variant &Variant::operator=(const Variant &p_variant) {

    if (unlikely(this == &p_variant))
        return *this;

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
        case VariantType::REAL: {

            _data._real = p_variant._data._real;
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

            *reinterpret_cast<ObjData *>(_data._mem) = p_variant._get_obj();
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

            *reinterpret_cast<PoolVector<uint8_t> *>(_data._mem) = *reinterpret_cast<const PoolVector<uint8_t> *>(p_variant._data._mem);
        } break;
        case VariantType::POOL_INT_ARRAY: {

            *reinterpret_cast<PoolVector<int> *>(_data._mem) = *reinterpret_cast<const PoolVector<int> *>(p_variant._data._mem);
        } break;
        case VariantType::POOL_REAL_ARRAY: {

            *reinterpret_cast<PoolVector<real_t> *>(_data._mem) = *reinterpret_cast<const PoolVector<real_t> *>(p_variant._data._mem);
        } break;
        case VariantType::POOL_STRING_ARRAY: {

            *reinterpret_cast<PoolVector<String> *>(_data._mem) = *reinterpret_cast<const PoolVector<String> *>(p_variant._data._mem);
        } break;
        case VariantType::POOL_VECTOR2_ARRAY: {

            *reinterpret_cast<PoolVector<Vector2> *>(_data._mem) = *reinterpret_cast<const PoolVector<Vector2> *>(p_variant._data._mem);
        } break;
        case VariantType::POOL_VECTOR3_ARRAY: {

            *reinterpret_cast<PoolVector<Vector3> *>(_data._mem) = *reinterpret_cast<const PoolVector<Vector3> *>(p_variant._data._mem);
        } break;
        case VariantType::POOL_COLOR_ARRAY: {

            *reinterpret_cast<PoolVector<Color> *>(_data._mem) = *reinterpret_cast<const PoolVector<Color> *>(p_variant._data._mem);
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
        case VariantType::REAL: {

            return hash_djb2_one_float(_data._real);
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

        } break;
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

            return hash_djb2_one_64(make_uint64_t(_get_obj().obj));
        }
        case VariantType::NODE_PATH: {

            return reinterpret_cast<const NodePath *>(_data._mem)->hash();
        }
        case VariantType::DICTIONARY: {

            return reinterpret_cast<const Dictionary *>(_data._mem)->hash();

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
        case VariantType::POOL_REAL_ARRAY: {

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
template<class T>
bool hash_compare_helper(T p_lhs, T p_rhs);
template<>
bool hash_compare_helper<float>(float p_lhs, float p_rhs)
{
    return (p_lhs == p_rhs) || (Math::is_nan(p_lhs) && Math::is_nan(p_rhs));
}
template<>
bool hash_compare_helper<Vector2>(Vector2 p_lhs, Vector2 p_rhs)
{
    return hash_compare_helper(p_lhs.x, p_rhs.x) && hash_compare_helper((p_lhs).y, (p_rhs).y);
}
template<>
bool hash_compare_helper<Vector3>(Vector3 p_lhs, Vector3 p_rhs)
{
    return hash_compare_helper(p_lhs.x, p_rhs.x) && hash_compare_helper((p_lhs).y, (p_rhs).y) && hash_compare_helper((p_lhs).z, (p_rhs).z);
}
template<>
bool hash_compare_helper<Quat>(Quat p_lhs, Quat p_rhs)
{
    return hash_compare_helper(p_lhs.x, p_rhs.x) && hash_compare_helper((p_lhs).y, (p_rhs).y) &&
           hash_compare_helper((p_lhs).z, (p_rhs).z) && hash_compare_helper((p_lhs).w, (p_rhs).w);
}
template<>
bool hash_compare_helper<Color>(Color p_lhs, Color p_rhs)
{
    return hash_compare_helper(p_lhs.r, p_rhs.r) && hash_compare_helper((p_lhs).g, (p_rhs).g) &&
           hash_compare_helper((p_lhs).b, (p_rhs).b) && hash_compare_helper((p_lhs).a, (p_rhs).a);
}
} // end of anonymous namespace

#define hash_compare_pool_array(p_lhs, p_rhs, p_type, p_compare_func)                   \
    const PoolVector<p_type> &l = *reinterpret_cast<const PoolVector<p_type> *>(p_lhs); \
    const PoolVector<p_type> &r = *reinterpret_cast<const PoolVector<p_type> *>(p_rhs); \
                                                                                        \
    if (l.size() != r.size())                                                           \
        return false;                                                                   \
                                                                                        \
    PoolVector<p_type>::Read lr = l.read();                                             \
    PoolVector<p_type>::Read rr = r.read();                                             \
                                                                                        \
    for (int i = 0; i < l.size(); ++i) {                                                \
        if (!p_compare_func((lr[i]), (rr[i])))                                          \
            return false;                                                               \
    }                                                                                   \
                                                                                        \
    return true

bool Variant::hash_compare(const Variant &p_variant) const {
    if (type != p_variant.type)
        return false;

    switch (type) {
        case VariantType::REAL: {
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

            return (hash_compare_helper(l->position, r->position)) &&
                   (hash_compare_helper(l->size, r->size));
        }

        case VariantType::TRANSFORM2D: {
            Transform2D *l = _data._transform2d;
            Transform2D *r = p_variant._data._transform2d;

            for (int i = 0; i < 3; i++) {
                if (!(hash_compare_helper(l->elements[i], r->elements[i])))
                    return false;
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

            return (hash_compare_helper(l->normal, r->normal)) &&
                   (hash_compare_helper(l->d, r->d));
        }

        case VariantType::AABB: {
            const ::AABB *l = _data._aabb;
            const ::AABB *r = p_variant._data._aabb;

            return (hash_compare_helper(l->position, r->position) &&
                    (hash_compare_helper(l->size, r->size)));

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
                if (!(hash_compare_helper(l->elements[i], r->elements[i])))
                    return false;
            }

            return true;
        }

        case VariantType::TRANSFORM: {
            const Transform *l = _data._transform;
            const Transform *r = p_variant._data._transform;

            for (int i = 0; i < 3; i++) {
                if (!(hash_compare_helper(l->basis.elements[i], r->basis.elements[i])))
                    return false;
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

            if (l.size() != r.size())
                return false;

            for (int i = 0; i < l.size(); ++i) {
                if (!l[i].hash_compare(r[i]))
                    return false;
            }

            return true;
        }

        case VariantType::POOL_REAL_ARRAY: {
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
            bool v;
            Variant r;
            evaluate(OP_EQUAL, *this, p_variant, r, v);
            return r;
    }

    return false;
}

bool Variant::is_ref() const {

    return type == VariantType::OBJECT && !_get_obj().ref.is_null();
}

Vector<Variant> varray() {

    return Vector<Variant>();
}

Vector<Variant> varray(const Variant &p_arg1) {

    Vector<Variant> v;
    v.push_back(p_arg1);
    return v;
}
Vector<Variant> varray(const Variant &p_arg1, const Variant &p_arg2) {

    Vector<Variant> v;
    v.push_back(p_arg1);
    v.push_back(p_arg2);
    return v;
}
Vector<Variant> varray(const Variant &p_arg1, const Variant &p_arg2, const Variant &p_arg3) {

    Vector<Variant> v;
    v.push_back(p_arg1);
    v.push_back(p_arg2);
    v.push_back(p_arg3);
    return v;
}
Vector<Variant> varray(const Variant &p_arg1, const Variant &p_arg2, const Variant &p_arg3, const Variant &p_arg4) {

    Vector<Variant> v;
    v.push_back(p_arg1);
    v.push_back(p_arg2);
    v.push_back(p_arg3);
    v.push_back(p_arg4);
    return v;
}

Vector<Variant> varray(const Variant &p_arg1, const Variant &p_arg2, const Variant &p_arg3, const Variant &p_arg4, const Variant &p_arg5) {

    Vector<Variant> v;
    v.push_back(p_arg1);
    v.push_back(p_arg2);
    v.push_back(p_arg3);
    v.push_back(p_arg4);
    v.push_back(p_arg5);
    return v;
}

void Variant::static_assign(const Variant &p_variant) {
}

bool Variant::is_shared() const {

    switch (type) {

        case VariantType::OBJECT: return true;
        case VariantType::ARRAY: return true;
        case VariantType::DICTIONARY: return true;
        default: {
        }
    }

    return false;
}

Variant Variant::call(const StringName &p_method, VARIANT_ARG_DECLARE) {
    VARIANT_ARGPTRS;
    int argc = 0;
    for (int i = 0; i < VARIANT_ARG_MAX; i++) {
        if (argptr[i]->get_type() == VariantType::NIL)
            break;
        argc++;
    }

    CallError error;

    Variant ret = call(p_method, argptr, argc, error);

    switch (error.error) {

        case CallError::CALL_ERROR_INVALID_ARGUMENT: {

            String err = "Invalid type for argument #" + itos(error.argument) + ", expected '" + Variant::get_type_name(error.expected) + "'.";
            ERR_PRINT(err)

        } break;
        case CallError::CALL_ERROR_INVALID_METHOD: {

            String err = "Invalid method '" + p_method + "' for type '" + Variant::get_type_name(type) + "'.";
            ERR_PRINT(err)
        } break;
        case CallError::CALL_ERROR_TOO_MANY_ARGUMENTS: {

            String err = "Too many arguments for method '" + p_method + "'";
            ERR_PRINT(err)
        } break;
        default: {
        }
    }

    return ret;
}

void Variant::construct_from_string(const String &p_string, Variant &r_value, ObjectConstruct p_obj_construct, void *p_construct_ud) {

    r_value = Variant();
}

String Variant::get_construct_string() const {

    String vars;
    VariantWriter::write_to_string(*this, vars);

    return vars;
}

String Variant::get_call_error_text(Object *p_base, const StringName &p_method, const Variant **p_argptrs, int p_argcount, const Variant::CallError &ce) {

    String err_text;

    if (ce.error == Variant::CallError::CALL_ERROR_INVALID_ARGUMENT) {
        int errorarg = ce.argument;
        if (p_argptrs) {
            err_text = "Cannot convert argument " + itos(errorarg + 1) + " from " + Variant::get_type_name(p_argptrs[errorarg]->get_type()) + " to " + Variant::get_type_name(ce.expected) + ".";
        } else {
            err_text = "Cannot convert argument " + itos(errorarg + 1) + " from [missing argptr, type unknown] to " + Variant::get_type_name(ce.expected) + ".";
        }
    } else if (ce.error == Variant::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS) {
        err_text = "Method expected " + itos(ce.argument) + " arguments, but called with " + itos(p_argcount) + ".";
    } else if (ce.error == Variant::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS) {
        err_text = "Method expected " + itos(ce.argument) + " arguments, but called with " + itos(p_argcount) + ".";
    } else if (ce.error == Variant::CallError::CALL_ERROR_INVALID_METHOD) {
        err_text = "Method not found.";
    } else if (ce.error == Variant::CallError::CALL_ERROR_INSTANCE_IS_NULL) {
        err_text = "Instance is null";
    } else if (ce.error == Variant::CallError::CALL_OK) {
        return "Call OK";
    }

    String class_name = p_base->get_class();
    Ref<Script> script = refFromRefPtr<Script>(p_base->get_script());
    if (script && PathUtils::is_resource_file(script->get_path())) {

        class_name += "(" + PathUtils::get_file(script->get_path()) + ")";
    }
    return "'" + class_name + "::" + String(p_method) + "': " + err_text;
}

String vformat(const String &p_text, const Variant &p1, const Variant &p2, const Variant &p3, const Variant &p4, const Variant &p5) {

    Array args;
    if (p1.get_type() != VariantType::NIL) {

        args.push_back(p1);

        if (p2.get_type() != VariantType::NIL) {

            args.push_back(p2);

            if (p3.get_type() != VariantType::NIL) {

                args.push_back(p3);

                if (p4.get_type() != VariantType::NIL) {

                    args.push_back(p4);

                    if (p5.get_type() != VariantType::NIL) {

                        args.push_back(p5);
                    }
                }
            }
        }
    }

    bool error = false;
    String fmt = StringUtils::sprintf(p_text,args, &error);

    ERR_FAIL_COND_V(error, String())

    return fmt;
}

void fill_with_all_variant_types(const char *nillname,char (&s)[7+(longest_variant_type_name+1)*int(VariantType::VARIANT_MAX)]) {

    assert(strlen(nillname)<=7);

    int write_idx = sprintf(s,"%s",nillname);
    for (int i = 1; i < int(VariantType::VARIANT_MAX); i++) {
        write_idx+=sprintf(s+write_idx,",%s",Variant::get_type_name(VariantType(i)));
    }

}
