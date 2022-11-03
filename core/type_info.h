/*************************************************************************/
/*  type_info.h                                                          */
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
#include "core/typedefs.h"
#include "core/variant.h"
#include "core/property_info.h"
#include "core/typesystem_decls.h"

#include "entt/core/fwd.hpp"

struct Frustum;

template <bool C, typename T = void>
struct EnableIf {

    using type = T;
};

template <typename T>
struct EnableIf<false, T> {
};

template <typename, typename>
struct TypesAreSame {

    static bool const value = false;
};

template <typename A>
struct TypesAreSame<A, A> {

    static bool const value = true;
};

template <typename B, typename D>
struct TypeInherits {

    static D *get_d();

    static char (&test(B *))[1];
    static char (&test(...))[2];

    static bool const value = sizeof(test(get_d())) == sizeof(char) &&
                              !TypesAreSame<B volatile const, void volatile const>::value;
};

// If the compiler fails because it's trying to instantiate the primary 'GetTypeInfo' template
// instead of one of the specializations, it's most likely because the type 'T' is not supported.
// If 'T' is a class that inherits 'Object', make sure it can see the actual class declaration
// instead of a forward declaration. You can always forward declare 'T' in a header file, and then
// include the actual declaration of 'T' in the source file where 'GetTypeInfo<T>' is instantiated.
template <class T, typename = void>
struct GetTypeInfo;

#define MAKE_TYPE_INFO(m_type, m_var_type)                                            \
    template <>                                                                       \
    struct GetTypeInfo<m_type> {                                                      \
        constexpr static const VariantType VARIANT_TYPE = m_var_type;                         \
        constexpr static const GodotTypeInfo::Metadata METADATA = GodotTypeInfo::METADATA_NONE; \
        constexpr static const TypePassBy PASS_BY = TypePassBy::Value; \
        constexpr static inline RawPropertyInfo get_class_info() {  \
            return RawPropertyInfo { nullptr,nullptr,nullptr,int8_t(VARIANT_TYPE)};\
        }\
    };                                                                                \
    template <>                                                                       \
    struct GetTypeInfo<const m_type &> {                                              \
        constexpr static const VariantType VARIANT_TYPE = m_var_type;                         \
        constexpr static const GodotTypeInfo::Metadata METADATA = GodotTypeInfo::METADATA_NONE; \
        constexpr static const TypePassBy PASS_BY = TypePassBy::ConstReference; \
        constexpr static inline RawPropertyInfo get_class_info() {  \
            return RawPropertyInfo { nullptr,nullptr,nullptr,int8_t(VARIANT_TYPE)};\
        }\
    }; \
    template <>                                                                       \
    struct GetTypeInfo<m_type &&> {                                              \
        constexpr static const VariantType VARIANT_TYPE = m_var_type;                         \
        constexpr static const GodotTypeInfo::Metadata METADATA = GodotTypeInfo::METADATA_NONE; \
        constexpr static const TypePassBy PASS_BY = TypePassBy::Move; \
        constexpr static inline RawPropertyInfo get_class_info() {  \
            return RawPropertyInfo { nullptr,nullptr,nullptr,int8_t(VARIANT_TYPE)};\
        }\
    };

#define MAKE_TYPE_INFO_WITH_META(m_type, m_var_type, m_metadata)    \
    template <>                                                     \
    struct GetTypeInfo<m_type> {                                    \
        constexpr static const VariantType VARIANT_TYPE = m_var_type;       \
        constexpr static const GodotTypeInfo::Metadata METADATA = m_metadata; \
        constexpr static const TypePassBy PASS_BY = TypePassBy::Value; \
        constexpr static inline RawPropertyInfo get_class_info() {  \
            return RawPropertyInfo { nullptr,nullptr,nullptr,int8_t(VARIANT_TYPE)};\
        }\
    };                                                              \
    template <>                                                     \
    struct GetTypeInfo<const m_type &> {                            \
        constexpr static const VariantType VARIANT_TYPE = m_var_type;       \
        constexpr static const GodotTypeInfo::Metadata METADATA = m_metadata; \
        constexpr static const TypePassBy PASS_BY = TypePassBy::ConstReference; \
        constexpr static inline RawPropertyInfo get_class_info() {  \
            return RawPropertyInfo { nullptr,nullptr,nullptr,int8_t(VARIANT_TYPE)};\
        }\
    };


