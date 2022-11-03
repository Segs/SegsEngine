/*************************************************************************/
/*  line_2d.h                                                            */
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

#include "node_2d.h"

class Curve;

enum class Line2DJointMode : int8_t {
        LINE_JOINT_SHARP = 0,
        LINE_JOINT_BEVEL,
        LINE_JOINT_ROUND
    };

enum class Line2DCapMode : int8_t {
        LINE_CAP_NONE = 0,
        LINE_CAP_BOX,
        LINE_CAP_ROUND
    };

enum class Line2DTextureMode : int8_t {
        LINE_TEXTURE_NONE = 0,
        LINE_TEXTURE_TILE,
        LINE_TEXTURE_STRETCH
    };
struct Line2DDrawableComponent {
    Vector<Vector2> _points;
    Color _default_color;
    Ref<Curve> _curve;
    Ref<Gradient> _gradient;
    Ref<Texture> _texture;
    float _width;
    float _sharp_limit;
    int _round_precision;
    Line2DJointMode _joint_mode;
    Line2DCapMode _begin_cap_mode;
    Line2DCapMode _end_cap_mode;
    Line2DTextureMode _texture_mode;
    bool _antialiased;

    Line2DDrawableComponent(const Line2DDrawableComponent &) = delete;
    Line2DDrawableComponent &operator=(const Line2DDrawableComponent &) = delete;
    Line2DDrawableComponent(Line2DDrawableComponent &&) = default;
    Line2DDrawableComponent &operator=(Line2DDrawableComponent &&) = default;
};
class GODOT_EXPORT Line2D : public Node2D {

    GDCLASS(Line2D,Node2D)

public:
    Line2D();
#ifdef TOOLS_ENABLED
    Rect2 _edit_get_rect() const override;
    bool _edit_use_rect() const override;
    bool _edit_is_selected_on_click(const Point2 &p_point, float p_tolerance) const override;
#endif
    void set_points(Span<const Vector2> p_points);
    Span<const Vector2> get_points() const;

    void set_point_position(int i, Vector2 pos);
    Vector2 get_point_position(int i) const;

    int get_point_count() const;

    void clear_points();

    void add_point(Vector2 pos, int atpos = -1);
    void remove_point(int i);

    void set_width(float width);
    float get_width() const;

    void set_curve(const Ref<Curve> &curve);
    Ref<Curve> get_curve() const;

    void set_default_color(Color color);
    Color get_default_color() const;

    void set_gradient(const Ref<Gradient> &gradient);
    Ref<Gradient> get_gradient() const;

    void set_texture(const Ref<Texture> &texture);
    Ref<Texture> get_texture() const;

    void set_texture_mode(const Line2DTextureMode mode);
    Line2DTextureMode get_texture_mode() const;

    void set_joint_mode(Line2DJointMode mode);
    Line2DJointMode get_joint_mode() const;

    void set_begin_cap_mode(Line2DCapMode mode);
    Line2DCapMode get_begin_cap_mode() const;

    void set_end_cap_mode(Line2DCapMode mode);
    Line2DCapMode get_end_cap_mode() const;

    void set_sharp_limit(float limit);
    float get_sharp_limit() const;

    void set_round_precision(int precision);
    int get_round_precision() const;

    void set_antialiased(bool p_antialiased);
    bool get_antialiased() const;
protected:
    void _notification(int p_what);
    void _draw();

    static void _bind_methods();

private:
    void _gradient_changed();
    void _curve_changed();

};
