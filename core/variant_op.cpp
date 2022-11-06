/*************************************************************************/
/*  variant_op.cpp                                                       */
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
#define QT_NO_META_MACROS
#include "variant.h"

#include "core/pool_vector.h"
#include "core/color.h"
#include "core/core_string_names.h"
#include "core/debugger/script_debugger.h"
#include "core/dictionary.h"
#include "core/list.h"
#include "core/math/aabb.h"
#include "core/math/basis.h"
#include "core/math/face3.h"
#include "core/math/plane.h"
#include "core/math/quat.h"
#include "core/math/transform.h"
#include "core/math/transform_2d.h"
#include "core/math/vector3.h"
#include "core/math/math_funcs.h"
#include "core/node_path.h"
#include "core/object.h"
#include "core/object_rc.h"
#include "core/ustring.h"
#include "core/object_db.h"
#include "core/script_language.h"
#include "core/rid.h"

#define CASE_TYPE_ALL(PREFIX, OP) \
    CASE_TYPE(PREFIX, OP, INT)    \
    CASE_TYPE_ALL_BUT_INT(PREFIX, OP)

#define CASE_TYPE_ALL_BUT_INT(PREFIX, OP)     \
    CASE_TYPE(PREFIX, OP, NIL)                \
    CASE_TYPE(PREFIX, OP, BOOL)               \
    CASE_TYPE(PREFIX, OP, FLOAT)              \
    CASE_TYPE(PREFIX, OP, STRING)             \
    CASE_TYPE(PREFIX, OP, VECTOR2)            \
    CASE_TYPE(PREFIX, OP, RECT2)              \
    CASE_TYPE(PREFIX, OP, VECTOR3)            \
    CASE_TYPE(PREFIX, OP, TRANSFORM2D)        \
    CASE_TYPE(PREFIX, OP, PLANE)              \
    CASE_TYPE(PREFIX, OP, QUAT)               \
    CASE_TYPE(PREFIX, OP, AABB)               \
    CASE_TYPE(PREFIX, OP, BASIS)              \
    CASE_TYPE(PREFIX, OP, TRANSFORM)          \
    CASE_TYPE(PREFIX, OP, COLOR)              \
    CASE_TYPE(PREFIX, OP, STRING_NAME)        \
    CASE_TYPE(PREFIX, OP, NODE_PATH)          \
    CASE_TYPE(PREFIX, OP, _RID)               \
    CASE_TYPE(PREFIX, OP, OBJECT)             \
    CASE_TYPE(PREFIX, OP, CALLABLE)           \
    CASE_TYPE(PREFIX, OP, SIGNAL)             \
    CASE_TYPE(PREFIX, OP, DICTIONARY)         \
    CASE_TYPE(PREFIX, OP, ARRAY)              \
    CASE_TYPE(PREFIX, OP, POOL_BYTE_ARRAY)    \
    CASE_TYPE(PREFIX, OP, POOL_INT_ARRAY)     \
    CASE_TYPE(PREFIX, OP, POOL_FLOAT32_ARRAY)    \
    CASE_TYPE(PREFIX, OP, POOL_STRING_ARRAY)  \
    CASE_TYPE(PREFIX, OP, POOL_VECTOR2_ARRAY) \
    CASE_TYPE(PREFIX, OP, POOL_VECTOR3_ARRAY) \
    CASE_TYPE(PREFIX, OP, POOL_COLOR_ARRAY)

#ifdef __GNUC__
#define TYPE(PREFIX, OP, TYPE) &&PREFIX##_##OP##_##TYPE

/* clang-format off */
#define TYPES(PREFIX, OP) {                   \
        TYPE(PREFIX, OP, NIL),                \
        TYPE(PREFIX, OP, BOOL),               \
        TYPE(PREFIX, OP, INT),                \
        TYPE(PREFIX, OP, FLOAT),              \
        TYPE(PREFIX, OP, STRING),             \
        TYPE(PREFIX, OP, VECTOR2),            \
        TYPE(PREFIX, OP, RECT2),              \
        TYPE(PREFIX, OP, VECTOR3),            \
        TYPE(PREFIX, OP, TRANSFORM2D),        \
        TYPE(PREFIX, OP, PLANE),              \
        TYPE(PREFIX, OP, QUAT),               \
        TYPE(PREFIX, OP, AABB),               \
        TYPE(PREFIX, OP, BASIS),              \
        TYPE(PREFIX, OP, TRANSFORM),          \
        TYPE(PREFIX, OP, COLOR),              \
        TYPE(PREFIX, OP, STRING_NAME),        \
        TYPE(PREFIX, OP, NODE_PATH),          \
        TYPE(PREFIX, OP, _RID),               \
        TYPE(PREFIX, OP, OBJECT),             \
        TYPE(PREFIX, OP, CALLABLE),           \
        TYPE(PREFIX, OP, SIGNAL),             \
        TYPE(PREFIX, OP, DICTIONARY),         \
        TYPE(PREFIX, OP, ARRAY),              \
        TYPE(PREFIX, OP, POOL_BYTE_ARRAY),    \
        TYPE(PREFIX, OP, POOL_INT_ARRAY),     \
        TYPE(PREFIX, OP, POOL_FLOAT32_ARRAY),    \
        TYPE(PREFIX, OP, POOL_STRING_ARRAY),  \
        TYPE(PREFIX, OP, POOL_VECTOR2_ARRAY), \
        TYPE(PREFIX, OP, POOL_VECTOR3_ARRAY), \
        TYPE(PREFIX, OP, POOL_COLOR_ARRAY),   \
}
/* clang-format on */

#define CASES(PREFIX) static const void *switch_table_##PREFIX[Variant::OP_MAX][(int)VariantType::VARIANT_MAX] = { \
    TYPES(PREFIX, OP_LESS),                                                \
}

#define SWITCH(PREFIX, op, val) goto *switch_table_##PREFIX[op][int(val)];
#define SWITCH_OP(PREFIX, OP, val)
#define CASE_TYPE(PREFIX, OP, TYPE) PREFIX##_##OP##_##TYPE:

#else
#define CASES(PREFIX)
#define SWITCH(PREFIX, op, val) switch (op)
#define SWITCH_OP(PREFIX, OP, val) \
    case OP:                       \
        switch (val)
#define CASE_TYPE(PREFIX, OP, TYPE) case VariantType::TYPE:
#endif

// We consider all uninitialized or empty types to be false based on the type's
// zeroiness.
bool Variant::booleanize() const {
    return !is_zero();
}

#define _RETURN(m_what) \
    {                   \
        r_ret = Variant(m_what); \
        return;         \
    }

#define _RETURN_FAIL     \
    {                    \
        r_valid = false; \
        return;          \
    }

#define DEFAULT_OP_NUM(m_prefix, m_op_name, m_name, m_op, m_type)             \
    CASE_TYPE(m_prefix, m_op_name, m_name) {                                  \
        if (p_b.type == VariantType::INT) _RETURN(p_a._data.m_type m_op p_b._data._int);   \
        if (p_b.type == VariantType::FLOAT) _RETURN(p_a._data.m_type m_op p_b._data._real); \
                                                                              \
        _RETURN_FAIL                                                          \
    }

#define DEFAULT_OP_NUM_NULL(m_prefix, m_op_name, m_name, m_op, m_type)        \
    CASE_TYPE(m_prefix, m_op_name, m_name) {                                  \
        if (p_b.type == VariantType::INT) _RETURN(p_a._data.m_type m_op p_b._data._int);   \
        if (p_b.type == VariantType::FLOAT) _RETURN(p_a._data.m_type m_op p_b._data._real); \
        if (p_b.type == VariantType::NIL) _RETURN(!(p_b.type m_op VariantType::NIL));                   \
                                                                              \
        _RETURN_FAIL                                                          \
    }

#define DEFAULT_OP_STR_NULL(m_prefix, m_op_name, m_name, m_op, m_type)                                                             \
    CASE_TYPE(m_prefix, m_op_name, m_name) {                                                                                       \
        if (p_b.type == VariantType::STRING)                                                                                                    \
            _RETURN((String)*reinterpret_cast<const m_type *>(p_a._data._mem) m_op *reinterpret_cast<const String *>(p_b._data._mem));     \
        if (p_b.type == VariantType::STRING_NAME)                                                                                               \
            _RETURN((String)*reinterpret_cast<const m_type *>(p_a._data._mem) m_op (String)*reinterpret_cast<const StringName *>(p_b._data._mem)); \
        if (p_b.type == VariantType::NODE_PATH)                                                                                                 \
            _RETURN((String)*reinterpret_cast<const m_type *>(p_a._data._mem) m_op (String)*reinterpret_cast<const NodePath *>(p_b._data._mem));   \
        if (p_b.type == VariantType::NIL)                                                                                                       \
            _RETURN(!(p_b.type m_op VariantType::NIL));                                                                                         \
                                                                                                                                   \
        _RETURN_FAIL                                                                                                               \
    }

#define DEFAULT_OP_STR_NULL_NP(m_prefix, m_op_name, m_name, m_op, m_type)                                                        \
    CASE_TYPE(m_prefix, m_op_name, m_name) {                                                                                     \
        if (p_b.type == VariantType::STRING)                                                                                                  \
            _RETURN(*reinterpret_cast<const m_type *>(p_a._data._mem) m_op *reinterpret_cast<const String *>(p_b._data._mem));   \
        if (p_b.type == VariantType::NODE_PATH)                                                                                               \
            _RETURN(*reinterpret_cast<const m_type *>(p_a._data._mem) m_op (String)*reinterpret_cast<const NodePath *>(p_b._data._mem)); \
        if (p_b.type == VariantType::NIL)                                                                                                     \
            _RETURN(!(p_b.type m_op VariantType::NIL));                                                                                       \
                                                                                                                                 \
        _RETURN_FAIL                                                                                                             \
    }

