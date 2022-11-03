/*************************************************************************/
/*  variant.h                                                            */
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

#include "core/godot_export.h"
#include "core/reflection_macros.h"
#include "core/math/math_defs.h"
#include "core/forward_decls.h"
#include "core/ref_ptr.h"
#include "core/vector.h"
#include "core/array.h"
#include "core/callable.h"
#include "core/object_rc.h"

#include <cstdint>
#include "EASTL/type_traits.h"

//class Node;
//class Control;
class Object;
class ObjectRC;
using UIString = class QString;
class RID;
struct RenderingEntity;
class Array;
class NodePath;
class Dictionary;
struct Frustum;
struct Color;
struct Vector2;
struct Vector3;
class Basis;
class StringName;
class AABB;
struct Rect2;
class Plane;
struct Transform2D;
class Transform;
class Face3;
class Quat;
using CharType = class QChar;
struct IP_Address;
struct PropertyInfo;
struct MethodInfo;
template <class T>
class PoolVector;
template <class T> struct Hasher;

template <class T>
T *object_cast(Object *p_object);

template<class T>
inline Ref<T> refFromVariant(const Variant& p_variant);

using PoolByteArray = PoolVector<uint8_t>;
using PoolIntArray = PoolVector<int>;
using PoolRealArray = PoolVector<real_t>;
//using PoolStringArray = PoolVector<UIString>;
using PoolStringArray = PoolVector<String>;
using PoolVector2Array = PoolVector<Vector2>;
using PoolVector3Array = PoolVector<Vector3>;
using PoolColorArray = PoolVector<Color>;


#define _REF_OBJ_PTR(m_variant) (reinterpret_cast<Ref<RefCounted> *>((m_variant)._get_obj().ref.get())->get())
#define _OBJ_PTR(m_variant) ((m_variant)._get_obj().rc ? (m_variant)._get_obj().rc->get_ptr() : _REF_OBJ_PTR(m_variant))
// _UNSAFE_OBJ_PROXY_PTR is needed for comparing an object Variant against NIL or compare two object Variants.
// It's guaranteed to be unique per object, in contrast to the pointer stored in the RC structure,
// which is set to null when the object is destroyed.
#define _UNSAFE_OBJ_PROXY_PTR(m_variant) ((m_variant)._get_obj().rc ? reinterpret_cast<uint8_t *>((m_variant)._get_obj().rc) : reinterpret_cast<uint8_t *>(_REF_OBJ_PTR(m_variant)))
#define MAX_RECURSION 100
// Temporary workaround until c++11 alignas()
#ifdef __GNUC__
#define GCC_ALIGNED_8 __attribute__((aligned(8)))
#else
#define GCC_ALIGNED_8
#endif
// If this changes the table in variant_op must be updated
enum class VariantType : int8_t {

    NIL = 0,

    // atomic types
    BOOL,
    INT,
    FLOAT,
    STRING,

    // math types

    VECTOR2, // 5
    RECT2,
    VECTOR3,
    TRANSFORM2D,
    PLANE,
    QUAT, // 10
    AABB,
    BASIS,
    TRANSFORM,

    // misc types
    COLOR,
    STRING_NAME, // 15
    NODE_PATH,
    _RID,
    OBJECT,
    CALLABLE,
    SIGNAL, // 20
    DICTIONARY,
    ARRAY,

    // arrays
    POOL_BYTE_ARRAY,
    POOL_INT_ARRAY,
    POOL_FLOAT32_ARRAY, //25
    POOL_STRING_ARRAY,
    POOL_VECTOR2_ARRAY,
    POOL_VECTOR3_ARRAY,
    POOL_COLOR_ARRAY,

    REN_ENT,
    VARIANT_MAX

};

SE_OPAQUE_TYPE(Variant)
class GODOT_EXPORT Variant {
private:
    friend struct VariantOps;
    // Variant takes 20 bytes when real_t is float, and 36 if double
    // it only allocates extra memory for aabb/matrix.

    VariantType type;

    struct ObjData {

        // Will be null for every type deriving from Reference as they have their
        // own reference count mechanism
        ObjectRC *rc;
        // Always initialized, but will be null if the Ref<> assigned was null
        // or this Variant is not even holding a Reference-derived object
        RefPtr ref;
    };

    _FORCE_INLINE_ ObjData &_get_obj();
    [[nodiscard]] _FORCE_INLINE_ const ObjData &_get_obj() const;

