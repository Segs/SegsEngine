/*************************************************************************/
/*  editor_atlas_packer.h                                                */
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

#include "core/math/vector2.h"

#include "core/vector.h"
#include "scene/resources/bit_map.h"

class EditorAtlasPacker {
public:
    struct Chart {
        PODVector<Vector2> vertices;
        struct Face {
            int vertex[3];
        };
        PODVector<Face> faces;
        Vector2 final_offset;

        bool can_transpose;
        bool transposed;
    };

private:
    struct PlottedBitmap {
        PODVector<int> top_heights;
        PODVector<int> bottom_heights;
        Vector2i offset;
        Vector2 final_pos;
        int chart_index;
        int area;
        bool transposed;


        bool operator<(const PlottedBitmap &p_bm) const {
            return area > p_bm.area;
        }
    };

    static void _plot_triangle(Ref<BitMap> p_bitmap, Vector2i *vertices);

public:
    static void chart_pack(PODVector<Chart> &charts, int &r_width, int &r_height, int p_atlas_max_size = 2048, int p_cell_resolution = 4);
};
