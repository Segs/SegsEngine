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
#include "core/math/math_defs.h"
#include "core/forward_decls.h"
#include "core/ref_ptr.h"
#include "core/vector.h"
#include "core/array.h"
#include "core/callable.h"
#include "core/object_id.h"
#include "core/object_rc.h"

#include <cstdint>
#include "EASTL/type_traits.h"


class Object;
class ObjectRC;
class Node; // helper
class Control; // helper
using UIString = class QString;
class RID;
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

using PoolByteArray = PoolVector<uint8_t>;
using PoolIntArray = PoolVector<int>;
using PoolRealArray = PoolVector<real_t>;
//using PoolStringArray = PoolVector<UIString>;
using PoolStringArray = PoolVector<String>;
using PoolVector2Array = PoolVector<Vector2>;
using PoolVector3Array = PoolVector<Vector3>;
using PoolColorArray = PoolVector<Color>;

#ifdef DEBUG_ENABLED
// Ideally, an inline member of ObjectRC, but would cause circular includes
#define _OBJ_PTR(m_variant) ((m_variant)._get_obj().rc ? (m_variant)._get_obj().rc->get_ptr() : reinterpret_cast<Ref<RefCounted> *>((m_variant)._get_obj().ref.get())->get())
#else
#define _OBJ_PTR(m_variant) ((m_variant)._get_obj().obj)
#endif

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
    NODE_PATH, // 15
    _RID,
    OBJECT,
    DICTIONARY,
    ARRAY,

    // arrays
    POOL_BYTE_ARRAY, // 20
    POOL_INT_ARRAY,
    POOL_REAL_ARRAY,
    POOL_STRING_ARRAY,
    POOL_VECTOR2_ARRAY,
    POOL_VECTOR3_ARRAY, // 25
    POOL_COLOR_ARRAY,

    VARIANT_MAX

};

class GODOT_EXPORT Variant {
private:
    friend struct _VariantCall;
    // Variant takes 20 bytes when real_t is float, and 36 if double
    // it only allocates extra memory for aabb/matrix.

    VariantType type;

    struct ObjData {

#ifdef DEBUG_ENABLED
        // Will be null for every type deriving from Reference as they have their
        // own reference count mechanism
        ObjectRC *rc;
#else
        Object *obj;
#endif
        // Always initialized, but will be null if the Ref<> assigned was null
        // or this Variant is not even holding a Reference-derived object
        RefPtr ref;
    };

    _FORCE_INLINE_ ObjData &_get_obj();
    _FORCE_INLINE_ const ObjData &_get_obj() const;

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

public:
    static const Variant null_variant;
    _FORCE_INLINE_ VariantType get_type() const { return type; }
    static const char *get_type_name(VariantType p_type);
    static StringName interned_type_name(VariantType p_type);
    static bool can_convert(VariantType p_type_from, VariantType p_type_to);
    static bool can_convert_strict(VariantType p_type_from, VariantType p_type_to);

    [[nodiscard]] bool is_ref() const;
    _FORCE_INLINE_ bool is_num() const { return type == VariantType::INT || type == VariantType::FLOAT; }
    _FORCE_INLINE_ bool is_array() const { return type >= VariantType::ARRAY; }
    [[nodiscard]] bool is_shared() const;
    [[nodiscard]] bool is_zero() const;
    [[nodiscard]] bool is_one() const;

    operator bool() const { return booleanize(); }
    operator signed int() const;
    operator unsigned int() const; // this is the real one
    operator signed short() const;
    operator unsigned short() const;
    operator signed char() const;
    operator unsigned char() const;
    //operator long unsigned int() const;
    operator int64_t() const;
    operator uint64_t() const;

