/*************************************************************************/
/*  vector3.h                                                            */
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

#include "core/math/math_defs.h"
#include "core/math/math_funcs.h"
#include "core/math/vector3i.h"
#include "core/typedefs.h"
#include "core/error_macros.h"
#include "core/forward_decls.h"

class Basis;

struct GODOT_EXPORT [[nodiscard]] Vector3 {

    enum Axis {
        AXIS_X,
        AXIS_Y,
        AXIS_Z,
    };

    real_t x;
    real_t y;
    real_t z;

    constexpr const real_t &operator[](int p_axis) const {

        return (&x)[p_axis];
    }

    constexpr real_t &operator[](int p_axis) {

        return (&x)[p_axis];
    }

    void set_axis(int p_axis, real_t p_value);
    real_t get_axis(int p_axis) const;

    void set_all(real_t p_value) {
        x = y = z = p_value;
    }

    int min_axis() const {
        return x < y ? (x < z ? 0 : 2) : (y < z ? 1 : 2);
    }
    int max_axis() const {
        return x < y ? (y < z ? 2 : 1) : (x < z ? 2 : 0);
    }

    real_t length() const;
    real_t length_squared() const;

    _FORCE_INLINE_ void normalize();
    Vector3 normalized() const;
    _FORCE_INLINE_ bool is_normalized() const;
    Vector3 inverse() const;

    void snap(Vector3 p_val);
    Vector3 snapped(Vector3 p_val) const;

    void rotate(Vector3 p_axis, real_t p_phi);
    Vector3 rotated(Vector3 p_axis, real_t p_phi) const;

    /* Static Methods between 2 vector3s */

    Vector3 linear_interpolate(Vector3 p_b, real_t p_t) const;
    Vector3 slerp(Vector3 p_b, real_t p_t) const;
    Vector3 cubic_interpolate(const Vector3 &p_b, const Vector3 &p_pre_a, const Vector3 &p_post_b, real_t p_t) const;
    Vector3 cubic_interpolaten(const Vector3 &p_b, const Vector3 &p_pre_a, const Vector3 &p_post_b, real_t p_t) const;
    Vector3 move_toward(Vector3 p_to, const real_t p_delta) const;

    Vector3 cross(Vector3 p_b) const;
    constexpr _FORCE_INLINE_ real_t dot(Vector3 p_b) const;
    Basis outer(Vector3 p_b) const;
    Basis to_diagonal_matrix() const;

    Vector3 abs() const;
    Vector3 floor() const;
    Vector3 sign() const;
    Vector3 ceil() const;
    Vector3 round() const;

    real_t distance_to(Vector3 p_b) const;
    real_t distance_squared_to(Vector3 p_b) const;

    Vector3 posmod(real_t p_mod) const;
    Vector3 posmodv(Vector3 p_modv) const;
    Vector3 project(Vector3 p_b) const;

    real_t angle_to(Vector3 p_b) const;
    real_t signed_angle_to(const Vector3 &p_to, const Vector3 &p_axis) const;
    Vector3 direction_to(Vector3 p_b) const;

    _FORCE_INLINE_ Vector3 slide(Vector3 p_normal) const;
    _FORCE_INLINE_ Vector3 bounce(Vector3 p_normal) const;
    _FORCE_INLINE_ Vector3 reflect(Vector3 p_normal) const;

    inline bool is_equal_approx(const Vector3 &p_v, real_t p_tolerance=CMP_EPSILON) const {
        return Math::is_equal_approx(x, p_v.x,p_tolerance) && Math::is_equal_approx(y, p_v.y,p_tolerance) && Math::is_equal_approx(z, p_v.z,p_tolerance);
    }
    /* Operators */

    _FORCE_INLINE_ Vector3 &operator+=(Vector3 p_v);
    Vector3 operator+(Vector3 p_v) const;
    _FORCE_INLINE_ Vector3 &operator-=(Vector3 p_v);
    Vector3 operator-(Vector3 p_v) const;
    _FORCE_INLINE_ Vector3 &operator*=(Vector3 p_v);
    Vector3 operator*(Vector3 p_v) const;
    _FORCE_INLINE_ Vector3 &operator/=(Vector3 p_v);
    Vector3 operator/(Vector3 p_v) const;

    _FORCE_INLINE_ Vector3 &operator*=(real_t p_scalar);
    Vector3 operator*(real_t p_scalar) const;
    _FORCE_INLINE_ Vector3 &operator/=(real_t p_scalar);
    Vector3 operator/(real_t p_scalar) const;

    Vector3 operator-() const;