template <>
struct GetTypeInfo<void> {
    static const VariantType VARIANT_TYPE = VariantType::NIL;
    static const GodotTypeInfo::Metadata METADATA = GodotTypeInfo::METADATA_NONE;
    constexpr static const TypePassBy PASS_BY = TypePassBy::Value;
    constexpr static inline RawPropertyInfo get_class_info() {
        return RawPropertyInfo {nullptr, nullptr, nullptr, int8_t(VARIANT_TYPE), PropertyHint::None };
    }
};

MAKE_TYPE_INFO(bool, VariantType::BOOL)
MAKE_TYPE_INFO_WITH_META(uint8_t, VariantType::INT, GodotTypeInfo::METADATA_INT_IS_UINT8)
MAKE_TYPE_INFO_WITH_META(int8_t, VariantType::INT, GodotTypeInfo::METADATA_INT_IS_INT8)
MAKE_TYPE_INFO_WITH_META(uint16_t, VariantType::INT, GodotTypeInfo::METADATA_INT_IS_UINT16)
MAKE_TYPE_INFO_WITH_META(int16_t, VariantType::INT, GodotTypeInfo::METADATA_INT_IS_INT16)
MAKE_TYPE_INFO_WITH_META(uint32_t, VariantType::INT, GodotTypeInfo::METADATA_INT_IS_UINT32)
MAKE_TYPE_INFO_WITH_META(int32_t, VariantType::INT, GodotTypeInfo::METADATA_INT_IS_INT32)
MAKE_TYPE_INFO_WITH_META(uint64_t, VariantType::INT, GodotTypeInfo::METADATA_INT_IS_UINT64)
MAKE_TYPE_INFO_WITH_META(int64_t, VariantType::INT, GodotTypeInfo::METADATA_INT_IS_INT64)
MAKE_TYPE_INFO(char16_t, VariantType::INT)
MAKE_TYPE_INFO_WITH_META(QChar, VariantType::INT, GodotTypeInfo::METADATA_INT_IS_UINT16)
MAKE_TYPE_INFO_WITH_META(float, VariantType::FLOAT, GodotTypeInfo::METADATA_REAL_IS_FLOAT)
MAKE_TYPE_INFO_WITH_META(double, VariantType::FLOAT, GodotTypeInfo::METADATA_REAL_IS_DOUBLE)

MAKE_TYPE_INFO(UIString, VariantType::STRING)
MAKE_TYPE_INFO(String, VariantType::STRING)
MAKE_TYPE_INFO_WITH_META(StringView, VariantType::STRING,GodotTypeInfo::METADATA_STRING_VIEW)
MAKE_TYPE_INFO(Vector2, VariantType::VECTOR2)
MAKE_TYPE_INFO(Rect2, VariantType::RECT2)
MAKE_TYPE_INFO(Vector3, VariantType::VECTOR3)
MAKE_TYPE_INFO(Transform2D, VariantType::TRANSFORM2D)
MAKE_TYPE_INFO(Plane, VariantType::PLANE)
MAKE_TYPE_INFO(Quat, VariantType::QUAT)
MAKE_TYPE_INFO(AABB, VariantType::AABB)
MAKE_TYPE_INFO(Basis, VariantType::BASIS)
MAKE_TYPE_INFO(Transform, VariantType::TRANSFORM)
MAKE_TYPE_INFO(Color, VariantType::COLOR)
MAKE_TYPE_INFO(NodePath, VariantType::NODE_PATH)
MAKE_TYPE_INFO(RID, VariantType::_RID)
MAKE_TYPE_INFO(Callable, VariantType::CALLABLE)
MAKE_TYPE_INFO(Signal, VariantType::SIGNAL)
MAKE_TYPE_INFO(Dictionary, VariantType::DICTIONARY)
MAKE_TYPE_INFO(Array, VariantType::ARRAY)
MAKE_TYPE_INFO(PoolByteArray, VariantType::POOL_BYTE_ARRAY)
MAKE_TYPE_INFO(PoolIntArray, VariantType::POOL_INT_ARRAY)
MAKE_TYPE_INFO(PoolRealArray, VariantType::POOL_FLOAT32_ARRAY)
MAKE_TYPE_INFO(PoolStringArray, VariantType::POOL_STRING_ARRAY)
MAKE_TYPE_INFO(PoolVector2Array, VariantType::POOL_VECTOR2_ARRAY)
MAKE_TYPE_INFO(PoolVector3Array, VariantType::POOL_VECTOR3_ARRAY)
MAKE_TYPE_INFO(PoolColorArray, VariantType::POOL_COLOR_ARRAY)