    union VariantUnion {
        bool _bool;
        int64_t _int;
        double _real;
        Transform2D *_transform2d;
        ::AABB *_aabb;
        Basis *_basis;
        Transform *_transform;
        void *_ptr; //generic pointer
        uint8_t _mem[sizeof(ObjData) > (sizeof(real_t) * 8) ? sizeof(ObjData) : (sizeof(real_t) * 8)];
        constexpr VariantUnion() : _bool(false) {}
        explicit constexpr VariantUnion(double f) : _real(f) {}
        explicit constexpr VariantUnion(int32_t f) : _int(f) {}
        explicit constexpr VariantUnion(uint32_t f) : _int(f) {}
        explicit constexpr VariantUnion(int64_t f) : _int(f) {}
        explicit constexpr VariantUnion(uint64_t f) : _int(f) {}
    } _data GCC_ALIGNED_8;
    void reference(const Variant &p_variant);
    void clear();
    template<class T>
    struct asHelper {
        T convertIt(const Variant &v)  {
            return (T)v;
        }
    };
    template<class T>
    struct asHelper<T *> {
        T *convertIt(const Variant &v)  {
            static_assert (eastl::is_base_of<Object,T>::value);
            return object_cast<T>((Object *)v);
        }
    };
    template<class T>
    struct asHelper<Ref<T>> {
        Ref<T> convertIt(const Variant &v)  {
            return refFromVariant<T>(v);
        }
    };
    // internal construction helper
    Variant(VariantType p_v,VariantUnion &&u) : type(p_v), _data(eastl::move(u)) {}
public:
    enum {
        // Maximum recursion depth allowed when serializing variants.
        MAX_RECURSION_DEPTH = 1024,
    };
    static const Variant null_variant;
    VariantType get_type() const { return type; }
    static const char *get_type_name(VariantType p_type);
    static StringName interned_type_name(VariantType p_type);
    static bool can_convert(VariantType p_type_from, VariantType p_type_to);
    static bool can_convert_strict(VariantType p_type_from, VariantType p_type_to);

    [[nodiscard]] bool is_ref() const;
    [[nodiscard]] bool is_num() const { return type == VariantType::INT || type == VariantType::FLOAT; }
    [[nodiscard]] bool is_array() const { return type >= VariantType::ARRAY; }
    [[nodiscard]] bool is_zero() const;
    //GameEntity get_object_instance_id() const;

    template <typename T>
    [[nodiscard]]
    typename eastl::decay<typename eastl::enable_if<!eastl::is_enum_v<T>, T>::type>::type
        as() const {
      return (T)asHelper< typename eastl::decay<T>::type>().convertIt(*this);
    }

    template<class T>
    [[nodiscard]]
    typename eastl::enable_if<eastl::is_enum_v<T>,T>::type as() const {
        return (T)static_cast<eastl::underlying_type_t<T>>(*this);

    }

    template<class T>
    [[nodiscard]] T *asT() const {
        static_assert (eastl::is_base_of<Object, T>::value);
        return object_cast<T>(as<Object*>());
    }

    // Not a recursive loop, as<String>,as<float>,as<StringName> are specialized.

    //NOTE: Code below is convoluted to prevent implicit bool conversions from all bool convertible types.
    template<class T ,
               class = typename eastl::enable_if<eastl::is_same<bool,T>::value>::type >
    Variant(T p_bool) : type(VariantType::BOOL) {
        _data._bool = p_bool;
    }
    template<class T ,
               class = typename eastl::enable_if<eastl::is_enum_v<T>>::type >
    Variant(T p_val,int=0) : Variant(eastl::underlying_type_t<T>(p_val)){
    }
    //Variant(VariantType p_v) : type(p_v) {}

    constexpr Variant(int8_t p_int)  : type(VariantType::INT),_data(p_int) { }
    constexpr Variant(uint8_t p_int)  : type(VariantType::INT),_data(p_int) { }
    constexpr Variant(int16_t p_int)  : type(VariantType::INT),_data(p_int) { }
    constexpr Variant(uint16_t p_int)  : type(VariantType::INT),_data(p_int) { }
    constexpr Variant(int32_t p_int)  : type(VariantType::INT),_data(p_int) { }
    constexpr Variant(uint32_t p_int)  : type(VariantType::INT),_data(p_int) { }
    constexpr Variant(int64_t p_int)   : type(VariantType::INT),_data(p_int) { }
    constexpr Variant(uint64_t p_int)  : type(VariantType::INT),_data(p_int) { }
    constexpr Variant(float p_float) : type(VariantType::FLOAT),_data(p_float) { }
    constexpr Variant(double p_float) : type(VariantType::FLOAT),_data(p_float) { }
    Variant(QChar p_char);
    //explicit Variant(const String &p_string);
    Variant(const char *p_string);
    Variant(StringView p_string);
    Variant(const String &p_string);
    Variant(StringName p_string);
    Variant(const CharType *p_wstring);
    Variant(const Vector2 &p_vector2);
    Variant(const Rect2 &p_rect2);
    Variant(const Vector3 &p_vector3);
    Variant(const Plane &p_plane);
    Variant(const ::AABB &p_aabb);
    Variant(const Quat &p_quat);
    Variant(const Basis &p_matrix);
    Variant(const Transform2D &p_transform);
    Variant(const Transform &p_transform);
    Variant(const Color &p_color);
    Variant(const NodePath &p_node_path);
    explicit Variant(const RefPtr &p_resource);
    Variant(const RID &p_rid);
    explicit Variant(const Object *p_object);
    explicit Variant(const Callable& p_callable);
    explicit Variant(const Signal& p_signal);
    Variant(const Dictionary &p_dictionary);
    Variant(Dictionary&& p_dictionary) noexcept;