#define DEFAULT_OP_STR_NULL_SN(m_prefix, m_op_name, m_name, m_op, m_type)                                                          \
    CASE_TYPE(m_prefix, m_op_name, m_name) {                                                                                       \
        if (p_b.type == VariantType::STRING)                                                                                                    \
            _RETURN(*reinterpret_cast<const m_type *>(p_a._data._mem) m_op *reinterpret_cast<const String *>(p_b._data._mem));     \
        if (p_b.type == VariantType::STRING_NAME)                                                                                               \
            _RETURN(*reinterpret_cast<const m_type *>(p_a._data._mem) m_op (String)*reinterpret_cast<const StringName *>(p_b._data._mem)); \
        if (p_b.type == VariantType::NIL)                                                                                                       \
            _RETURN(!(p_b.type m_op VariantType::NIL));                                                                                         \
                                                                                                                                   \
        _RETURN_FAIL                                                                                                               \
    }


#define DEFAULT_OP_LOCALMEM_REV(m_prefix, m_op_name, m_name, m_op, m_type)                                                     \
    CASE_TYPE(m_prefix, m_op_name, m_name) {                                                                                   \
        if (p_b.type == VariantType::m_name)                                                                                                \
            _RETURN(*reinterpret_cast<const m_type *>(p_b._data._mem) m_op *reinterpret_cast<const m_type *>(p_a._data._mem)); \
                                                                                                                               \
        _RETURN_FAIL                                                                                                           \
    }

#define DEFAULT_OP_LOCALMEM(m_prefix, m_op_name, m_name, m_op, m_type)                                                         \
    CASE_TYPE(m_prefix, m_op_name, m_name) {                                                                                   \
        if (p_b.type == VariantType::m_name)                                                                                                \
            _RETURN(*reinterpret_cast<const m_type *>(p_a._data._mem) m_op *reinterpret_cast<const m_type *>(p_b._data._mem)); \
                                                                                                                               \
        _RETURN_FAIL                                                                                                           \
    }

#define DEFAULT_OP_LOCALMEM_NULL(m_prefix, m_op_name, m_name, m_op, m_type)                                                    \
    CASE_TYPE(m_prefix, m_op_name, m_name) {                                                                                   \
        if (p_b.type == VariantType::m_name)                                                                                                \
            _RETURN(*reinterpret_cast<const m_type *>(p_a._data._mem) m_op *reinterpret_cast<const m_type *>(p_b._data._mem)); \
        if (p_b.type == VariantType::NIL)                                                                                                   \
            _RETURN(!(p_b.type m_op VariantType::NIL));                                                                                     \
                                                                                                                               \
        _RETURN_FAIL                                                                                                           \
    }

#define DEFAULT_OP_PTRREF_NULL(m_prefix, m_op_name, m_name, m_op, m_sub) \
    CASE_TYPE(m_prefix, m_op_name, m_name) {                             \
        if (p_b.type == VariantType::m_name)                                          \
            _RETURN(*p_a._data.m_sub m_op *p_b._data.m_sub);             \
        if (p_b.type == VariantType::NIL)                                             \
            _RETURN(!(p_b.type m_op VariantType::NIL));                               \
                                                                         \
        _RETURN_FAIL                                                     \
    }

#define DEFAULT_OP_ARRAY_EQ(m_prefix, m_op_name, m_name, m_type)                                  \
    CASE_TYPE(m_prefix, m_op_name, m_name) {                                                      \
        if (p_b.type == VariantType::NIL)                                                                      \
            _RETURN(false)                                                                        \
        DEFAULT_OP_ARRAY_OP_BODY(m_prefix, m_op_name, m_name, m_type, !=, !=, true, false, false) \
    }

#define DEFAULT_OP_ARRAY_NEQ(m_prefix, m_op_name, m_name, m_type)                                \
    CASE_TYPE(m_prefix, m_op_name, m_name) {                                                     \
        if (p_b.type == VariantType::NIL)                                                                     \
            _RETURN(true)                                                                        \
        DEFAULT_OP_ARRAY_OP_BODY(m_prefix, m_op_name, m_name, m_type, !=, !=, false, true, true) \
    }

#define DEFAULT_OP_ARRAY_LT(m_prefix, m_op_name, m_name, m_type) \
    DEFAULT_OP_ARRAY_OP(m_prefix, m_op_name, m_name, m_type, <, !=, false, a_len < array_b.size(), true)

#define DEFAULT_OP_ARRAY_GT(m_prefix, m_op_name, m_name, m_type) \
    DEFAULT_OP_ARRAY_OP(m_prefix, m_op_name, m_name, m_type, >, !=, false, a_len < array_b.size(), true)

#define DEFAULT_OP_ARRAY_OP(m_prefix, m_op_name, m_name, m_type, m_opa, m_opb, m_ret_def, m_ret_s, m_ret_f)      \
    CASE_TYPE(m_prefix, m_op_name, m_name) {                                                                     \
        DEFAULT_OP_ARRAY_OP_BODY(m_prefix, m_op_name, m_name, m_type, m_opa, m_opb, m_ret_def, m_ret_s, m_ret_f) \
    }

#define DEFAULT_OP_ARRAY_OP_BODY(m_prefix, m_op_name, m_name, m_type, m_opa, m_opb, m_ret_def, m_ret_s, m_ret_f) \
    if (p_a.type != p_b.type)                                                                                    \
        _RETURN_FAIL                                                                                             \
                                                                                                                 \
    const PoolVector<m_type> &array_a = *reinterpret_cast<const PoolVector<m_type> *>(p_a._data._mem);           \
    const PoolVector<m_type> &array_b = *reinterpret_cast<const PoolVector<m_type> *>(p_b._data._mem);           \
                                                                                                                 \
    int a_len = array_a.size();                                                                                  \
    if (a_len m_opa array_b.size()) {                                                                            \
        _RETURN(m_ret_s);                                                                                        \
    } else {                                                                                                     \
                                                                                                                 \
        PoolVector<m_type>::Read ra = array_a.read();                                                            \
        PoolVector<m_type>::Read rb = array_b.read();                                                            \
                                                                                                                 \
        for (int i = 0; i < a_len; i++) {                                                                        \
            if (ra[i] m_opb rb[i])                                                                               \
                _RETURN(m_ret_f);                                                                                \
        }                                                                                                        \
                                                                                                                 \
        _RETURN(m_ret_def);                                                                                      \
    }

#define DEFAULT_OP_ARRAY_ADD(m_prefix, m_op_name, m_name, m_type)                                          \
    CASE_TYPE(m_prefix, m_op_name, m_name) {                                                               \
        if (p_a.type != p_b.type)                                                                          \
            _RETURN_FAIL;                                                                                  \
                                                                                                           \
        const PoolVector<m_type> &array_a = *reinterpret_cast<const PoolVector<m_type> *>(p_a._data._mem); \
        const PoolVector<m_type> &array_b = *reinterpret_cast<const PoolVector<m_type> *>(p_b._data._mem); \
        PoolVector<m_type> sum = array_a;                                                                  \
        sum.append_array(array_b);                                                                         \
        _RETURN(sum);                                                                                      \
    }