#define MAKE_GENERIC_SPAN_INFO(type) \
    template <>\
    struct GetTypeInfo<Span<type>> {\
        constexpr static const VariantType VARIANT_TYPE = VariantType::ARRAY;\
        constexpr static const GodotTypeInfo::Metadata METADATA = GodotTypeInfo::METADATA_NON_COW_CONTAINER;\
        constexpr static const TypePassBy PASS_BY = TypePassBy::Value; \
        constexpr static inline RawPropertyInfo get_class_info() {  \
            return RawPropertyInfo { nullptr,nullptr,nullptr,int8_t(VARIANT_TYPE)};\
        }\
    };\
    template <>\
    struct GetTypeInfo<Span<const type>> {\
        constexpr static const VariantType VARIANT_TYPE = VariantType::ARRAY;\
        constexpr static const GodotTypeInfo::Metadata METADATA = GodotTypeInfo::METADATA_NON_COW_CONTAINER;\
        constexpr static const TypePassBy PASS_BY = TypePassBy::Value; \
        constexpr static inline RawPropertyInfo get_class_info() {  \
            return RawPropertyInfo { nullptr,nullptr,nullptr,int8_t(VARIANT_TYPE)};\
        }\
    };
#define MAKE_SPAN_INFO(type,internal) \
    template <>\
    struct GetTypeInfo<Span<type>> {\
        constexpr static const VariantType VARIANT_TYPE = internal;\
        constexpr static const GodotTypeInfo::Metadata METADATA = GodotTypeInfo::METADATA_NON_COW_CONTAINER;\
        constexpr static const TypePassBy PASS_BY = TypePassBy::Value; \
        constexpr static inline RawPropertyInfo get_class_info() {  \
            return RawPropertyInfo { nullptr,nullptr,nullptr,int8_t(VARIANT_TYPE)};\
        }\
    };\
    template <>\
    struct GetTypeInfo<Span<const type>> {\
        constexpr static const VariantType VARIANT_TYPE = internal;\
        constexpr static const GodotTypeInfo::Metadata METADATA = GodotTypeInfo::METADATA_NON_COW_CONTAINER;\
        constexpr static const TypePassBy PASS_BY = TypePassBy::Value; \
        constexpr static inline RawPropertyInfo get_class_info() {  \
            return RawPropertyInfo { nullptr,nullptr,nullptr,int8_t(VARIANT_TYPE)};\
        }\
    };

MAKE_GENERIC_SPAN_INFO(Plane)
MAKE_SPAN_INFO(Vector2,VariantType::POOL_VECTOR2_ARRAY)
MAKE_SPAN_INFO(Vector3,VariantType::POOL_VECTOR3_ARRAY)
MAKE_SPAN_INFO(Color,VariantType::POOL_COLOR_ARRAY)
MAKE_SPAN_INFO(float,VariantType::POOL_FLOAT32_ARRAY)
MAKE_SPAN_INFO(int,VariantType::POOL_INT_ARRAY)
MAKE_SPAN_INFO(uint8_t,VariantType::POOL_BYTE_ARRAY)

MAKE_TYPE_INFO(StringName, VariantType::STRING_NAME)
MAKE_TYPE_INFO(IP_Address, VariantType::STRING)


//for RefPtr
template <>
struct GetTypeInfo<RefPtr> {
    constexpr static const VariantType VARIANT_TYPE = VariantType::OBJECT;
    constexpr static const GodotTypeInfo::Metadata METADATA = GodotTypeInfo::METADATA_NONE;
    constexpr static const TypePassBy PASS_BY = TypePassBy::Value;
    constexpr static inline RawPropertyInfo get_class_info() {
        return RawPropertyInfo{ nullptr,"RefCounted","RefCounted",int8_t(VariantType::OBJECT), PropertyHint::ResourceType };
    }
};