    bool operator==(Vector3 p_v) const { return x == p_v.x && y == p_v.y && z == p_v.z; }
    bool operator!=(Vector3 p_v) const { return x != p_v.x || y != p_v.y || z != p_v.z; }
    bool operator<(Vector3 p_v) const;
    bool operator<=(Vector3 p_v) const;
    bool operator>(Vector3 p_v) const;
    bool operator>=(Vector3 p_v) const;

    operator String() const;
    explicit constexpr operator Vector3i() const {
        return Vector3i((int)x, (int)y, (int)z);
    }

    explicit constexpr Vector3(Vector3i p_ivec) : x(float(p_ivec.x)),y(float(p_ivec.y)),z(float(p_ivec.z)) {}
    constexpr Vector3() noexcept : Vector3(0,0,0) {}
    constexpr Vector3(real_t p_x, real_t p_y, real_t p_z) noexcept : x(p_x),y(p_y),z(p_z) {}
};
static_assert (std::is_trivially_copyable<Vector3>() );

inline Vector3 Vector3::cross(Vector3 p_b) const {

    Vector3 ret(
            (y * p_b.z) - (z * p_b.y),
            (z * p_b.x) - (x * p_b.z),
            (x * p_b.y) - (y * p_b.x));

    return ret;
}

constexpr real_t Vector3::dot(Vector3 p_b) const {

    return x * p_b.x + y * p_b.y + z * p_b.z;
}

inline Vector3 Vector3::abs() const {

    return Vector3(Math::abs(x), Math::abs(y), Math::abs(z));
}

inline Vector3 Vector3::sign() const {
    return Vector3(SGN(x), SGN(y), SGN(z));
}

inline Vector3 Vector3::floor() const {

    return Vector3(Math::floor(x), Math::floor(y), Math::floor(z));
}

inline Vector3 Vector3::ceil() const {

    return Vector3(Math::ceil(x), Math::ceil(y), Math::ceil(z));
}

inline Vector3 Vector3::round() const {

    return Vector3(Math::round(x), Math::round(y), Math::round(z));
}

inline Vector3 Vector3::linear_interpolate(Vector3 p_b, real_t p_t) const {

    return Vector3(
            x + (p_t * (p_b.x - x)),
            y + (p_t * (p_b.y - y)),
            z + (p_t * (p_b.z - z)));
}

inline Vector3 Vector3::slerp(Vector3 p_b, real_t p_t) const {
    real_t theta = angle_to(p_b);
    return rotated(cross(p_b).normalized(), theta * p_t);
}

inline real_t Vector3::distance_to(Vector3 p_b) const {

    return (p_b - *this).length();
}

inline real_t Vector3::distance_squared_to(Vector3 p_b) const {

    return (p_b - *this).length_squared();
}

inline Vector3 Vector3::posmod(real_t p_mod) const {
    return Vector3(Math::fposmod(x, p_mod), Math::fposmod(y, p_mod), Math::fposmod(z, p_mod));
}

inline Vector3 Vector3::posmodv(Vector3 p_modv) const {
    return Vector3(Math::fposmod(x, p_modv.x), Math::fposmod(y, p_modv.y), Math::fposmod(z, p_modv.z));
}

inline Vector3 Vector3::project(Vector3 p_b) const {
    return p_b * (dot(p_b) / p_b.length_squared());
}

inline real_t Vector3::angle_to(Vector3 p_b) const {

    return Math::atan2(cross(p_b).length(), dot(p_b));
}

inline real_t Vector3::signed_angle_to(const Vector3 &p_to, const Vector3 &p_axis) const {
    Vector3 cross_to = cross(p_to);
    real_t unsigned_angle = Math::atan2(cross_to.length(), dot(p_to));
    real_t sign = cross_to.dot(p_axis);
    return (sign < 0) ? -unsigned_angle : unsigned_angle;
}

inline Vector3 Vector3::direction_to(Vector3 p_b) const {
    Vector3 ret(p_b.x - x, p_b.y - y, p_b.z - z);
    ret.normalize();
    return ret;
}

/* Operators */

Vector3 &Vector3::operator+=(Vector3 p_v) {

    x += p_v.x;
    y += p_v.y;
    z += p_v.z;
    return *this;
}

inline Vector3 Vector3::operator+(Vector3 p_v) const {

    return Vector3(x + p_v.x, y + p_v.y, z + p_v.z);
}

Vector3 &Vector3::operator-=(Vector3 p_v) {

    x -= p_v.x;
    y -= p_v.y;
    z -= p_v.z;
    return *this;
}
inline Vector3 Vector3::operator-(Vector3 p_v) const {

    return Vector3(x - p_v.x, y - p_v.y, z - p_v.z);
}