void Variant::evaluate(Operator p_op, const Variant &p_a, const Variant &p_b, Variant &r_ret, bool &r_valid) {

    CASES(math);
    r_valid = true;

    SWITCH(math, p_op, p_a.type) {

        SWITCH_OP(math, OP_LESS, p_a.type) {
            CASE_TYPE(math, OP_LESS, BOOL) {
                if (p_b.type != VariantType::BOOL)
                    _RETURN_FAIL

                _RETURN(p_a._data._bool < p_b._data._bool)
            }

            CASE_TYPE(math, OP_LESS, OBJECT) {
                if (p_b.type != VariantType::OBJECT)
                    _RETURN_FAIL
                _RETURN((_UNSAFE_OBJ_PROXY_PTR(p_a) < _UNSAFE_OBJ_PROXY_PTR(p_b)))
            }

            DEFAULT_OP_LOCALMEM_NULL(math, OP_LESS, CALLABLE, < , Callable);
            DEFAULT_OP_LOCALMEM_NULL(math, OP_LESS, SIGNAL, < , Signal);

            CASE_TYPE(math, OP_LESS, ARRAY) {
                if (p_b.type != VariantType::ARRAY)
                    _RETURN_FAIL

                const Array *arr_a = reinterpret_cast<const Array *>(p_a._data._mem);
                const Array *arr_b = reinterpret_cast<const Array *>(p_b._data._mem);

                int l = arr_a->size();
                if (arr_b->size() < l)
                    _RETURN(false)
                for (int i = 0; i < l; i++) {
                    if (!((*arr_a)[i] < (*arr_b)[i])) {
                        _RETURN(true)
                    }
                }

                _RETURN(false);
            }

            DEFAULT_OP_NUM(math, OP_LESS, INT, <, _int)
            DEFAULT_OP_NUM(math, OP_LESS, FLOAT, <, _real)
            CASE_TYPE(math, OP_LESS, STRING) {
                StringView self(*reinterpret_cast<const String *>(p_a._data._mem));

                if (p_b.type == VariantType::STRING) _RETURN(self.compare(*reinterpret_cast<const String *>(p_b._data._mem))<0);
                if (p_b.type == VariantType::NODE_PATH) _RETURN(self.compare((String)*reinterpret_cast<const NodePath *>(p_b._data._mem))<0)
                if (p_b.type == VariantType::STRING_NAME) _RETURN(self.compare(reinterpret_cast<const StringName *>(p_b._data._mem)->asCString())<0)

                _RETURN_FAIL
            }
            CASE_TYPE(math, OP_LESS, STRING_NAME) {
                StringView self(*reinterpret_cast<const StringName *>(p_a._data._mem));

                if (p_b.type == VariantType::STRING) _RETURN(self.compare(*reinterpret_cast<const String *>(p_b._data._mem))<0);
                if (p_b.type == VariantType::NODE_PATH) _RETURN(self.compare((String)*reinterpret_cast<const NodePath *>(p_b._data._mem))<0)
                if (p_b.type == VariantType::STRING_NAME) _RETURN(self.compare(reinterpret_cast<const StringName *>(p_b._data._mem)->asCString())<0)

                _RETURN_FAIL
            }
            DEFAULT_OP_LOCALMEM(math, OP_LESS, VECTOR2, <, Vector2)
            DEFAULT_OP_LOCALMEM(math, OP_LESS, VECTOR3, <, Vector3)
            //DEFAULT_OP_LOCALMEM(math, OP_LESS, _RID, <, RID)
            DEFAULT_OP_ARRAY_LT(math, OP_LESS, POOL_BYTE_ARRAY, uint8_t)
            DEFAULT_OP_ARRAY_LT(math, OP_LESS, POOL_INT_ARRAY, int)
            DEFAULT_OP_ARRAY_LT(math, OP_LESS, POOL_FLOAT32_ARRAY, real_t)
            DEFAULT_OP_ARRAY_LT(math, OP_LESS, POOL_STRING_ARRAY, String)
            DEFAULT_OP_ARRAY_LT(math, OP_LESS, POOL_VECTOR2_ARRAY, Vector3)
            DEFAULT_OP_ARRAY_LT(math, OP_LESS, POOL_VECTOR3_ARRAY, Vector3)
            DEFAULT_OP_ARRAY_LT(math, OP_LESS, POOL_COLOR_ARRAY, Color)

            CASE_TYPE(math, OP_LESS, _RID)
            CASE_TYPE(math, OP_LESS, NIL)
            CASE_TYPE(math, OP_LESS, RECT2)
            CASE_TYPE(math, OP_LESS, TRANSFORM2D)
            CASE_TYPE(math, OP_LESS, PLANE)
            CASE_TYPE(math, OP_LESS, QUAT)
            CASE_TYPE(math, OP_LESS, AABB)
            CASE_TYPE(math, OP_LESS, BASIS)
            CASE_TYPE(math, OP_LESS, TRANSFORM)
            CASE_TYPE(math, OP_LESS, COLOR)
            CASE_TYPE(math, OP_LESS, NODE_PATH)
            CASE_TYPE(math, OP_LESS, DICTIONARY)
            _RETURN_FAIL
        }
    }
}
namespace
{
    template<typename T>
    bool compare(const PoolVector<T> &a, const PoolVector<T>& b)
    {
        int a_len = a.size();
        if (a_len != b.size()) {
            return false;
        }
        typename PoolVector<T>::Read ra = a.read();
        typename PoolVector<T>::Read rb = b.read();
        for (int i = 0; i < a_len; i++)
        {
            if (ra[i] != rb[i]) {
                return false;
            }
        }
        return true;
    }
}

bool Variant::evaluate_equal(const Variant &p_a, const Variant &p_b) {
    switch (p_a.type) {
        case VariantType::NIL: {
            if (p_b.type == VariantType::NIL)
                return true;
            if (p_b.type == VariantType::OBJECT)
                return _UNSAFE_OBJ_PROXY_PTR(p_b) == nullptr;
            if (p_b.type == VariantType::CALLABLE)
                return (*reinterpret_cast<const Callable *>(p_b._data._mem)) == Callable();
            if (p_b.type == VariantType::SIGNAL)
                return (*reinterpret_cast<const Signal *>(p_b._data._mem)) == Signal();

            return false;
        }

        case VariantType::BOOL: {
            if (p_b.type != VariantType::BOOL) {
                if (p_b.type == VariantType::NIL)
                    return false;

                break;
            }

            return p_a._data._bool == p_b._data._bool;
        }

        case VariantType::OBJECT: {
            if (p_b.type == VariantType::OBJECT)
                return (_UNSAFE_OBJ_PROXY_PTR(p_a) == _UNSAFE_OBJ_PROXY_PTR(p_b));
            if (p_b.type == VariantType::NIL)
                return _UNSAFE_OBJ_PROXY_PTR(p_a) == nullptr;
            break;
        }
        case VariantType::CALLABLE: {
            const Callable &ca(*reinterpret_cast<const Callable *>(p_a._data._mem));
            if (p_b.type == VariantType::NIL)
                return ca == Callable();
            if (p_b.type == VariantType::CALLABLE)
                return ca == *reinterpret_cast<const Callable *>(p_b._data._mem);
            break;
        }
        case VariantType::SIGNAL: {
            const Signal &sa(*reinterpret_cast<const Signal *>(p_a._data._mem));
            if (p_b.type == VariantType::NIL)
                return sa == Signal();
            if (p_b.type == VariantType::SIGNAL)
                return sa == *reinterpret_cast<const Signal *>(p_b._data._mem);
            break;
        }

        case VariantType::DICTIONARY: {
            if (p_b.type == VariantType::NIL)
                return false;
            if (p_b.type != VariantType::DICTIONARY) {

                break;
            }

            const Dictionary *arr_a = reinterpret_cast<const Dictionary *>(p_a._data._mem);
            const Dictionary *arr_b = reinterpret_cast<const Dictionary *>(p_b._data._mem);

            return *arr_a == *arr_b;
        }

        case VariantType::ARRAY: {
            if (p_b.type == VariantType::NIL)
                return false;
            if (p_b.type != VariantType::ARRAY) {
                break;
            }
            const Array *arr_a = reinterpret_cast<const Array *>(p_a._data._mem);
            const Array *arr_b = reinterpret_cast<const Array *>(p_b._data._mem);

            int l = arr_a->size();
            if (arr_b->size() != l)
                return false;
            for (int i = 0; i < l; i++) {
                if (!((*arr_a)[i] == (*arr_b)[i])) {
                    return false;
                }
            }

            return true;
        }

        case VariantType::REN_ENT: {
            if (p_b.type == VariantType::NIL)
                return false;
            if (p_b.type == VariantType::REN_ENT)
                return p_a._data._int == p_b._data._int;
            break;
        }

        case VariantType::INT: {
            if (p_b.type == VariantType::NIL)
                return false;
            if (p_b.type == VariantType::INT)
                return p_a._data._int == p_b._data._int;
            if (p_b.type == VariantType::FLOAT)
                return p_a._data._int == p_b._data._real;

            break;
        }
        case VariantType::FLOAT: {
            if (p_b.type == VariantType::NIL)
                return false;
            if (p_b.type == VariantType::INT)
                return p_a._data._real == p_b._data._int;
            if (p_b.type == VariantType::FLOAT)
                return p_a._data._real == p_b._data._real;
            break;
        }
        case VariantType::STRING: {
            if (p_b.type == VariantType::NIL)
                return false;
            StringView self(*reinterpret_cast<const String *>(p_a._data._mem));
            if (p_b.type == VariantType::STRING)
                return self == *reinterpret_cast<const String *>(p_b._data._mem);
            if (p_b.type == VariantType::NODE_PATH)
                return self == (String)*reinterpret_cast<const NodePath *>(p_b._data.
                                                                               _mem);
            if (p_b.type == VariantType::STRING_NAME)
                return self == reinterpret_cast<const StringName *>(p_b._data._mem)
                       ->asCString();

            break;
        }
        case VariantType::STRING_NAME: {
            if (p_b.type == VariantType::NIL)
                return false;
            StringView self(*reinterpret_cast<const StringName *>(p_a._data._mem));
            if (p_b.type == VariantType::STRING)
                return self == *reinterpret_cast<const String *>(p_b._data._mem);
            if (p_b.type == VariantType::NODE_PATH)
                return self == (String)*reinterpret_cast<const NodePath *>(p_b._data.
                                                                               _mem);
            if (p_b.type == VariantType::STRING_NAME)
                return self == reinterpret_cast<const StringName *>(p_b._data._mem)
                       ->asCString();

            break;
        }

        case VariantType::VECTOR2: {
            if (p_b.type == VariantType::NIL)
                return false;
            if (p_b.type == VariantType::VECTOR2)
                return *reinterpret_cast<const Vector2 *>(p_a._data._mem) == *reinterpret_cast<const Vector2 *>(p_b.
                           _data.
                           _mem);
            break;
        }
        case VariantType::RECT2: {
            if (p_b.type == VariantType::NIL)
                return false;
            if (p_b.type == VariantType::RECT2)
                return *reinterpret_cast<const Rect2 *>(p_a._data._mem) == *
                       reinterpret_cast<const Rect2 *>(p_b._data._mem);
            break;
        }
        case VariantType::TRANSFORM2D: {
            if (p_b.type == VariantType::NIL)
                return false;
            if (p_b.type == VariantType::TRANSFORM2D)
                return *p_a._data._transform2d == *p_b._data._transform2d;
            break;
        }
        case VariantType::VECTOR3: {
            if (p_b.type == VariantType::NIL)
                return false;
            if (p_b.type == VariantType::VECTOR3)
                return *reinterpret_cast<const Vector3 *>(p_a._data._mem) == *
                       reinterpret_cast<const Vector3 *>(p_b._data._mem);
            break;
        }
        case VariantType::PLANE: {
            if (p_b.type == VariantType::NIL)
                return false;
            if (p_b.type == VariantType::PLANE)
                return *reinterpret_cast<const Plane *>(p_a._data._mem) == *
                       reinterpret_cast<const Plane *>(p_b._data._mem);
            break;
        }
        case VariantType::QUAT: {
            if (p_b.type == VariantType::NIL)
                return false;
            if (p_b.type == VariantType::QUAT)
                return *reinterpret_cast<const Quat *>(p_a._data._mem) == *
                       reinterpret_cast<const Quat *>(p_b._data._mem);
            break;
        }
        case VariantType::AABB: {
            if (p_b.type == VariantType::NIL)
                return false;
            if (p_b.type == VariantType::AABB)
                return *p_a._data._aabb == *p_b._data._aabb;
            break;
        }
        case VariantType::BASIS: {
            if (p_b.type == VariantType::NIL)
                return false;
            if (p_b.type == VariantType::BASIS)
                return *p_a._data._basis == *p_b._data._basis;
            break;
        }
        case VariantType::TRANSFORM: {
            if (p_b.type == VariantType::NIL)
                return false;
            if (p_b.type == VariantType::TRANSFORM)
                return *p_a._data._transform == *p_b._data._transform;
            break;
        }
        case VariantType::COLOR: {
            if (p_b.type == VariantType::NIL)
                return false;
            if (p_b.type == VariantType::COLOR)
                return *reinterpret_cast<const Color *>(p_a._data._mem) == *
                       reinterpret_cast<const Color *>(p_b._data._mem);
            break;
        }
        case VariantType::NODE_PATH: {
            if (p_b.type == VariantType::NIL)
                return false;
            if (p_b.type == VariantType::STRING)
                return (String)*reinterpret_cast<const NodePath *>(p_a._data._mem) == *
                       reinterpret_cast<const String *>(p_b._data._mem);
            if (p_b.type == VariantType::STRING_NAME)
                return (String)*reinterpret_cast<const NodePath *>(p_a._data._mem)
                       == (String)*reinterpret_cast<const StringName *>(p_b._data._mem);
            if (p_b.type == VariantType::NODE_PATH)
                return (String)*reinterpret_cast<const NodePath *>(p_a._data._mem) ==
                       (String)*reinterpret_cast<const NodePath *>(p_b._data._mem);
            break;
        }
        case VariantType::_RID: {
            if (p_b.type == VariantType::NIL)
                return false;
            if (p_b.type == VariantType::_RID)
                return *reinterpret_cast<const RID *>(p_a._data._mem) == *reinterpret_cast
                       <const RID *>(p_b._data._mem);
            break;
        }

        case VariantType::POOL_BYTE_ARRAY: {
            if (p_b.type == VariantType::NIL)
                return false;
            if (p_a.type != p_b.type) {
                break;
            }
            const PoolVector<uint8_t> &array_a = *reinterpret_cast<const PoolVector<uint8_t> *>(p_a._data._mem);
            const PoolVector<uint8_t> &array_b = *reinterpret_cast<const PoolVector<uint8_t> *>(p_b._data._mem);
            return compare(array_a, array_b);
        }
        case VariantType::POOL_INT_ARRAY: {
            if (p_b.type == VariantType::NIL)
                return false;
            if (p_a.type != p_b.type) {
                break;
            }
            const PoolVector<int> &array_a = *reinterpret_cast<const PoolVector<int> *>(p_a._data._mem);
            const PoolVector<int> &array_b = *reinterpret_cast<const PoolVector<int> *>(p_b._data._mem);
            return compare(array_a, array_b);
        }
        case VariantType::POOL_FLOAT32_ARRAY: {
            if (p_b.type == VariantType::NIL)
                return false;
            if (p_a.type != p_b.type) {
                break;
            }
            const PoolVector<float> &array_a = *reinterpret_cast<const PoolVector<float> *>(p_a._data._mem);
            const PoolVector<float> &array_b = *reinterpret_cast<const PoolVector<float> *>(p_b._data._mem);
            return compare(array_a, array_b);
        }
        case VariantType::POOL_STRING_ARRAY: {
            if (p_b.type == VariantType::NIL)
                return false;
            if (p_a.type != p_b.type) {
                break;
            }
            const PoolVector<String> &array_a = *reinterpret_cast<const PoolVector<String> *>(p_a._data._mem);
            const PoolVector<String> &array_b = *reinterpret_cast<const PoolVector<String> *>(p_b._data._mem);
            return compare(array_a, array_b);
        }
        case VariantType::POOL_VECTOR2_ARRAY: {
            if (p_b.type == VariantType::NIL)
                return false;
            if (p_a.type != p_b.type) {
                break;
            }
            const PoolVector<Vector2> &array_a = *reinterpret_cast<const PoolVector<Vector2> *>(p_a._data._mem);
            const PoolVector<Vector2> &array_b = *reinterpret_cast<const PoolVector<Vector2> *>(p_b._data._mem);
            return compare(array_a, array_b);
        }
        case VariantType::POOL_VECTOR3_ARRAY: {
            if (p_b.type == VariantType::NIL)
                return false;
            if (p_a.type != p_b.type) {
                break;
            }
            const auto &array_a = *reinterpret_cast<const PoolVector<Vector3> *>(p_a._data._mem);
            const auto &array_b = *reinterpret_cast<const PoolVector<Vector3> *>(p_b._data._mem);
            return compare(array_a, array_b);
        }
        case VariantType::POOL_COLOR_ARRAY: {
            if (p_b.type == VariantType::NIL)
                return false;
            if (p_a.type != p_b.type) {
                break;
            }
            const auto &array_a = *reinterpret_cast<const PoolVector<Color> *>(p_a._data._mem);
            const auto &array_b = *reinterpret_cast<const PoolVector<Color> *>(p_b._data._mem);
            return compare(array_a, array_b);
        }
    }
    return false;
}