template <>
struct GetTypeInfo<const RefPtr &> {
    constexpr static const VariantType VARIANT_TYPE = VariantType::OBJECT;
    constexpr static const GodotTypeInfo::Metadata METADATA = GodotTypeInfo::METADATA_NONE;
    constexpr static const TypePassBy PASS_BY = TypePassBy::ConstReference;
    constexpr static inline RawPropertyInfo get_class_info() {
        return RawPropertyInfo{ nullptr,"RefCounted","RefCounted",int8_t(VariantType::OBJECT), PropertyHint::ResourceType };
    }
};

//for variant
template <>
struct GetTypeInfo<Variant> {
    constexpr static const VariantType VARIANT_TYPE = VariantType::NIL;
    constexpr static const GodotTypeInfo::Metadata METADATA = GodotTypeInfo::METADATA_NONE;
    constexpr static const TypePassBy PASS_BY = TypePassBy::Value;
    constexpr static inline RawPropertyInfo get_class_info() {
        return RawPropertyInfo{ nullptr, nullptr, nullptr, int8_t(VARIANT_TYPE), PropertyHint::None,
            PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NIL_IS_VARIANT };
    }
};

template <>
struct GetTypeInfo<const Variant &> {
    constexpr static const VariantType VARIANT_TYPE = VariantType::NIL;
    constexpr static const GodotTypeInfo::Metadata METADATA = GodotTypeInfo::METADATA_NONE;
    constexpr static const TypePassBy PASS_BY = TypePassBy::ConstReference;
    constexpr static inline RawPropertyInfo get_class_info() {
        return RawPropertyInfo{ nullptr, nullptr, nullptr, int8_t(VARIANT_TYPE), PropertyHint::None,
            PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NIL_IS_VARIANT };
    }
};

#define MAKE_TEMPLATE_TYPE_INFO(m_template, m_type, m_var_type)                       \
    template <>                                                                       \
    struct GetTypeInfo<m_template<m_type> > {                                         \
        constexpr static const VariantType VARIANT_TYPE = m_var_type;                         \
        constexpr static const GodotTypeInfo::Metadata METADATA = GodotTypeInfo::METADATA_NONE; \
        constexpr static const TypePassBy PASS_BY = TypePassBy::Reference; \
        constexpr static inline RawPropertyInfo get_class_info() {\
            return RawPropertyInfo { nullptr,nullptr,nullptr,int8_t(VARIANT_TYPE)};\
        }\
    };                                                                                \
    template <>                                                                       \
    struct GetTypeInfo<const m_template<m_type> &> {                                  \
        constexpr static const VariantType VARIANT_TYPE = m_var_type;                         \
        constexpr static const GodotTypeInfo::Metadata METADATA = GodotTypeInfo::METADATA_NONE; \
        constexpr static const TypePassBy PASS_BY = TypePassBy::ConstReference; \
        constexpr static inline RawPropertyInfo get_class_info() {\
            return RawPropertyInfo { nullptr,nullptr,nullptr,int8_t(VARIANT_TYPE)};\
        }\
    };
#define MAKE_TEMPLATE_TYPE_INFO_META(m_template, m_type, m_var_type,m_meta)           \
    template <>                                                                       \
    struct GetTypeInfo<m_template<m_type> > {                                         \
        constexpr static const VariantType VARIANT_TYPE = m_var_type;                         \
        constexpr static const GodotTypeInfo::Metadata METADATA = m_meta; \
        constexpr static const TypePassBy PASS_BY = TypePassBy::Reference; \
        constexpr static inline RawPropertyInfo get_class_info() {\
            return RawPropertyInfo { nullptr,nullptr,nullptr,int8_t(VARIANT_TYPE)};\
        }\
    };                                                                                \
    template <>                                                                       \
    struct GetTypeInfo<const m_template<m_type> &> {                                  \
        constexpr static const VariantType VARIANT_TYPE = m_var_type;                         \
        constexpr static const GodotTypeInfo::Metadata METADATA = m_meta; \
        constexpr static const TypePassBy PASS_BY = TypePassBy::ConstReference; \
        constexpr static inline RawPropertyInfo get_class_info() {\
            return RawPropertyInfo { nullptr,nullptr,nullptr,int8_t(VARIANT_TYPE)};\
        }\
    };\
    template <>                                                                       \
    struct GetTypeInfo<m_template<m_type> &&> { \
        \
            constexpr static const VariantType VARIANT_TYPE = m_var_type;                         \
            constexpr static const GodotTypeInfo::Metadata METADATA = m_meta; \
            constexpr static const TypePassBy PASS_BY = TypePassBy::Move; \
            constexpr static inline RawPropertyInfo get_class_info() { \
                return RawPropertyInfo{ nullptr,nullptr,nullptr,int8_t(VARIANT_TYPE) }; \
        }\
    };