    template <typename T>
    [[nodiscard]] T as() const {
        return (T)*this;
    }
    template<class T>
    struct asHelper {
        T convertIt(const Variant &v)  {
            return v.as<T>();
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
    [[nodiscard]] T asT() const {
        return asHelper<T>().convertIt(*this);
    }
    template <typename T>
    [[nodiscard]] Vector<T> asVector() const;
    // Not a recursive loop, as<String>,as<float>,as<StringName> are specialized.
    operator UIString() const;
    operator String() const;
    operator float() const;
    operator StringName() const;

    operator Vector2() const;
    operator Rect2() const;
    operator Vector3() const;
    operator Plane() const;
    operator ::AABB() const;
    operator Quat() const;
    operator Basis() const;
    operator Transform() const;
    operator Transform2D() const;

    operator Color() const;
    operator NodePath() const;
    operator RefPtr() const;
    operator RID() const;

    operator Node *() const;
    operator Control *() const;
    operator Object *() const;

    operator Dictionary() const;
    operator Array() const;

    operator PoolVector<uint8_t>() const;
    operator PoolVector<int>() const;
    operator PoolVector<real_t>() const;
    operator PoolVector<String>() const;
    operator PoolVector<Vector2>() const;
    operator PoolVector<Vector3>() const;
    operator PoolVector<Color>() const;
    operator PoolVector<Plane>() const;
    operator PoolVector<Face3>() const;

    // some core type enums to convert to
    operator Margin() const;
    operator Orientation() const;

    operator IP_Address() const;
    //NOTE: Code below is convoluted to prevent implicit bool conversions from all bool convertible types.
    template<class T ,
               class = typename eastl::enable_if<eastl::is_same<bool,T>::value>::type >
    Variant(T p_bool) {
        type = VariantType::BOOL;
        _data._bool = p_bool;
    }
    template<class T ,
               class = typename eastl::enable_if<eastl::is_enum_v<T>>::type >
    Variant(T p_bool,int=0) : Variant(eastl::underlying_type_t<T>(p_bool)){
    }
    //Variant(VariantType p_v) : type(p_v) {}
    Variant(VariantType p_v,VariantUnion u) : type(p_v), _data(u) {}

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

    template<class T>
    static Variant from(const T &v) {
        return Variant(v);
    }
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

    explicit Variant(const IP_Address &p_address);

    // If this changes the table in variant_op must be updated
    enum Operator {

        //comparison
        OP_EQUAL,
        OP_NOT_EQUAL,
        OP_LESS,
        OP_LESS_EQUAL,
        OP_GREATER,
        OP_GREATER_EQUAL,
        //mathematic
        OP_ADD,
        OP_SUBTRACT,
        OP_MULTIPLY,
        OP_DIVIDE,
        OP_NEGATE,
        OP_POSITIVE,
        OP_MODULE,
        OP_STRING_CONCAT,
        //bitwise
        OP_SHIFT_LEFT,
        OP_SHIFT_RIGHT,
        OP_BIT_AND,
        OP_BIT_OR,
        OP_BIT_XOR,
        OP_BIT_NEGATE,
        //logic
        OP_AND,
        OP_OR,
        OP_XOR,
        OP_NOT,
        //containment
        OP_IN,
        OP_MAX

    };

    static const char * get_operator_name(Operator p_op);
    static void evaluate(Operator p_op, const Variant &p_a, const Variant &p_b, Variant &r_ret, bool &r_valid);
    static _FORCE_INLINE_ Variant evaluate(Operator p_op, const Variant &p_a, const Variant &p_b) {

        bool valid = true;
        Variant res;
        evaluate(p_op, p_a, p_b, res, valid);
        return res;
    }

    void zero();
    [[nodiscard]] Variant duplicate(bool deep = false) const;
    static void blend(const Variant &a, const Variant &b, float c, Variant &r_dst);
    static void interpolate(const Variant &a, const Variant &b, float c, Variant &r_dst);

    void call_ptr(const StringName &p_method, const Variant **p_args, int p_argcount, Variant *r_ret, Callable::CallError &r_error);
    Variant call(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error);
    Variant call(const StringName &p_method, const Variant &p_arg1 = Variant(), const Variant &p_arg2 = Variant(), const Variant &p_arg3 = Variant(), const Variant &p_arg4 = Variant(), const Variant &p_arg5 = Variant());

    static String get_call_error_text(Object *p_base, const StringName &p_method, const Variant **p_argptrs, int p_argcount, const Callable::CallError &ce);

    static Variant construct(const VariantType, const Variant **p_args, int p_argcount, Callable::CallError &r_error, bool p_strict = true);

    void get_method_list(Vector<MethodInfo> *p_list) const;
    bool has_method(const StringName &p_method) const;
    static Span<const VariantType> get_method_argument_types(VariantType p_type, const StringName &p_method);
    static Span<const Variant> get_method_default_arguments(VariantType p_type, const StringName &p_method);
    static VariantType get_method_return_type(VariantType p_type, const StringName &p_method, bool *r_has_return = nullptr);
    static Span<const StringView> get_method_argument_names(VariantType p_type, const StringName &p_method);
    static bool is_method_const(VariantType p_type, const StringName &p_method);

    void set_named(const StringName &p_index, const Variant &p_value, bool *r_valid = nullptr);
    Variant get_named(const StringName &p_index, bool *r_valid = nullptr) const;

    void set(const Variant &p_index, const Variant &p_value, bool *r_valid = nullptr);
    Variant get(const Variant &p_index, bool *r_valid = nullptr) const;
    bool in(const Variant &p_index, bool *r_valid = nullptr) const;

    bool iter_init(Variant &r_iter, bool &r_valid) const;
    bool iter_next(Variant &r_iter, bool &r_valid) const;
    Variant iter_get(const Variant &r_iter, bool &r_valid) const;

    void get_property_list(Vector<PropertyInfo> *p_list) const;

    //argsVariant call()

    bool operator==(const Variant &p_variant) const;
    bool operator!=(const Variant &p_variant) const;
    bool operator<(const Variant &p_variant) const;
    [[nodiscard]] uint32_t hash() const;

    [[nodiscard]] bool hash_compare(const Variant &p_variant) const;
    bool booleanize() const;
    String stringify(Vector<const void *> &stack) const;

    void static_assign(const Variant &p_variant);
    static void get_constructor_list(VariantType p_type, Vector<MethodInfo> *p_list);
    static void get_constants_for_type(VariantType p_type, Vector<StringName> *p_constants);
    static bool has_constant(VariantType p_type, const StringName &p_value);
    static Variant get_constant_value(VariantType p_type, const StringName &p_value, bool *r_valid = nullptr);

    using ObjectDeConstruct = UIString (*)(const Variant &, void *);
    using ObjectConstruct = void (*)(const UIString &, void *, Variant &);

    [[nodiscard]] String get_construct_string() const;
    static void construct_from_string(const UIString &p_string, Variant &r_value, ObjectConstruct p_obj_construct = nullptr, void *p_construct_ud = nullptr);

    Variant &operator=(const Variant &p_variant); // only this is enough for all the other types
    Variant(const Variant &p_variant);
    constexpr Variant(Variant && oth) noexcept : type(oth.type),_data(oth._data) {
        oth.type = VariantType::NIL;
        //NOTE: oth._data is not cleared here since setting type to NIL should suffice.
    }
    constexpr Variant() : type(VariantType::NIL) {}
    _FORCE_INLINE_ ~Variant() {
        if (type != VariantType::NIL) clear();
    }
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

GODOT_EXPORT String vformat(StringView p_text, const Variant &p1 = Variant(), const Variant &p2 = Variant(), const Variant &p3 = Variant(), const Variant &p4 = Variant(), const Variant &p5 = Variant());

template <> GODOT_EXPORT UIString Variant::as<UIString>() const;
template <> GODOT_EXPORT String Variant::as<String>() const;
template <> GODOT_EXPORT StringView Variant::as<StringView>() const;
template <> GODOT_EXPORT StringName Variant::as<StringName>() const;
template <> GODOT_EXPORT float Variant::as<float>() const;
template <> GODOT_EXPORT double Variant::as<double>() const;
template <> GODOT_EXPORT QChar Variant::as<QChar>() const;
template <> GODOT_EXPORT NodePath Variant::as<NodePath>() const;
template <> GODOT_EXPORT IP_Address Variant::as<IP_Address>() const;
template <> GODOT_EXPORT Transform Variant::as<Transform>() const;
template <> GODOT_EXPORT Basis Variant::as<Basis>() const;
template <> GODOT_EXPORT Quat Variant::as<Quat>() const;
template <> GODOT_EXPORT ObjectID Variant::as<ObjectID>() const;

template <> GODOT_EXPORT PoolVector<String> Variant::as<PoolVector<String>>() const;
template <> GODOT_EXPORT PoolVector<RID> Variant::as<PoolVector<RID>>() const;

template <> GODOT_EXPORT Vector<String> Variant::as<Vector<String>>() const;
template <> GODOT_EXPORT Vector<uint8_t> Variant::as<Vector<uint8_t>>() const;
template <> GODOT_EXPORT Vector<int> Variant::asVector<int>() const;
template <> GODOT_EXPORT Vector<Plane> Variant::asVector<Plane>() const;

// All `as` overloads returing a Span are restricted to no-conversion/no-allocation cases.
template <> GODOT_EXPORT Span<const uint8_t> Variant::as<Span<const uint8_t>>() const;
template <> GODOT_EXPORT Span<const int> Variant::as<Span<const int>>() const;
template <> GODOT_EXPORT Span<const float> Variant::as<Span<const float>>() const;
template <> GODOT_EXPORT Span<const Vector2> Variant::as<Span<const Vector2>>() const;
template <> GODOT_EXPORT Span<const Vector3> Variant::as<Span<const Vector3>>() const;

template <> GODOT_EXPORT Variant Variant::from(const PoolVector<RID> &p_array);
template <> inline Variant Variant::from(const ObjectID &ob) { return {VariantType::INT,VariantUnion((uint64_t)ob)}; }

template <> GODOT_EXPORT Variant Variant::from(const Vector<String> &);
template <> GODOT_EXPORT Variant Variant::from(const Vector<StringView> &);
template <> GODOT_EXPORT Variant Variant::from(const Vector<StringName> &);
template <> GODOT_EXPORT Variant Variant::from(const Vector<Variant> &);
template <> GODOT_EXPORT Variant Variant::from(const Frustum &p_array);
template <> GODOT_EXPORT Variant Variant::from(const Span<const Vector2> &);
template <> GODOT_EXPORT Variant Variant::from(const Span<const Vector3> &);

template <> GODOT_EXPORT Variant Variant::move_from(Vector<Variant> &&);

template<class T>
Vector<T> asVec(const Array &a) {
    Vector<T> res;
    res.reserve(a.size());
    for(int i=0,fin=a.size(); i<fin; ++i)
        res.emplace_back(a.get(i).as<T>());
    return res;
}
template<class T>
PoolVector<T> asPool(const Array &a) {
    PoolVector<T> res;
    for(int i=0,fin=a.size(); i<fin; ++i)
        res.push_back(a.get(i).as<T>());
    return res;
}
