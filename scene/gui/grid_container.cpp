/*************************************************************************/
/*  grid_container.cpp                                                   */
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

#include "grid_container.h"
#include "core/method_bind.h"
#include "core/set.h"

IMPL_GDCLASS(GridContainer)

void GridContainer::_notification(int p_what) {

    switch (p_what) {

        case NOTIFICATION_SORT_CHILDREN: {


            HashMap<int, int> col_minw; // Max of min_width  of all controls in each col (indexed by col).
            HashMap<int, int> row_minh; // Max of min_height of all controls in each row (indexed by row).
            Set<int> col_expanded; // Columns which have the SIZE_EXPAND flag set.
            Set<int> row_expanded; // Rows which have the SIZE_EXPAND flag set.

            int hsep = get_theme_constant("hseparation");
            int vsep = get_theme_constant("vseparation");
            int max_col = MIN(get_child_count(), columns);
            int max_row = std::ceil((float)get_child_count() / (float)columns);

            // Compute the per-column/per-row data.
            int valid_controls_index = 0;
            for (int i = 0; i < get_child_count(); i++) {
                Control *c = object_cast<Control>(get_child(i));
                if (!c || !c->is_visible_in_tree()) {
                    continue;
                }
                if (c->is_set_as_top_level()) {
                    continue;
                }

                int row = valid_controls_index / columns;
                int col = valid_controls_index % columns;
                valid_controls_index++;

                Size2i ms = c->get_combined_minimum_size();
                if (col_minw.contains(col))
                    col_minw[col] = M_MAX(col_minw[col], ms.width);
                else
                    col_minw[col] = ms.width;
                if (row_minh.contains(row))
                    row_minh[row] = M_MAX(row_minh[row], ms.height);
                else
                    row_minh[row] = ms.height;

                if (c->get_h_size_flags() & SIZE_EXPAND) {
                    col_expanded.insert(col);
                }
                if (c->get_v_size_flags() & SIZE_EXPAND) {
                    row_expanded.insert(row);
                }
            }
            // Consider all empty columns expanded.
            for (int i = valid_controls_index; i < columns; i++) {
                col_expanded.insert(i);
            }

            // Evaluate the remaining space for expanded columns/rows.
            Size2 remaining_space = get_size();
            for (eastl::pair<const int,int> &E : col_minw) {
                if (!col_expanded.contains(E.first))
                    remaining_space.width -= E.second;
            }

            for (eastl::pair<const int,int> &E : row_minh) {
                if (!row_expanded.contains(E.first))
                    remaining_space.height -= E.second;
            }
            remaining_space.height -= vsep * M_MAX(max_row - 1, 0);
            remaining_space.width -= hsep * M_MAX(max_col - 1, 0);

            bool can_fit = false;
            while (!can_fit && !col_expanded.empty()) {
                // Check if all minwidth constraints are OK if we use the remaining space.
                can_fit = true;
                int max_index = *col_expanded.begin();
                for (int E : col_expanded) {
                    if (col_minw[E] > col_minw[max_index]) {
                        max_index = E;
                    }
                    if (can_fit && (remaining_space.width / col_expanded.size()) < col_minw[E]) {
                        can_fit = false;
                    }
                }

                // If not, the column with maximum minwidth is not expanded.
                if (!can_fit) {
                    col_expanded.erase(max_index);
                    remaining_space.width -= col_minw[max_index];
                }
            }

            can_fit = false;
            while (!can_fit && !row_expanded.empty()) {
                // Check if all minheight constraints are OK if we use the remaining space.
                can_fit = true;
                int max_index = *row_expanded.begin();
                for (int E : row_expanded) {
                    if (row_minh[E] > row_minh[max_index]) {
                        max_index = E;
                    }
                    if (can_fit && (remaining_space.height / row_expanded.size()) < row_minh[E]) {
                        can_fit = false;
                    }
                }

                // If not, the row with maximum minheight is not expanded.
                if (!can_fit) {
                    row_expanded.erase(max_index);
                    remaining_space.height -= row_minh[max_index];
                }
            }

            // Finally, fit the nodes.
            int col_expand = !col_expanded.empty() ? remaining_space.width / col_expanded.size() : 0;
            int row_expand = !row_expanded.empty() ? remaining_space.height / row_expanded.size() : 0;

            int col_ofs = 0;
            int row_ofs = 0;

            valid_controls_index = 0;
            for (int i = 0; i < get_child_count(); i++) {
                Control *c = object_cast<Control>(get_child(i));
                if (!c || !c->is_visible_in_tree())
                    continue;
                int row = valid_controls_index / columns;
                int col = valid_controls_index % columns;
                valid_controls_index++;

                if (col == 0) {
                    col_ofs = 0;
                    if (row > 0)
                        row_ofs += (row_expanded.contains(row - 1) ? row_expand : row_minh[row - 1]) + vsep;
                }

                Point2 p(col_ofs, row_ofs);
                Size2 s(col_expanded.contains(col) ? col_expand : col_minw[col], row_expanded.contains(row) ? row_expand : row_minh[row]);

                fit_child_in_rect(c, Rect2(p, s));

                col_ofs += s.width + hsep;
            }

        } break;
        case NOTIFICATION_THEME_CHANGED: {

            minimum_size_changed();
        } break;
    }
}

void GridContainer::set_columns(int p_columns) {

    ERR_FAIL_COND(p_columns < 1);
    columns = p_columns;
    queue_sort();
    minimum_size_changed();
}

void GridContainer::_bind_methods() {

    BIND_METHOD(GridContainer,set_columns);
    BIND_METHOD(GridContainer,get_columns);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "columns", PropertyHint::Range, "1,1024,1"), "set_columns", "get_columns");
}

Size2 GridContainer::get_minimum_size() const {

    HashMap<int, int> col_minw;
    HashMap<int, int> row_minh;

    int hsep = get_theme_constant("hseparation");
    int vsep = get_theme_constant("vseparation");

    int max_row = 0;
    int max_col = 0;

    int valid_controls_index = 0;
    for (int i = 0; i < get_child_count(); i++) {

        Control *c = object_cast<Control>(get_child(i));
        if (!c || !c->is_visible())
            continue;
        int row = valid_controls_index / columns;
        int col = valid_controls_index % columns;
        valid_controls_index++;

        Size2i ms = c->get_combined_minimum_size();
        if (col_minw.contains(col))
            col_minw[col] = M_MAX(col_minw[col], ms.width);
        else
            col_minw[col] = ms.width;

        if (row_minh.contains(row))
            row_minh[row] = M_MAX(row_minh[row], ms.height);
        else
            row_minh[row] = ms.height;
        max_col = M_MAX(col, max_col);
        max_row = M_MAX(row, max_row);
    }

    Size2 ms;

    for (eastl::pair<const int,int> &E : col_minw) {
        ms.width += E.second;
    }

    for (eastl::pair<const int,int> &E : row_minh) {
        ms.height += E.second;
    }

    ms.height += vsep * max_row;
    ms.width += hsep * max_col;

    return ms;
}

GridContainer::GridContainer() = default;