MAKE_TEMPLATE_TYPE_INFO_META(Vector, uint8_t, VariantType::POOL_BYTE_ARRAY,GodotTypeInfo::METADATA_NON_COW_CONTAINER)
MAKE_TEMPLATE_TYPE_INFO_META(Vector, int, VariantType::POOL_INT_ARRAY,GodotTypeInfo::METADATA_NON_COW_CONTAINER)
MAKE_TEMPLATE_TYPE_INFO_META(Vector, float, VariantType::POOL_FLOAT32_ARRAY,GodotTypeInfo::METADATA_NON_COW_CONTAINER)
MAKE_TEMPLATE_TYPE_INFO_META(Vector, String, VariantType::POOL_STRING_ARRAY,GodotTypeInfo::METADATA_NON_COW_CONTAINER)
MAKE_TEMPLATE_TYPE_INFO_META(Vector, StringName, VariantType::POOL_STRING_ARRAY,GodotTypeInfo::METADATA_NON_COW_CONTAINER)
MAKE_TEMPLATE_TYPE_INFO_META(Vector, Vector2, VariantType::POOL_VECTOR2_ARRAY,GodotTypeInfo::METADATA_NON_COW_CONTAINER)
MAKE_TEMPLATE_TYPE_INFO_META(Vector, Vector3, VariantType::POOL_VECTOR3_ARRAY,GodotTypeInfo::METADATA_NON_COW_CONTAINER)
MAKE_TEMPLATE_TYPE_INFO_META(Vector, Color, VariantType::POOL_COLOR_ARRAY,GodotTypeInfo::METADATA_NON_COW_CONTAINER)
MAKE_TEMPLATE_TYPE_INFO_META(Vector, RID, VariantType::ARRAY,GodotTypeInfo::METADATA_NON_COW_CONTAINER)
MAKE_TEMPLATE_TYPE_INFO_META(Vector, Face3, VariantType::POOL_VECTOR3_ARRAY,GodotTypeInfo::METADATA_NON_COW_CONTAINER)

MAKE_TEMPLATE_TYPE_INFO(Vector, Variant, VariantType::ARRAY)
MAKE_TEMPLATE_TYPE_INFO(Vector, Plane, VariantType::ARRAY)

// Return by vector of pointers
template <typename T>
struct GetTypeInfo<Vector<T *> > {
    constexpr static const VariantType VARIANT_TYPE = VariantType::ARRAY;
    constexpr static const GodotTypeInfo::Metadata METADATA = GodotTypeInfo::METADATA_NON_COW_CONTAINER;
    constexpr static const TypePassBy PASS_BY = TypePassBy::Value;
    constexpr static inline RawPropertyInfo get_class_info() {
        return RawPropertyInfo { nullptr,nullptr,nullptr,int8_t(VARIANT_TYPE)};
    }\
};

MAKE_TEMPLATE_TYPE_INFO(PoolVector, RID, VariantType::ARRAY)
MAKE_TEMPLATE_TYPE_INFO(PoolVector, Plane, VariantType::ARRAY)
MAKE_TEMPLATE_TYPE_INFO(PoolVector, Face3, VariantType::POOL_VECTOR3_ARRAY)