    Variant(const Array &p_array);
    Variant(Array &&p_array) noexcept;

    Variant(const PoolVector<Plane> &p_array); // helper
    Variant(const PoolVector<uint8_t> &p_raw_array);
    Variant(const PoolVector<int> &p_int_array);
    Variant(const PoolVector<real_t> &p_real_array);
    Variant(const PoolVector<UIString> &p_string_array);
    Variant(const PoolVector<String> &p_string_array);
    Variant(const PoolVector<Vector3> &p_vector3_array);
    Variant(const PoolVector<Color> &p_color_array);
    Variant(const PoolVector<Face3> &p_face_array);
    explicit Variant(const PoolVector<Vector2> &p_vector2_array); // helper

    Variant(const Vector<uint8_t> &p_raw_array);
    Variant(const Vector<int> &p_int_array);
    Variant(const Vector<float> &);
    Variant(const Vector<Vector2> &);
    Variant(const Vector<Vector3> &);
    Variant(const Vector<Face3> &);
    Variant(const Vector<Color> &);
    Variant(const Vector<Plane> &);
    Variant &operator[](const Variant &p_key) = delete;
    const Variant &operator[](const Variant &p_key) const = delete;

    explicit Variant(const IP_Address &p_address);

    template<class T>
    static Variant from(const T &v) {
        return Variant(v);
    }
    static Variant from(GameEntity ob) { assert(entt::to_integral(ob)!=0); return {VariantType::INT,VariantUnion(entt::to_integral(ob))}; }
    static Variant from(RenderingEntity ob) { assert(entt::to_integral(ob)!=0); return {VariantType::REN_ENT,VariantUnion(entt::to_integral(ob))}; }
    template<class T>
    static Variant move_from(T &&v) {
        return Variant(eastl::move(v));
    }
    template<class T>
    static Variant from(const Vector<T *> &ar) {
        Array res;
        int idx=0;
        res.resize(ar.size());
        for(T * v : ar)
            res[idx++] = Variant(v);
        return res;
    }
    template<class T>
    static Variant fromVector(Span<const T> v);
    template<class T>
    static Variant fromVectorBuiltin(Span<const T> v);


    // If this changes the table in variant_op must be updated
    enum Operator {
        //comparison
        OP_LESS,
        OP_MAX
    };

    //static const char * get_operator_name(Operator p_op);
    static void evaluate(Operator p_op, const Variant &p_a, const Variant &p_b, Variant &r_ret, bool &r_valid);
    static bool evaluate_equal(const Variant &p_a, const Variant &p_b);

    void zero();
    [[nodiscard]] Variant duplicate(bool deep = false) const;
    template<class T>
    [[nodiscard]] T duplicate_t(bool deep = false) const {
        return duplicate(deep).as<T>();
    }
    static void blend(const Variant &a, const Variant &b, float c, Variant &r_dst);
    static void interpolate(const Variant &a, const Variant &b, float c, Variant &r_dst);

    //Variant call(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error);
    static String get_call_error_text(Object* p_base, const StringName& p_method, const Variant** p_argptrs, int p_argcount, const Callable::CallError& ce);
    static String get_callable_error_text(const Callable& p_callable, const Variant** p_argptrs, int p_argcount, const Callable::CallError& ce);

    static Variant construct_default(VariantType p_type);
    static Variant construct(const VariantType, const Variant &p_arg, Callable::CallError &r_error);

    void set_named(const StringName &p_index, const Variant &p_value, bool *r_valid = nullptr);
    [[nodiscard]] Variant get_named(const StringName &p_index, bool *r_valid = nullptr) const;

    void set_indexed(int p_index, const Variant &p_value, bool *r_valid = nullptr);
    Variant get(int p_index, bool *r_valid = nullptr) const;

