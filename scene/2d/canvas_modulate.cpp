/*************************************************************************/
/*  canvas_modulate.cpp                                                  */
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

#include "canvas_modulate.h"
#include "scene/main/scene_tree.h"
#include "core/method_bind.h"
#include "core/translation_helpers.h"

IMPL_GDCLASS(CanvasModulate)

void CanvasModulate::_notification(int p_what) {

    StringName name("_canvas_modulate_" + itos(get_canvas().get_id()));
    if (p_what == NOTIFICATION_ENTER_CANVAS) {

        if (is_visible_in_tree()) {
            VisualServer::get_singleton()->canvas_set_modulate(get_canvas(), color);
            add_to_group(name);
        }

    } else if (p_what == NOTIFICATION_EXIT_CANVAS) {

        if (is_visible_in_tree()) {
            VisualServer::get_singleton()->canvas_set_modulate(get_canvas(), Color(1, 1, 1, 1));
            remove_from_group(name);
        }
    } else if (p_what == NOTIFICATION_VISIBILITY_CHANGED) {

        if (is_visible_in_tree()) {
            VisualServer::get_singleton()->canvas_set_modulate(get_canvas(), color);
            add_to_group(name);
        } else {
            VisualServer::get_singleton()->canvas_set_modulate(get_canvas(), Color(1, 1, 1, 1));
            remove_from_group(name);
        }

        update_configuration_warning();
    }
}

void CanvasModulate::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_color", {"color"}), &CanvasModulate::set_color);
    MethodBinder::bind_method(D_METHOD("get_color"), &CanvasModulate::get_color);

    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "color"), "set_color", "get_color");
}

void CanvasModulate::set_color(const Color &p_color) {

    color = p_color;
    if (is_visible_in_tree()) {
        VisualServer::get_singleton()->canvas_set_modulate(get_canvas(), color);
    }
}
Color CanvasModulate::get_color() const {

    return color;
}

StringName CanvasModulate::get_configuration_warning() const {

    if (!is_visible_in_tree() || !is_inside_tree())
        return StringName();

    StringName name("_canvas_modulate_" + itos(get_canvas().get_id()));
    Deque<Node *> nodes;
    get_tree()->get_nodes_in_group(name, &nodes);

    if (nodes.size() > 1) {
        return TTR("Only one visible CanvasModulate is allowed per scene (or set of instanced scenes). The first created one will work, while the rest will be ignored.");
    }

    return StringName();
}

CanvasModulate::CanvasModulate() {
    color = Color(1, 1, 1, 1);
}

CanvasModulate::~CanvasModulate() {
}