template <>
struct GetTypeInfo<Frustum> {
    constexpr static const VariantType VARIANT_TYPE = VariantType::ARRAY;
    constexpr static const GodotTypeInfo::Metadata METADATA = GodotTypeInfo::METADATA_NONE;
    constexpr static const TypePassBy PASS_BY = TypePassBy::Reference;
    constexpr static inline RawPropertyInfo get_class_info() {
        return RawPropertyInfo { nullptr,nullptr,nullptr,int8_t(VARIANT_TYPE)};
    }
};

template <typename T>
struct GetTypeInfo<T *, typename EnableIf<TypeInherits<Object, T>::value>::type> {
    constexpr static const VariantType VARIANT_TYPE = VariantType::OBJECT;
    constexpr static const GodotTypeInfo::Metadata METADATA = GodotTypeInfo::METADATA_NONE;
    constexpr static const TypePassBy PASS_BY = TypePassBy::Pointer;
    constexpr static inline RawPropertyInfo get_class_info() {
        return RawPropertyInfo { nullptr,nullptr,T::get_class_static(),int8_t(VARIANT_TYPE)};
    }
};

template <typename T>
struct GetTypeInfo<const T *, typename EnableIf<TypeInherits<Object, T>::value>::type> {
    constexpr static const VariantType VARIANT_TYPE = VariantType::OBJECT;
    constexpr static const GodotTypeInfo::Metadata METADATA = GodotTypeInfo::METADATA_NONE;
    constexpr static const TypePassBy PASS_BY = TypePassBy::Pointer;
    constexpr static inline RawPropertyInfo get_class_info() {
        return RawPropertyInfo { nullptr,nullptr,T::get_class_static(),int8_t(VARIANT_TYPE)};
    }
};

#define TEMPL_MAKE_ENUM_TYPE_INFO(m_enum, m_impl)                                                                      \
    template <> struct GetTypeInfo<m_impl> {                                                                           \
        constexpr static const VariantType VARIANT_TYPE = VariantType::INT;                                            \
        constexpr static const GodotTypeInfo::Metadata METADATA = GodotTypeInfo::METADATA_NONE;                        \
    constexpr static const TypePassBy PASS_BY = TypePassBy::Value;                                                     \
        constexpr static inline RawPropertyInfo get_class_info() {                                                     \
            return RawPropertyInfo { nullptr, nullptr, #m_impl, int8_t(VARIANT_TYPE),                                  \
                PropertyHint::None, PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_CLASS_IS_ENUM };                           \
        }                                                                                                              \
    }

#define EXTERN_MAKE_ENUM_TYPE_INFO(m_enum) extern template class GetTypeInfo<m_enum>

#define MAKE_ENUM_TYPE_INFO(m_enum) TEMPL_MAKE_ENUM_TYPE_INFO(m_enum, m_enum)

//Entity IDs


#define MAKE_ENTITY_TYPE_INFO(m_entity)                                                                            \
template <> struct GetTypeInfo<m_entity> {                                                                         \
    constexpr static const VariantType VARIANT_TYPE = VariantType::INT;                                            \
    constexpr static const GodotTypeInfo::Metadata METADATA = GodotTypeInfo::METADATA_IS_ENTITY_ID;                \
    constexpr static const TypePassBy PASS_BY = TypePassBy::Value;                                                 \
    constexpr static inline RawPropertyInfo get_class_info() {                                                     \
        return RawPropertyInfo{ nullptr, nullptr, #m_entity, int8_t(VARIANT_TYPE), PropertyHint::None,    \
            PROPERTY_USAGE_DEFAULT };                                                                              \
    }                                                                                                              \
};

#include "core/engine_entities.h"

MAKE_ENTITY_TYPE_INFO(RenderingEntity)
MAKE_ENTITY_TYPE_INFO(GameEntity)
template <typename T>
inline StringName __constant_get_enum_name(T /*param*/, const char *p_constant) {
    if (GetTypeInfo<T>::VARIANT_TYPE == VariantType::NIL) {
        ERR_PRINT(String("Missing VARIANT_ENUM_CAST for constant's enum: ") + p_constant);
    }
    const char *name=GetTypeInfo<T>::get_class_info().class_name;
    return StringName(name ? StaticCString(name,true) : StringName());
}

#define CLASS_INFO(m_type) (GetTypeInfo<m_type *>::get_class_info())