void Variant::set_named(const StringName &p_index, const Variant &p_value, bool *r_valid) {

    bool valid = false;
    switch (type) {
        case VariantType::VECTOR2: {
            if (p_value.type == VariantType::INT) {
                Vector2 *v = reinterpret_cast<Vector2 *>(_data._mem);
                if (p_index == CoreStringNames::singleton->x) {
                    v->x = p_value._data._int;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->y) {
                    v->y = p_value._data._int;
                    valid = true;
                }
            } else if (p_value.type == VariantType::FLOAT) {
                Vector2 *v = reinterpret_cast<Vector2 *>(_data._mem);
                if (p_index == CoreStringNames::singleton->x) {
                    v->x = p_value._data._real;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->y) {
                    v->y = p_value._data._real;
                    valid = true;
                }
            }

        } break;
        case VariantType::RECT2: {

            if (p_value.type == VariantType::VECTOR2) {
                Rect2 *v = reinterpret_cast<Rect2 *>(_data._mem);
                //scalar name
                if (p_index == CoreStringNames::singleton->position) {
                    v->position = *reinterpret_cast<const Vector2 *>(p_value._data._mem);
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->size) {
                    v->size = *reinterpret_cast<const Vector2 *>(p_value._data._mem);
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->end) {
                    v->size = *reinterpret_cast<const Vector2 *>(p_value._data._mem) - v->position;
                    valid = true;
                }
            }
        } break;
        case VariantType::TRANSFORM2D: {

            if (p_value.type == VariantType::VECTOR2) {
                Transform2D *v = _data._transform2d;
                if (p_index == CoreStringNames::singleton->x) {
                    v->elements[0] = *reinterpret_cast<const Vector2 *>(p_value._data._mem);
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->y) {
                    v->elements[1] = *reinterpret_cast<const Vector2 *>(p_value._data._mem);
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->origin) {
                    v->elements[2] = *reinterpret_cast<const Vector2 *>(p_value._data._mem);
                    valid = true;
                }
            }

        } break;
        case VariantType::VECTOR3: {

            if (p_value.type == VariantType::INT) {
                Vector3 *v = reinterpret_cast<Vector3 *>(_data._mem);
                if (p_index == CoreStringNames::singleton->x) {
                    v->x = p_value._data._int;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->y) {
                    v->y = p_value._data._int;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->z) {
                    v->z = p_value._data._int;
                    valid = true;
                }
            } else if (p_value.type == VariantType::FLOAT) {
                Vector3 *v = reinterpret_cast<Vector3 *>(_data._mem);
                if (p_index == CoreStringNames::singleton->x) {
                    v->x = p_value._data._real;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->y) {
                    v->y = p_value._data._real;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->z) {
                    v->z = p_value._data._real;
                    valid = true;
                }
            }

        } break;
        case VariantType::PLANE: {

            if (p_value.type == VariantType::INT) {
                Plane *v = reinterpret_cast<Plane *>(_data._mem);
                if (p_index == CoreStringNames::singleton->x) {
                    v->normal.x = p_value._data._int;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->y) {
                    v->normal.y = p_value._data._int;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->z) {
                    v->normal.z = p_value._data._int;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->d) {
                    v->d = p_value._data._int;
                    valid = true;
                }
            } else if (p_value.type == VariantType::FLOAT) {
                Plane *v = reinterpret_cast<Plane *>(_data._mem);
                if (p_index == CoreStringNames::singleton->x) {
                    v->normal.x = p_value._data._real;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->y) {
                    v->normal.y = p_value._data._real;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->z) {
                    v->normal.z = p_value._data._real;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->d) {
                    v->d = p_value._data._real;
                    valid = true;
                }

            } else if (p_value.type == VariantType::VECTOR3) {
                Plane *v = reinterpret_cast<Plane *>(_data._mem);
                if (p_index == CoreStringNames::singleton->normal) {
                    v->normal = *reinterpret_cast<const Vector3 *>(p_value._data._mem);
                    valid = true;
                }
            }

        } break;
        case VariantType::QUAT: {

            if (p_value.type == VariantType::INT) {
                Quat *v = reinterpret_cast<Quat *>(_data._mem);
                if (p_index == CoreStringNames::singleton->x) {
                    v->x = p_value._data._int;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->y) {
                    v->y = p_value._data._int;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->z) {
                    v->z = p_value._data._int;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->w) {
                    v->w = p_value._data._int;
                    valid = true;
                }
            } else if (p_value.type == VariantType::FLOAT) {
                Quat *v = reinterpret_cast<Quat *>(_data._mem);
                if (p_index == CoreStringNames::singleton->x) {
                    v->x = p_value._data._real;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->y) {
                    v->y = p_value._data._real;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->z) {
                    v->z = p_value._data._real;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->w) {
                    v->w = p_value._data._real;
                    valid = true;
                }
            }

        } break; // 10
        case VariantType::AABB: {

            if (p_value.type == VariantType::VECTOR3) {
                ::AABB *v = _data._aabb;
                //scalar name
                if (p_index == CoreStringNames::singleton->position) {
                    v->position = *reinterpret_cast<const Vector3 *>(p_value._data._mem);
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->size) {
                    v->size = *reinterpret_cast<const Vector3 *>(p_value._data._mem);
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->end) {
                    v->size = *reinterpret_cast<const Vector3 *>(p_value._data._mem) - v->position;
                    valid = true;
                }
            }
        } break;
        case VariantType::BASIS: {

            if (p_value.type == VariantType::VECTOR3) {
                Basis *v = _data._basis;
                //scalar name
                if (p_index == CoreStringNames::singleton->x) {
                    v->set_axis(0, *reinterpret_cast<const Vector3 *>(p_value._data._mem));
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->y) {
                    v->set_axis(1, *reinterpret_cast<const Vector3 *>(p_value._data._mem));
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->z) {
                    v->set_axis(2, *reinterpret_cast<const Vector3 *>(p_value._data._mem));
                    valid = true;
                }
            }
        } break;
        case VariantType::TRANSFORM: {

            if (p_value.type == VariantType::BASIS && p_index == CoreStringNames::singleton->basis) {
                _data._transform->basis = *p_value._data._basis;
                valid = true;
            } else if (p_value.type == VariantType::VECTOR3 && p_index == CoreStringNames::singleton->origin) {
                _data._transform->origin = *reinterpret_cast<const Vector3 *>(p_value._data._mem);
                valid = true;
            }

        } break;
        case VariantType::COLOR: {

            if (p_value.type == VariantType::INT) {
                Color *v = reinterpret_cast<Color *>(_data._mem);
                if (p_index == CoreStringNames::singleton->r) {
                    v->r = p_value._data._int;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->g) {
                    v->g = p_value._data._int;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->b) {
                    v->b = p_value._data._int;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->a) {
                    v->a = p_value._data._int;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->r8) {
                    v->r = p_value._data._int / 255.0;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->g8) {
                    v->g = p_value._data._int / 255.0;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->b8) {
                    v->b = p_value._data._int / 255.0;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->a8) {
                    v->a = p_value._data._int / 255.0;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->h) {
                    v->set_hsv(p_value._data._int, v->get_s(), v->get_v(), v->a);
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->s) {
                    v->set_hsv(v->get_h(), p_value._data._int, v->get_v(), v->a);
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->v) {
                    v->set_hsv(v->get_h(), v->get_s(), p_value._data._int, v->a);
                    valid = true;
                }
            } else if (p_value.type == VariantType::FLOAT) {
                Color *v = reinterpret_cast<Color *>(_data._mem);
                if (p_index == CoreStringNames::singleton->r) {
                    v->r = p_value._data._real;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->g) {
                    v->g = p_value._data._real;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->b) {
                    v->b = p_value._data._real;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->a) {
                    v->a = p_value._data._real;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->r8) {
                    v->r = p_value._data._real / 255.0;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->g8) {
                    v->g = p_value._data._real / 255.0;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->b8) {
                    v->b = p_value._data._real / 255.0;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->a8) {
                    v->a = p_value._data._real / 255.0;
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->h) {
                    v->set_hsv(p_value._data._real, v->get_s(), v->get_v(), v->a);
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->s) {
                    v->set_hsv(v->get_h(), p_value._data._real, v->get_v(), v->a);
                    valid = true;
                } else if (p_index == CoreStringNames::singleton->v) {
                    v->set_hsv(v->get_h(), v->get_s(), p_value._data._real, v->a);
                    valid = true;
                }
            }
        } break;
        case VariantType::OBJECT: {

            Object *obj = _OBJ_PTR(*this);
            if (unlikely(!obj)) {
#ifdef DEBUG_ENABLED
                if (ScriptDebugger::get_singleton() && _get_obj().rc && !object_for_entity(_get_obj().rc->instance_id)) {
                    ERR_PRINT("Attempted set on a deleted object.");
                }
#endif
                break;
            }
            obj->set(p_index, p_value, &valid);

        } break;
        default: {
            set_indexed(StringUtils::to_int(p_index), p_value, &valid);
        } break;
    }

    if (r_valid) {
        *r_valid = valid;
    }
}

