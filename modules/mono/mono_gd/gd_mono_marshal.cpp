/*************************************************************************/
/*  gd_mono_marshal.cpp                                                  */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
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

#include "gd_mono_marshal.h"

#include "gd_mono.h"
#include "gd_mono_cache.h"
#include "gd_mono_class.h"
#include "../managed_callable.h"
#include "../signal_awaiter_utils.h"
#include "core/object_db.h"
#include "core/pool_vector.h"
#include "core/rid.h"
#include "core/math/face3.h"


namespace GDMonoMarshal {
namespace {
template<class T>
static MonoClass *cached_class();

template<>
MonoClass *cached_class<uint8_t>() {
    return CACHED_CLASS_RAW(uint8_t);
}
template<>
MonoClass *cached_class<int32_t>() {
    return CACHED_CLASS_RAW(int32_t);
}
template<>
MonoClass *cached_class<float>() {
    return CACHED_CLASS_RAW(float);
}
template<>
MonoClass *cached_class<String>() {
    return CACHED_CLASS_RAW(String);
}

template<typename T>
MonoArray *impl_container_to_mono_array(const PoolVector<T> &p_array) {
    auto r = p_array.read();

    MonoArray *ret = mono_array_new(mono_domain_get(), cached_class<T>(), p_array.size());

    for (int i = 0; i < p_array.size(); i++) {
        mono_array_set(ret, T, i, r[i]);
    }

    return ret;
}
template<typename T>
PoolVector<T> impl_mono_array_to_pool_vec(MonoArray *p_array) {
    PoolVector<T> ret;
    if (!p_array)
        return ret;
    int length = mono_array_length(p_array);
    ret.resize(length);
    auto w = ret.write();

    for (int i = 0; i < length; i++) {
        w[i] = mono_array_get(p_array, T, i);
    }
    return ret;
}
template<typename T>
Vector<T> impl_mono_array_to_vector(MonoArray *p_array) {
    Vector<T> ret;
    if (!p_array)
        return ret;
    int length = mono_array_length(p_array);
    ret.reserve(length);
    for (int i = 0; i < length; i++) {
        ret.emplace_back(mono_array_get(p_array, T, i));
    }
    return ret;
}

template<typename T>
MonoArray *impl_container_to_mono_array(Span<const T> p_array) {
    MonoArray *ret = mono_array_new(mono_domain_get(), cached_class<T>(), p_array.size());

    for (int i = 0, fin = p_array.size(); i < fin; ++i) {
        mono_array_set(ret, T, i, p_array[i]);
    }

    return ret;
}

} // end of contained anonymous namespace
// TODO: Use memcpy where possible
template<>
GODOT_EXPORT MonoArray *container_to_mono_array<int>(const PoolVector<int> &p_array)
{
    return impl_container_to_mono_array<int>(p_array);
}
template<>
GODOT_EXPORT PoolVector<int> mono_array_to_pool_vec<int>(MonoArray *p_array) {
    return impl_mono_array_to_pool_vec<int>(p_array);
}

template<>
GODOT_EXPORT MonoArray *container_to_mono_array<int>(Span<const int> p_array)
{
    return impl_container_to_mono_array<int>(p_array);
}
template<>
GODOT_EXPORT Vector<int> mono_array_to_vector<int>(MonoArray *p_array) {
    return impl_mono_array_to_vector<int>(p_array);
}


template<>
GODOT_EXPORT MonoArray *container_to_mono_array<uint8_t>(const PoolVector<uint8_t> &p_array) {
    return impl_container_to_mono_array<uint8_t>(p_array);
}
template<>
GODOT_EXPORT PoolVector<uint8_t> mono_array_to_pool_vec<uint8_t>(MonoArray *p_array) {
    return impl_mono_array_to_pool_vec<uint8_t>(p_array);
}

template<>
GODOT_EXPORT MonoArray *container_to_mono_array<uint8_t>(Span<const uint8_t> p_array) {
    return impl_container_to_mono_array<uint8_t>(p_array);
}
template<>
GODOT_EXPORT Vector<uint8_t> mono_array_to_vector<uint8_t>(MonoArray *p_array) {
    return impl_mono_array_to_vector<uint8_t>(p_array);
}

template<>
GODOT_EXPORT MonoArray *container_to_mono_array<float>(const PoolVector<float> &p_array) {
    return impl_container_to_mono_array<float>(p_array);
}
template<>
GODOT_EXPORT PoolVector<float> mono_array_to_pool_vec<float>(MonoArray *p_array) {
    return impl_mono_array_to_pool_vec<float>(p_array);
}

template<>
GODOT_EXPORT MonoArray *container_to_mono_array<float>(Span<const float> p_array) {
    return impl_container_to_mono_array<float>(p_array);
}
template<>
GODOT_EXPORT Vector<float> mono_array_to_vector<float>(MonoArray *p_array) {
    return impl_mono_array_to_vector<float>(p_array);
}

template<>
GODOT_EXPORT MonoArray *container_to_mono_array<String>(const PoolVector<String> &p_array) {
    PoolStringArray::Read r = p_array.read();

    MonoArray *ret = mono_array_new(mono_domain_get(), cached_class<String>(), p_array.size());

    for (int i = 0; i < p_array.size(); i++) {
        MonoString *boxed = mono_string_from_godot(r[i]);
        mono_array_setref(ret, i, boxed);
    }

    return ret;
}

template<>
GODOT_EXPORT MonoArray *container_to_mono_array<StringName>(Span<const StringName> p_array) {
    MonoArray *ret = mono_array_new(mono_domain_get(), cached_class<String>(), p_array.size());

    for (size_t i = 0; i < p_array.size(); i++) {
        MonoString *boxed = mono_string_from_godot(p_array[i]);
        mono_array_setref(ret, i, boxed);
    }
    return ret;
}

template<>
GODOT_EXPORT MonoArray *container_to_mono_array<String>(Span<const String> p_array) {
    MonoArray *ret = mono_array_new(mono_domain_get(), cached_class<String>(), p_array.size());

    for (size_t i = 0; i < p_array.size(); i++) {
        MonoString *boxed = mono_string_from_godot(p_array[i]);
        mono_array_setref(ret, i, boxed);
    }
    return ret;
}
template<>
GODOT_EXPORT PoolVector<String> mono_array_to_pool_vec<String>(MonoArray *p_array) {
    PoolVector<String> ret;
    if (!p_array)
        return ret;
    int length = mono_array_length(p_array);
    ret.resize(length);
    auto w = ret.write();

    for (int i = 0; i < length; i++) {
        MonoString *elem = mono_array_get(p_array, MonoString *, i);
        w[i] = mono_string_to_godot(elem);
    }

    return ret;
}
template<>
GODOT_EXPORT Vector<String> mono_array_to_vector<String>(MonoArray *p_array) {
    Vector<String> ret;
    if (!p_array)
        return ret;
    int length = mono_array_length(p_array);
    ret.reserve(length);
    for (int i = 0; i < length; i++) {
        MonoString *elem = mono_array_get(p_array, MonoString *, i);
        ret.emplace_back(mono_string_to_godot(elem));
    }

    return ret;
}

template<>
GODOT_EXPORT MonoArray *container_to_mono_array<Color>(const PoolVector<Color> &p_array) {
    PoolColorArray::Read r = p_array.read();

    MonoArray *ret = mono_array_new(mono_domain_get(), CACHED_CLASS_RAW(Color), p_array.size());

    for (int i = 0; i < p_array.size(); i++) {
        M_Color *raw = (M_Color *)mono_array_addr_with_size(ret, sizeof(M_Color), i);
        *raw = MARSHALLED_OUT(Color, r[i]);
    }

    return ret;
}
template<>
GODOT_EXPORT MonoArray *container_to_mono_array<Color>(Span<const Color> p_array) {
    MonoArray *ret = mono_array_new(mono_domain_get(), CACHED_CLASS_RAW(Color), p_array.size());

    for (int i = 0; i < p_array.size(); i++) {
        M_Color *raw = (M_Color *)mono_array_addr_with_size(ret, sizeof(M_Color), i);
        *raw = MARSHALLED_OUT(Color, p_array[i]);
    }

    return ret;
}

template<>
GODOT_EXPORT PoolVector<Color> mono_array_to_pool_vec<Color>(MonoArray *p_array) {
    PoolColorArray ret;
    if (!p_array)
        return ret;
    int length = mono_array_length(p_array);
    ret.resize(length);
    PoolColorArray::Write w = ret.write();

    for (int i = 0; i < length; i++) {
        w[i] = MARSHALLED_IN(Color, (M_Color *)mono_array_addr_with_size(p_array, sizeof(M_Color), i));
    }
    return ret;
}
template<>
GODOT_EXPORT Vector<Color> mono_array_to_vector<Color>(MonoArray *p_array) {
    Vector<Color> ret;
    if (!p_array)
        return ret;
    int length = mono_array_length(p_array);
    ret.reserve(length);
    for (int i = 0; i < length; i++) {
        ret.emplace_back(MARSHALLED_IN(Color, (M_Color *)mono_array_addr_with_size(p_array, sizeof(M_Color), i)));
    }

    return ret;
}


template<>
GODOT_EXPORT MonoArray * container_to_mono_array<Vector2>(const PoolVector<Vector2> &p_array) {
    PoolVector2Array::Read r = p_array.read();

    MonoArray *ret = mono_array_new(mono_domain_get(), CACHED_CLASS_RAW(Vector2), p_array.size());

    for (int i = 0; i < p_array.size(); i++) {
        M_Vector2 *raw = (M_Vector2 *)mono_array_addr_with_size(ret, sizeof(M_Vector2), i);
        *raw = MARSHALLED_OUT(Vector2, r[i]);
    }

    return ret;
}
template<>
GODOT_EXPORT MonoArray* container_to_mono_array<Vector2>(Span<const Vector2> p_array) {
    MonoArray* ret = mono_array_new(mono_domain_get(), CACHED_CLASS_RAW(Vector2), p_array.size());

    for (int i = 0, fin = p_array.size(); i < fin; ++i) {
        M_Vector2* raw = (M_Vector2*)mono_array_addr_with_size(ret, sizeof(M_Vector2), i);
        *raw = MARSHALLED_OUT(Vector2, p_array[i]);
    }
    return ret;
}

template<>
GODOT_EXPORT PoolVector<Vector2> mono_array_to_pool_vec<Vector2>(MonoArray *p_array) {
    PoolVector<Vector2> ret;
    if (!p_array)
        return ret;
    int length = mono_array_length(p_array);
    ret.resize(length);
    PoolVector2Array::Write w = ret.write();

    for (int i = 0; i < length; i++) {
        w[i] = MARSHALLED_IN(Vector2, (M_Vector2 *)mono_array_addr_with_size(p_array, sizeof(M_Vector2), i));
    }

    return ret;
}

template<>
GODOT_EXPORT Vector<Vector2> mono_array_to_vector<Vector2>(MonoArray* p_array) {
    Vector<Vector2> ret;
    if (!p_array)
        return ret;
    int length = mono_array_length(p_array);
    ret.reserve(length);

    for (int i = 0; i < length; i++) {
        ret.emplace_back(MARSHALLED_IN(Vector2, (M_Vector2*)mono_array_addr_with_size(p_array, sizeof(M_Vector2), i)));
    }

    return ret;
}



template<>
GODOT_EXPORT MonoArray *container_to_mono_array<Vector3>(const PoolVector<Vector3> &p_array) {
    PoolVector3Array::Read r = p_array.read();

    MonoArray *ret = mono_array_new(mono_domain_get(), CACHED_CLASS_RAW(Vector3), p_array.size());

    for (int i = 0; i < p_array.size(); i++) {
        M_Vector3 *raw = (M_Vector3 *)mono_array_addr_with_size(ret, sizeof(M_Vector3), i);
        *raw = MARSHALLED_OUT(Vector3, r[i]);
    }
    return ret;
}
template<>
GODOT_EXPORT MonoArray *container_to_mono_array<Vector3>(Span<const Vector3> p_array) {
    MonoArray *ret = mono_array_new(mono_domain_get(), CACHED_CLASS_RAW(Vector3), p_array.size());

    for (int i = 0, fin = p_array.size(); i < fin; ++i) {
        M_Vector3 *raw = (M_Vector3 *)mono_array_addr_with_size(ret, sizeof(M_Vector3), i);
        *raw = MARSHALLED_OUT(Vector3, p_array[i]);
    }
    return ret;
}

template<>
GODOT_EXPORT Vector<Vector3> mono_array_to_vector<Vector3>(MonoArray *p_array) {
    Vector<Vector3> ret;
    if (!p_array)
        return ret;
    int length = mono_array_length(p_array);
    ret.reserve(length);

    for (int i = 0; i < length; i++) {
        ret.emplace_back(MARSHALLED_IN(Vector3, (M_Vector3 *)mono_array_addr_with_size(p_array, sizeof(M_Vector3), i)));
    }

    return ret;
}
template<>
GODOT_EXPORT PoolVector<Vector3> mono_array_to_pool_vec<Vector3>(MonoArray *p_array) {
    PoolVector<Vector3> ret;
    if (!p_array)
        return ret;
    int length = mono_array_length(p_array);
    ret.resize(length);
    PoolVector3Array::Write w = ret.write();

    for (int i = 0; i < length; i++) {
        w[i] = MARSHALLED_IN(Vector3, (M_Vector3 *)mono_array_addr_with_size(p_array, sizeof(M_Vector3), i));
    }

    return ret;
}

template<>
GODOT_EXPORT MonoArray *container_to_mono_array<Face3>(const PoolVector<Face3>& p_array)
{
    auto r = p_array.read();

    MonoArray *ret = mono_array_new(mono_domain_get(), CACHED_CLASS_RAW(Vector3), p_array.size()*3);

    for (int i = 0; i < p_array.size(); i++) {
        M_Vector3 *raw = (M_Vector3 *)mono_array_addr_with_size(ret, sizeof(M_Vector3), 3*i+0);
        *raw = MARSHALLED_OUT(Vector3, r[i].vertex[0]);
        raw = (M_Vector3 *)mono_array_addr_with_size(ret, sizeof(M_Vector3), 3 * i + 1);
        *raw = MARSHALLED_OUT(Vector3, r[i].vertex[1]);
        raw = (M_Vector3 *)mono_array_addr_with_size(ret, sizeof(M_Vector3), 3 * i + 2);
        *raw = MARSHALLED_OUT(Vector3, r[i].vertex[2]);
    }

    return ret;
}

template<>
GODOT_EXPORT MonoArray *container_to_mono_array<Face3>(Span<const Face3> p_array)
{
    MonoArray *ret = mono_array_new(mono_domain_get(), CACHED_CLASS_RAW(Vector3), p_array.size()*3);

    for (int i = 0; i < p_array.size(); i++) {
        M_Vector3 *raw = (M_Vector3 *)mono_array_addr_with_size(ret, sizeof(M_Vector3), 3*i+0);
        *raw = MARSHALLED_OUT(Vector3, p_array[i].vertex[0]);
        raw = (M_Vector3 *)mono_array_addr_with_size(ret, sizeof(M_Vector3), 3 * i + 1);
        *raw = MARSHALLED_OUT(Vector3, p_array[i].vertex[1]);
        raw = (M_Vector3 *)mono_array_addr_with_size(ret, sizeof(M_Vector3), 3 * i + 2);
        *raw = MARSHALLED_OUT(Vector3, p_array[i].vertex[2]);
    }

    return ret;
}

MonoArray* container_to_mono_array(const Array& p_array, MonoClass* p_array_type_class) {
    int length = p_array.size();
    MonoArray* ret = mono_array_new(mono_domain_get(), p_array_type_class, length);

    for (int i = 0; i < length; i++) {
        MonoObject* boxed = variant_to_mono_object(p_array[i]);
        mono_array_setref(ret, i, boxed);
    }
    return ret;
}

// Fast path for mono_array_to_span

VariantType managed_to_variant_type(const ManagedType &p_type, bool *r_nil_is_variant) {
    switch (p_type.type_encoding) {
        case MONO_TYPE_BOOLEAN:
            return VariantType::BOOL;

        case MONO_TYPE_I1:
            return VariantType::INT;
        case MONO_TYPE_I2:
            return VariantType::INT;
        case MONO_TYPE_I4:
            return VariantType::INT;
        case MONO_TYPE_I8:
            return VariantType::INT;

        case MONO_TYPE_U1:
            return VariantType::INT;
        case MONO_TYPE_U2:
            return VariantType::INT;
        case MONO_TYPE_U4:
            return VariantType::INT;
        case MONO_TYPE_U8:
            return VariantType::INT;

        case MONO_TYPE_R4:
            return VariantType::FLOAT;
        case MONO_TYPE_R8:
            return VariantType::FLOAT;

        case MONO_TYPE_STRING: {
            return VariantType::STRING;
        } break;

        case MONO_TYPE_VALUETYPE: {
            GDMonoClass *vtclass = p_type.type_class;

            if (vtclass == CACHED_CLASS(Vector2)) {
                return VariantType::VECTOR2;
            }

            if (vtclass == CACHED_CLASS(Rect2))
                return VariantType::RECT2;

            if (vtclass == CACHED_CLASS(Transform2D))
                return VariantType::TRANSFORM2D;

            if (vtclass == CACHED_CLASS(Vector3))
                return VariantType::VECTOR3;

            if (vtclass == CACHED_CLASS(Basis))
                return VariantType::BASIS;

            if (vtclass == CACHED_CLASS(Quat)) {
                return VariantType::QUAT;
            }

            if (vtclass == CACHED_CLASS(Transform)) {
                return VariantType::TRANSFORM;
            }

            if (vtclass == CACHED_CLASS(AABB)) {
                return VariantType::AABB;
            }

            if (vtclass == CACHED_CLASS(Color)) {
                return VariantType::COLOR;
            }

            if (vtclass == CACHED_CLASS(Plane)) {
                return VariantType::PLANE;
            }

            if (vtclass == CACHED_CLASS(Callable)) {
                return VariantType::CALLABLE;
            }

            if (vtclass == CACHED_CLASS(SignalInfo)) {
                return VariantType::SIGNAL;
            }
            if (mono_class_is_enum(vtclass->get_mono_ptr())) {
                return VariantType::INT;
            }
        } break;

        case MONO_TYPE_ARRAY:
        case MONO_TYPE_SZARRAY: {
            MonoArrayType *array_type = mono_type_get_array_type(p_type.type_class->get_mono_type());

            if (array_type->eklass == CACHED_CLASS_RAW(MonoObject)) {
                return VariantType::ARRAY;
            }

            if (array_type->eklass == CACHED_CLASS_RAW(uint8_t))
                return VariantType::POOL_BYTE_ARRAY;

            if (array_type->eklass == CACHED_CLASS_RAW(int32_t))
                return VariantType::POOL_INT_ARRAY;

            if (array_type->eklass == CACHED_CLASS_RAW(float))
                return VariantType::POOL_FLOAT32_ARRAY;

            if (array_type->eklass == CACHED_CLASS_RAW(String))
                return VariantType::POOL_STRING_ARRAY;

            if (array_type->eklass == CACHED_CLASS_RAW(Vector2))
                return VariantType::POOL_VECTOR2_ARRAY;

            if (array_type->eklass == CACHED_CLASS_RAW(Vector3))
                return VariantType::POOL_VECTOR3_ARRAY;

            if (array_type->eklass == CACHED_CLASS_RAW(Color))
                return VariantType::POOL_COLOR_ARRAY;

            GDMonoClass *array_type_class = GDMono::get_singleton()->get_class(array_type->eklass);
            if (CACHED_CLASS(GodotObject)->is_assignable_from(array_type_class)) {
                return VariantType::ARRAY;
            }
        } break;

        case MONO_TYPE_CLASS: {
            GDMonoClass *type_class = p_type.type_class;

            // GodotObject
            if (CACHED_CLASS(GodotObject)->is_assignable_from(type_class)) {
                return VariantType::OBJECT;
            }

            if (CACHED_CLASS(StringName) == type_class) {
                return VariantType::STRING_NAME;
            }

            if (CACHED_CLASS(NodePath) == type_class) {
                return VariantType::NODE_PATH;
            }

            if (CACHED_CLASS(RID) == type_class) {
                return VariantType::_RID;
            }

            if (CACHED_CLASS(Dictionary) == type_class) {
                return VariantType::DICTIONARY;
            }

            if (CACHED_CLASS(Array) == type_class) {
                return VariantType::ARRAY;
            }

            // IDictionary
            if (p_type.type_class == CACHED_CLASS(System_Collections_IDictionary)) {
                return VariantType::DICTIONARY;
            }

            // ICollection or IEnumerable
            if (p_type.type_class == CACHED_CLASS(System_Collections_ICollection) ||
                    p_type.type_class == CACHED_CLASS(System_Collections_IEnumerable)) {
                return VariantType::ARRAY;
            }
        } break;

        case MONO_TYPE_OBJECT: {
            if (r_nil_is_variant) {
                *r_nil_is_variant = true;
            }
            return VariantType::NIL;
        } break;

        case MONO_TYPE_GENERICINST: {
            MonoReflectionType *reftype = mono_type_get_object(mono_domain_get(), p_type.type_class->get_mono_type());

            // Godot.Collections.Dictionary<TKey, TValue>
            if (GDMonoUtils::Marshal::type_is_generic_dictionary(reftype)) {
                return VariantType::DICTIONARY;
            }

            // Godot.Collections.Array<T>
            if (GDMonoUtils::Marshal::type_is_generic_array(reftype)) {
                return VariantType::ARRAY;
            }

            // System.Collections.Generic.Dictionary<TKey, TValue>
            if (GDMonoUtils::Marshal::type_is_system_generic_dictionary(reftype)) {
                return VariantType::DICTIONARY;
            }

            // System.Collections.Generic.List<T>
            if (GDMonoUtils::Marshal::type_is_system_generic_list(reftype)) {
                return VariantType::ARRAY;
            }

            // IDictionary<TKey, TValue>
            if (GDMonoUtils::Marshal::type_is_generic_idictionary(reftype)) {
                return VariantType::DICTIONARY;
            }

            // ICollection<T> or IEnumerable<T>
            if (GDMonoUtils::Marshal::type_is_generic_icollection(reftype) || GDMonoUtils::Marshal::type_is_generic_ienumerable(reftype)) {
                return VariantType::ARRAY;
            }
        } break;

        default: {
        } break;
    }

    if (r_nil_is_variant) {
        *r_nil_is_variant = false;
    }
    // Unknown
    return VariantType::NIL;
}

bool try_get_array_element_type(const ManagedType &p_array_type, ManagedType &r_elem_type) {
    switch (p_array_type.type_encoding) {
        case MONO_TYPE_ARRAY:
        case MONO_TYPE_SZARRAY: {
            MonoArrayType *array_type = mono_type_get_array_type(p_array_type.type_class->get_mono_type());
            GDMonoClass *array_type_class = GDMono::get_singleton()->get_class(array_type->eklass);
            r_elem_type = ManagedType::from_class(array_type_class);
            return true;
        } break;
        case MONO_TYPE_GENERICINST: {
            MonoReflectionType *array_reftype = mono_type_get_object(mono_domain_get(), p_array_type.type_class->get_mono_type());

            if (GDMonoUtils::Marshal::type_is_generic_array(array_reftype) ||
                    GDMonoUtils::Marshal::type_is_system_generic_list(array_reftype) ||
                    GDMonoUtils::Marshal::type_is_generic_icollection(array_reftype) ||
                    GDMonoUtils::Marshal::type_is_generic_ienumerable(array_reftype)) {
                MonoReflectionType *elem_reftype;

                GDMonoUtils::Marshal::array_get_element_type(array_reftype, &elem_reftype);

                r_elem_type = ManagedType::from_reftype(elem_reftype);
                return true;
            }
        } break;
        default: {
        } break;
    }

    return false;
}

MonoString *variant_to_mono_string(const Variant &p_var) {
    if (p_var.get_type() == VariantType::NIL) {
        return nullptr; // Otherwise, Variant -> String would return the string "Null"
    }
    return mono_string_from_godot(p_var.as<String>());
}
MonoArray *variant_to_mono_array(const Variant &p_var, GDMonoClass *p_type_class) {
    MonoArrayType *array_type = mono_type_get_array_type(p_type_class->get_mono_type());

    if (array_type->eklass == CACHED_CLASS_RAW(MonoObject)) {
        return container_to_mono_array(p_var.as<Array>());
    }

    if (array_type->eklass == CACHED_CLASS_RAW(uint8_t)) {
        return container_to_mono_array(p_var.as<PoolByteArray>());
    }

    if (array_type->eklass == CACHED_CLASS_RAW(int32_t)) {
        return container_to_mono_array(p_var.as<PoolIntArray>());
    }

    // if (array_type->eklass == CACHED_CLASS_RAW(int64_t)) {
    //     return container_to_mono_array(p_var.as<PoolInt64Array>());
    // }

    if (array_type->eklass == CACHED_CLASS_RAW(float)) {
        return container_to_mono_array(p_var.as<PoolRealArray>());
    }

    // if (array_type->eklass == CACHED_CLASS_RAW(double)) {
    //     return container_to_mono_array(p_var.as<PoolFloat64Array>());
    // }

    if (array_type->eklass == CACHED_CLASS_RAW(String)) {
        return container_to_mono_array(p_var.as<PoolStringArray>());
    }

    if (array_type->eklass == CACHED_CLASS_RAW(Vector2)) {
        return container_to_mono_array(p_var.as<PoolVector2Array>());
    }

    if (array_type->eklass == CACHED_CLASS_RAW(Vector3)) {
        return container_to_mono_array(p_var.as<PoolVector3Array>());
    }

    if (array_type->eklass == CACHED_CLASS_RAW(Color)) {
        return container_to_mono_array(p_var.as<PoolColorArray>());
    }

    if (mono_class_is_assignable_from(CACHED_CLASS(GodotObject)->get_mono_ptr(), array_type->eklass)) {
        return container_to_mono_array(p_var.as<Array>(), array_type->eklass);
    }

    ERR_FAIL_V_MSG(nullptr, "Attempted to convert Variant to array of unsupported element type:" +
                                    GDMonoClass::get_full_name(array_type->eklass) + "'.");
}
MonoObject *variant_to_mono_object_of_class(const Variant &p_var, GDMonoClass *p_type_class) {
    // GodotObject
    if (CACHED_CLASS(GodotObject)->is_assignable_from(p_type_class)) {
        return GDMonoUtils::unmanaged_get_managed(p_var.as<Object*>());
    }

    if (CACHED_CLASS(StringName) == p_type_class) {
        return GDMonoUtils::create_managed_from(p_var.as<StringName>());
    }

    if (CACHED_CLASS(NodePath) == p_type_class) {
        return GDMonoUtils::create_managed_from(p_var.as<NodePath>());
    }

    if (CACHED_CLASS(RID) == p_type_class) {
        return GDMonoUtils::create_managed_from(p_var.operator ::RID());
    }

    // Godot.Collections.Dictionary or IDictionary
    if (CACHED_CLASS(Dictionary) == p_type_class || CACHED_CLASS(System_Collections_IDictionary) == p_type_class) {
        return GDMonoUtils::create_managed_from(p_var.as<Dictionary>(), CACHED_CLASS(Dictionary));
    }

    // Godot.Collections.Array or ICollection or IEnumerable
    if (CACHED_CLASS(Array) == p_type_class ||
            CACHED_CLASS(System_Collections_ICollection) == p_type_class ||
            CACHED_CLASS(System_Collections_IEnumerable) == p_type_class) {
        return GDMonoUtils::create_managed_from(p_var.as<Array>(), CACHED_CLASS(Array));
    }

    ERR_FAIL_V_MSG(nullptr, "Attempted to convert Variant to unsupported type: '" +
                                    p_type_class->get_full_name() + "'.");
}

MonoObject *variant_to_mono_object_of_genericinst(const Variant &p_var, GDMonoClass *p_type_class) {
    MonoReflectionType *reftype = mono_type_get_object(mono_domain_get(), p_type_class->get_mono_type());

    // Godot.Collections.Dictionary<TKey, TValue>
    if (GDMonoUtils::Marshal::type_is_generic_dictionary(reftype)) {
        return GDMonoUtils::create_managed_from(p_var.as<Dictionary>(), p_type_class);
    }

    // Godot.Collections.Array<T>
    if (GDMonoUtils::Marshal::type_is_generic_array(reftype)) {
        return GDMonoUtils::create_managed_from(p_var.as<Array>(), p_type_class);
    }

    // System.Collections.Generic.Dictionary<TKey, TValue>
    if (GDMonoUtils::Marshal::type_is_system_generic_dictionary(reftype)) {
        MonoReflectionType *key_reftype = nullptr;
        MonoReflectionType *value_reftype = nullptr;
        GDMonoUtils::Marshal::dictionary_get_key_value_types(reftype, &key_reftype, &value_reftype);
        return Dictionary_to_system_generic_dict(p_var.as<Dictionary>(), p_type_class, key_reftype, value_reftype);
    }

    // System.Collections.Generic.List<T>
    if (GDMonoUtils::Marshal::type_is_system_generic_list(reftype)) {
        MonoReflectionType *elem_reftype = nullptr;
        GDMonoUtils::Marshal::array_get_element_type(reftype, &elem_reftype);
        return Array_to_system_generic_list(p_var.as<Array>(), p_type_class, elem_reftype);
    }

    // IDictionary<TKey, TValue>
    if (GDMonoUtils::Marshal::type_is_generic_idictionary(reftype)) {
        MonoReflectionType *key_reftype;
        MonoReflectionType *value_reftype;
        GDMonoUtils::Marshal::dictionary_get_key_value_types(reftype, &key_reftype, &value_reftype);
        GDMonoClass *godot_dict_class = GDMonoUtils::Marshal::make_generic_dictionary_type(key_reftype, value_reftype);

        return GDMonoUtils::create_managed_from(p_var.as<Dictionary>(), godot_dict_class);
    }

    // ICollection<T> or IEnumerable<T>
    if (GDMonoUtils::Marshal::type_is_generic_icollection(reftype) || GDMonoUtils::Marshal::type_is_generic_ienumerable(reftype)) {
        MonoReflectionType *elem_reftype;
        GDMonoUtils::Marshal::array_get_element_type(reftype, &elem_reftype);
        GDMonoClass *godot_array_class = GDMonoUtils::Marshal::make_generic_array_type(elem_reftype);

        return GDMonoUtils::create_managed_from(p_var.as<Array>(), godot_array_class);
    }

    ERR_FAIL_V_MSG(nullptr, "Attempted to convert Variant to unsupported generic type: '" +
                                    p_type_class->get_full_name() + "'.");
}

MonoObject *variant_to_mono_object(const Variant &p_var) {
    // Variant
    switch (p_var.get_type()) {
        case VariantType::BOOL: {
            MonoBoolean val = p_var.as<bool>();
            return BOX_BOOLEAN(val);
        }
        case VariantType::INT: {
            int64_t val = p_var.as<int64_t>();
            bool in64bit_range = (val >= eastl::numeric_limits<int32_t>::max()) || (val < eastl::numeric_limits<int32_t>::min());
            return in64bit_range ? BOX_INT64(val) : BOX_INT32(val);
        }
        case VariantType::FLOAT: {
#ifdef REAL_T_IS_DOUBLE
            double val = p_var.as<double>();
            return BOX_DOUBLE(val);
#else
            float val = p_var.as<float>();
            return BOX_FLOAT(val);
#endif
        }
        case VariantType::STRING:
            return (MonoObject *)mono_string_from_godot(p_var.as<String>());
        case VariantType::VECTOR2: {
            GDMonoMarshal::M_Vector2 from = MARSHALLED_OUT(Vector2, p_var.operator ::Vector2());
            return mono_value_box(mono_domain_get(), CACHED_CLASS_RAW(Vector2), &from);
        }
        // case VariantType::VECTOR2I: {
        //     GDMonoMarshal::M_Vector2i from = MARSHALLED_OUT(Vector2i, p_var.operator ::Vector2i());
        //     return mono_value_box(mono_domain_get(), CACHED_CLASS_RAW(Vector2i), &from);
        // }
        case VariantType::RECT2: {
            GDMonoMarshal::M_Rect2 from = MARSHALLED_OUT(Rect2, p_var.operator ::Rect2());
            return mono_value_box(mono_domain_get(), CACHED_CLASS_RAW(Rect2), &from);
        }
        // case VariantType::RECT2I: {
        //     GDMonoMarshal::M_Rect2i from = MARSHALLED_OUT(Rect2i, p_var.operator ::Rect2i());
        //     return mono_value_box(mono_domain_get(), CACHED_CLASS_RAW(Rect2i), &from);
        // }
        case VariantType::VECTOR3: {
            GDMonoMarshal::M_Vector3 from = MARSHALLED_OUT(Vector3, p_var.operator ::Vector3());
            return mono_value_box(mono_domain_get(), CACHED_CLASS_RAW(Vector3), &from);
        }
        // case VariantType::VECTOR3I: {
        //     GDMonoMarshal::M_Vector3i from = MARSHALLED_OUT(Vector3i, p_var.operator ::Vector3i());
        //     return mono_value_box(mono_domain_get(), CACHED_CLASS_RAW(Vector3i), &from);
        // }
        case VariantType::TRANSFORM2D: {
            GDMonoMarshal::M_Transform2D from = MARSHALLED_OUT(Transform2D, p_var.operator ::Transform2D());
            return mono_value_box(mono_domain_get(), CACHED_CLASS_RAW(Transform2D), &from);
        }
        case VariantType::PLANE: {
            GDMonoMarshal::M_Plane from = MARSHALLED_OUT(Plane, p_var.operator ::Plane());
            return mono_value_box(mono_domain_get(), CACHED_CLASS_RAW(Plane), &from);
        }
        case VariantType::QUAT: {
            GDMonoMarshal::M_Quat from = MARSHALLED_OUT(Quat, p_var.operator ::Quat());
            return mono_value_box(mono_domain_get(), CACHED_CLASS_RAW(Quat), &from);
        }
        case VariantType::AABB: {
            GDMonoMarshal::M_AABB from = MARSHALLED_OUT(AABB, p_var.operator ::AABB());
            return mono_value_box(mono_domain_get(), CACHED_CLASS_RAW(AABB), &from);
        }
        case VariantType::BASIS: {
            GDMonoMarshal::M_Basis from = MARSHALLED_OUT(Basis, p_var.operator ::Basis());
            return mono_value_box(mono_domain_get(), CACHED_CLASS_RAW(Basis), &from);
        }
        case VariantType::TRANSFORM: {
            GDMonoMarshal::M_Transform from = MARSHALLED_OUT(Transform, p_var.operator ::Transform());
            return mono_value_box(mono_domain_get(), CACHED_CLASS_RAW(Transform), &from);
        }
        case VariantType::COLOR: {
            GDMonoMarshal::M_Color from = MARSHALLED_OUT(Color, p_var.operator ::Color());
            return mono_value_box(mono_domain_get(), CACHED_CLASS_RAW(Color), &from);
        }
        case VariantType::STRING_NAME:
            return GDMonoUtils::create_managed_from(p_var.as<StringName>());
        case VariantType::NODE_PATH:
            return GDMonoUtils::create_managed_from(p_var.as<NodePath>());
        case VariantType::_RID:
            return GDMonoUtils::create_managed_from(p_var.operator ::RID());
        case VariantType::OBJECT:
            return GDMonoUtils::unmanaged_get_managed(p_var.as<Object*>());
        case VariantType::CALLABLE: {
            GDMonoMarshal::M_Callable from = GDMonoMarshal::callable_to_managed(p_var.as<Callable>());
            return mono_value_box(mono_domain_get(), CACHED_CLASS_RAW(Callable), &from);
        }
        case VariantType::SIGNAL: {
            GDMonoMarshal::M_SignalInfo from = GDMonoMarshal::signal_info_to_managed(p_var.as<Signal>());
            return mono_value_box(mono_domain_get(), CACHED_CLASS_RAW(SignalInfo), &from);
        }
        case VariantType::DICTIONARY:
            return GDMonoUtils::create_managed_from(p_var.as<Dictionary>(), CACHED_CLASS(Dictionary));
        case VariantType::ARRAY:
            return GDMonoUtils::create_managed_from(p_var.as<Array>(), CACHED_CLASS(Array));
        case VariantType::POOL_BYTE_ARRAY:
            return (MonoObject *)container_to_mono_array(p_var.as<PoolByteArray>());
        case VariantType::POOL_INT_ARRAY:
            return (MonoObject *)container_to_mono_array(p_var.as<PoolIntArray>());
        // case VariantType::POOL_INT64_ARRAY:
        //     return (MonoObject *)container_to_mono_array(p_var.as<PoolInt64Array>());
        case VariantType::POOL_FLOAT32_ARRAY:
            return (MonoObject *)container_to_mono_array(p_var.as<PoolRealArray>());
        // case VariantType::POOL_FLOAT64_ARRAY:
        //     return (MonoObject *)PoolFloat64Array_to_mono_array(p_var.as<PoolFloat64Array>());
        case VariantType::POOL_STRING_ARRAY:
            return (MonoObject *)container_to_mono_array(p_var.as<PoolStringArray>());
        case VariantType::POOL_VECTOR2_ARRAY:
            return (MonoObject *)container_to_mono_array(p_var.as<PoolVector2Array>());
        case VariantType::POOL_VECTOR3_ARRAY:
            return (MonoObject *)container_to_mono_array(p_var.as<PoolVector3Array>());
        case VariantType::POOL_COLOR_ARRAY:
            return (MonoObject *)container_to_mono_array(p_var.as<PoolColorArray>());
        default:
            return nullptr;
    }
}

String mono_to_utf8_string(MonoString *p_mono_string) {
    MonoError error;
    char *utf8 = mono_string_to_utf8_checked(p_mono_string, &error);

    if (!mono_error_ok(&error)) {
        ERR_PRINT(String() + "Failed to convert MonoString* to UTF-8: '" + mono_error_get_message(&error) + "'.");
        mono_error_cleanup(&error);
        return String();
    }

    String ret = String(utf8);

    mono_free(utf8);

    return ret;
}

UIString mono_to_utf16_string(MonoString *p_mono_string) {
    int len = mono_string_length(p_mono_string);
    UIString ret;

    if (len == 0)
        return ret;
#ifdef _WIN32
    return UIString::fromWCharArray(mono_string_chars(p_mono_string),len);
#else
    return UIString::fromUtf16(mono_string_chars(p_mono_string),len);
#endif
}


size_t variant_get_managed_unboxed_size(const ManagedType& p_type) {
    // This method prints no errors for unsupported types. It's called on all methods, not only
    // those that end up being invoked with Variant parameters.

    // For MonoObject* we return 0, as it doesn't need to be stored.
    constexpr size_t zero_for_mono_object = 0;

    switch (p_type.type_encoding) {
    case MONO_TYPE_BOOLEAN:
        return sizeof(MonoBoolean);
    case MONO_TYPE_CHAR:
        return sizeof(uint16_t);
    case MONO_TYPE_I1:
        return sizeof(int8_t);
    case MONO_TYPE_I2:
        return sizeof(int16_t);
    case MONO_TYPE_I4:
        return sizeof(int32_t);
    case MONO_TYPE_I8:
        return sizeof(int64_t);
    case MONO_TYPE_U1:
        return sizeof(uint8_t);
    case MONO_TYPE_U2:
        return sizeof(uint16_t);
    case MONO_TYPE_U4:
        return sizeof(uint32_t);
    case MONO_TYPE_U8:
        return sizeof(uint64_t);
    case MONO_TYPE_R4:
        return sizeof(float);
    case MONO_TYPE_R8:
        return sizeof(double);
    case MONO_TYPE_VALUETYPE: {
        GDMonoClass* vtclass = p_type.type_class;

#define RETURN_CHECK_FOR_STRUCT(m_struct)    \
    if (vtclass == CACHED_CLASS(m_struct)) { \
        return sizeof(M_##m_struct);         \
    }

        RETURN_CHECK_FOR_STRUCT(Vector2);
        //RETURN_CHECK_FOR_STRUCT(Vector2i);
        RETURN_CHECK_FOR_STRUCT(Rect2);
        //RETURN_CHECK_FOR_STRUCT(Rect2i);
        RETURN_CHECK_FOR_STRUCT(Transform2D);
        RETURN_CHECK_FOR_STRUCT(Vector3);
        //RETURN_CHECK_FOR_STRUCT(Vector3i);
        RETURN_CHECK_FOR_STRUCT(Basis);
        RETURN_CHECK_FOR_STRUCT(Quat);
        RETURN_CHECK_FOR_STRUCT(Transform);
        RETURN_CHECK_FOR_STRUCT(AABB);
        RETURN_CHECK_FOR_STRUCT(Color);
        RETURN_CHECK_FOR_STRUCT(Plane);
        RETURN_CHECK_FOR_STRUCT(Callable);
        //RETURN_CHECK_FOR_STRUCT(SignalInfo);

#undef RETURN_CHECK_FOR_STRUCT

        if (mono_class_is_enum(vtclass->get_mono_ptr())) {
            MonoType* enum_basetype = mono_class_enum_basetype(vtclass->get_mono_ptr());
            switch (mono_type_get_type(enum_basetype)) {
            case MONO_TYPE_BOOLEAN:
                return sizeof(MonoBoolean);
            case MONO_TYPE_CHAR:
                return sizeof(uint16_t);
            case MONO_TYPE_I1:
                return sizeof(int8_t);
            case MONO_TYPE_I2:
                return sizeof(int16_t);
            case MONO_TYPE_I4:
                return sizeof(int32_t);
            case MONO_TYPE_I8:
                return sizeof(int64_t);
            case MONO_TYPE_U1:
                return sizeof(uint8_t);
            case MONO_TYPE_U2:
                return sizeof(uint16_t);
            case MONO_TYPE_U4:
                return sizeof(uint32_t);
            case MONO_TYPE_U8:
                return sizeof(uint64_t);
            default: {
                // Enum with unsupported base type. We return nullptr MonoObject* on error.
                return zero_for_mono_object;
            }
            }
        }

        // Enum with unsupported value type. We return nullptr MonoObject* on error.
    } break;
    case MONO_TYPE_STRING:
        return zero_for_mono_object;
    case MONO_TYPE_ARRAY:
    case MONO_TYPE_SZARRAY:
    case MONO_TYPE_CLASS:
    case MONO_TYPE_GENERICINST:
        return zero_for_mono_object;
    case MONO_TYPE_OBJECT:
        return zero_for_mono_object;
    }

    // Unsupported type encoding. We return nullptr MonoObject* on error.
    return zero_for_mono_object;
}
void *variant_to_managed_unboxed(const Variant &p_var, const ManagedType &p_type, void *r_buffer, unsigned int &r_offset) {
#define RETURN_TYPE_VAL(m_type, m_val)             \
    *reinterpret_cast<m_type *>(r_buffer) = m_val; \
    r_offset += sizeof(m_type);                    \
    return r_buffer;

    switch (p_type.type_encoding) {
        case MONO_TYPE_BOOLEAN:
            RETURN_TYPE_VAL(MonoBoolean, (MonoBoolean)p_var.as<bool>());
        case MONO_TYPE_CHAR:
            RETURN_TYPE_VAL(uint16_t, p_var.as<unsigned short> ());
        case MONO_TYPE_I1:
            RETURN_TYPE_VAL(int8_t, p_var.as<signed char>());
        case MONO_TYPE_I2:
            RETURN_TYPE_VAL(int16_t, p_var.as<signed short>());
        case MONO_TYPE_I4:
            RETURN_TYPE_VAL(int32_t, p_var.as<signed int>());
        case MONO_TYPE_I8:
            RETURN_TYPE_VAL(int64_t, p_var.as<int64_t>());
        case MONO_TYPE_U1:
            RETURN_TYPE_VAL(uint8_t, p_var.as<unsigned char>());
        case MONO_TYPE_U2:
            RETURN_TYPE_VAL(uint16_t, p_var.as<unsigned short>());
        case MONO_TYPE_U4:
            RETURN_TYPE_VAL(uint32_t, p_var.as<unsigned int>());
        case MONO_TYPE_U8:
            RETURN_TYPE_VAL(uint64_t, p_var.as<uint64_t>());
        case MONO_TYPE_R4:
            RETURN_TYPE_VAL(float, p_var.as<float>());
        case MONO_TYPE_R8:
            RETURN_TYPE_VAL(double, p_var.as<double>());
        case MONO_TYPE_VALUETYPE: {
            GDMonoClass *vtclass = p_type.type_class;

#define RETURN_CHECK_FOR_STRUCT(m_struct)                                                         \
    if (vtclass == CACHED_CLASS(m_struct)) {                                                      \
        GDMonoMarshal::M_##m_struct from = MARSHALLED_OUT(m_struct, p_var.as<m_struct>()); \
        RETURN_TYPE_VAL(M_##m_struct, from);                                                      \
    }

            RETURN_CHECK_FOR_STRUCT(Vector2);
            //RETURN_CHECK_FOR_STRUCT(Vector2i);
            RETURN_CHECK_FOR_STRUCT(Rect2);
            //RETURN_CHECK_FOR_STRUCT(Rect2i);
            RETURN_CHECK_FOR_STRUCT(Transform2D);
            RETURN_CHECK_FOR_STRUCT(Vector3);
            //RETURN_CHECK_FOR_STRUCT(Vector3i);
            RETURN_CHECK_FOR_STRUCT(Basis);
            RETURN_CHECK_FOR_STRUCT(Quat);
            RETURN_CHECK_FOR_STRUCT(Transform);
            RETURN_CHECK_FOR_STRUCT(AABB);
            RETURN_CHECK_FOR_STRUCT(Color);
            RETURN_CHECK_FOR_STRUCT(Plane);

#undef RETURN_CHECK_FOR_STRUCT

            if (vtclass == CACHED_CLASS(Callable)) {
                GDMonoMarshal::M_Callable from = GDMonoMarshal::callable_to_managed(p_var.as<Callable>());
                RETURN_TYPE_VAL(M_Callable, from);
            }

            if (vtclass == CACHED_CLASS(SignalInfo)) {
                GDMonoMarshal::M_SignalInfo from = GDMonoMarshal::signal_info_to_managed(p_var.as<Signal>());
                RETURN_TYPE_VAL(M_SignalInfo, from);
            }

            if (mono_class_is_enum(vtclass->get_mono_ptr())) {
                MonoType *enum_basetype = mono_class_enum_basetype(vtclass->get_mono_ptr());
                switch (mono_type_get_type(enum_basetype)) {
                    case MONO_TYPE_BOOLEAN: {
                        MonoBoolean val = p_var.as<bool>();
                        RETURN_TYPE_VAL(MonoBoolean, val);
                    }
                    case MONO_TYPE_CHAR: {
                        uint16_t val = p_var.as<unsigned short>();
                        RETURN_TYPE_VAL(uint16_t, val);
                    }
                    case MONO_TYPE_I1: {
                        int8_t val = p_var.as<signed char>();
                        RETURN_TYPE_VAL(int8_t, val);
                    }
                    case MONO_TYPE_I2: {
                        int16_t val = p_var.as<signed short>();
                        RETURN_TYPE_VAL(int16_t, val);
                    }
                    case MONO_TYPE_I4: {
                        int32_t val = p_var.as<signed int>();
                        RETURN_TYPE_VAL(int32_t, val);
                    }
                    case MONO_TYPE_I8: {
                        int64_t val = p_var.as<int64_t>();
                        RETURN_TYPE_VAL(int64_t, val);
                    }
                    case MONO_TYPE_U1: {
                        uint8_t val = p_var.as<unsigned char>();
                        RETURN_TYPE_VAL(uint8_t, val);
                    }
                    case MONO_TYPE_U2: {
                        uint16_t val = p_var.as<unsigned short>();
                        RETURN_TYPE_VAL(uint16_t, val);
                    }
                    case MONO_TYPE_U4: {
                        uint32_t val = p_var.as<unsigned int>();
                        RETURN_TYPE_VAL(uint32_t, val);
                    }
                    case MONO_TYPE_U8: {
                        uint64_t val = p_var.as<uint64_t>();
                        RETURN_TYPE_VAL(uint64_t, val);
                    }
                    default: {
                        ERR_FAIL_V_MSG(nullptr, "Attempted to convert Variant to enum value of unsupported base type: '" +
                                                        GDMonoClass::get_full_name(mono_class_from_mono_type(enum_basetype)) + "'.");
                    }
                }
            }

            ERR_FAIL_V_MSG(nullptr, "Attempted to convert Variant to unsupported value type: '" +
                                            p_type.type_class->get_full_name() + "'.");
        } break;
#undef RETURN_TYPE_VAL
        case MONO_TYPE_STRING:
            return variant_to_mono_string(p_var);
        case MONO_TYPE_ARRAY:
        case MONO_TYPE_SZARRAY:
            return variant_to_mono_array(p_var, p_type.type_class);
        case MONO_TYPE_CLASS:
            return variant_to_mono_object_of_class(p_var, p_type.type_class);
        case MONO_TYPE_GENERICINST:
            return variant_to_mono_object_of_genericinst(p_var, p_type.type_class);
        case MONO_TYPE_OBJECT:
            return variant_to_mono_object(p_var);
    }

    ERR_FAIL_V_MSG(nullptr, "Attempted to convert Variant to unsupported type with encoding: " +
                                    itos(p_type.type_encoding) + ".");
}

MonoObject *variant_to_mono_object(const Variant *p_var, const ManagedType &p_type) {
    switch (p_type.type_encoding) {
        case MONO_TYPE_BOOLEAN: {
            MonoBoolean val = p_var->as<bool>();
            return BOX_BOOLEAN(val);
        }

        case MONO_TYPE_CHAR: {
            uint16_t val = p_var->as<unsigned short>();
            return BOX_UINT16(val);
        }

        case MONO_TYPE_I1: {
            int8_t val = p_var->as<signed char>();
            return BOX_INT8(val);
        }
        case MONO_TYPE_I2: {
            int16_t val = p_var->as<signed short>();
            return BOX_INT16(val);
        }
        case MONO_TYPE_I4: {
            int32_t val = p_var->as<signed int>();
            return BOX_INT32(val);
        }
        case MONO_TYPE_I8: {
            int64_t val = p_var->as<int64_t>();
            return BOX_INT64(val);
        }

        case MONO_TYPE_U1: {
            uint8_t val = p_var->as<unsigned char>();
            return BOX_UINT8(val);
        }
        case MONO_TYPE_U2: {
            uint16_t val = p_var->as<unsigned short>();
            return BOX_UINT16(val);
        }
        case MONO_TYPE_U4: {
            uint32_t val = p_var->as<unsigned int>();
            return BOX_UINT32(val);
        }
        case MONO_TYPE_U8: {
            uint64_t val = p_var->as<uint64_t>();
            return BOX_UINT64(val);
        }

        case MONO_TYPE_R4: {
            float val = p_var->as<float>();
            return BOX_FLOAT(val);
        }
        case MONO_TYPE_R8: {
            double val = p_var->as<double>();
            return BOX_DOUBLE(val);
        }

        case MONO_TYPE_STRING: {
            if (p_var->get_type() == VariantType::NIL) {
                return nullptr; // Otherwise, Variant -> String would return the string "Null"
            }
            return (MonoObject *)mono_string_from_godot(p_var->as<String>());
        }

        case MONO_TYPE_VALUETYPE: {
            GDMonoClass *vtclass = p_type.type_class;

#define RETURN_CHECK_FOR_STRUCT(m_struct)                                                         \
    if (vtclass == CACHED_CLASS(m_struct)) {                                                      \
        GDMonoMarshal::M_##m_struct from = MARSHALLED_OUT(m_struct, p_var->as<m_struct>()); \
        return mono_value_box(mono_domain_get(), CACHED_CLASS_RAW(m_struct), &from);              \
            }

            RETURN_CHECK_FOR_STRUCT(Vector2);
            //RETURN_CHECK_FOR_STRUCT(Vector2i);
            RETURN_CHECK_FOR_STRUCT(Rect2);
            //RETURN_CHECK_FOR_STRUCT(Rect2i);
            RETURN_CHECK_FOR_STRUCT(Transform2D);
            RETURN_CHECK_FOR_STRUCT(Vector3);
            //RETURN_CHECK_FOR_STRUCT(Vector3i);
            RETURN_CHECK_FOR_STRUCT(Basis);
            RETURN_CHECK_FOR_STRUCT(Quat);
            RETURN_CHECK_FOR_STRUCT(Transform);
            RETURN_CHECK_FOR_STRUCT(AABB);
            RETURN_CHECK_FOR_STRUCT(Color);
            RETURN_CHECK_FOR_STRUCT(Plane);

#undef RETURN_CHECK_FOR_STRUCT

            if (vtclass == CACHED_CLASS(Callable)) {
                GDMonoMarshal::M_Callable from = GDMonoMarshal::callable_to_managed(p_var->as<Callable>());
                return mono_value_box(mono_domain_get(), CACHED_CLASS_RAW(Callable), &from);
            }

            if (vtclass == CACHED_CLASS(SignalInfo)) {
                GDMonoMarshal::M_SignalInfo from = GDMonoMarshal::signal_info_to_managed(p_var->as<Signal>());
                return mono_value_box(mono_domain_get(), CACHED_CLASS_RAW(SignalInfo), &from);
            }
            if (mono_class_is_enum(vtclass->get_mono_ptr())) {
                MonoType *enum_basetype = mono_class_enum_basetype(vtclass->get_mono_ptr());
                MonoClass *enum_baseclass = mono_class_from_mono_type(enum_basetype);
                switch (mono_type_get_type(enum_basetype)) {
                    case MONO_TYPE_BOOLEAN: {
                        MonoBoolean val = p_var->as<bool>();
                        return BOX_ENUM(enum_baseclass, val);
                    }
                    case MONO_TYPE_CHAR: {
                        uint16_t val = p_var->as<unsigned short>();
                        return BOX_ENUM(enum_baseclass, val);
                    }
                    case MONO_TYPE_I1: {
                        int8_t val = p_var->as<signed char>();
                        return BOX_ENUM(enum_baseclass, val);
                    }
                    case MONO_TYPE_I2: {
                        int16_t val = p_var->as<signed short>();
                        return BOX_ENUM(enum_baseclass, val);
                    }
                    case MONO_TYPE_I4: {
                        int32_t val = p_var->as<signed int>();
                        return BOX_ENUM(enum_baseclass, val);
                    }
                    case MONO_TYPE_I8: {
                        int64_t val = p_var->as<int64_t>();
                        return BOX_ENUM(enum_baseclass, val);
                    }
                    case MONO_TYPE_U1: {
                        uint8_t val = p_var->as<unsigned char>();
                        return BOX_ENUM(enum_baseclass, val);
                    }
                    case MONO_TYPE_U2: {
                        uint16_t val = p_var->as<unsigned short>();
                        return BOX_ENUM(enum_baseclass, val);
                    }
                    case MONO_TYPE_U4: {
                        uint32_t val = p_var->as<unsigned int>();
                        return BOX_ENUM(enum_baseclass, val);
                    }
                    case MONO_TYPE_U8: {
                        uint64_t val = p_var->as<uint64_t>();
                        return BOX_ENUM(enum_baseclass, val);
                    }
                    default: {
                        ERR_FAIL_V_MSG(nullptr, "Attempted to convert Variant to enum value of unsupported base type: '" +
                                                        GDMonoClass::get_full_name(enum_baseclass) + "'.");
                    }
                }
            }
        } break;

        case MONO_TYPE_ARRAY:
        case MONO_TYPE_SZARRAY: {
            MonoArrayType *array_type = mono_type_get_array_type(p_type.type_class->get_mono_type());

            if (array_type->eklass == CACHED_CLASS_RAW(MonoObject)) {
                return (MonoObject *)container_to_mono_array(p_var->as<Array>());
            }

            if (array_type->eklass == CACHED_CLASS_RAW(uint8_t))
                return (MonoObject *)container_to_mono_array(p_var->as<PoolByteArray>());

            if (array_type->eklass == CACHED_CLASS_RAW(int32_t))
                return (MonoObject *)container_to_mono_array(p_var->as<PoolIntArray>());

            if (array_type->eklass == CACHED_CLASS_RAW(float))
                return (MonoObject *)container_to_mono_array(p_var->as<PoolRealArray>());

            if (array_type->eklass == CACHED_CLASS_RAW(String))
                return (MonoObject *)container_to_mono_array(p_var->as<PoolStringArray>());

            if (array_type->eklass == CACHED_CLASS_RAW(Vector2))
                return (MonoObject *)container_to_mono_array(p_var->as<PoolVector2Array>());

            if (array_type->eklass == CACHED_CLASS_RAW(Vector3))
                return (MonoObject *)container_to_mono_array(p_var->as<PoolVector3Array>());

            if (array_type->eklass == CACHED_CLASS_RAW(Color))
                return (MonoObject *)container_to_mono_array(p_var->as<PoolColorArray>());

            GDMonoClass *array_type_class = GDMono::get_singleton()->get_class(array_type->eklass);
            if (CACHED_CLASS(GodotObject)->is_assignable_from(array_type_class))
                return (MonoObject *)container_to_mono_array(p_var->as<Array>(), array_type_class);

            ERR_FAIL_V_MSG(nullptr, "Attempted to convert Variant to a managed array of unmarshallable element type.");
            break;
        }

        case MONO_TYPE_CLASS: {
            GDMonoClass *type_class = p_type.type_class;

            // GodotObject
            if (CACHED_CLASS(GodotObject)->is_assignable_from(type_class)) {
                return GDMonoUtils::unmanaged_get_managed(p_var->as<Object *>());
            }
            if (CACHED_CLASS(StringName) == type_class) {
                return GDMonoUtils::create_managed_from(p_var->as<StringName>());
            }
            if (CACHED_CLASS(NodePath) == type_class) {
                return GDMonoUtils::create_managed_from(p_var->as<NodePath>());
            }

            if (CACHED_CLASS(RID) == type_class) {
                return GDMonoUtils::create_managed_from(p_var->as<RID>());
            }

            // Godot.Collections.Dictionary or IDictionary
            if (CACHED_CLASS(Dictionary) == type_class || CACHED_CLASS(System_Collections_IDictionary) == type_class) {
                return GDMonoUtils::create_managed_from(p_var->as<Dictionary>(), CACHED_CLASS(Dictionary));
            }

            // Godot.Collections.Array or ICollection or IEnumerable
            if (CACHED_CLASS(Array) == type_class ||
                    CACHED_CLASS(System_Collections_ICollection) == type_class ||
                    CACHED_CLASS(System_Collections_IEnumerable) == type_class) {
                return GDMonoUtils::create_managed_from(p_var->as<Array>(), CACHED_CLASS(Array));
            }
        } break;
        case MONO_TYPE_OBJECT: {
            // Variant
            switch (p_var->get_type()) {
                case VariantType::BOOL: {
                    MonoBoolean val = p_var->as<bool>();
                    return BOX_BOOLEAN(val);
                }
                case VariantType::INT: {
                    int32_t val = p_var->as<signed int>();
                    return BOX_INT32(val);
                }
                case VariantType::FLOAT: {
#ifdef REAL_T_IS_DOUBLE
                    double val = p_var->as<double>();
                    return BOX_DOUBLE(val);
#else
                    float val = p_var->as<float>();
                    return BOX_FLOAT(val);
#endif
                }
                case VariantType::STRING:
                    return (MonoObject *)mono_string_from_godot(p_var->as<String>());
                case VariantType::VECTOR2: {
                    GDMonoMarshal::M_Vector2 from = MARSHALLED_OUT(Vector2, p_var->as<::Vector2>());
                    return mono_value_box(mono_domain_get(), CACHED_CLASS_RAW(Vector2), &from);
                }
                case VariantType::RECT2: {
                    GDMonoMarshal::M_Rect2 from = MARSHALLED_OUT(Rect2, p_var->as<::Rect2>());
                    return mono_value_box(mono_domain_get(), CACHED_CLASS_RAW(Rect2), &from);
                }
                case VariantType::VECTOR3: {
                    GDMonoMarshal::M_Vector3 from = MARSHALLED_OUT(Vector3, p_var->as<::Vector3>());
                    return mono_value_box(mono_domain_get(), CACHED_CLASS_RAW(Vector3), &from);
                }
                case VariantType::TRANSFORM2D: {
                    GDMonoMarshal::M_Transform2D from = MARSHALLED_OUT(Transform2D, p_var->as<::Transform2D>());
                    return mono_value_box(mono_domain_get(), CACHED_CLASS_RAW(Transform2D), &from);
                }
                case VariantType::PLANE: {
                    GDMonoMarshal::M_Plane from = MARSHALLED_OUT(Plane, p_var->as<::Plane>());
                    return mono_value_box(mono_domain_get(), CACHED_CLASS_RAW(Plane), &from);
                }
                case VariantType::QUAT: {
                    GDMonoMarshal::M_Quat from = MARSHALLED_OUT(Quat, p_var->as<::Quat>());
                    return mono_value_box(mono_domain_get(), CACHED_CLASS_RAW(Quat), &from);
                }
                case VariantType::AABB: {
                    GDMonoMarshal::M_AABB from = MARSHALLED_OUT(AABB, p_var->as<::AABB>());
                    return mono_value_box(mono_domain_get(), CACHED_CLASS_RAW(AABB), &from);
                }
                case VariantType::BASIS: {
                    GDMonoMarshal::M_Basis from = MARSHALLED_OUT(Basis, p_var->as<::Basis>());
                    return mono_value_box(mono_domain_get(), CACHED_CLASS_RAW(Basis), &from);
                }
                case VariantType::TRANSFORM: {
                    GDMonoMarshal::M_Transform from = MARSHALLED_OUT(Transform, p_var->as<::Transform>());
                    return mono_value_box(mono_domain_get(), CACHED_CLASS_RAW(Transform), &from);
                }
                case VariantType::COLOR: {
                    GDMonoMarshal::M_Color from = MARSHALLED_OUT(Color, p_var->as<::Color>());
                    return mono_value_box(mono_domain_get(), CACHED_CLASS_RAW(Color), &from);
                }
                case VariantType::STRING_NAME:
                    return GDMonoUtils::create_managed_from(p_var->as<StringName>());
                case VariantType::NODE_PATH:
                    return GDMonoUtils::create_managed_from(p_var->as<NodePath>());
                case VariantType::_RID:
                    return GDMonoUtils::create_managed_from(p_var->as<RID>());
                case VariantType::OBJECT:
                    return GDMonoUtils::unmanaged_get_managed(p_var->as<Object *>());
                case VariantType::CALLABLE: {
                    GDMonoMarshal::M_Callable from = GDMonoMarshal::callable_to_managed(p_var->as<Callable>());
                    return mono_value_box(mono_domain_get(), CACHED_CLASS_RAW(Callable), &from);
                }
                case VariantType::SIGNAL: {
                    GDMonoMarshal::M_SignalInfo from = GDMonoMarshal::signal_info_to_managed(p_var->as<Signal>());
                    return mono_value_box(mono_domain_get(), CACHED_CLASS_RAW(SignalInfo), &from);
                }
                case VariantType::DICTIONARY:
                    return GDMonoUtils::create_managed_from(p_var->as<Dictionary>(), CACHED_CLASS(Dictionary));
                case VariantType::ARRAY:
                    return GDMonoUtils::create_managed_from(p_var->as<Array>(), CACHED_CLASS(Array));
                case VariantType::POOL_BYTE_ARRAY:
                    return (MonoObject *)container_to_mono_array(p_var->as<PoolByteArray>());
                case VariantType::POOL_INT_ARRAY:
                    return (MonoObject *)container_to_mono_array(p_var->as<PoolIntArray>());
                case VariantType::POOL_FLOAT32_ARRAY:
                    return (MonoObject *)container_to_mono_array(p_var->as<PoolRealArray>());
                case VariantType::POOL_STRING_ARRAY:
                    return (MonoObject *)container_to_mono_array(p_var->as<PoolStringArray>());
                case VariantType::POOL_VECTOR2_ARRAY:
                    return (MonoObject *)container_to_mono_array(p_var->as<PoolVector2Array>());
                case VariantType::POOL_VECTOR3_ARRAY:
                    return (MonoObject *)container_to_mono_array(p_var->as<PoolVector3Array>());
                case VariantType::POOL_COLOR_ARRAY:
                    return (MonoObject *)container_to_mono_array(p_var->as<PoolColorArray>());
                default:
                    return nullptr;
            }
            break;
            case MONO_TYPE_GENERICINST: {
                MonoReflectionType *reftype = mono_type_get_object(mono_domain_get(), p_type.type_class->get_mono_type());

                // Godot.Collections.Dictionary<TKey, TValue>
                if (GDMonoUtils::Marshal::type_is_generic_dictionary(reftype)) {
                    return GDMonoUtils::create_managed_from(p_var->as<Dictionary>(), p_type.type_class);
                }

                // Godot.Collections.Array<T>
                if (GDMonoUtils::Marshal::type_is_generic_array(reftype)) {
                    return GDMonoUtils::create_managed_from(p_var->as<Array>(), p_type.type_class);
                }

                // System.Collections.Generic.Dictionary<TKey, TValue>
                if (GDMonoUtils::Marshal::type_is_system_generic_dictionary(reftype)) {
                    MonoReflectionType *key_reftype = nullptr;
                    MonoReflectionType *value_reftype = nullptr;
                    GDMonoUtils::Marshal::dictionary_get_key_value_types(reftype, &key_reftype, &value_reftype);
                    return Dictionary_to_system_generic_dict(p_var->as<Dictionary>(), p_type.type_class, key_reftype, value_reftype);
                }

                // System.Collections.Generic.List<T>
                if (GDMonoUtils::Marshal::type_is_system_generic_list(reftype)) {
                    MonoReflectionType *elem_reftype = nullptr;
                    GDMonoUtils::Marshal::array_get_element_type(reftype, &elem_reftype);
                    return Array_to_system_generic_list(p_var->as<Array>(), p_type.type_class, elem_reftype);
                }

                // IDictionary<TKey, TValue>
                if (GDMonoUtils::Marshal::type_is_generic_idictionary(reftype)) {
                    MonoReflectionType *key_reftype;
                    MonoReflectionType *value_reftype;
                    GDMonoUtils::Marshal::dictionary_get_key_value_types(reftype, &key_reftype, &value_reftype);
                    GDMonoClass *godot_dict_class = GDMonoUtils::Marshal::make_generic_dictionary_type(key_reftype, value_reftype);

                    return GDMonoUtils::create_managed_from(p_var->as<Dictionary>(), godot_dict_class);
                }

                // ICollection<T> or IEnumerable<T>
                if (GDMonoUtils::Marshal::type_is_generic_icollection(reftype) || GDMonoUtils::Marshal::type_is_generic_ienumerable(reftype)) {
                    MonoReflectionType *elem_reftype;
                    GDMonoUtils::Marshal::array_get_element_type(reftype, &elem_reftype);
                    GDMonoClass *godot_array_class = GDMonoUtils::Marshal::make_generic_array_type(elem_reftype);

                    return GDMonoUtils::create_managed_from(p_var->as<Array>(), godot_array_class);
                }
            } break;
        } break;
    }

    ERR_FAIL_V_MSG(nullptr, "Attempted to convert Variant to an unmarshallable managed type. Name: '" +
                                 p_type.type_class->get_name() + "' Encoding: " + itos(p_type.type_encoding) + ".");
}

Variant mono_object_to_variant_impl(MonoObject *p_obj, const ManagedType &p_type, bool p_fail_with_err = true) {

    ERR_FAIL_COND_V(!p_type.type_class, Variant());

    switch (p_type.type_encoding) {
        case MONO_TYPE_BOOLEAN:
            return (bool)unbox<MonoBoolean>(p_obj);

        case MONO_TYPE_CHAR:
            return unbox<uint16_t>(p_obj);

        case MONO_TYPE_I1:
            return unbox<int8_t>(p_obj);
        case MONO_TYPE_I2:
            return unbox<int16_t>(p_obj);
        case MONO_TYPE_I4:
            return unbox<int32_t>(p_obj);
        case MONO_TYPE_I8:
            return unbox<int64_t>(p_obj);

        case MONO_TYPE_U1:
            return unbox<uint8_t>(p_obj);
        case MONO_TYPE_U2:
            return unbox<uint16_t>(p_obj);
        case MONO_TYPE_U4:
            return unbox<uint32_t>(p_obj);
        case MONO_TYPE_U8:
            return unbox<uint64_t>(p_obj);

        case MONO_TYPE_R4:
            return unbox<float>(p_obj);
        case MONO_TYPE_R8:
            return unbox<double>(p_obj);

        case MONO_TYPE_STRING: {
            if (p_obj == nullptr) {
                return Variant(); // NIL
            }
            return mono_string_to_godot_not_null((MonoString *)p_obj);
        } break;

        case MONO_TYPE_VALUETYPE: {
            GDMonoClass *vtclass = p_type.type_class;

            if (vtclass == CACHED_CLASS(Vector2)) {
                return MARSHALLED_IN(Vector2, unbox_addr<GDMonoMarshal::M_Vector2>(p_obj));
            }

//            if (vtclass == CACHED_CLASS(Vector2i)) {
//                return MARSHALLED_IN(Vector2i, unbox_addr<GDMonoMarshal::M_Vector2i>(p_obj));
//            }

            if (vtclass == CACHED_CLASS(Rect2)) {
                return MARSHALLED_IN(Rect2, unbox_addr<GDMonoMarshal::M_Rect2>(p_obj));
            }

//            if (vtclass == CACHED_CLASS(Rect2i)) {
//                return MARSHALLED_IN(Rect2i, unbox_addr<GDMonoMarshal::M_Rect2i>(p_obj));
//            }

            if (vtclass == CACHED_CLASS(Transform2D)) {
                return MARSHALLED_IN(Transform2D, unbox_addr<GDMonoMarshal::M_Transform2D>(p_obj));
            }

            if (vtclass == CACHED_CLASS(Vector3)) {
                return MARSHALLED_IN(Vector3, unbox_addr<GDMonoMarshal::M_Vector3>(p_obj));
            }

//            if (vtclass == CACHED_CLASS(Vector3i)) {
//                return MARSHALLED_IN(Vector3i, unbox_addr<GDMonoMarshal::M_Vector3i>(p_obj));
//            }

            if (vtclass == CACHED_CLASS(Basis)) {
                return MARSHALLED_IN(Basis, unbox_addr<GDMonoMarshal::M_Basis>(p_obj));
            }

            if (vtclass == CACHED_CLASS(Quat)) {
                return MARSHALLED_IN(Quat, unbox_addr<GDMonoMarshal::M_Quat>(p_obj));
            }

            if (vtclass == CACHED_CLASS(Transform)) {
                return MARSHALLED_IN(Transform, unbox_addr<GDMonoMarshal::M_Transform>(p_obj));
            }

            if (vtclass == CACHED_CLASS(AABB)) {
                return MARSHALLED_IN(AABB, unbox_addr<GDMonoMarshal::M_AABB>(p_obj));
            }

            if (vtclass == CACHED_CLASS(Color)) {
                return MARSHALLED_IN(Color, unbox_addr<GDMonoMarshal::M_Color>(p_obj));
            }

            if (vtclass == CACHED_CLASS(Plane)) {
                return MARSHALLED_IN(Plane, unbox_addr<GDMonoMarshal::M_Plane>(p_obj));
            }

            if (vtclass == CACHED_CLASS(Callable)) {
                return Variant(managed_to_callable(unbox<GDMonoMarshal::M_Callable>(p_obj)));
            }

            if (vtclass == CACHED_CLASS(SignalInfo)) {
                return Variant(managed_to_signal_info(unbox<GDMonoMarshal::M_SignalInfo>(p_obj)));
            }

            if (mono_class_is_enum(vtclass->get_mono_ptr())) {
                return unbox<int32_t>(p_obj);
            }
        } break;

        case MONO_TYPE_ARRAY:
        case MONO_TYPE_SZARRAY: {
            MonoArrayType *array_type = mono_type_get_array_type(p_type.type_class->get_mono_type());

            if (array_type->eklass == CACHED_CLASS_RAW(MonoObject)) {
                return mono_array_to_Array((MonoArray *)p_obj);
            }

            if (array_type->eklass == CACHED_CLASS_RAW(uint8_t))
                return mono_array_to_pool_vec<uint8_t>((MonoArray *)p_obj);

            if (array_type->eklass == CACHED_CLASS_RAW(int32_t))
                return mono_array_to_pool_vec<int32_t>((MonoArray *)p_obj);

            if (array_type->eklass == CACHED_CLASS_RAW(float))
                return mono_array_to_pool_vec<float>((MonoArray *)p_obj);

            if (array_type->eklass == CACHED_CLASS_RAW(String))
                return mono_array_to_pool_vec<String>((MonoArray *)p_obj);

            if (array_type->eklass == CACHED_CLASS_RAW(Vector2))
                return Variant::from(mono_array_to_pool_vec<Vector2>((MonoArray *)p_obj));

            if (array_type->eklass == CACHED_CLASS_RAW(Vector3))
                return mono_array_to_pool_vec<Vector3>((MonoArray *)p_obj);

            if (array_type->eklass == CACHED_CLASS_RAW(Color))
                return mono_array_to_pool_vec<Color>((MonoArray *)p_obj);

            GDMonoClass *array_type_class = GDMono::get_singleton()->get_class(array_type->eklass);
            if (CACHED_CLASS(GodotObject)->is_assignable_from(array_type_class)) {
                return mono_array_to_Array((MonoArray *)p_obj);
            }
            if (p_fail_with_err) {
                ERR_FAIL_V_MSG(Variant(), "Attempted to convert a managed array of unmarshallable element type to Variant.");
            } else {
                return Variant();
            }
        } break;

        case MONO_TYPE_CLASS: {
            GDMonoClass *type_class = p_type.type_class;

            // GodotObject
            if (CACHED_CLASS(GodotObject)->is_assignable_from(type_class)) {
                Object *ptr = unbox<Object *>(CACHED_FIELD(GodotObject, ptr)->get_value(p_obj));
                if (ptr != nullptr) {
                    RefCounted *ref = object_cast<RefCounted>(ptr);
                    return ref ? Variant(Ref<RefCounted>(ref)) : Variant(ptr);
                }
                return Variant();
            }

            if (CACHED_CLASS(StringName) == type_class) {
                StringName *ptr = unbox<StringName *>(CACHED_FIELD(StringName, ptr)->get_value(p_obj));
                return ptr ? Variant(*ptr) : Variant();
            }
            if (CACHED_CLASS(NodePath) == type_class) {
                NodePath *ptr = unbox<NodePath *>(CACHED_FIELD(NodePath, ptr)->get_value(p_obj));
                return ptr ? Variant(*ptr) : Variant();
            }

            if (CACHED_CLASS(RID) == type_class) {
                RID *ptr = unbox<RID *>(CACHED_FIELD(RID, ptr)->get_value(p_obj));
                return ptr ? Variant(*ptr) : Variant();
            }

            // Godot.Collections.Dictionary
            if (CACHED_CLASS(Dictionary) == type_class) {
                MonoException *exc = nullptr;
                Dictionary *ptr = CACHED_METHOD_THUNK(Dictionary, GetPtr).invoke(p_obj, &exc);
                UNHANDLED_EXCEPTION(exc);
                return ptr ? Variant(*ptr) : Variant();
            }

            // Godot.Collections.Array
            if (CACHED_CLASS(Array) == type_class) {
                MonoException *exc = nullptr;
                Array *ptr = CACHED_METHOD_THUNK(Array, GetPtr).invoke(p_obj, &exc);
                UNHANDLED_EXCEPTION(exc);
                return ptr ? Variant(*ptr) : Variant();
            }

        } break;

        case MONO_TYPE_GENERICINST: {
            MonoReflectionType *reftype = mono_type_get_object(mono_domain_get(), p_type.type_class->get_mono_type());

            // Godot.Collections.Dictionary<TKey, TValue>
            if (GDMonoUtils::Marshal::type_is_generic_dictionary(reftype)) {
                MonoException *exc = nullptr;
                MonoObject *ret = p_type.type_class->get_method("GetPtr")->invoke(p_obj, &exc);
                UNHANDLED_EXCEPTION(exc);
                return *unbox<Dictionary *>(ret);
            }

            // Godot.Collections.Array<T>
            if (GDMonoUtils::Marshal::type_is_generic_array(reftype)) {
                MonoException *exc = nullptr;
                MonoObject *ret = p_type.type_class->get_method("GetPtr")->invoke(p_obj, &exc);
                UNHANDLED_EXCEPTION(exc);
                return *unbox<Array *>(ret);
            }

            // System.Collections.Generic.Dictionary<TKey, TValue>
            if (GDMonoUtils::Marshal::type_is_system_generic_dictionary(reftype)) {
                MonoReflectionType *key_reftype = nullptr;
                MonoReflectionType *value_reftype = nullptr;
                GDMonoUtils::Marshal::dictionary_get_key_value_types(reftype, &key_reftype, &value_reftype);
                return system_generic_dict_to_Dictionary(p_obj, p_type.type_class, key_reftype, value_reftype);
            }

            // System.Collections.Generic.List<T>
            if (GDMonoUtils::Marshal::type_is_system_generic_list(reftype)) {
                MonoReflectionType *elem_reftype = nullptr;
                GDMonoUtils::Marshal::array_get_element_type(reftype, &elem_reftype);
                return system_generic_list_to_Array(p_obj, p_type.type_class, elem_reftype);
            }
        } break;
    }

    if (p_fail_with_err) {
        ERR_FAIL_V_MSG(Variant(), "Attempted to convert an unmarshallable managed type to Variant. Name: '" +
                                          p_type.type_class->get_name() + "' Encoding: " + itos(p_type.type_encoding) + ".");
    } else {
        return Variant();
    }
}

GODOT_EXPORT Variant mono_object_to_variant(MonoObject *p_obj) {
    if (!p_obj) {
        return Variant();
    }
    ManagedType type = ManagedType::from_class(mono_object_get_class(p_obj));

    return mono_object_to_variant_impl(p_obj, type);
}

GODOT_EXPORT Variant mono_object_to_variant(MonoObject *p_obj, const ManagedType &p_type) {
    if (!p_obj) {
        return Variant();
    }

    return mono_object_to_variant_impl(p_obj, p_type);
}

Variant mono_object_to_variant_no_err(MonoObject *p_obj, const ManagedType &p_type) {
    if (!p_obj) {
        return Variant();
    }

    return mono_object_to_variant_impl(p_obj, p_type, /* fail_with_err: */ false);
}

String mono_object_to_variant_string(MonoObject *p_obj, MonoException **r_exc) {
    if (p_obj == nullptr) {
        return "null";
    }

    ManagedType type = ManagedType::from_class(mono_object_get_class(p_obj));
    Variant var = GDMonoMarshal::mono_object_to_variant_no_err(p_obj, type);

    if (var.get_type() == VariantType::NIL && p_obj != nullptr) {
        // Cannot convert MonoObject* to Variant; fallback to 'ToString()'.
        MonoException *exc = nullptr;
        MonoString *mono_str = GDMonoUtils::object_to_string(p_obj, &exc);

        if (exc) {
            if (r_exc) {
                *r_exc = exc;
            }
            return String();
        }

        return GDMonoMarshal::mono_string_to_godot(mono_str);
    } else {
        return var.as<String>();
    }
}

MonoObject *Dictionary_to_system_generic_dict(const Dictionary &p_dict, GDMonoClass *p_class, MonoReflectionType *p_key_reftype, MonoReflectionType *p_value_reftype) {
    String ctor_desc = ":.ctor(System.Collections.Generic.IDictionary`2<" + GDMonoUtils::get_type_desc(p_key_reftype) +
                       ", " + GDMonoUtils::get_type_desc(p_value_reftype) + ">)";
    GDMonoMethod *ctor = p_class->get_method_with_desc(ctor_desc, true);
    CRASH_COND(ctor == nullptr);

    MonoObject *mono_object = mono_object_new(mono_domain_get(), p_class->get_mono_ptr());
    ERR_FAIL_NULL_V(mono_object, nullptr);

    GDMonoClass *godot_dict_class = GDMonoUtils::Marshal::make_generic_dictionary_type(p_key_reftype, p_value_reftype);
    MonoObject *godot_dict = GDMonoUtils::create_managed_from(p_dict, godot_dict_class);

    void *ctor_args[1] = { godot_dict };

    MonoException *exc = nullptr;
    ctor->invoke_raw(mono_object, ctor_args, &exc);
    UNHANDLED_EXCEPTION(exc);

    return mono_object;
}

Dictionary system_generic_dict_to_Dictionary(MonoObject *p_obj, [[maybe_unused]] GDMonoClass *p_class, MonoReflectionType *p_key_reftype, MonoReflectionType *p_value_reftype) {
    GDMonoClass *godot_dict_class = GDMonoUtils::Marshal::make_generic_dictionary_type(p_key_reftype, p_value_reftype);
    String ctor_desc = ":.ctor(System.Collections.Generic.IDictionary`2<" + GDMonoUtils::get_type_desc(p_key_reftype) +
                       ", " + GDMonoUtils::get_type_desc(p_value_reftype) + ">)";
    GDMonoMethod *godot_dict_ctor = godot_dict_class->get_method_with_desc(ctor_desc, true);
    CRASH_COND(godot_dict_ctor == nullptr);

    MonoObject *godot_dict = mono_object_new(mono_domain_get(), godot_dict_class->get_mono_ptr());
    ERR_FAIL_NULL_V(godot_dict, Dictionary());

    void *ctor_args[1] = { p_obj };

    MonoException *exc = nullptr;
    godot_dict_ctor->invoke_raw(godot_dict, ctor_args, &exc);
    UNHANDLED_EXCEPTION(exc);

    exc = nullptr;
    MonoObject *ret = godot_dict_class->get_method("GetPtr")->invoke(godot_dict, &exc);
    UNHANDLED_EXCEPTION(exc);

    return *unbox<Dictionary *>(ret);
}

MonoObject *Array_to_system_generic_list(const Array &p_array, GDMonoClass *p_class, MonoReflectionType *p_elem_reftype) {
    GDMonoClass *elem_class = ManagedType::from_reftype(p_elem_reftype).type_class;

    String ctor_desc = ":.ctor(System.Collections.Generic.IEnumerable`1<" + elem_class->get_type_desc() + ">)";
    GDMonoMethod *ctor = p_class->get_method_with_desc(ctor_desc, true);
    CRASH_COND(ctor == nullptr);

    MonoObject *mono_object = mono_object_new(mono_domain_get(), p_class->get_mono_ptr());
    ERR_FAIL_NULL_V(mono_object, nullptr);

    void *ctor_args[1] = { container_to_mono_array(p_array, elem_class) };

    MonoException *exc = nullptr;
    ctor->invoke_raw(mono_object, ctor_args, &exc);
    UNHANDLED_EXCEPTION(exc);

    return mono_object;
}

Array system_generic_list_to_Array(MonoObject *p_obj, GDMonoClass *p_class, [[maybe_unused]] MonoReflectionType *p_elem_reftype) {
    GDMonoMethod *to_array = p_class->get_method("ToArray", 0);
    CRASH_COND(to_array == nullptr);

    MonoException *exc = nullptr;
    MonoArray *mono_array = (MonoArray *)to_array->invoke_raw(p_obj, nullptr, &exc);
    UNHANDLED_EXCEPTION(exc);

    return mono_array_to_Array(mono_array);
}

MonoArray *container_to_mono_array(const Array &p_array) {
    MonoArray *ret = mono_array_new(mono_domain_get(), CACHED_CLASS_RAW(MonoObject), p_array.size());

    for (int i = 0; i < p_array.size(); i++) {
        MonoObject *boxed = variant_to_mono_object(p_array[i]);
        mono_array_setref(ret, i, boxed);
    }

    return ret;
}

MonoArray *container_to_mono_array(const Array &p_array, GDMonoClass *p_array_type_class) {
    int length = p_array.size();
    MonoArray *ret = mono_array_new(mono_domain_get(), p_array_type_class->get_mono_ptr(), length);

    for (int i = 0; i < length; i++) {
        MonoObject *boxed = variant_to_mono_object(p_array[i]);
        mono_array_setref(ret, i, boxed);
    }

    return ret;
}

Array mono_array_to_Array(MonoArray *p_array) {
    Array ret;
    if (!p_array) {
        return ret;
    }
    int length = mono_array_length(p_array);
    ret.resize(length);

    for (int i = 0; i < length; i++) {
        MonoObject *elem = mono_array_get(p_array, MonoObject *, i);
        ret[i] = mono_object_to_variant(elem);
    }

    return ret;
}

Callable managed_to_callable(const M_Callable &p_managed_callable) {
    if (p_managed_callable.delegate) {
        // TODO: Use pooling for ManagedCallable instances.
        CallableCustom *managed_callable = memnew(ManagedCallable(p_managed_callable.delegate));
        return Callable(managed_callable);
    } else {
        Object *target = p_managed_callable.target ?
                                 unbox<Object *>(CACHED_FIELD(GodotObject, ptr)->get_value(p_managed_callable.target)) :
                                 nullptr;
        StringName *method_ptr = unbox<StringName *>(CACHED_FIELD(StringName, ptr)->get_value(p_managed_callable.method_string_name));
        StringName method = method_ptr ? *method_ptr : StringName();
        return Callable(target, method);
    }
}

M_Callable callable_to_managed(const Callable &p_callable) {
    if (p_callable.is_custom()) {
        CallableCustom *custom = p_callable.get_custom();
        CallableCustom::CompareEqualFunc compare_equal_func = custom->get_compare_equal_func();

        if (compare_equal_func == ManagedCallable::compare_equal_func_ptr) {
            ManagedCallable *managed_callable = static_cast<ManagedCallable *>(custom);
            return {
                nullptr, nullptr,
                managed_callable->get_delegate()
            };
        } else if (compare_equal_func == SignalAwaiterCallable::compare_equal_func_ptr) {
            SignalAwaiterCallable *signal_awaiter_callable = static_cast<SignalAwaiterCallable *>(custom);
            return {
                GDMonoUtils::unmanaged_get_managed(object_for_entity(signal_awaiter_callable->get_object())),
                GDMonoUtils::create_managed_from(signal_awaiter_callable->get_signal()),
                nullptr
            };
        } else if (compare_equal_func == EventSignalCallable::compare_equal_func_ptr) {
            EventSignalCallable *event_signal_callable = static_cast<EventSignalCallable *>(custom);
            return {
                GDMonoUtils::unmanaged_get_managed(object_for_entity(event_signal_callable->get_object())),
                GDMonoUtils::create_managed_from(event_signal_callable->get_signal()),
                nullptr
            };
        }

        // Some other CallableCustom. We only support ManagedCallable.
        return { nullptr, nullptr, nullptr };
    } else {
        MonoObject *target_managed = GDMonoUtils::unmanaged_get_managed(p_callable.get_object());
        MonoObject *method_string_name_managed = GDMonoUtils::create_managed_from(p_callable.get_method());
        return { target_managed, method_string_name_managed, nullptr };
    }
}

Signal managed_to_signal_info(const M_SignalInfo &p_managed_signal) {
    Object *owner = p_managed_signal.owner ?
                            unbox<Object *>(CACHED_FIELD(GodotObject, ptr)->get_value(p_managed_signal.owner)) :
                            nullptr;
    StringName *name_ptr = unbox<StringName *>(CACHED_FIELD(StringName, ptr)->get_value(p_managed_signal.name_string_name));
    StringName name = name_ptr ? *name_ptr : StringName();
    return Signal(owner, name);
}

M_SignalInfo signal_info_to_managed(const Signal &p_signal) {
    Object *owner = p_signal.get_object();
    MonoObject *owner_managed = GDMonoUtils::unmanaged_get_managed(owner);
    MonoObject *name_string_name_managed = GDMonoUtils::create_managed_from(p_signal.get_name());
    return { owner_managed, name_string_name_managed };
}

} // namespace GDMonoMarshal