    void get_property_list(Vector<PropertyInfo> *p_list) const;

    //argsVariant call()

    bool deep_equal(const Variant &p_variant, int p_recursion_count = 0) const;
    bool operator==(const Variant &p_variant) const;
    bool operator!=(const Variant &p_variant) const;
    bool operator<(const Variant &p_variant) const;
    [[nodiscard]] uint32_t hash() const;

    [[nodiscard]] bool hash_compare(const Variant &p_variant) const;
    bool booleanize() const;
    String stringify(Vector<const void *> &stack) const;

    using ObjectConstruct = void (*)(const UIString &, void *, Variant &);

    [[nodiscard]] String get_construct_string() const;

    Variant &operator=(const Variant &p_variant); // only this is enough for all the other types
    Variant(const Variant &p_variant);
    constexpr Variant(Variant && oth) noexcept : type(oth.type),_data(oth._data) {
        oth.type = VariantType::NIL;
        //NOTE: oth._data is not cleared here since setting type to NIL should suffice.
    }
    constexpr Variant() : type(VariantType::NIL) {}
    _FORCE_INLINE_ ~Variant() {
        if (type != VariantType::NIL) {
            clear();
        }
    }

    [[nodiscard]] explicit operator ::AABB() const;
    [[nodiscard]] explicit operator Array() const;
    [[nodiscard]] explicit operator Basis() const;
    [[nodiscard]] explicit operator Color() const;
    [[nodiscard]] explicit operator Dictionary() const;
    [[nodiscard]] explicit operator IP_Address() const;
    [[nodiscard]] explicit operator NodePath() const;
    [[nodiscard]] explicit operator Object *() const;
    [[nodiscard]] explicit operator GameEntity() const;
    [[nodiscard]] explicit operator RenderingEntity() const;
    [[nodiscard]] explicit operator Plane() const;
    [[nodiscard]] explicit operator PoolVector<Color>() const;
    [[nodiscard]] explicit operator PoolVector<Face3>() const;
    [[nodiscard]] explicit operator PoolVector<Plane>() const;
    [[nodiscard]] explicit operator PoolVector<RID>() const;
    [[nodiscard]] explicit operator PoolVector<String>() const;
    [[nodiscard]] explicit operator PoolVector<Vector2>() const;
    [[nodiscard]] explicit operator PoolVector<Vector3>() const;
    [[nodiscard]] explicit operator PoolVector<float>() const;
    [[nodiscard]] explicit operator PoolVector<int>() const;
    [[nodiscard]] explicit operator PoolVector<uint8_t>() const;
    [[nodiscard]] explicit operator QChar() const;
    [[nodiscard]] explicit operator Quat() const;
    [[nodiscard]] explicit operator RID() const;
    [[nodiscard]] explicit operator Rect2() const;
    [[nodiscard]] explicit operator RefPtr() const;
    [[nodiscard]] explicit operator Span<const Vector2>() const;
    [[nodiscard]] explicit operator Span<const Vector3>() const;
    [[nodiscard]] explicit operator Span<const Color>() const;
    [[nodiscard]] explicit operator Span<const float>() const;
    [[nodiscard]] explicit operator Span<const int>() const;
    [[nodiscard]] explicit operator Span<const uint8_t>() const;
    [[nodiscard]] explicit operator String() const;
    [[nodiscard]] explicit operator StringName() const;
    [[nodiscard]] explicit operator StringView() const;
    [[nodiscard]] explicit operator Transform() const;
    [[nodiscard]] explicit operator Transform2D() const;
    [[nodiscard]] explicit operator UIString() const;
    [[nodiscard]] explicit operator Vector2() const;
    [[nodiscard]] explicit operator Vector3() const;
    [[nodiscard]] explicit operator Vector<Color>() const;
    [[nodiscard]] explicit operator Vector<Plane>() const;
    [[nodiscard]] explicit operator Vector<RID>() const;
    [[nodiscard]] explicit operator Vector<String>() const;
    [[nodiscard]] explicit operator Vector<Variant>() const;
    [[nodiscard]] explicit operator Vector<Vector2>() const;
    [[nodiscard]] explicit operator Vector<Vector3>() const;
    [[nodiscard]] explicit operator Vector<float>() const;
    [[nodiscard]] explicit operator Vector<int>() const;
    [[nodiscard]] explicit operator Vector<uint8_t>() const;
    [[nodiscard]] explicit operator double() const;
    [[nodiscard]] explicit operator float() const;
    [[nodiscard]] explicit operator int64_t() const;
    [[nodiscard]] explicit operator signed char() const;
    [[nodiscard]] explicit operator signed int() const;
    [[nodiscard]] explicit operator signed short() const;
    [[nodiscard]] explicit operator uint64_t() const;
    [[nodiscard]] explicit operator unsigned char() const;
    [[nodiscard]] explicit operator unsigned int() const; // this is the real one
    [[nodiscard]] explicit operator unsigned short() const;
    [[nodiscard]] explicit operator bool() const { return booleanize();  }
//    [[nodiscard]] explicit operator Control *() const;
//    [[nodiscard]] explicit operator Node *() const;
    [[nodiscard]] explicit operator Callable() const;
    [[nodiscard]] explicit operator Signal() const;
    template<typename E, eastl::enable_if_t<eastl::is_enum<E>::value>* = nullptr>
    [[nodiscard]] explicit operator E() const { return (E)((eastl::underlying_type_t<E>)*this); }
    template<typename E, eastl::enable_if_t< eastl::is_pointer_v<E> >* = nullptr>
    [[nodiscard]] explicit operator E() const { return object_cast<eastl::remove_pointer_t<E>>((Object *)(*this)); }

};
static constexpr int longest_variant_type_name=16;
//! Fill correctly sized char buffer with all variant names
void fill_with_all_variant_types(const char *nillname, char (&s)[7+(longest_variant_type_name+1)*int8_t(VariantType::VARIANT_MAX)]);