Variant Variant::get_named(const StringName &p_index, bool *r_valid) const {

    if (r_valid) {
        *r_valid = true;
    }
    switch (type) {
        case VariantType::VECTOR2: {
            const Vector2 *v = reinterpret_cast<const Vector2 *>(_data._mem);
            if (p_index == CoreStringNames::singleton->x) {
                return v->x;
            } else if (p_index == CoreStringNames::singleton->y) {
                return v->y;
            }

        } break;
        case VariantType::RECT2: {

            const Rect2 *v = reinterpret_cast<const Rect2 *>(_data._mem);
            //scalar name
            if (p_index == CoreStringNames::singleton->position) {
                return v->position;
            }
            if (p_index == CoreStringNames::singleton->size) {
                return v->size;
            }
            if (p_index == CoreStringNames::singleton->end) {
                return v->size + v->position;
            }
        } break;
        case VariantType::TRANSFORM2D: {

            const Transform2D *v = _data._transform2d;
            if (p_index == CoreStringNames::singleton->x) {
                return v->elements[0];
            } else if (p_index == CoreStringNames::singleton->y) {
                return v->elements[1];
            } else if (p_index == CoreStringNames::singleton->origin) {
                return v->elements[2];
            }

        } break;
        case VariantType::VECTOR3: {

            const Vector3 *v = reinterpret_cast<const Vector3 *>(_data._mem);
            if (p_index == CoreStringNames::singleton->x) {
                return v->x;
            } else if (p_index == CoreStringNames::singleton->y) {
                return v->y;
            } else if (p_index == CoreStringNames::singleton->z) {
                return v->z;
            }

        } break;
        case VariantType::PLANE: {

            const Plane *v = reinterpret_cast<const Plane *>(_data._mem);
            if (p_index == CoreStringNames::singleton->x) {
                return v->normal.x;
            } else if (p_index == CoreStringNames::singleton->y) {
                return v->normal.y;
            } else if (p_index == CoreStringNames::singleton->z) {
                return v->normal.z;
            } else if (p_index == CoreStringNames::singleton->d) {
                return v->d;
            } else if (p_index == CoreStringNames::singleton->normal) {
                return v->normal;
            }

        } break;
        case VariantType::QUAT: {

            const Quat *v = reinterpret_cast<const Quat *>(_data._mem);
            if (p_index == CoreStringNames::singleton->x) {
                return v->x;
            } else if (p_index == CoreStringNames::singleton->y) {
                return v->y;
            } else if (p_index == CoreStringNames::singleton->z) {
                return v->z;
            } else if (p_index == CoreStringNames::singleton->w) {
                return v->w;
            }

        } break; // 10
        case VariantType::AABB: {

            const ::AABB *v = _data._aabb;
            //scalar name
            if (p_index == CoreStringNames::singleton->position) {
                return v->position;
            } else if (p_index == CoreStringNames::singleton->size) {
                return v->size;
            } else if (p_index == CoreStringNames::singleton->end) {
                return v->size + v->position;
            }
        } break;
        case VariantType::BASIS: {

            const Basis *v = _data._basis;
            //scalar name
            if (p_index == CoreStringNames::singleton->x) {
                return v->get_axis(0);
            } else if (p_index == CoreStringNames::singleton->y) {
                return v->get_axis(1);
            } else if (p_index == CoreStringNames::singleton->z) {
                return v->get_axis(2);
            }

        } break;
        case VariantType::TRANSFORM: {

            if (p_index == CoreStringNames::singleton->basis) {
                return _data._transform->basis;
            } else if (p_index == CoreStringNames::singleton->origin) {
                return _data._transform->origin;
            }

        } break;
        case VariantType::COLOR: {

            const Color *v = reinterpret_cast<const Color *>(_data._mem);
            if (p_index == CoreStringNames::singleton->r) {
                return v->r;
            } else if (p_index == CoreStringNames::singleton->g) {
                return v->g;
            } else if (p_index == CoreStringNames::singleton->b) {
                return v->b;
            } else if (p_index == CoreStringNames::singleton->a) {
                return v->a;
            } else if (p_index == CoreStringNames::singleton->r8) {
                return int(Math::round(v->r * 255.0));
            } else if (p_index == CoreStringNames::singleton->g8) {
                return int(Math::round(v->g * 255.0));
            } else if (p_index == CoreStringNames::singleton->b8) {
                return int(Math::round(v->b * 255.0));
            } else if (p_index == CoreStringNames::singleton->a8) {
                return int(Math::round(v->a * 255.0));
            } else if (p_index == CoreStringNames::singleton->h) {
                return v->get_h();
            } else if (p_index == CoreStringNames::singleton->s) {
                return v->get_s();
            } else if (p_index == CoreStringNames::singleton->v) {
                return v->get_v();
            }
        } break;
        case VariantType::OBJECT: {

            Object *obj = _OBJ_PTR(*this);
            if (unlikely(!obj)) {
                if (r_valid)
                    *r_valid = false;
#ifdef DEBUG_ENABLED
                if (ScriptDebugger::get_singleton() && _get_obj().rc && !object_for_entity(_get_obj().rc->instance_id)) {
                    WARN_PRINT("Attempted get on a deleted object.");
                }
#endif
                return Variant();
            }

            return obj->get(p_index, r_valid);

        } break;
        default: {
            WARN_PRINT("Attempt to get named value from unhandled VariantType.");
            return get(StringUtils::to_int(p_index));
        }
    }

    if (r_valid) {
        *r_valid = false;
    }
    return Variant();
}

