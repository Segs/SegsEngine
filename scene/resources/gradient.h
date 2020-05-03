/*************************************************************************/
/*  gradient.h                                                           */
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

#include "core/resource.h"
#include "core/color.h"

class Gradient : public Resource {
    GDCLASS(Gradient,Resource)
    OBJ_SAVE_TYPE(Gradient)

public:
    struct Point {
        float offset;
        Color color;
        bool operator<(const Point &p_ponit) const {
            return offset < p_ponit.offset;
        }
    };

private:
    Vector<Point> points;
    bool is_sorted;

protected:
    static void _bind_methods();

public:
    Gradient();
    ~Gradient() override;

    void add_point(float p_offset, const Color &p_color);
    void remove_point(int p_index);

    void set_points(const Vector<Point> &p_points);
    Vector<Point> &get_points();

    void set_offset(int pos, const float offset);
    float get_offset(int pos) const;

    void set_color(int pos, const Color &color);
    Color get_color(int pos) const;

    void set_offsets(const Vector<float> &p_offsets);
    Vector<float> get_offsets() const;

    void set_colors(const Vector<Color> &p_colors);
    Vector<Color> get_colors() const;

    Color get_color_at_offset(float p_offset);
    Color interpolate(float p_offset) {
        // c# scripting api helper
        return get_color_at_offset(p_offset);
    }

    int get_point_count() const;
};
