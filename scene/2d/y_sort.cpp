/*************************************************************************/
/*  y_sort.cpp                                                           */
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

#include "y_sort.h"
#include "core/method_bind.h"
#include "servers/rendering_server.h"

IMPL_GDCLASS(YSort)

void YSort::set_sort_enabled(bool p_enabled) {

    sort_enabled = p_enabled;
    RenderingServer::get_singleton()->canvas_item_set_sort_children_by_y(get_canvas_item(), sort_enabled);
}

bool YSort::is_sort_enabled() const {

    return sort_enabled;
}

void YSort::_bind_methods() {

    SE_BIND_METHOD(YSort,set_sort_enabled);
    SE_BIND_METHOD(YSort,is_sort_enabled);

    ADD_GROUP("Sort", "sort_");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "sort_enabled"), "set_sort_enabled", "is_sort_enabled");
}

YSort::YSort() {
    set_sort_enabled(sort_enabled);
}