#define DEFAULT_OP_ARRAY_CMD(m_name, m_type, skip_test, cmd)                             \
    case VariantType::m_name: {                                                                       \
        skip_test;                                                                       \
        int index = p_index;                                                         \
            m_type *arr = reinterpret_cast<m_type *>(_data._mem);                        \
                                                                                         \
            if (index < 0)                                                               \
                index += arr->size();                                                    \
            if (index >= 0 && index < arr->size()) {                                     \
                valid = true;                                                            \
                cmd;                                                                     \
        }                                                                                \
    } break;

#define DEFAULT_OP_DVECTOR_SET(m_name, dv_type, skip_cond) \
    DEFAULT_OP_ARRAY_CMD(m_name, PoolVector<dv_type>, if (skip_cond) return;, arr->set(index, p_value.as<dv_type>()); return )

#define DEFAULT_OP_DVECTOR_GET(m_name, dv_type) \
    DEFAULT_OP_ARRAY_CMD(m_name, const PoolVector<dv_type>, ;, return arr->get(index))

void Variant::set_indexed(int p_index, const Variant& p_value, bool* r_valid) {

    static bool _dummy = false;

    bool &valid = r_valid ? *r_valid : _dummy;
    valid = false;

    switch (type) {
        case VariantType::NIL:
        case VariantType::BOOL:
        case VariantType::INT:
        case VariantType::FLOAT:
        case VariantType::RECT2:
        case VariantType::PLANE:
        case VariantType::QUAT:
        case VariantType::AABB:
        case VariantType::NODE_PATH:
        case VariantType::_RID:
        {
            return;
        }
        case VariantType::REN_ENT: {
            assert(false);
            return;
        }
        case VariantType::STRING: {

            String *str = reinterpret_cast<String *>(_data._mem);
            int len = str->length();
            if (p_index < 0)
                p_index += len;
            if (p_index < 0 || p_index >= len)
                return;

            String chr;
            if (p_value.type == VariantType::INT || p_value.type == VariantType::FLOAT) {

                chr.push_back(p_value.as<int>());
            } else if (p_value.type == VariantType::STRING) {

                chr = p_value.as<String>();
            } else {
                return;
            }

            *str = String(StringUtils::substr(*str,0, p_index)) + chr + StringUtils::substr(*str, p_index + 1, len);
            valid = true;
            return;

        }
        case VariantType::VECTOR2: {

            if (p_value.type != VariantType::INT && p_value.type != VariantType::FLOAT)
                return;

            if (p_index < 0)
                p_index += 2;
            if (p_index >= 0 && p_index < 2) {

                    Vector2 *v = reinterpret_cast<Vector2 *>(_data._mem);
                    valid = true;
                (*v)[p_index] = p_value.as<float>();
                    return;
            }

        } break; // 5
        case VariantType::TRANSFORM2D: {

            if (p_value.type != VariantType::VECTOR2)
                return;

            if (p_index < 0)
                p_index += 3;
            if (p_index >= 0 && p_index < 3) {
                    Transform2D *v = _data._transform2d;

                    valid = true;
                v->elements[p_index] = p_value.as<Vector2>();
                }
                    return;

        } break;
        case VariantType::VECTOR3: {

            if (p_value.type != VariantType::INT && p_value.type != VariantType::FLOAT)
                return;

            if (p_index < 0)
                p_index += 3;
            if (p_index >= 0 && p_index < 3) {

                    Vector3 *v = reinterpret_cast<Vector3 *>(_data._mem);
                    valid = true;
                (*v)[p_index] = p_value.as<float>();
                    return;
            }

        } break;

        case VariantType::BASIS: {

            if (p_value.type != VariantType::VECTOR3)
                return;

            if (p_index < 0)
                p_index += 3;
            if (p_index >= 0 && p_index < 3) {
                    Basis *v = _data._basis;

                    valid = true;
                v->set_axis(p_index, p_value.as<Vector3>());
                    return;
            }

        } break;
        case VariantType::TRANSFORM: {


                if (p_value.type != VariantType::VECTOR3)
                    return;

            if (p_index < 0)
                p_index += 4;
            if (p_index >= 0 && p_index < 4) {
                    Transform *v = _data._transform;
                    valid = true;
                if (p_index == 3)
                        v->origin = p_value.as<Vector3>();
                    else
                    v->basis.set_axis(p_index, p_value.as<Vector3>());
                    return;
            }

        } break;
        case VariantType::COLOR: {

            if (p_value.type != VariantType::INT && p_value.type != VariantType::FLOAT)
                return;

            if (p_index < 0)
                p_index += 4;
            if (p_index >= 0 && p_index < 4) {
                Color *v = reinterpret_cast<Color *>(_data._mem);
                v->component(p_index) = p_value.as<float>();
                    valid = true;
                }
        } break;
        case VariantType::OBJECT: {

            Object *obj = _OBJ_PTR(*this);
            if (unlikely(!obj)) {
#ifdef DEBUG_ENABLED
                valid = false;
                if (ScriptDebugger::get_singleton() && _get_obj().rc && !object_for_entity(_get_obj().rc->instance_id)) {
                    ERR_PRINT("Attempted set on a deleted object.");
                }
#endif
                return;
            }

                obj->setvar(p_index, p_value, r_valid);
            return;
        } break;
        case VariantType::DICTIONARY: {

            Dictionary *dic = reinterpret_cast<Dictionary *>(_data._mem);
            StringName key(dic->get_key_at_index(p_index));
            valid = !key.empty();

            if(valid) {
                dic->operator[](key) = p_value;
            }
            return;
        }
            DEFAULT_OP_ARRAY_CMD(ARRAY, Array, ;, (*arr)[index] = p_value; return ) // 20
            DEFAULT_OP_DVECTOR_SET(POOL_BYTE_ARRAY, uint8_t, p_value.type != VariantType::FLOAT && p_value.type != VariantType::INT)
            DEFAULT_OP_DVECTOR_SET(POOL_INT_ARRAY, int, p_value.type != VariantType::FLOAT && p_value.type != VariantType::INT)
            DEFAULT_OP_DVECTOR_SET(POOL_FLOAT32_ARRAY, real_t, p_value.type != VariantType::FLOAT && p_value.type != VariantType::INT)
            DEFAULT_OP_DVECTOR_SET(POOL_STRING_ARRAY, String, p_value.type != VariantType::STRING)
            DEFAULT_OP_DVECTOR_SET(POOL_VECTOR2_ARRAY, Vector2, p_value.type != VariantType::VECTOR2) // 25
            DEFAULT_OP_DVECTOR_SET(POOL_VECTOR3_ARRAY, Vector3, p_value.type != VariantType::VECTOR3)
            DEFAULT_OP_DVECTOR_SET(POOL_COLOR_ARRAY, Color, p_value.type != VariantType::COLOR)
        default:
            return;
    }
}