Vector<Variant> varray(std::initializer_list<Variant> v = {});
inline Vector<Variant> varray(const Variant &p_arg1) { return varray({p_arg1}); }
inline Vector<Variant> varray(const Variant &p_arg1, const Variant &p_arg2) { return varray({ p_arg1,p_arg2 }); }
inline Vector<Variant> varray(const Variant &p_arg1, const Variant &p_arg2, const Variant &p_arg3) { return varray({ p_arg1,p_arg2,p_arg3 }); }
inline Vector<Variant> varray(const Variant &p_arg1, const Variant &p_arg2, const Variant &p_arg3, const Variant &p_arg4) { return varray({ p_arg1,p_arg2,p_arg3,p_arg4 }); }
inline Vector<Variant> varray(const Variant &p_arg1, const Variant &p_arg2, const Variant &p_arg3, const Variant &p_arg4, const Variant &p_arg5) { return varray({ p_arg1,p_arg2,p_arg3,p_arg4,p_arg5 }); }

namespace eastl {
    template<>
    struct hash<Variant> {
        size_t operator()(const Variant &p_variant) const {
            return p_variant.hash();
        }
    };
}

template<>
struct Hasher<Variant> {

    uint32_t operator()(const Variant &p_variant) const { return p_variant.hash(); }
};

struct VariantComparator {

    static _FORCE_INLINE_ bool compare(const Variant &p_lhs, const Variant &p_rhs) { return p_lhs.hash_compare(p_rhs); }
    _FORCE_INLINE_ bool operator()(const Variant &p_lhs, const Variant &p_rhs) const { return p_lhs.hash_compare(p_rhs); }
};

Variant::ObjData &Variant::_get_obj() {

    return *reinterpret_cast<ObjData *>(&_data._mem[0]);
}

const Variant::ObjData &Variant::_get_obj() const {

    return *reinterpret_cast<const ObjData *>(&_data._mem[0]);
}

// All `as` overloads returing a Span are restricted to no-conversion/no-allocation cases.
// some core type enums to convert to

template<class T>
constexpr T variantAs(const Variant &f) {
    return (T)f;
}

template <> GODOT_EXPORT Variant Variant::from(const PoolVector<RID> &p_array);

template <> GODOT_EXPORT Variant Variant::from(const Vector<String> &);
template <> GODOT_EXPORT Variant Variant::from(const Vector<StringView> &);
template <> GODOT_EXPORT Variant Variant::from(const Vector<StringName> &);
template <> GODOT_EXPORT Variant Variant::from(const Vector<Variant> &);
template <> GODOT_EXPORT Variant Variant::from(const Frustum &);
template <> GODOT_EXPORT Variant Variant::from(const Span<const Vector2> &);
template <> GODOT_EXPORT Variant Variant::from(const Span<const Vector3> &);

template <> GODOT_EXPORT Variant Variant::move_from(Vector<Variant> &&);
struct GODOT_EXPORT VariantOps {
    static void resize(Variant& arg, int new_size);
    static int size(const Variant& arg);
    static Variant duplicate(const Variant& arg,bool deep=false);
    static void remove(Variant& arg, int idx);
    static void insert(Variant& arg, int idx, Variant &&value);
};

extern const Vector<Variant> null_variant_pvec;
