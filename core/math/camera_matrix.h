/*************************************************************************/
/*  camera_matrix.h                                                      */
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

#include "core/math/math_defs.h"   // for real_t
#include "core/math/math_funcs.h"  // for Math
#include "core/math/plane.h"       // for Plane
#include "core/math/vector2.h"     // for Vector2
#include "core/math/vector3.h"     // for Vector3
#include "core/math/transform.h"

struct Rect2;

struct GODOT_EXPORT Frustum {
    Plane planes[6];
    operator eastl::span<Plane,6>() { return planes; }
    operator eastl::span<const Plane,6>() const { return planes; }
    [[nodiscard]] Plane *begin() { return planes; }
    [[nodiscard]] Plane *end() { return planes+6; }
    Plane &operator[](int idx) { return planes[idx]; }
    [[nodiscard]] constexpr bool empty() const {
        return *this == Frustum();
    }
    constexpr bool operator==(const Frustum &other) const {
        for(int i=0; i<6; ++i) {
            if(planes[i]!=other.planes[i])
                return false;
        }
        return true;
    }
    void clear() {
        *this = Frustum();
    }
    // MSVC in conformance mode fails to resolve eastl::internal::HasSizeAndData<Frustum>::value to false, so we pretend we're a container :/
    constexpr Plane *data() { return planes; }
    constexpr const Plane *data() const { return planes; }
    constexpr size_t size() const { return 6; }
};

struct GODOT_EXPORT CameraMatrix {

    enum Planes {
        PLANE_NEAR,
        PLANE_FAR,
        PLANE_LEFT,
        PLANE_TOP,
        PLANE_RIGHT,
        PLANE_BOTTOM
    };

    real_t matrix[4][4];

    float determinant() const;
    void set_identity();
    void set_zero();
    void set_light_bias();
    void set_depth_correction(bool p_flip_y = true);
    void set_light_atlas_rect(const Rect2 &p_rect);
    void set_perspective(real_t p_fovy_degrees, real_t p_aspect, real_t p_z_near, real_t p_z_far, bool p_flip_fov = false);
    void set_perspective(real_t p_fovy_degrees, real_t p_aspect, real_t p_z_near, real_t p_z_far, bool p_flip_fov, int p_eye, real_t p_intraocular_dist, real_t p_convergence_dist);
    void set_for_hmd(int p_eye, real_t p_aspect, real_t p_intraocular_dist, real_t p_display_width, real_t p_display_to_lens, real_t p_oversample, real_t p_z_near, real_t p_z_far);
    void set_orthogonal(real_t p_left, real_t p_right, real_t p_bottom, real_t p_top, real_t p_znear, real_t p_zfar);
    void set_orthogonal(real_t p_size, real_t p_aspect, real_t p_znear, real_t p_zfar, bool p_flip_fov = false);
    void set_frustum(real_t p_left, real_t p_right, real_t p_bottom, real_t p_top, real_t p_near, real_t p_far);
    void set_frustum(real_t p_size, real_t p_aspect, Vector2 p_offset, real_t p_near, real_t p_far, bool p_flip_fov = false);

    static real_t get_fovy(real_t p_fovx, real_t p_aspect) {

        return Math::rad2deg(Math::atan(p_aspect * Math::tan(Math::deg2rad(p_fovx) * 0.5f)) * 2.0f);
    }

    real_t get_z_far() const;
    real_t get_z_near() const;
    real_t get_aspect() const;
    real_t get_fov() const;
    bool is_orthogonal() const;

    Frustum get_projection_planes(const Transform &p_transform) const;

    bool get_endpoints(const Transform &p_transform, Vector3 *p_8points) const;
    Vector2 get_viewport_half_extents() const;
    void get_far_plane_size(real_t &r_width, real_t &r_height) const;

    void invert();
    CameraMatrix inverse() const;

    CameraMatrix operator*(const CameraMatrix &p_matrix) const;

    Plane xform4(const Plane &p_vec4) const;
    _FORCE_INLINE_ Vector3 xform(const Vector3 &p_vec3) const;

    operator String() const;

    void scale_translate_to_fit(const AABB &p_aabb);
    void make_scale(const Vector3 &p_scale);
    int get_pixels_per_meter(int p_for_pixel_width) const;
    operator Transform() const;

    void flip_y();

    bool operator==(const CameraMatrix &p_cam) const {
        for (uint32_t i = 0; i < 4; i++) {
            for (uint32_t j = 0; j < 4; j++) {
                if (matrix[i][j] != p_cam.matrix[i][j]) {
                    return false;
                }
            }
        }
        return true;
    }

    bool operator!=(const CameraMatrix &p_cam) const {
        return !(*this == p_cam);
    }

    CameraMatrix();
    CameraMatrix(const Transform &p_transform);
    ~CameraMatrix() = default;
};

Vector3 CameraMatrix::xform(const Vector3 &p_vec3) const {

    Vector3 ret;
    ret.x = matrix[0][0] * p_vec3.x + matrix[1][0] * p_vec3.y + matrix[2][0] * p_vec3.z + matrix[3][0];
    ret.y = matrix[0][1] * p_vec3.x + matrix[1][1] * p_vec3.y + matrix[2][1] * p_vec3.z + matrix[3][1];
    ret.z = matrix[0][2] * p_vec3.x + matrix[1][2] * p_vec3.y + matrix[2][2] * p_vec3.z + matrix[3][2];
    const real_t w = matrix[0][3] * p_vec3.x + matrix[1][3] * p_vec3.y + matrix[2][3] * p_vec3.z + matrix[3][3];
    return ret / w;
}