Variant Variant::get(int p_index, bool *r_valid) const {

    static bool _dummy = false;

    bool &valid = r_valid ? *r_valid : _dummy;

    valid = false;

    switch (type) {
        case VariantType::NIL:
        case VariantType::BOOL:
        case VariantType::INT:
        case VariantType::FLOAT:
        case VariantType::RECT2:
        case VariantType::PLANE:
        case VariantType::QUAT:
        case VariantType::AABB:
        {
            return Variant();
        }
        case VariantType::REN_ENT: {
            assert(false);
            return Variant();
        }

        case VariantType::STRING: {

                const String *str = reinterpret_cast<const String *>(_data._mem);
            if (p_index < 0)
                p_index += str->length();
            if (p_index >= 0 && p_index < str->length()) {

                    valid = true;
                return StringUtils::substr(*str, p_index, 1);
            }

        } break;
        case VariantType::VECTOR2: {

                // scalar index
            if (p_index < 0)
                p_index += 2;
            if (p_index >= 0 && p_index < 2) {

                    const Vector2 *v = reinterpret_cast<const Vector2 *>(_data._mem);
                    valid = true;
                return (*v)[p_index];
            }

        } break; // 5
        case VariantType::VECTOR3: {

            if (p_index < 0)
                p_index += 3;
            if (p_index >= 0 && p_index < 3) {

                    const Vector3 *v = reinterpret_cast<const Vector3 *>(_data._mem);
                    valid = true;
                return (*v)[p_index];
            }

        } break;
        case VariantType::TRANSFORM2D: {

            if (p_index < 0)
                p_index += 3;
            if (p_index >= 0 && p_index < 3) {
                    const Transform2D *v = _data._transform2d;

                    valid = true;
                return v->elements[p_index];
            }
        } break;
        case VariantType::BASIS: {

            int index = p_index;
                if (index < 0)
                    index += 3;
                if (index >= 0 && index < 3) {
                    const Basis *v = _data._basis;

                    valid = true;
                    return v->get_axis(index);
            }

        } break;
        case VariantType::TRANSFORM: {

            int index = p_index;
                if (index < 0)
                    index += 4;
                if (index >= 0 && index < 4) {
                    const Transform *v = _data._transform;
                    valid = true;
                    return index == 3 ? v->origin : v->basis.get_axis(index);
            }

        } break;
        case VariantType::COLOR: {

            int idx = p_index;
                if (idx < 0)
                    idx += 4;
                if (idx >= 0 && idx < 4) {
                    const Color *v = reinterpret_cast<const Color *>(_data._mem);
                    valid = true;
                    return v->component(idx);
            }

        } break;
        case VariantType::NODE_PATH: {
        } break; // 15
        case VariantType::_RID: {
        } break;
        case VariantType::OBJECT: {
            Object *obj = _OBJ_PTR(*this);
            if (unlikely(!obj)) {
                valid = false;
#ifdef DEBUG_ENABLED
                if (ScriptDebugger::get_singleton() && _get_obj().rc && !object_for_entity(_get_obj().rc->instance_id)) {
                    WARN_PRINT("Attempted get on a deleted object.");
                }
#endif
                return Variant();
            }

            return obj->getvar(p_index, r_valid);

        } break;
        case VariantType::DICTIONARY: {

            const Dictionary *dic = reinterpret_cast<const Dictionary *>(_data._mem);
            StringName key(dic->get_key_at_index(p_index));
            const Variant *res = dic->getptr(key);
            if (res) {
                valid = true;
                return *res;
            }
        } break;
            DEFAULT_OP_ARRAY_CMD(ARRAY, const Array, ;, return (*arr)[index]) // 20
            DEFAULT_OP_DVECTOR_GET(POOL_BYTE_ARRAY, uint8_t)
            DEFAULT_OP_DVECTOR_GET(POOL_INT_ARRAY, int)
            DEFAULT_OP_DVECTOR_GET(POOL_FLOAT32_ARRAY, real_t)
            DEFAULT_OP_DVECTOR_GET(POOL_STRING_ARRAY, String)
            DEFAULT_OP_DVECTOR_GET(POOL_VECTOR2_ARRAY, Vector2) // 25
            DEFAULT_OP_DVECTOR_GET(POOL_VECTOR3_ARRAY, Vector3)
            DEFAULT_OP_DVECTOR_GET(POOL_COLOR_ARRAY, Color)
        default:
            return Variant();
    }

    return Variant();
}

void Variant::get_property_list(Vector<PropertyInfo> *p_list) const {

    switch (type) {
        case VariantType::VECTOR2: {

            p_list->emplace_back(VariantType::FLOAT, "x");
            p_list->emplace_back(VariantType::FLOAT, "y");

        } break; // 5
        case VariantType::RECT2: {

            p_list->emplace_back(VariantType::VECTOR2, "position");
            p_list->emplace_back(VariantType::VECTOR2, "size");
            p_list->emplace_back(VariantType::VECTOR2, "end");

        } break;
        case VariantType::VECTOR3: {

            p_list->emplace_back(VariantType::FLOAT, "x");
            p_list->emplace_back(VariantType::FLOAT, "y");
            p_list->emplace_back(VariantType::FLOAT, "z");

        } break;
        case VariantType::TRANSFORM2D: {

            p_list->emplace_back(VariantType::VECTOR2, "x");
            p_list->emplace_back(VariantType::VECTOR2, "y");
            p_list->emplace_back(VariantType::VECTOR2, "origin");

        } break;
        case VariantType::PLANE: {

            p_list->emplace_back(VariantType::VECTOR3, "normal");
            p_list->emplace_back(VariantType::FLOAT, "x");
            p_list->emplace_back(VariantType::FLOAT, "y");
            p_list->emplace_back(VariantType::FLOAT, "z");
            p_list->emplace_back(VariantType::FLOAT, "d");

        } break;
        case VariantType::QUAT: {

            p_list->emplace_back(VariantType::FLOAT, "x");
            p_list->emplace_back(VariantType::FLOAT, "y");
            p_list->emplace_back(VariantType::FLOAT, "z");
            p_list->emplace_back(VariantType::FLOAT, "w");

        } break; // 10
        case VariantType::AABB: {
            p_list->emplace_back(VariantType::VECTOR3, "position");
            p_list->emplace_back(VariantType::VECTOR3, "size");
            p_list->emplace_back(VariantType::VECTOR3, "end");
        } break;
        case VariantType::BASIS: {

            p_list->emplace_back(VariantType::VECTOR3, "x");
            p_list->emplace_back(VariantType::VECTOR3, "y");
            p_list->emplace_back(VariantType::VECTOR3, "z");

        } break;
        case VariantType::TRANSFORM: {

            p_list->emplace_back(VariantType::BASIS, "basis");
            p_list->emplace_back(VariantType::VECTOR3, "origin");

        } break;
        case VariantType::COLOR: {
            p_list->emplace_back(VariantType::FLOAT, "r");
            p_list->emplace_back(VariantType::FLOAT, "g");
            p_list->emplace_back(VariantType::FLOAT, "b");
            p_list->emplace_back(VariantType::FLOAT, "a");
            p_list->emplace_back(VariantType::FLOAT, "h");
            p_list->emplace_back(VariantType::FLOAT, "s");
            p_list->emplace_back(VariantType::FLOAT, "v");
            p_list->emplace_back(VariantType::INT, "r8");
            p_list->emplace_back(VariantType::INT, "g8");
            p_list->emplace_back(VariantType::INT, "b8");
            p_list->emplace_back(VariantType::INT, "a8");

        } break;
        case VariantType::NODE_PATH: {
        } break; // 15
        case VariantType::_RID: {
        } break;
        case VariantType::OBJECT: {

            Object *obj = _OBJ_PTR(*this);
            if (unlikely(!obj)) {
#ifdef DEBUG_ENABLED
                if (_get_obj().rc) {
                    WARN_PRINT("Attempted get property list on a deleted object.");
                }
#endif
                return;
            }

            obj->get_property_list(p_list);
        } break;
        case VariantType::DICTIONARY: {

            const Dictionary *dic = reinterpret_cast<const Dictionary *>(_data._mem);
            auto keys(dic->get_key_list());
            for(auto &E : keys ) {
                p_list->emplace_back(VariantType::STRING, StringName(E));
            }
        } break;
        case VariantType::ARRAY: // 20
        case VariantType::POOL_BYTE_ARRAY:
        case VariantType::POOL_INT_ARRAY:
        case VariantType::POOL_FLOAT32_ARRAY:
        case VariantType::POOL_STRING_ARRAY:
        case VariantType::POOL_VECTOR2_ARRAY: // 25
        case VariantType::POOL_VECTOR3_ARRAY:
        case VariantType::POOL_COLOR_ARRAY: {

            //nothing
        } break;
        default: {
        }
    }
}

Variant Variant::duplicate(bool deep) const {
    switch (type) {
        case VariantType::OBJECT: {
            /*  breaks stuff :(
            if (deep && !_get_obj().ref.is_null()) {
                Ref<Resource> resource = _get_obj().ref;
                if (resource.is_valid()) {
                    return resource->duplicate(true);
                }
            }
            */
            return *this;
        }
        case VariantType::DICTIONARY:
            return as<Dictionary>().duplicate(deep);
        case VariantType::ARRAY:
            return as<Array>().duplicate(deep);
        default:
            return *this;
    }
}

void Variant::blend(const Variant &a, const Variant &b, float c, Variant &r_dst) {
    if (a.type != b.type) {
        if (a.is_num() && b.is_num()) {
            real_t va = a.as<real_t>();
            real_t vb = b.as<real_t>();
            r_dst = va + vb * c;
        } else {
            r_dst = a;
        }
        return;
    }

    switch (a.type) {
        case VariantType::NIL: {
            r_dst = Variant();
        }
            return;
        case VariantType::INT: {
            int64_t va = a._data._int;
            int64_t vb = b._data._int;
            r_dst = int(va + vb * c + 0.5f);
        }
            return;
        case VariantType::FLOAT: {
            double ra = a._data._real;
            double rb = b._data._real;
            r_dst = ra + rb * c;
        }
            return;
        case VariantType::VECTOR2: {
            r_dst = *reinterpret_cast<const Vector2 *>(a._data._mem) + *reinterpret_cast<const Vector2 *>(b._data._mem) * c;
        }
            return;
        case VariantType::RECT2: {
            const Rect2 *ra = reinterpret_cast<const Rect2 *>(a._data._mem);
            const Rect2 *rb = reinterpret_cast<const Rect2 *>(b._data._mem);
            r_dst = Rect2(ra->position + rb->position * c, ra->size + rb->size * c);
        }
            return;
        case VariantType::VECTOR3: {
            r_dst = *reinterpret_cast<const Vector3 *>(a._data._mem) + *reinterpret_cast<const Vector3 *>(b._data._mem) * c;
        }
            return;
        case VariantType::AABB: {
            const ::AABB *ra = reinterpret_cast<const ::AABB *>(a._data._mem);
            const ::AABB *rb = reinterpret_cast<const ::AABB *>(b._data._mem);
            r_dst = ::AABB(ra->position + rb->position * c, ra->size + rb->size * c);
        }
            return;
        case VariantType::QUAT: {
            Quat empty_rot;
            const Quat *qa = reinterpret_cast<const Quat *>(a._data._mem);
            const Quat *qb = reinterpret_cast<const Quat *>(b._data._mem);
            r_dst = *qa * empty_rot.slerp(*qb, c);
        }
            return;
        case VariantType::COLOR: {
            const Color *ca = reinterpret_cast<const Color *>(a._data._mem);
            const Color *cb = reinterpret_cast<const Color *>(b._data._mem);
            float new_r = ca->r + cb->r * c;
            float new_g = ca->g + cb->g * c;
            float new_b = ca->b + cb->b * c;
            float new_a = ca->a + cb->a * c;
            new_r = new_r > 1.0f ? 1.0f : new_r;
            new_g = new_g > 1.0f ? 1.0f : new_g;
            new_b = new_b > 1.0f ? 1.0f : new_b;
            new_a = new_a > 1.0f ? 1.0f : new_a;
            r_dst = Color(new_r, new_g, new_b, new_a);
        }
            return;
        default: {
            r_dst = c < 0.5f ? a : b;
        }
            return;
    }
}