Vector3 &Vector3::operator*=(Vector3 p_v) {

    x *= p_v.x;
    y *= p_v.y;
    z *= p_v.z;
    return *this;
}
inline Vector3 Vector3::operator*(Vector3 p_v) const {

    return Vector3(x * p_v.x, y * p_v.y, z * p_v.z);
}

Vector3 &Vector3::operator/=(Vector3 p_v) {

    x /= p_v.x;
    y /= p_v.y;
    z /= p_v.z;
    return *this;
}

inline Vector3 Vector3::operator/(Vector3 p_v) const {

    return Vector3(x / p_v.x, y / p_v.y, z / p_v.z);
}

Vector3 &Vector3::operator*=(real_t p_scalar) {

    x *= p_scalar;
    y *= p_scalar;
    z *= p_scalar;
    return *this;
}

_FORCE_INLINE_ Vector3 operator*(real_t p_scalar, Vector3 p_vec) {

    return p_vec * p_scalar;
}

inline Vector3 Vector3::operator*(real_t p_scalar) const {

    return Vector3(x * p_scalar, y * p_scalar, z * p_scalar);
}

Vector3 &Vector3::operator/=(real_t p_scalar) {

    x /= p_scalar;
    y /= p_scalar;
    z /= p_scalar;
    return *this;
}

inline Vector3 Vector3::operator/(real_t p_scalar) const {

    return Vector3(x / p_scalar, y / p_scalar, z / p_scalar);
}

inline Vector3 Vector3::operator-() const {

    return Vector3(-x, -y, -z);
}

inline bool Vector3::operator<(Vector3 p_v) const {

    if (x == p_v.x) {
        if (y == p_v.y)
            return z < p_v.z;
        else
            return y < p_v.y;
    } else {
        return x < p_v.x;
    }
}

inline bool Vector3::operator>(Vector3 p_v) const {

    if (x == p_v.x) {
        if (y == p_v.y)
            return z > p_v.z;
        else
            return y > p_v.y;
    } else {
        return x > p_v.x;
    }
}

inline bool Vector3::operator<=(Vector3 p_v) const {

    if (x != p_v.x) {
        return x < p_v.x;
    }
    if (y != p_v.y)
        return y < p_v.y;

    return z <= p_v.z;
}

inline bool Vector3::operator>=(Vector3 p_v) const {

    if (x == p_v.x) {
        if (y == p_v.y)
            return z >= p_v.z;
        else
            return y > p_v.y;
    } else {
        return x > p_v.x;
    }
}

inline Vector3 vec3_cross(Vector3 p_a, Vector3 p_b) {

    return p_a.cross(p_b);
}

inline real_t vec3_dot(Vector3 p_a, Vector3 p_b) {

    return p_a.dot(p_b);
}

inline real_t Vector3::length() const {

    real_t x2 = x * x;
    real_t y2 = y * y;
    real_t z2 = z * z;

    return Math::sqrt(x2 + y2 + z2);
}

inline real_t Vector3::length_squared() const {

    real_t x2 = x * x;
    real_t y2 = y * y;
    real_t z2 = z * z;

    return x2 + y2 + z2;
}

void Vector3::normalize() {

    real_t lengthsq = length_squared();
    if (lengthsq == 0.0f) {
        x = y = z = 0;
    } else {
        real_t length = Math::sqrt(lengthsq);
        x /= length;
        y /= length;
        z /= length;
    }
}

inline Vector3 Vector3::normalized() const {

    Vector3 v = *this;
    v.normalize();
    return v;
}

bool Vector3::is_normalized() const {
    // use length_squared() instead of length() to avoid sqrt(), makes it more stringent.
    return Math::is_equal_approx(length_squared(), 1, float(UNIT_EPSILON));
}

inline Vector3 Vector3::inverse() const {

    return Vector3(1.0f / x, 1.0f / y, 1.0f / z);
}

// slide returns the component of the vector along the given plane, specified by its normal vector.
Vector3 Vector3::slide(Vector3 p_normal) const {
#ifdef MATH_CHECKS
    ERR_FAIL_COND_V_MSG(!p_normal.is_normalized(), Vector3(), "The normal Vector3 must be normalized.");
#endif
    return *this - p_normal * this->dot(p_normal);
}

Vector3 Vector3::bounce(Vector3 p_normal) const {
    return -reflect(p_normal);
}

Vector3 Vector3::reflect(Vector3 p_normal) const {
#ifdef MATH_CHECKS
    ERR_FAIL_COND_V_MSG(!p_normal.is_normalized(), Vector3(), "The normal Vector3 must be normalized.");
#endif
    return 2.0f * p_normal * this->dot(p_normal) - *this;
}