void Variant::interpolate(const Variant &a, const Variant &b, float c, Variant &r_dst) {

    if (a.type != b.type) {
        if (a.is_num() && b.is_num()) {
            //not as efficient but..
            real_t va = a.as<real_t>();;
            real_t vb = b.as<real_t>();;
            r_dst = va + (vb - va) * c;

        } else {
            r_dst = a;
        }
        return;
    }

    switch (a.type) {

        case VariantType::NIL: {
            r_dst = Variant();
        }
            return;
        case VariantType::BOOL: {
            r_dst = a;
        }
            return;
        case VariantType::INT: {
            int64_t va = a._data._int;
            int64_t vb = b._data._int;
            r_dst = int(va + (vb - va) * c);
        }
            return;
        case VariantType::FLOAT: {
            real_t va = a._data._real;
            real_t vb = b._data._real;
            r_dst = va + (vb - va) * c;
        }
            return;
        case VariantType::STRING: {
            //this is pretty funny and bizarre, but artists like to use it for typewritter effects
            String sa = *reinterpret_cast<const String *>(a._data._mem);
            String sb = *reinterpret_cast<const String *>(b._data._mem);
            String dst;
            size_t sa_len = sa.length();
            size_t sb_len = sb.length();
            size_t csize = sa_len + (sb_len - sa_len) * c;
            if (csize == 0) {
                r_dst = "";
                return;
            }
            dst.resize(csize);
            size_t split = csize / 2;

            for (size_t i = 0; i < csize; i++) {

                char chr = ' ';

                if (i < split) {

                    if (i < sa.length())
                        chr = sa[i];
                    else if (i < sb.length())
                        chr = sb[i];

                } else {

                    if (i < sb.length())
                        chr = sb[i];
                    else if (i < sa.length())
                        chr = sa[i];
                }

                dst[i]=chr;
            }

            r_dst = dst;
        }
            return;
        case VariantType::VECTOR2: {
            r_dst = reinterpret_cast<const Vector2 *>(a._data._mem)->linear_interpolate(*reinterpret_cast<const Vector2 *>(b._data._mem), c);
        }
            return;
        case VariantType::RECT2: {
            r_dst = Rect2(reinterpret_cast<const Rect2 *>(a._data._mem)->position.linear_interpolate(reinterpret_cast<const Rect2 *>(b._data._mem)->position, c), reinterpret_cast<const Rect2 *>(a._data._mem)->size.linear_interpolate(reinterpret_cast<const Rect2 *>(b._data._mem)->size, c));
        }
            return;
        case VariantType::VECTOR3: {
            r_dst = reinterpret_cast<const Vector3 *>(a._data._mem)->linear_interpolate(*reinterpret_cast<const Vector3 *>(b._data._mem), c);
        }
            return;
        case VariantType::TRANSFORM2D: {
            r_dst = a._data._transform2d->interpolate_with(*b._data._transform2d, c);
        }
            return;
        case VariantType::PLANE: {
            r_dst = a;
        }
            return;
        case VariantType::QUAT: {
            r_dst = reinterpret_cast<const Quat *>(a._data._mem)->slerp(*reinterpret_cast<const Quat *>(b._data._mem), c);
        }
            return;
        case VariantType::AABB: {
            r_dst = ::AABB(a._data._aabb->position.linear_interpolate(b._data._aabb->position, c), a._data._aabb->size.linear_interpolate(b._data._aabb->size, c));
        }
            return;
        case VariantType::BASIS: {
            r_dst = Transform(*a._data._basis).interpolate_with(Transform(*b._data._basis), c).basis;
        }
            return;
        case VariantType::TRANSFORM: {
            r_dst = a._data._transform->interpolate_with(*b._data._transform, c);
        }
            return;
        case VariantType::COLOR: {
            r_dst = reinterpret_cast<const Color *>(a._data._mem)->linear_interpolate(*reinterpret_cast<const Color *>(b._data._mem), c);
        }
            return;
        case VariantType::NODE_PATH: {
            r_dst = a;
        }
            return;
        case VariantType::_RID: {
            r_dst = a;
        }
            return;
        case VariantType::OBJECT: {
            r_dst = a;
        }
            return;
        case VariantType::DICTIONARY: {
        }
            return;
        case VariantType::ARRAY: {
            r_dst = a;
        }
            return;
        case VariantType::POOL_BYTE_ARRAY: {
            r_dst = a;
        }
            return;
        case VariantType::POOL_INT_ARRAY: {
            const PoolVector<int> *arr_a = reinterpret_cast<const PoolVector<int> *>(a._data._mem);
            const PoolVector<int> *arr_b = reinterpret_cast<const PoolVector<int> *>(b._data._mem);
            int sz = arr_a->size();
            if (sz == 0 || arr_b->size() != sz) {

                r_dst = a;
            } else {

                PoolVector<int> v;
                v.resize(sz);
                {
                    PoolVector<int>::Write vw = v.write();
                    PoolVector<int>::Read ar = arr_a->read();
                    PoolVector<int>::Read br = arr_b->read();

                    Variant va;
                    for (int i = 0; i < sz; i++) {
                        Variant::interpolate(ar[i], br[i], c, va);
                        vw[i] = va.as<int>();
                    }
                }
                r_dst = v;
            }
        }
            return;
        case VariantType::POOL_FLOAT32_ARRAY: {
            const PoolVector<real_t> *arr_a = reinterpret_cast<const PoolVector<real_t> *>(a._data._mem);
            const PoolVector<real_t> *arr_b = reinterpret_cast<const PoolVector<real_t> *>(b._data._mem);
            int sz = arr_a->size();
            if (sz == 0 || arr_b->size() != sz) {

                r_dst = a;
            } else {

                PoolVector<real_t> v;
                v.resize(sz);
                {
                    PoolVector<real_t>::Write vw = v.write();
                    PoolVector<real_t>::Read ar = arr_a->read();
                    PoolVector<real_t>::Read br = arr_b->read();

                    Variant va;
                    for (int i = 0; i < sz; i++) {
                        Variant::interpolate(ar[i], br[i], c, va);
                        vw[i] = va.as<real_t>();
                    }
                }
                r_dst = v;
            }
        }
            return;
        case VariantType::POOL_STRING_ARRAY: {
            r_dst = a;
        }
            return;
        case VariantType::POOL_VECTOR2_ARRAY: {
            const PoolVector<Vector2> *arr_a = reinterpret_cast<const PoolVector<Vector2> *>(a._data._mem);
            const PoolVector<Vector2> *arr_b = reinterpret_cast<const PoolVector<Vector2> *>(b._data._mem);
            int sz = arr_a->size();
            if (sz == 0 || arr_b->size() != sz) {

                r_dst = a;
            } else {

                PoolVector<Vector2> v;
                v.resize(sz);
                {
                    PoolVector<Vector2>::Write vw = v.write();
                    PoolVector<Vector2>::Read ar = arr_a->read();
                    PoolVector<Vector2>::Read br = arr_b->read();

                    for (int i = 0; i < sz; i++) {
                        vw[i] = ar[i].linear_interpolate(br[i], c);
                    }
                }
                r_dst = Variant(v);
            }
        }
            return;
        case VariantType::POOL_VECTOR3_ARRAY: {

            const PoolVector<Vector3> *arr_a = reinterpret_cast<const PoolVector<Vector3> *>(a._data._mem);
            const PoolVector<Vector3> *arr_b = reinterpret_cast<const PoolVector<Vector3> *>(b._data._mem);
            int sz = arr_a->size();
            if (sz == 0 || arr_b->size() != sz) {

                r_dst = a;
            } else {

                PoolVector<Vector3> v;
                v.resize(sz);
                {
                    PoolVector<Vector3>::Write vw = v.write();
                    PoolVector<Vector3>::Read ar = arr_a->read();
                    PoolVector<Vector3>::Read br = arr_b->read();

                    for (int i = 0; i < sz; i++) {
                        vw[i] = ar[i].linear_interpolate(br[i], c);
                    }
                }
                r_dst = v;
            }
        }
            return;
        case VariantType::POOL_COLOR_ARRAY: {
            const PoolVector<Color> *arr_a = reinterpret_cast<const PoolVector<Color> *>(a._data._mem);
            const PoolVector<Color> *arr_b = reinterpret_cast<const PoolVector<Color> *>(b._data._mem);
            int sz = arr_a->size();
            if (sz == 0 || arr_b->size() != sz) {

                r_dst = a;
            } else {

                PoolVector<Color> v;
                v.resize(sz);
                {
                    PoolVector<Color>::Write vw = v.write();
                    PoolVector<Color>::Read ar = arr_a->read();
                    PoolVector<Color>::Read br = arr_b->read();

                    for (int i = 0; i < sz; i++) {
                        vw[i] = ar[i].linear_interpolate(br[i], c);
                    }
                }
                r_dst = v;
            }
        }
            return;
        default: {

            r_dst = a;
        }
    }
}

#undef TYPES
#undef QT_NO_META_MACROS
