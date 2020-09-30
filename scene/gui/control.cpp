/*************************************************************************/
/*  control.cpp                                                          */
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

#include "control.h"

#include "core/callable_method_pointer.h"
#include "core/message_queue.h"
#include "core/method_bind.h"
#include "core/object_db.h"
#include "core/os/os.h"
#include "core/script_language.h"
#include "scene/main/canvas_layer.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/resources/style_box.h"
#include "scene/resources/theme.h"
#include "scene/scene_string_names.h"
#include "servers/rendering_server.h"
#include "scene/gui/control_enum_casters.h"
#include "scene/resources/font.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_settings.h"
#include "editor/plugins/canvas_item_editor_plugin.h"
#endif

#include "EASTL/sort.h"

//Debugging helper for listing cases where we fail to query for specific icon
//#define WARN_ON_MISSING_ICONS

#ifdef WARN_ON_MISSING_ICONS
#include "core/string_formatter.h"

void warn_missing_icon(const char* type, const char* icon) {
    WARN_PRINT(FormatVE("Missing icon for %s:%s", type, icon));
}
#define WARN_MISSING_ICON(theme,icon,type,name) \
    if(theme->is_default_icon(icon))\
        warn_missing_icon(type.asCString(),name.asCString());\
    else\
        ((void)9)
#else
#define WARN_MISSING_ICON(theme,icon,type,name)
#endif


IMPL_GDCLASS(Control)

#ifdef TOOLS_ENABLED
Dictionary Control::_edit_get_state() const {

    Dictionary s;
    s["rotation"] = get_rotation();
    s["scale"] = get_scale();
    s["pivot"] = get_pivot_offset();
    Array anchors;
    anchors.push_back(get_anchor(Margin::Left));
    anchors.push_back(get_anchor(Margin::Top));
    anchors.push_back(get_anchor(Margin::Right));
    anchors.push_back(get_anchor(Margin::Bottom));
    s["anchors"] = anchors;
    Array margins;
    margins.push_back(get_margin(Margin::Left));
    margins.push_back(get_margin(Margin::Top));
    margins.push_back(get_margin(Margin::Right));
    margins.push_back(get_margin(Margin::Bottom));
    s["margins"] = margins;
    return s;
}

void Control::_edit_set_state(const Dictionary &p_state) {

    Dictionary state = p_state;

    set_rotation(state["rotation"].as<float>());
    set_scale(state["scale"].as<Vector2>());
    set_pivot_offset(state["pivot"].as<Vector2>());
    Array anchors = state["anchors"].as<Array>();
    data.anchor[(int8_t)Margin::Left] = anchors[0].as<float>();
    data.anchor[(int8_t)Margin::Top] = anchors[1].as<float>();
    data.anchor[(int8_t)Margin::Right] = anchors[2].as<float>();
    data.anchor[(int8_t)Margin::Bottom] = anchors[3].as<float>();
    Array margins = state["margins"].as<Array>();
    data.margin[(int8_t)Margin::Left] = margins[0].as<float>();
    data.margin[(int8_t)Margin::Top] = margins[1].as<float>();
    data.margin[(int8_t)Margin::Right] = margins[2].as<float>();
    data.margin[(int8_t)Margin::Bottom] = margins[3].as<float>();
    _size_changed();
}

void Control::_edit_set_position(const Point2 &p_position) {
#ifdef TOOLS_ENABLED
    set_position(p_position, CanvasItemEditor::get_singleton()->is_anchors_mode_enabled());
#else
    // Unlikely to happen. TODO: enclose all _edit_ functions into TOOLS_ENABLED
    set_position(p_position);
#endif
};

Point2 Control::_edit_get_position() const {
    return get_position();
};

void Control::_edit_set_scale(const Size2 &p_scale) {
    set_scale(p_scale);
}

Size2 Control::_edit_get_scale() const {
    return data.scale;
}

void Control::_edit_set_rect(const Rect2 &p_edit_rect) {
#ifdef TOOLS_ENABLED
    set_position((get_position() + get_transform().basis_xform(p_edit_rect.position)).snapped(Vector2(1, 1)), CanvasItemEditor::get_singleton()->is_anchors_mode_enabled());
    set_size(p_edit_rect.size.snapped(Vector2(1, 1)), CanvasItemEditor::get_singleton()->is_anchors_mode_enabled());
#else
    // Unlikely to happen. TODO: enclose all _edit_ functions into TOOLS_ENABLED
    set_position((get_position() + get_transform().basis_xform(p_edit_rect.position)).snapped(Vector2(1, 1)));
    set_size(p_edit_rect.size.snapped(Vector2(1, 1)));
#endif
}

Rect2 Control::_edit_get_rect() const {
    return Rect2(Point2(), get_size());
}

bool Control::_edit_use_rect() const {
    return true;
}

void Control::_edit_set_rotation(float p_rotation) {
    set_rotation(p_rotation);
}

float Control::_edit_get_rotation() const {
    return get_rotation();
}

bool Control::_edit_use_rotation() const {
    return true;
}

void Control::_edit_set_pivot(const Point2 &p_pivot) {
    Vector2 delta_pivot = p_pivot - get_pivot_offset();
    Vector2 move = Vector2((cos(data.rotation) - 1.0f) * delta_pivot.x - sin(data.rotation) * delta_pivot.y, sin(data.rotation) * delta_pivot.x + (cos(data.rotation) - 1.0f) * delta_pivot.y);
    set_position(get_position() + move);
    set_pivot_offset(p_pivot);
}

Point2 Control::_edit_get_pivot() const {
    return get_pivot_offset();
}

bool Control::_edit_use_pivot() const {
    return true;
}
Size2 Control::_edit_get_minimum_size() const {

    return get_combined_minimum_size();
}

#endif // TOOLS_ENABLED
void Control::set_custom_minimum_size(const Size2 &p_custom) {

    if (p_custom == data.custom_minimum_size)
        return;
    data.custom_minimum_size = p_custom;
    minimum_size_changed();
}

Size2 Control::get_custom_minimum_size() const {

    return data.custom_minimum_size;
}

void Control::_update_minimum_size_cache() {

    Size2 minsize = get_minimum_size();
    minsize.x = M_MAX(minsize.x, data.custom_minimum_size.x);
    minsize.y = M_MAX(minsize.y, data.custom_minimum_size.y);

    bool size_changed = false;
    if (data.minimum_size_cache != minsize)
        size_changed = true;

    data.minimum_size_cache = minsize;
    data.minimum_size_valid = true;

    if (size_changed)
        minimum_size_changed();
}

Size2 Control::get_combined_minimum_size() const {

    if (!data.minimum_size_valid) {
        const_cast<Control *>(this)->_update_minimum_size_cache();
    }
    return data.minimum_size_cache;
}

Transform2D Control::_get_internal_transform() const {

    Transform2D rot_scale;
    rot_scale.set_rotation_and_scale(data.rotation, data.scale);
    Transform2D offset;
    offset.set_origin(-data.pivot_offset);

    return offset.affine_inverse() * (rot_scale * offset);
}

bool Control::_set(const StringName &p_name, const Variant &p_value) {

    StringName name = p_name;
    if (!StringUtils::begins_with(name,"custom")) {
        return false;
    }

    StringName dname(StringUtils::get_slice(name,'/', 1));
    if (p_value.get_type() == VariantType::NIL) {

        if (StringUtils::begins_with(name,"custom_icons/")) {
            if (data.icon_override.contains(dname)) {
                data.icon_override[dname]->disconnect("changed",callable_mp(this, &ClassName::_override_changed));
            }
            data.icon_override.erase(dname);
            notification(NOTIFICATION_THEME_CHANGED);
        } else if (StringUtils::begins_with(name,"custom_shaders/")) {
            if (data.shader_override.contains(dname)) {
                data.shader_override[dname]->disconnect("changed",callable_mp(this, &ClassName::_override_changed));
            }
            data.shader_override.erase(dname);
            notification(NOTIFICATION_THEME_CHANGED);
        } else if (StringUtils::begins_with(name,"custom_styles/")) {
            if (data.style_override.contains(dname)) {
                data.style_override[dname]->disconnect("changed",callable_mp(this, &ClassName::_override_changed));
            }
            data.style_override.erase(dname);
            notification(NOTIFICATION_THEME_CHANGED);
        } else if (StringUtils::begins_with(name,"custom_fonts/")) {
            if (data.font_override.contains(dname)) {
                data.font_override[dname]->disconnect("changed",callable_mp(this, &ClassName::_override_changed));
            }
            data.font_override.erase(dname);
            notification(NOTIFICATION_THEME_CHANGED);
        } else if (StringUtils::begins_with(name,"custom_colors/")) {
            data.color_override.erase(dname);
            notification(NOTIFICATION_THEME_CHANGED);
        } else if (StringUtils::begins_with(name,"custom_constants/")) {
            data.constant_override.erase(dname);
            notification(NOTIFICATION_THEME_CHANGED);
        } else
            return false;

    } else {
        if (StringUtils::begins_with(name,"custom_icons/")) {
            add_icon_override(dname, refFromVariant<Texture>(p_value));
        } else if (StringUtils::begins_with(name,"custom_shaders/")) {
            add_shader_override(dname, refFromVariant<Shader>(p_value));
        } else if (StringUtils::begins_with(name,"custom_styles/")) {
            add_style_override(dname, refFromVariant<StyleBox>(p_value));
        } else if (StringUtils::begins_with(name,"custom_fonts/")) {
            add_font_override(dname, refFromVariant<Font>(p_value));
        } else if (StringUtils::begins_with(name,"custom_colors/")) {
            add_color_override(dname, p_value.as<Color>());
        } else if (StringUtils::begins_with(name,"custom_constants/")) {
            add_constant_override(dname, p_value.as<int>());
        } else
            return false;
    }
    return true;
}

void Control::_update_minimum_size() {

    if (!is_inside_tree())
        return;

    Size2 minsize = get_combined_minimum_size();
    if (minsize.x > data.size_cache.x ||
            minsize.y > data.size_cache.y) {
        _size_changed();
    }

    data.updating_last_minimum_size = false;

    if (minsize != data.last_minimum_size) {
        data.last_minimum_size = minsize;
        emit_signal(SceneStringNames::get_singleton()->minimum_size_changed);
    }
}

bool Control::_get(const StringName &p_name, Variant &r_ret) const {

    StringView sname = p_name;

    if (!StringUtils::begins_with(sname,"custom")) {
        return false;
    }

    StringName name(StringUtils::get_slice(sname,'/', 1));
    if (StringUtils::begins_with(sname,"custom_icons/")) {
        r_ret = data.icon_override.contains(name) ? Variant(data.icon_override.at(name)) : Variant();
    } else if (StringUtils::begins_with(sname,"custom_shaders/")) {
        r_ret = data.shader_override.contains(name) ? Variant(data.shader_override.at(name)) : Variant();
    } else if (StringUtils::begins_with(sname,"custom_styles/")) {
        r_ret = data.style_override.contains(name) ? Variant(data.style_override.at(name)) : Variant();
    } else if (StringUtils::begins_with(sname,"custom_fonts/")) {
        r_ret = data.font_override.contains(name) ? Variant(data.font_override.at(name)) : Variant();
    } else if (StringUtils::begins_with(sname,"custom_colors/")) {
        r_ret = data.color_override.contains(name) ? Variant(data.color_override.at(name)) : Variant();
    } else if (StringUtils::begins_with(sname,"custom_constants/")) {
        r_ret = data.constant_override.contains(name) ? Variant(data.constant_override.at(name)) : Variant();
    } else
        return false;

    return true;
}
void Control::_get_property_list(Vector<PropertyInfo> *p_list) const {

    Ref<Theme> theme = Theme::get_default();
    /* Using the default theme since the properties below are meant for editor only
    if (data.theme) {

        theme = data.theme;
    } else {
        theme = Theme::get_default();

    }*/

    {
        Vector<StringName> names;
        theme->get_icon_list(get_class_name(), &names);
        String basename("custom_icons/");
        for (const StringName &E : names) {

            uint32_t hint = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_CHECKABLE;
            if (data.icon_override.contains(E))
                hint |= PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED;

            p_list->push_back(PropertyInfo(VariantType::OBJECT, StringName(basename + E), PropertyHint::ResourceType, "Texture", hint));
        }
    }
    {
        Vector<StringName> names;
        theme->get_shader_list(get_class_name(), &names);
        String basename("custom_shaders/");
        for (const StringName &E : names) {

            uint32_t hint = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_CHECKABLE;
            if (data.shader_override.contains(E))
                hint |= PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED;

            p_list->push_back(PropertyInfo(VariantType::OBJECT, StringName(basename + E), PropertyHint::ResourceType, "Shader,VisualShader", hint));
        }
    }
    {
        Vector<StringName> names(theme->get_stylebox_list(get_class_name()));
        String basename("custom_styles/");
        for (const StringName &E : names) {

            uint32_t hint = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_CHECKABLE;
            if (data.style_override.contains(E))
                hint |= PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED;

            p_list->push_back(PropertyInfo(VariantType::OBJECT, StringName(basename + E), PropertyHint::ResourceType, "StyleBox", hint));
        }
    }
    {
        Vector<StringName> names;
        theme->get_font_list(get_class_name(), &names);
        String basename("custom_fonts/");
        for (const StringName &E : names) {

            uint32_t hint = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_CHECKABLE;
            if (data.font_override.contains(E))
                hint |= PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED;

            p_list->push_back(PropertyInfo(VariantType::OBJECT, StringName(basename + E), PropertyHint::ResourceType, "Font", hint));
        }
    }
    {
        Vector<StringName> names;
        theme->get_color_list(get_class_name(), &names);
        String basename("custom_colors/");
        for (const StringName &E : names) {

            uint32_t hint = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_CHECKABLE;
            if (data.color_override.contains(E))
                hint |= PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED;

            p_list->push_back(PropertyInfo(VariantType::COLOR, StringName(basename + E), PropertyHint::None, "", hint));
        }
    }
    {
        Vector<StringName> names;
        theme->get_constant_list(get_class_name(), &names);
        String basename("custom_constants/");
        for (const StringName &E : names) {

            uint32_t hint = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_CHECKABLE;
            if (data.constant_override.contains(E))
                hint |= PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED;

            p_list->push_back(PropertyInfo(VariantType::INT, StringName(basename + E), PropertyHint::Range, "-16384,16384", hint));
        }
    }
}

Control *Control::get_parent_control() const {

    return data.parent;
}

void Control::_resize(const Size2 &p_size) {

    _size_changed();
}

//moved theme configuration here, so controls can set up even if still not inside active scene

void Control::add_child_notify(Node *p_child) {

    Control *child_c = object_cast<Control>(p_child);
    if (!child_c)
        return;

    if (not child_c->data.theme && data.theme_owner) {
        _propagate_theme_changed(child_c, data.theme_owner); //need to propagate here, since many controls may require setting up stuff
    }
}

void Control::remove_child_notify(Node *p_child) {

    Control *child_c = object_cast<Control>(p_child);
    if (!child_c)
        return;

    if (child_c->data.theme_owner && not child_c->data.theme) {
        _propagate_theme_changed(child_c, nullptr);
    }
}

void Control::_update_canvas_item_transform() {

    Transform2D xform = _get_internal_transform();
    xform[2] += get_position();

    // We use a little workaround to avoid flickering when moving the pivot with _edit_set_pivot()
    if (is_inside_tree() && Math::abs(Math::sin(data.rotation * 4.0f)) < 0.00001f &&
            get_viewport()->is_snap_controls_to_pixels_enabled()) {
        xform[2] = xform[2].round();
    }

    RenderingServer::get_singleton()->canvas_item_set_transform(get_canvas_item(), xform);
}

void Control::_notification(int p_notification) {

    switch (p_notification) {

        case NOTIFICATION_ENTER_TREE: {

        } break;
        case NOTIFICATION_POST_ENTER_TREE: {
            data.minimum_size_valid = false;
            _size_changed();
        } break;
        case NOTIFICATION_EXIT_TREE: {

            get_viewport()->_gui_remove_control(this);

        } break;

        case NOTIFICATION_ENTER_CANVAS: {

            data.parent = object_cast<Control>(get_parent());

            if (is_set_as_toplevel()) {
                get_viewport()->_gui_add_subwindow_control(this);
                data.SI = this;

                if (not data.theme && data.parent && data.parent->data.theme_owner) {
                    data.theme_owner = data.parent->data.theme_owner;
                    notification(NOTIFICATION_THEME_CHANGED);
                }

            } else {

                Node *parent = this; //meh
                Control *parent_control = nullptr;
                bool subwindow = false;

                while (parent) {

                    parent = parent->get_parent();

                    if (!parent)
                        break;

                    CanvasItem *ci = object_cast<CanvasItem>(parent);
                    if (ci && ci->is_set_as_toplevel()) {
                        subwindow = true;
                        break;
                    }

                    parent_control = object_cast<Control>(parent);

                    if (parent_control) {
                        break;
                    } else if (ci) {

                    } else {
                        break;
                    }
                }

                if (parent_control) {
                    //do nothing, has a parent control
                    if (not data.theme && parent_control->data.theme_owner) {
                        data.theme_owner = parent_control->data.theme_owner;
                        notification(NOTIFICATION_THEME_CHANGED);
                    }
                } else if (subwindow) {
                    //is a subwindow (process input before other controls for that canvas)
                    get_viewport()->_gui_add_subwindow_control(this);
                    data.SI = this;
                } else {
                    //is a regular root control
                    get_viewport()->_gui_add_root_control(this);
                    data.RI = this;
                }

                data.parent_canvas_item = get_parent_item();

                if (data.parent_canvas_item) {

                    data.parent_canvas_item->connect("item_rect_changed",callable_mp(this, &ClassName::_size_changed));
                } else {
                    //connect viewport
                    get_viewport()->connect("size_changed",callable_mp(this, &ClassName::_size_changed));
                }
            }

            /*
            if (not data.theme && data.parent && data.parent->data.theme_owner) {
                data.theme_owner=data.parent->data.theme_owner;
                notification(NOTIFICATION_THEME_CHANGED);
            }
            */

        } break;
        case NOTIFICATION_EXIT_CANVAS: {

            if (data.parent_canvas_item) {

                data.parent_canvas_item->disconnect("item_rect_changed",callable_mp(this, &ClassName::_size_changed));
                data.parent_canvas_item = nullptr;
            } else if (!is_set_as_toplevel()) {
                //disconnect viewport
                get_viewport()->disconnect("size_changed",callable_mp(this, &ClassName::_size_changed));
            }

            if (data.MI != nullptr) {
                get_viewport()->_gui_remove_modal_control(data.MI);
                data.MI = nullptr;
            }

            if (data.SI != nullptr) {
                get_viewport()->_gui_remove_subwindow_control(data.SI);
                data.SI = nullptr;
            }

            if (data.RI) {
                get_viewport()->_gui_remove_root_control(data.RI);
                data.RI = nullptr;
            }

            data.parent = nullptr;
            data.parent_canvas_item = nullptr;
            /*
            if (data.theme_owner && not data.theme) {
                data.theme_owner=nullptr;
                notification(NOTIFICATION_THEME_CHANGED);
            }
            */

        } break;
        case NOTIFICATION_MOVED_IN_PARENT: {
            // some parents need to know the order of the childrens to draw (like TabContainer)
            // update if necessary
            if (data.parent)
                data.parent->update();
            update();

            if (data.SI) {
                get_viewport()->_gui_set_subwindow_order_dirty();
            }
            if (data.RI) {
                get_viewport()->_gui_set_root_order_dirty();
            }

        } break;
        case NOTIFICATION_RESIZED: {

            emit_signal(SceneStringNames::get_singleton()->resized);
        } break;
        case NOTIFICATION_DRAW: {

            _update_canvas_item_transform();
            RenderingServer::get_singleton()->canvas_item_set_custom_rect(get_canvas_item(), !data.disable_visibility_clip, Rect2(Point2(), get_size()));
            RenderingServer::get_singleton()->canvas_item_set_clip(get_canvas_item(), data.clip_contents);
            //emit_signal(SceneStringNames::get_singleton()->draw);

        } break;
        case NOTIFICATION_MOUSE_ENTER: {

            emit_signal(SceneStringNames::get_singleton()->mouse_entered);
        } break;
        case NOTIFICATION_MOUSE_EXIT: {

            emit_signal(SceneStringNames::get_singleton()->mouse_exited);
        } break;
        case NOTIFICATION_FOCUS_ENTER: {

            emit_signal(SceneStringNames::get_singleton()->focus_entered);
            update();
        } break;
        case NOTIFICATION_FOCUS_EXIT: {

            emit_signal(SceneStringNames::get_singleton()->focus_exited);
            update();

        } break;
        case NOTIFICATION_THEME_CHANGED: {
            minimum_size_changed();
            update();
        } break;
        case NOTIFICATION_MODAL_CLOSE: {

            emit_signal("modal_closed");
        } break;
        case NOTIFICATION_VISIBILITY_CHANGED: {

            if (!is_visible_in_tree()) {

                if (get_viewport() != nullptr)
                    get_viewport()->_gui_hid_control(this);

                if (is_inside_tree()) {
                    _modal_stack_remove();
                }

                //remove key focus
                //remove modalness
            } else {
                data.minimum_size_valid = false;
                _size_changed();
            }

        } break;
        case SceneTree::NOTIFICATION_WM_UNFOCUS_REQUEST: {

            get_viewport()->_gui_unfocus_control(this);

        } break;
    }
}

bool Control::clips_input() const {

    if (get_script_instance()) {
        return get_script_instance()->call(SceneStringNames::get_singleton()->_clips_input).as<bool>();
    }
    return false;
}
bool Control::has_point(const Point2 &p_point) const {

    if (get_script_instance()) {
        Variant v = p_point;
        const Variant *p = &v;
        Callable::CallError ce;
        Variant ret = get_script_instance()->call(SceneStringNames::get_singleton()->has_point, &p, 1, ce);
        if (ce.error == Callable::CallError::CALL_OK) {
            return ret.as<bool>();
        }
    }
    /*if (has_stylebox("mask")) {
        Ref<StyleBox> mask = get_stylebox("mask");
        return mask->test_mask(p_point,Rect2(Point2(),get_size()));
    }*/
    return Rect2(Point2(), get_size()).has_point(p_point);
}

void Control::set_drag_forwarding(Control *p_target) {

    if (p_target)
        data.drag_owner = p_target->get_instance_id();
    else
        data.drag_owner = 0;
}

Variant Control::get_drag_data(const Point2 &p_point) {

    if (data.drag_owner.is_valid()) {
        Object *obj = gObjectDB().get_instance(data.drag_owner);
        if (obj) {
            Control *c = object_cast<Control>(obj);
            return c->call_va("get_drag_data_fw", p_point, Variant(this));
        }
    }

    if (get_script_instance()) {
        Variant v = p_point;
        const Variant *p = &v;
        Callable::CallError ce;
        Variant ret = get_script_instance()->call(SceneStringNames::get_singleton()->get_drag_data, &p, 1, ce);
        if (ce.error == Callable::CallError::CALL_OK)
            return ret;
    }

    return Variant();
}

bool Control::can_drop_data(const Point2 &p_point, const Variant &p_data) const {

    if (data.drag_owner.is_valid()) {
        Object *obj = gObjectDB().get_instance(data.drag_owner);
        if (obj) {
            Control *c = object_cast<Control>(obj);
            return c->call_va("can_drop_data_fw", p_point, p_data, Variant(this)).as<bool>();
        }
    }

    if (get_script_instance()) {
        Variant v = p_point;
        const Variant *p[2] = { &v, &p_data };
        Callable::CallError ce;
        Variant ret = get_script_instance()->call(SceneStringNames::get_singleton()->can_drop_data, p, 2, ce);
        if (ce.error == Callable::CallError::CALL_OK)
            return ret.as<bool>();
    }

    return false;
}
void Control::drop_data(const Point2 &p_point, const Variant &p_data) {

    if (data.drag_owner.is_valid()) {
        Object *obj = gObjectDB().get_instance(data.drag_owner);
        if (obj) {
            Control *c = object_cast<Control>(obj);
            c->call_va("drop_data_fw", p_point, p_data, Variant(this));
            return;
        }
    }

    if (get_script_instance()) {
        Variant v = p_point;
        const Variant *p[2] = { &v, &p_data };
        Callable::CallError ce;
        Variant ret = get_script_instance()->call(SceneStringNames::get_singleton()->drop_data, p, 2, ce);
        if (ce.error == Callable::CallError::CALL_OK)
            return;
    }
}

void Control::force_drag(const Variant &p_data, Control *p_control) {

    ERR_FAIL_COND(!is_inside_tree());
    ERR_FAIL_COND(p_data.get_type() == VariantType::NIL);

    get_viewport()->_gui_force_drag(this, p_data, p_control);
}

void Control::set_drag_preview(Control *p_control) {

    ERR_FAIL_COND(!is_inside_tree());
    ERR_FAIL_COND(!get_viewport()->gui_is_dragging());
    get_viewport()->_gui_set_drag_preview(this, p_control);
}

bool Control::is_window_modal_on_top() const {

    if (!is_inside_tree())
        return false;

    return get_viewport()->_gui_is_modal_on_top(this);
}

uint64_t Control::get_modal_frame() const {

    return data.modal_frame;
}

Size2 Control::get_minimum_size() const {

    ScriptInstance *si = const_cast<Control *>(this)->get_script_instance();
    if (si) {

        Callable::CallError ce;
        Variant s = si->call(SceneStringNames::get_singleton()->_get_minimum_size, nullptr, 0, ce);
        if (ce.error == Callable::CallError::CALL_OK)
            return s.as<Vector2>();
    }
    return Size2();
}

Ref<Texture> Control::get_icon(const StringName &p_name, const StringName &p_type) const {

    if (p_type.empty() || p_type == get_class_name()) {

        auto tex = data.icon_override.find(p_name);
        if (tex!=data.icon_override.end())
            return tex->second;
    }

    StringName type = p_type ? p_type : get_class_name();

    // try with custom themes
    Control *theme_owner = data.theme_owner;

    while (theme_owner) {

        StringName class_name = type;

        while (class_name != StringName()) {
            if (theme_owner->data.theme->has_icon(p_name, class_name)) {
                Ref<Texture> res(theme_owner->data.theme->get_icon(p_name, class_name));
                WARN_MISSING_ICON(theme_owner->data.theme,res,p_name,class_name);
                return res;
            }

            class_name = ClassDB::get_parent_class_nocheck(class_name);
        }

        Control *parent = object_cast<Control>(theme_owner->get_parent());

        if (parent)
            theme_owner = parent->data.theme_owner;
        else
            theme_owner = nullptr;
    }

    if (Theme::get_project_default()) {
        if (Theme::get_project_default()->has_icon(p_name, type)) {
            Ref<Texture> res(Theme::get_project_default()->get_icon(p_name, type));
            WARN_MISSING_ICON(Theme::get_project_default(), res, p_name, type);
            return res;
        }
    }

    Ref<Texture> res(Theme::get_default()->get_icon(p_name, type));
    WARN_MISSING_ICON(Theme::get_default(), res, p_name, type);
    return res;
}

Ref<Shader> Control::get_shader(const StringName &p_name, const StringName &p_type) const {
    if (p_type.empty() || p_type == get_class_name()) {

        auto sdr = data.shader_override.find(p_name);
        if (sdr!=data.shader_override.end())
            return sdr->second;
    }

    StringName type = p_type ? p_type : get_class_name();

    // try with custom themes
    Control *theme_owner = data.theme_owner;

    while (theme_owner) {

        StringName class_name = type;

        while (class_name != StringName()) {
            if (theme_owner->data.theme->has_shader(p_name, class_name)) {
                return theme_owner->data.theme->get_shader(p_name, class_name);
            }

            class_name = ClassDB::get_parent_class_nocheck(class_name);
        }

        Control *parent = object_cast<Control>(theme_owner->get_parent());

        if (parent)
            theme_owner = parent->data.theme_owner;
        else
            theme_owner = nullptr;
    }

    if (Theme::get_project_default()) {
        if (Theme::get_project_default()->has_shader(p_name, type)) {
            return Theme::get_project_default()->get_shader(p_name, type);
        }
    }

    return Theme::get_default()->get_shader(p_name, type);
}

Ref<StyleBox> Control::get_stylebox(const StringName &p_name, const StringName &p_type) const {

    if (p_type.empty() || p_type == get_class_name()) {

        auto style = data.style_override.find(p_name);
        if (style!=data.style_override.end())
            return style->second;
    }

    StringName type = p_type ? p_type : get_class_name();

    // try with custom themes
    Control *theme_owner = data.theme_owner;

    StringName class_name = type;

    while (theme_owner) {

        while (class_name != StringName()) {
            if (theme_owner->data.theme->has_stylebox(p_name, class_name)) {
                return theme_owner->data.theme->get_stylebox(p_name, class_name);
            }

            class_name = ClassDB::get_parent_class_nocheck(class_name);
        }

        class_name = type;

        Control *parent = object_cast<Control>(theme_owner->get_parent());

        if (parent)
            theme_owner = parent->data.theme_owner;
        else
            theme_owner = nullptr;
    }

    while (class_name != StringName()) {
        if (Theme::get_project_default() && Theme::get_project_default()->has_stylebox(p_name, type))
            return Theme::get_project_default()->get_stylebox(p_name, type);

        if (Theme::get_default()->has_stylebox(p_name, class_name))
            return Theme::get_default()->get_stylebox(p_name, class_name);

        class_name = ClassDB::get_parent_class_nocheck(class_name);
    }
    return Theme::get_default()->get_stylebox(p_name, type);
}
Ref<Font> Control::get_font(const StringName &p_name, const StringName &p_type) const {

    if (p_type.empty() || p_type == get_class_name()) {
        auto font = data.font_override.find(p_name);
        if (font!=data.font_override.end())
            return font->second;
    }

    StringName type = p_type ? p_type : get_class_name();

    // try with custom themes
    Control *theme_owner = data.theme_owner;

    while (theme_owner) {

        StringName class_name = type;

        while (class_name != StringName()) {
            if (theme_owner->data.theme->has_font(p_name, class_name)) {
                return theme_owner->data.theme->get_font(p_name, class_name);
            }

            class_name = ClassDB::get_parent_class_nocheck(class_name);
        }

        if (theme_owner->data.theme->get_default_theme_font())
            return theme_owner->data.theme->get_default_theme_font();
        Control *parent = object_cast<Control>(theme_owner->get_parent());

        if (parent)
            theme_owner = parent->data.theme_owner;
        else
            theme_owner = nullptr;
    }

    return Theme::get_default()->get_font(p_name, type);
}
Color Control::get_color(const StringName &p_name, const StringName &p_type) const {

    if (p_type.empty() || p_type == get_class_name()) {
        auto color = data.color_override.find(p_name);
        if (color!=data.color_override.end())
            return color->second;
    }

    StringName type = p_type ? p_type : get_class_name();
    // try with custom themes
    Control *theme_owner = data.theme_owner;

    while (theme_owner) {

        StringName class_name = type;

        while (class_name != StringName()) {
            if (theme_owner->data.theme->has_color(p_name, class_name)) {
                return theme_owner->data.theme->get_color(p_name, class_name);
            }

            class_name = ClassDB::get_parent_class_nocheck(class_name);
        }

        Control *parent = object_cast<Control>(theme_owner->get_parent());

        if (parent)
            theme_owner = parent->data.theme_owner;
        else
            theme_owner = nullptr;
    }

    if (Theme::get_project_default()) {
        if (Theme::get_project_default()->has_color(p_name, type)) {
            return Theme::get_project_default()->get_color(p_name, type);
        }
    }
    return Theme::get_default()->get_color(p_name, type);
}

int Control::get_constant(const StringName &p_name, const StringName &p_type) const {

    if (p_type.empty() || p_type == get_class_name()) {
        auto constant = data.constant_override.find(p_name);
        if (constant!=data.constant_override.end())
            return constant->second;
    }

    StringName type = p_type ? p_type : get_class_name();
    // try with custom themes
    Control *theme_owner = data.theme_owner;

    while (theme_owner) {

        StringName class_name = type;

        while (class_name != StringName()) {
            if (theme_owner->data.theme->has_constant(p_name, class_name)) {
                return theme_owner->data.theme->get_constant(p_name, class_name);
            }

            class_name = ClassDB::get_parent_class_nocheck(class_name);
        }

        Control *parent = object_cast<Control>(theme_owner->get_parent());

        if (parent)
            theme_owner = parent->data.theme_owner;
        else
            theme_owner = nullptr;
    }

    if (Theme::get_project_default()) {
        if (Theme::get_project_default()->has_constant(p_name, type)) {
            return Theme::get_project_default()->get_constant(p_name, type);
        }
    }
    return Theme::get_default()->get_constant(p_name, type);
}

bool Control::has_icon_override(const StringName &p_name) const {

    auto tex = data.icon_override.find(p_name);
    return tex != data.icon_override.end() && tex->second;
}

bool Control::has_shader_override(const StringName &p_name) const {

    auto sdr = data.shader_override.find(p_name);
    return sdr != data.shader_override.end() && sdr->second;
}

bool Control::has_stylebox_override(const StringName &p_name) const {

    auto style = data.style_override.find(p_name);
    return style != data.style_override.end() && style->second;
}

bool Control::has_font_override(const StringName &p_name) const {

    auto font = data.font_override.find(p_name);
    return font != data.font_override.end() && font->second;
}

bool Control::has_color_override(const StringName &p_name) const {

    return data.color_override.contains(p_name);
}

bool Control::has_constant_override(const StringName &p_name) const {

    return data.constant_override.contains(p_name);
}

bool Control::has_icon(const StringName &p_name, const StringName &p_type) const {

    if (p_type == StringName() || p_type == get_class_name()) {
        if (has_icon_override(p_name))
            return true;
    }

    StringName type = p_type ? p_type : get_class_name();

    // try with custom themes
    Control *theme_owner = data.theme_owner;

    while (theme_owner) {

        StringName class_name = type;

        while (class_name != StringName()) {
            if (theme_owner->data.theme->has_icon(p_name, class_name)) {
                return true;
            }
            class_name = ClassDB::get_parent_class_nocheck(class_name);
        }

        Control *parent = object_cast<Control>(theme_owner->get_parent());

        if (parent)
            theme_owner = parent->data.theme_owner;
        else
            theme_owner = nullptr;
    }

    if (Theme::get_project_default()) {
        if (Theme::get_project_default()->has_color(p_name, type)) {
            return true;
        }
    }
    return Theme::get_default()->has_icon(p_name, type);
}

bool Control::has_shader(const StringName &p_name, const StringName &p_type) const {

    if (p_type == StringName() || p_type == get_class_name()) {
        if (has_shader_override(p_name))
            return true;
    }

    StringName type = p_type ? p_type : get_class_name();

    // try with custom themes
    Control *theme_owner = data.theme_owner;

    while (theme_owner) {

        StringName class_name = type;

        while (class_name != StringName()) {
            if (theme_owner->data.theme->has_shader(p_name, class_name)) {
                return true;
            }
            class_name = ClassDB::get_parent_class_nocheck(class_name);
        }

        Control *parent = object_cast<Control>(theme_owner->get_parent());

        if (parent)
            theme_owner = parent->data.theme_owner;
        else
            theme_owner = nullptr;
    }

    if (Theme::get_project_default()) {
        if (Theme::get_project_default()->has_shader(p_name, type)) {
            return true;
        }
    }
    return Theme::get_default()->has_shader(p_name, type);
}
bool Control::has_stylebox(const StringName &p_name, const StringName &p_type) const {

    if (p_type == StringName() || p_type == get_class_name()) {
        if (has_stylebox_override(p_name))
            return true;
    }

    StringName type = p_type ? p_type : get_class_name();

    // try with custom themes
    Control *theme_owner = data.theme_owner;

    while (theme_owner) {

        StringName class_name = type;

        while (class_name != StringName()) {
            if (theme_owner->data.theme->has_stylebox(p_name, class_name)) {
                return true;
            }
            class_name = ClassDB::get_parent_class_nocheck(class_name);
        }

        Control *parent = object_cast<Control>(theme_owner->get_parent());

        if (parent)
            theme_owner = parent->data.theme_owner;
        else
            theme_owner = nullptr;
    }

    if (Theme::get_project_default()) {
        if (Theme::get_project_default()->has_stylebox(p_name, type)) {
            return true;
        }
    }
    return Theme::get_default()->has_stylebox(p_name, type);
}
bool Control::has_font(const StringName &p_name, const StringName &p_type) const {

    if (p_type == StringName() || p_type == get_class_name()) {
        if (has_font_override(p_name))
            return true;
    }

    StringName type = p_type ? p_type : get_class_name();

    // try with custom themes
    Control *theme_owner = data.theme_owner;

    while (theme_owner) {

        StringName class_name = type;

        while (class_name != StringName()) {
            if (theme_owner->data.theme->has_font(p_name, class_name)) {
                return true;
            }
            class_name = ClassDB::get_parent_class_nocheck(class_name);
        }

        Control *parent = object_cast<Control>(theme_owner->get_parent());

        if (parent)
            theme_owner = parent->data.theme_owner;
        else
            theme_owner = nullptr;
    }

    if (Theme::get_project_default()) {
        if (Theme::get_project_default()->has_font(p_name, type)) {
            return true;
        }
    }
    return Theme::get_default()->has_font(p_name, type);
}

bool Control::has_color(const StringName &p_name, const StringName &p_type) const {

    if (p_type == StringName() || p_type == get_class_name()) {
        if (has_color_override(p_name))
            return true;
    }

    StringName type = p_type ? p_type : get_class_name();

    // try with custom themes
    Control *theme_owner = data.theme_owner;

    while (theme_owner) {

        StringName class_name = type;

        while (class_name != StringName()) {
            if (theme_owner->data.theme->has_color(p_name, class_name)) {
                return true;
            }
            class_name = ClassDB::get_parent_class_nocheck(class_name);
        }

        Control *parent = object_cast<Control>(theme_owner->get_parent());

        if (parent)
            theme_owner = parent->data.theme_owner;
        else
            theme_owner = nullptr;
    }

    if (Theme::get_project_default()) {
        if (Theme::get_project_default()->has_color(p_name, type)) {
            return true;
        }
    }
    return Theme::get_default()->has_color(p_name, type);
}

bool Control::has_constant(const StringName &p_name, const StringName &p_type) const {

    if (p_type == StringName() || p_type == get_class_name()) {
        if (has_constant_override(p_name))
            return true;
    }

    StringName type = p_type ? p_type : get_class_name();

    // try with custom themes
    Control *theme_owner = data.theme_owner;

    while (theme_owner) {

        StringName class_name = type;

        while (class_name != StringName()) {
            if (theme_owner->data.theme->has_constant(p_name, class_name)) {
                return true;
            }
            class_name = ClassDB::get_parent_class_nocheck(class_name);
        }

        Control *parent = object_cast<Control>(theme_owner->get_parent());

        if (parent)
            theme_owner = parent->data.theme_owner;
        else
            theme_owner = nullptr;
    }

    if (Theme::get_project_default()) {
        if (Theme::get_project_default()->has_constant(p_name, type)) {
            return true;
        }
    }
    return Theme::get_default()->has_constant(p_name, type);
}

void Control::set_tooltip_utf8(StringView p_tooltip)
{
    data.tooltip = StringName(p_tooltip);
    update_configuration_warning();

}

Rect2 Control::get_parent_anchorable_rect() const {
    if (!is_inside_tree())
        return Rect2();

    Rect2 parent_rect;
    if (data.parent_canvas_item) {
        parent_rect = data.parent_canvas_item->get_anchorable_rect();
    } else {
        parent_rect = get_viewport()->get_visible_rect();
    }

    return parent_rect;
}

Size2 Control::get_parent_area_size() const {

    return get_parent_anchorable_rect().size;
}

void Control::_size_changed() {

    Rect2 parent_rect = get_parent_anchorable_rect();

    float margin_pos[4];

    for (int i = 0; i < 4; i++) {

        float area = parent_rect.size[i & 1];
        margin_pos[i] = data.margin[i] + (data.anchor[i] * area);
    }

    Point2 new_pos_cache = Point2(margin_pos[0], margin_pos[1]);
    Size2 new_size_cache = Point2(margin_pos[2], margin_pos[3]) - new_pos_cache;

    Size2 minimum_size = get_combined_minimum_size();

    if (minimum_size.width > new_size_cache.width) {
        if (data.h_grow == GROW_DIRECTION_BEGIN) {
            new_pos_cache.x += new_size_cache.width - minimum_size.width;
        } else if (data.h_grow == GROW_DIRECTION_BOTH) {
            new_pos_cache.x += 0.5 * (new_size_cache.width - minimum_size.width);
        }

        new_size_cache.width = minimum_size.width;
    }

    if (minimum_size.height > new_size_cache.height) {
        if (data.v_grow == GROW_DIRECTION_BEGIN) {
            new_pos_cache.y += new_size_cache.height - minimum_size.height;
        } else if (data.v_grow == GROW_DIRECTION_BOTH) {
            new_pos_cache.y += 0.5 * (new_size_cache.height - minimum_size.height);
        }

        new_size_cache.height = minimum_size.height;
    }

    bool pos_changed = new_pos_cache != data.pos_cache;
    bool size_changed = new_size_cache != data.size_cache;

    data.pos_cache = new_pos_cache;
    data.size_cache = new_size_cache;

    if (is_inside_tree()) {
        if (size_changed) {
            notification(NOTIFICATION_RESIZED);
        }
        if (pos_changed || size_changed) {
            item_rect_changed(size_changed);
            _change_notify_margins();
            _notify_transform();
        }

        if (pos_changed && !size_changed) {
            _update_canvas_item_transform(); //move because it won't be updated
        }
    }
}

void Control::set_anchor(Margin p_margin, float p_anchor, bool p_keep_margin, bool p_push_opposite_anchor) {
    ERR_FAIL_INDEX((int)p_margin, int(Margin::Max));

    Rect2 parent_rect = get_parent_anchorable_rect();
    float parent_range = (p_margin == Margin::Left || p_margin == Margin::Right) ? parent_rect.size.x : parent_rect.size.y;
    float previous_margin_pos = data.margin[(int)p_margin] + data.anchor[(int)p_margin] * parent_range;
    float previous_opposite_margin_pos = data.margin[((int)p_margin + 2) % 4] + data.anchor[((int)p_margin + 2) % 4] * parent_range;

    data.anchor[(int)p_margin] = p_anchor;

    if (((p_margin == Margin::Left || p_margin == Margin::Top) && data.anchor[(int)p_margin] > data.anchor[((int)p_margin + 2) % 4]) ||
            ((p_margin == Margin::Right || p_margin == Margin::Bottom) && data.anchor[(int)p_margin] < data.anchor[((int)p_margin + 2) % 4])) {
        if (p_push_opposite_anchor) {
            data.anchor[((int)p_margin + 2) % 4] = data.anchor[(int)p_margin];
        } else {
            data.anchor[(int)p_margin] = data.anchor[((int)p_margin + 2) % 4];
        }
    }

    if (!p_keep_margin) {
        data.margin[(int)p_margin] = previous_margin_pos - data.anchor[(int)p_margin] * parent_range;
        if (p_push_opposite_anchor) {
            data.margin[((int)p_margin + 2) % 4] = previous_opposite_margin_pos - data.anchor[((int)p_margin + 2) % 4] * parent_range;
        }
    }
    if (is_inside_tree()) {
        _size_changed();
    }

    update();
    Object_change_notify(this,"anchor_left");
    Object_change_notify(this,"anchor_right");
    Object_change_notify(this,"anchor_top");
    Object_change_notify(this,"anchor_bottom");
}

void Control::_set_anchor(Margin p_margin, float p_anchor) {
    set_anchor(p_margin, p_anchor);
}

void Control::set_anchor_and_margin(Margin p_margin, float p_anchor, float p_pos, bool p_push_opposite_anchor) {

    set_anchor(p_margin, p_anchor, false, p_push_opposite_anchor);
    set_margin(p_margin, p_pos);
}

void Control::set_anchors_preset(LayoutPreset p_preset, bool p_keep_margins) {
    //Left
    switch (p_preset) {
        case PRESET_TOP_LEFT:
        case PRESET_BOTTOM_LEFT:
        case PRESET_CENTER_LEFT:
        case PRESET_TOP_WIDE:
        case PRESET_BOTTOM_WIDE:
        case PRESET_LEFT_WIDE:
        case PRESET_HCENTER_WIDE:
        case PRESET_WIDE:
            set_anchor(Margin::Left, ANCHOR_BEGIN, p_keep_margins);
            break;

        case PRESET_CENTER_TOP:
        case PRESET_CENTER_BOTTOM:
        case PRESET_CENTER:
        case PRESET_VCENTER_WIDE:
            set_anchor(Margin::Left, 0.5, p_keep_margins);
            break;

        case PRESET_TOP_RIGHT:
        case PRESET_BOTTOM_RIGHT:
        case PRESET_CENTER_RIGHT:
        case PRESET_RIGHT_WIDE:
            set_anchor(Margin::Left, ANCHOR_END, p_keep_margins);
            break;
    }

    // Top
    switch (p_preset) {
        case PRESET_TOP_LEFT:
        case PRESET_TOP_RIGHT:
        case PRESET_CENTER_TOP:
        case PRESET_LEFT_WIDE:
        case PRESET_RIGHT_WIDE:
        case PRESET_TOP_WIDE:
        case PRESET_VCENTER_WIDE:
        case PRESET_WIDE:
            set_anchor(Margin::Top, ANCHOR_BEGIN, p_keep_margins);
            break;

        case PRESET_CENTER_LEFT:
        case PRESET_CENTER_RIGHT:
        case PRESET_CENTER:
        case PRESET_HCENTER_WIDE:
            set_anchor(Margin::Top, 0.5, p_keep_margins);
            break;

        case PRESET_BOTTOM_LEFT:
        case PRESET_BOTTOM_RIGHT:
        case PRESET_CENTER_BOTTOM:
        case PRESET_BOTTOM_WIDE:
            set_anchor(Margin::Top, ANCHOR_END, p_keep_margins);
            break;
    }

    // Right
    switch (p_preset) {
        case PRESET_TOP_LEFT:
        case PRESET_BOTTOM_LEFT:
        case PRESET_CENTER_LEFT:
        case PRESET_LEFT_WIDE:
            set_anchor(Margin::Right, ANCHOR_BEGIN, p_keep_margins);
            break;

        case PRESET_CENTER_TOP:
        case PRESET_CENTER_BOTTOM:
        case PRESET_CENTER:
        case PRESET_VCENTER_WIDE:
            set_anchor(Margin::Right, 0.5, p_keep_margins);
            break;

        case PRESET_TOP_RIGHT:
        case PRESET_BOTTOM_RIGHT:
        case PRESET_CENTER_RIGHT:
        case PRESET_TOP_WIDE:
        case PRESET_RIGHT_WIDE:
        case PRESET_BOTTOM_WIDE:
        case PRESET_HCENTER_WIDE:
        case PRESET_WIDE:
            set_anchor(Margin::Right, ANCHOR_END, p_keep_margins);
            break;
    }

    // Bottom
    switch (p_preset) {
        case PRESET_TOP_LEFT:
        case PRESET_TOP_RIGHT:
        case PRESET_CENTER_TOP:
        case PRESET_TOP_WIDE:
            set_anchor(Margin::Bottom, ANCHOR_BEGIN, p_keep_margins);
            break;

        case PRESET_CENTER_LEFT:
        case PRESET_CENTER_RIGHT:
        case PRESET_CENTER:
        case PRESET_HCENTER_WIDE:
            set_anchor(Margin::Bottom, 0.5, p_keep_margins);
            break;

        case PRESET_BOTTOM_LEFT:
        case PRESET_BOTTOM_RIGHT:
        case PRESET_CENTER_BOTTOM:
        case PRESET_LEFT_WIDE:
        case PRESET_RIGHT_WIDE:
        case PRESET_BOTTOM_WIDE:
        case PRESET_VCENTER_WIDE:
        case PRESET_WIDE:
            set_anchor(Margin::Bottom, ANCHOR_END, p_keep_margins);
            break;
    }
}

void Control::set_margins_preset(LayoutPreset p_preset, LayoutPresetMode p_resize_mode, int p_margin) {
    ERR_FAIL_INDEX((int)p_preset, 16);
    ERR_FAIL_INDEX((int)p_resize_mode, 4);
    // Calculate the size if the node is not resized
    Size2 min_size = get_minimum_size();
    Size2 new_size = get_size();
    if (p_resize_mode == PRESET_MODE_MINSIZE || p_resize_mode == PRESET_MODE_KEEP_HEIGHT) {
        new_size.x = min_size.x;
    }
    if (p_resize_mode == PRESET_MODE_MINSIZE || p_resize_mode == PRESET_MODE_KEEP_WIDTH) {
        new_size.y = min_size.y;
    }

    Rect2 parent_rect = get_parent_anchorable_rect();

    //Left
    switch (p_preset) {
        case PRESET_TOP_LEFT:
        case PRESET_BOTTOM_LEFT:
        case PRESET_CENTER_LEFT:
        case PRESET_TOP_WIDE:
        case PRESET_BOTTOM_WIDE:
        case PRESET_LEFT_WIDE:
        case PRESET_HCENTER_WIDE:
        case PRESET_WIDE:
            data.margin[0] = parent_rect.size.x * (0.0f - data.anchor[0]) + p_margin + parent_rect.position.x;
            break;

        case PRESET_CENTER_TOP:
        case PRESET_CENTER_BOTTOM:
        case PRESET_CENTER:
        case PRESET_VCENTER_WIDE:
            data.margin[0] = parent_rect.size.x * (0.5f - data.anchor[0]) - new_size.x / 2 + parent_rect.position.x;
            break;

        case PRESET_TOP_RIGHT:
        case PRESET_BOTTOM_RIGHT:
        case PRESET_CENTER_RIGHT:
        case PRESET_RIGHT_WIDE:
            data.margin[0] = parent_rect.size.x * (1.0f - data.anchor[0]) - new_size.x - p_margin + parent_rect.position.x;
            break;
    }

    // Top
    switch (p_preset) {
        case PRESET_TOP_LEFT:
        case PRESET_TOP_RIGHT:
        case PRESET_CENTER_TOP:
        case PRESET_LEFT_WIDE:
        case PRESET_RIGHT_WIDE:
        case PRESET_TOP_WIDE:
        case PRESET_VCENTER_WIDE:
        case PRESET_WIDE:
            data.margin[1] = parent_rect.size.y * (0.0f - data.anchor[1]) + p_margin + parent_rect.position.y;
            break;

        case PRESET_CENTER_LEFT:
        case PRESET_CENTER_RIGHT:
        case PRESET_CENTER:
        case PRESET_HCENTER_WIDE:
            data.margin[1] = parent_rect.size.y * (0.5f - data.anchor[1]) - new_size.y / 2 + parent_rect.position.y;
            break;

        case PRESET_BOTTOM_LEFT:
        case PRESET_BOTTOM_RIGHT:
        case PRESET_CENTER_BOTTOM:
        case PRESET_BOTTOM_WIDE:
            data.margin[1] = parent_rect.size.y * (1.0f - data.anchor[1]) - new_size.y - p_margin + parent_rect.position.y;
            break;
    }

    // Right
    switch (p_preset) {
        case PRESET_TOP_LEFT:
        case PRESET_BOTTOM_LEFT:
        case PRESET_CENTER_LEFT:
        case PRESET_LEFT_WIDE:
            data.margin[2] = parent_rect.size.x * (0.0f - data.anchor[2]) + new_size.x + p_margin + parent_rect.position.x;
            break;

        case PRESET_CENTER_TOP:
        case PRESET_CENTER_BOTTOM:
        case PRESET_CENTER:
        case PRESET_VCENTER_WIDE:
            data.margin[2] = parent_rect.size.x * (0.5f - data.anchor[2]) + new_size.x / 2 + parent_rect.position.x;
            break;

        case PRESET_TOP_RIGHT:
        case PRESET_BOTTOM_RIGHT:
        case PRESET_CENTER_RIGHT:
        case PRESET_TOP_WIDE:
        case PRESET_RIGHT_WIDE:
        case PRESET_BOTTOM_WIDE:
        case PRESET_HCENTER_WIDE:
        case PRESET_WIDE:
            data.margin[2] = parent_rect.size.x * (1.0f - data.anchor[2]) - p_margin + parent_rect.position.x;
            break;
    }

    // Bottom
    switch (p_preset) {
        case PRESET_TOP_LEFT:
        case PRESET_TOP_RIGHT:
        case PRESET_CENTER_TOP:
        case PRESET_TOP_WIDE:
            data.margin[3] = parent_rect.size.y * (0.0f - data.anchor[3]) + new_size.y + p_margin + parent_rect.position.y;
            break;

        case PRESET_CENTER_LEFT:
        case PRESET_CENTER_RIGHT:
        case PRESET_CENTER:
        case PRESET_HCENTER_WIDE:
            data.margin[3] = parent_rect.size.y * (0.5f - data.anchor[3]) + new_size.y / 2 + parent_rect.position.y;
            break;

        case PRESET_BOTTOM_LEFT:
        case PRESET_BOTTOM_RIGHT:
        case PRESET_CENTER_BOTTOM:
        case PRESET_LEFT_WIDE:
        case PRESET_RIGHT_WIDE:
        case PRESET_BOTTOM_WIDE:
        case PRESET_VCENTER_WIDE:
        case PRESET_WIDE:
            data.margin[3] = parent_rect.size.y * (1.0f - data.anchor[3]) - p_margin + parent_rect.position.y;
            break;
    }

    _size_changed();
}

void Control::set_anchors_and_margins_preset(LayoutPreset p_preset, LayoutPresetMode p_resize_mode, int p_margin) {
    set_anchors_preset(p_preset);
    set_margins_preset(p_preset, p_resize_mode, p_margin);
}

float Control::get_anchor(Margin p_margin) const {
    ERR_FAIL_INDEX_V(int(p_margin), 4, 0.0);

    return data.anchor[(int)p_margin];
}

void Control::_change_notify_margins() {

    // this avoids sending the whole object data again on a change
    Object_change_notify(this,"margin_left");
    Object_change_notify(this,"margin_top");
    Object_change_notify(this,"margin_right");
    Object_change_notify(this,"margin_bottom");
    Object_change_notify(this,"rect_position");
    Object_change_notify(this,"rect_size");
}

void Control::set_margin(Margin p_margin, float p_value) {
    ERR_FAIL_INDEX((int)p_margin, 4);

    data.margin[(int)p_margin] = p_value;
    _size_changed();
}

void Control::set_begin(const Size2 &p_point) {

    data.margin[0] = p_point.x;
    data.margin[1] = p_point.y;
    _size_changed();
}

void Control::set_end(const Size2 &p_point) {

    data.margin[2] = p_point.x;
    data.margin[3] = p_point.y;
    _size_changed();
}

float Control::get_margin(Margin p_margin) const {
    ERR_FAIL_INDEX_V((int)p_margin, 4, 0);

    return data.margin[(int)p_margin];
}

Size2 Control::get_begin() const {

    return Size2(data.margin[0], data.margin[1]);
}
Size2 Control::get_end() const {

    return Size2(data.margin[2], data.margin[3]);
}

Point2 Control::get_global_position() const {

    return get_global_transform().get_origin();
}

void Control::_set_global_position(const Point2 &p_point) {
    set_global_position(p_point);
}

void Control::set_global_position(const Point2 &p_point, bool p_keep_margins) {

    Transform2D inv;

    if (data.parent_canvas_item) {

        inv = data.parent_canvas_item->get_global_transform().affine_inverse();
    }

    set_position(inv.xform(p_point), p_keep_margins);
}

void Control::_compute_anchors(Rect2 p_rect, const float p_margins[4], float (&r_anchors)[4]) {

    Size2 parent_rect_size = get_parent_anchorable_rect().size;
    ERR_FAIL_COND(parent_rect_size.x == 0.0f);
    ERR_FAIL_COND(parent_rect_size.y == 0.0f);

    r_anchors[0] = (p_rect.position.x - p_margins[0]) / parent_rect_size.x;
    r_anchors[1] = (p_rect.position.y - p_margins[1]) / parent_rect_size.y;
    r_anchors[2] = (p_rect.position.x + p_rect.size.x - p_margins[2]) / parent_rect_size.x;
    r_anchors[3] = (p_rect.position.y + p_rect.size.y - p_margins[3]) / parent_rect_size.y;
}

void Control::_compute_margins(Rect2 p_rect, const float p_anchors[4], float (&r_margins)[4]) {

    Size2 parent_rect_size = get_parent_anchorable_rect().size;
    r_margins[0] = p_rect.position.x - (p_anchors[0] * parent_rect_size.x);
    r_margins[1] = p_rect.position.y - (p_anchors[1] * parent_rect_size.y);
    r_margins[2] = p_rect.position.x + p_rect.size.x - (p_anchors[2] * parent_rect_size.x);
    r_margins[3] = p_rect.position.y + p_rect.size.y - (p_anchors[3] * parent_rect_size.y);
}

void Control::_set_position(const Size2 &p_point) {
    set_position(p_point);
}

void Control::set_position(const Size2 &p_point, bool p_keep_margins) {
    if (p_keep_margins) {
        _compute_anchors(Rect2(p_point, data.size_cache), data.margin, data.anchor);
        Object_change_notify(this,"anchor_left");
        Object_change_notify(this,"anchor_right");
        Object_change_notify(this,"anchor_top");
        Object_change_notify(this,"anchor_bottom");
    } else {
        _compute_margins(Rect2(p_point, data.size_cache), data.anchor, data.margin);
    }
    _size_changed();
}

void Control::_set_size(const Size2 &p_size) {
    set_size(p_size);
}

void Control::set_size(const Size2 &p_size, bool p_keep_margins) {

    Size2 new_size = p_size;
    Size2 min = get_combined_minimum_size();
    if (new_size.x < min.x)
        new_size.x = min.x;
    if (new_size.y < min.y)
        new_size.y = min.y;

    if (p_keep_margins) {
        _compute_anchors(Rect2(data.pos_cache, new_size), data.margin, data.anchor);
        Object_change_notify(this,"anchor_left");
        Object_change_notify(this,"anchor_right");
        Object_change_notify(this,"anchor_top");
        Object_change_notify(this,"anchor_bottom");
    } else {
        _compute_margins(Rect2(data.pos_cache, new_size), data.anchor, data.margin);
    }
    _size_changed();
}

Size2 Control::get_position() const {

    return data.pos_cache;
}

Size2 Control::get_size() const {

    return data.size_cache;
}

Rect2 Control::get_global_rect() const {

    return Rect2(get_global_position(), get_size());
}

Rect2 Control::get_window_rect() const {
    ERR_FAIL_COND_V(!is_inside_tree(), Rect2());
    Rect2 gr = get_global_rect();
    gr.position += get_viewport()->get_visible_rect().position;
    return gr;
}

Rect2 Control::get_rect() const {

    return Rect2(get_position(), get_size());
}

Rect2 Control::get_anchorable_rect() const {

    return Rect2(Point2(), get_size());
}

void Control::add_icon_override(const StringName &p_name, const Ref<Texture> &p_icon) {

    if (data.icon_override.contains(p_name)) {
        data.icon_override[p_name]->disconnect("changed",callable_mp(this, &ClassName::_override_changed));
    }

    // clear if "null" is passed instead of a icon
    if (not p_icon) {
        data.icon_override.erase(p_name);
    } else {
        data.icon_override[p_name] = p_icon;
        if (data.icon_override[p_name]) {
            data.icon_override[p_name]->connect("changed",callable_mp(this, &ClassName::_override_changed), null_variant_pvec, ObjectNS::CONNECT_REFERENCE_COUNTED);
        }
    }
    notification(NOTIFICATION_THEME_CHANGED);
}

void Control::add_shader_override(const StringName &p_name, const Ref<Shader> &p_shader) {

    if (data.shader_override.contains(p_name)) {
        data.shader_override[p_name]->disconnect("changed",callable_mp(this, &ClassName::_override_changed));
    }

    // clear if "null" is passed instead of a shader
    if (not p_shader) {
        data.shader_override.erase(p_name);
    } else {
        data.shader_override[p_name] = p_shader;
        if (data.shader_override[p_name]) {
            data.shader_override[p_name]->connect("changed",callable_mp(this, &ClassName::_override_changed), null_variant_pvec, ObjectNS::CONNECT_REFERENCE_COUNTED);
        }
    }
    notification(NOTIFICATION_THEME_CHANGED);
}
void Control::add_style_override(const StringName &p_name, const Ref<StyleBox> &p_style) {

    if (data.style_override.contains(p_name)) {
        data.style_override[p_name]->disconnect("changed",callable_mp(this, &ClassName::_override_changed));
    }

    // clear if "null" is passed instead of a style
    if (not p_style) {
        data.style_override.erase(p_name);
    } else {
        data.style_override[p_name] = p_style;
        if (data.style_override[p_name]) {
            data.style_override[p_name]->connect("changed",callable_mp(this, &ClassName::_override_changed), null_variant_pvec, ObjectNS::CONNECT_REFERENCE_COUNTED);
        }
    }
    notification(NOTIFICATION_THEME_CHANGED);
}

void Control::add_font_override(const StringName &p_name, const Ref<Font> &p_font) {

    if (data.font_override.contains(p_name)) {
        data.font_override[p_name]->disconnect("changed",callable_mp(this, &ClassName::_override_changed));
    }

    // clear if "null" is passed instead of a font
    if (not p_font) {
        data.font_override.erase(p_name);
    } else {
        data.font_override[p_name] = p_font;
        if (data.font_override[p_name]) {
            data.font_override[p_name]->connect("changed",callable_mp(this, &ClassName::_override_changed), null_variant_pvec, ObjectNS::CONNECT_REFERENCE_COUNTED);
        }
    }
    notification(NOTIFICATION_THEME_CHANGED);
}
void Control::add_color_override(const StringName &p_name, const Color &p_color) {

    data.color_override[p_name] = p_color;
    notification(NOTIFICATION_THEME_CHANGED);
}
void Control::add_constant_override(const StringName &p_name, int p_constant) {

    data.constant_override[p_name] = p_constant;
    notification(NOTIFICATION_THEME_CHANGED);
}

void Control::set_focus_mode(FocusMode p_focus_mode) {

    if (is_inside_tree() && p_focus_mode == FOCUS_NONE && data.focus_mode != FOCUS_NONE && has_focus())
        release_focus();

    data.focus_mode = p_focus_mode;
}

static Control *_next_control(Control *p_from) {

    if (p_from->is_set_as_toplevel())
        return nullptr; // can't go above

    Control *parent = object_cast<Control>(p_from->get_parent());

    if (!parent) {

        return nullptr;
    }

    int next = p_from->get_position_in_parent();
    ERR_FAIL_INDEX_V(next, parent->get_child_count(), nullptr);
    for (int i = (next + 1); i < parent->get_child_count(); i++) {

        Control *c = object_cast<Control>(parent->get_child(i));
        if (!c || !c->is_visible_in_tree() || c->is_set_as_toplevel())
            continue;

        return c;
    }

    //no next in parent, try the same in parent
    return _next_control(parent);
}

Control *Control::find_next_valid_focus() const {

    Control *from = const_cast<Control *>(this);

    while (true) {

        // If the focus property is manually overwritten, attempt to use it.

        if (!data.focus_next.is_empty()) {
            Node *n = get_node(data.focus_next);
            Control *c;
            if (n) {
                c = object_cast<Control>(n);
                ERR_FAIL_COND_V_MSG(!c, nullptr, "Next focus node is not a control: " + n->get_name() + ".");
            } else {
                return nullptr;
            }
            if (c->is_visible() && c->get_focus_mode() != FOCUS_NONE)
                return c;
        }

        // find next child

        Control *next_child = nullptr;

        for (int i = 0; i < from->get_child_count(); i++) {

            Control *c = object_cast<Control>(from->get_child(i));
            if (!c || !c->is_visible_in_tree() || c->is_set_as_toplevel()) {
                continue;
            }

            next_child = c;
            break;
        }

        if (!next_child) {

            next_child = _next_control(from);
            if (!next_child) { //nothing else.. go up and find either window or subwindow
                next_child = const_cast<Control *>(this);
                while (next_child && !next_child->is_set_as_toplevel()) {

                    next_child = object_cast<Control>(next_child->get_parent());
                }

                if (!next_child) {

                    next_child = const_cast<Control *>(this);
                    while (next_child) {

                        if (next_child->data.SI || next_child->data.RI)
                            break;
                        next_child = next_child->get_parent_control();
                    }
                }
            }
        }

        if (next_child == this) // no next control->
            return (get_focus_mode() == FOCUS_ALL) ? next_child : nullptr;
        if (next_child) {
            if (next_child->get_focus_mode() == FOCUS_ALL)
                return next_child;
            from = next_child;
        } else
            break;
    }

    return nullptr;
}

static Control *_prev_control(Control *p_from) {

    Control *child = nullptr;
    for (int i = p_from->get_child_count() - 1; i >= 0; i--) {

        Control *c = object_cast<Control>(p_from->get_child(i));
        if (!c || !c->is_visible_in_tree() || c->is_set_as_toplevel())
            continue;

        child = c;
        break;
    }

    if (!child)
        return p_from;

    //no prev in parent, try the same in parent
    return _prev_control(child);
}

Control *Control::find_prev_valid_focus() const {
    Control *from = const_cast<Control *>(this);

    while (true) {

        // If the focus property is manually overwritten, attempt to use it.

        if (!data.focus_prev.is_empty()) {
            Node *n = get_node(data.focus_prev);
            Control *c;
            if (n) {
                c = object_cast<Control>(n);
                ERR_FAIL_COND_V_MSG(!c, nullptr, "Previous focus node is not a control: " + n->get_name() + ".");
            } else {
                return nullptr;
            }
            if (c->is_visible() && c->get_focus_mode() != FOCUS_NONE)
                return c;
        }

        // find prev child

        Control *prev_child = nullptr;

        if (from->is_set_as_toplevel() || !object_cast<Control>(from->get_parent())) {

            //find last of the children

            prev_child = _prev_control(from);

        } else {

            for (int i = (from->get_position_in_parent() - 1); i >= 0; i--) {

                Control *c = object_cast<Control>(from->get_parent()->get_child(i));

                if (!c || !c->is_visible_in_tree() || c->is_set_as_toplevel()) {
                    continue;
                }

                prev_child = c;
                break;
            }

            if (!prev_child) {

                prev_child = object_cast<Control>(from->get_parent());
            } else {

                prev_child = _prev_control(prev_child);
            }
        }

        if (prev_child == this) // no prev control->
            return (get_focus_mode() == FOCUS_ALL) ? prev_child : nullptr;

        if (prev_child->get_focus_mode() == FOCUS_ALL)
            return prev_child;

        from = prev_child;
    }

    return nullptr;
}

Control::FocusMode Control::get_focus_mode() const {

    return data.focus_mode;
}
bool Control::has_focus() const {

    return is_inside_tree() && get_viewport()->_gui_control_has_focus(this);
}

void Control::grab_focus() {

    ERR_FAIL_COND(!is_inside_tree());

    if (data.focus_mode == FOCUS_NONE) {
        WARN_PRINT("This control can't grab focus. Use set_focus_mode() to allow a control to get focus.");
        return;
    }

    get_viewport()->_gui_control_grab_focus(this);
}

void Control::release_focus() {

    ERR_FAIL_COND(!is_inside_tree());

    if (!has_focus())
        return;

    get_viewport()->_gui_remove_focus();
    update();
}

bool Control::is_toplevel_control() const {

    return is_inside_tree() && (!data.parent_canvas_item && !data.RI && is_set_as_toplevel());
}

void Control::show_modal(bool p_exclusive) {

    ERR_FAIL_COND(!is_inside_tree());
    ERR_FAIL_COND(!data.SI);

    if (is_visible_in_tree())
        hide();

    ERR_FAIL_COND(data.MI != nullptr);
    show();
    raise();
    data.modal_exclusive = p_exclusive;
    data.MI = this;
    get_viewport()->_gui_show_modal(this);
    data.modal_frame = Engine::get_singleton()->get_frames_drawn();
}

void Control::_modal_set_prev_focus_owner(ObjectID p_prev) {
    data.modal_prev_focus_owner = p_prev;
}

void Control::_modal_stack_remove() {

    ERR_FAIL_COND(!is_inside_tree());

    if (!data.MI)
        return;

    Control * element = data.MI;
    data.MI = nullptr;

    get_viewport()->_gui_remove_from_modal_stack(element, data.modal_prev_focus_owner);

    data.modal_prev_focus_owner = 0;
}

void Control::_propagate_theme_changed(CanvasItem *p_at, Control *p_owner, bool p_assign) {

    Control *c = object_cast<Control>(p_at);

    if (c && c != p_owner && c->data.theme) // has a theme, this can't be propagated
        return;

    for (int i = 0; i < p_at->get_child_count(); i++) {

        CanvasItem *child = object_cast<CanvasItem>(p_at->get_child(i));
        if (child) {
            _propagate_theme_changed(child, p_owner, p_assign);
        }
    }

    if (c) {

        if (p_assign) {
            c->data.theme_owner = p_owner;
        }
        c->notification(NOTIFICATION_THEME_CHANGED);
    }
}

void Control::_theme_changed() {

    _propagate_theme_changed(this, this, false);
}

void Control::set_theme(const Ref<Theme> &p_theme) {

    if (data.theme == p_theme)
        return;

    if (data.theme) {
        data.theme->disconnect("changed",callable_mp(this, &ClassName::_theme_changed));
    }

    data.theme = p_theme;
    if (p_theme) {

        data.theme_owner = this;
        _propagate_theme_changed(this, this);
    } else {

        Control *parent = object_cast<Control>(get_parent());
        if (parent && parent->data.theme_owner) {
            _propagate_theme_changed(this, parent->data.theme_owner);
        } else {

            _propagate_theme_changed(this, nullptr);
        }
    }

    if (data.theme) {
        data.theme->connect("changed",callable_mp(this, &ClassName::_theme_changed), varray(), ObjectNS::CONNECT_QUEUED);
    }
}

void Control::accept_event() {

    if (is_inside_tree())
        get_viewport()->_gui_accept_event();
}

Ref<Theme> Control::get_theme() const {

    return data.theme;
}

void Control::set_tooltip(const StringName &p_tooltip) {

    data.tooltip = p_tooltip;
    update_configuration_warning();
}

StringName Control::get_tooltip(const Point2 &p_pos) const {

    return data.tooltip;
}
Control *Control::make_custom_tooltip(StringView p_text) const {
    if (get_script_instance()) {
        return const_cast<Control *>(this)->call_va("_make_custom_tooltip", p_text).as<Control *>();
    }
    return nullptr;
}

void Control::set_default_cursor_shape(CursorShape p_shape) {

    ERR_FAIL_INDEX(int(p_shape), CURSOR_MAX);

    data.default_cursor = p_shape;
}

Control::CursorShape Control::get_default_cursor_shape() const {

    return data.default_cursor;
}
Control::CursorShape Control::get_cursor_shape(const Point2 &p_pos) const {

    return data.default_cursor;
}

Transform2D Control::get_transform() const {

    Transform2D xform = _get_internal_transform();
    xform[2] += get_position();
    return xform;
}

StringName Control::_get_tooltip() const {

    return data.tooltip;
}

void Control::set_focus_neighbour(Margin p_margin, const NodePath &p_neighbour) {

    ERR_FAIL_INDEX((int)p_margin, 4);
    data.focus_neighbour[(int)p_margin] = p_neighbour;
}

NodePath Control::get_focus_neighbour(Margin p_margin) const {

    ERR_FAIL_INDEX_V((int)p_margin, 4, NodePath());
    return data.focus_neighbour[(int)p_margin];
}

void Control::set_focus_next(const NodePath &p_next) {

    data.focus_next = p_next;
}

NodePath Control::get_focus_next() const {

    return data.focus_next;
}

void Control::set_focus_previous(const NodePath &p_prev) {

    data.focus_prev = p_prev;
}

NodePath Control::get_focus_previous() const {

    return data.focus_prev;
}

#define MAX_NEIGHBOUR_SEARCH_COUNT 512

Control *Control::_get_focus_neighbour(Margin p_margin, int p_count) {

    ERR_FAIL_INDEX_V((int)p_margin, 4, NULL);

    if (p_count >= MAX_NEIGHBOUR_SEARCH_COUNT)
        return nullptr;
    if (!data.focus_neighbour[(int)p_margin].is_empty()) {

        Control *c = nullptr;
        Node *n = get_node(data.focus_neighbour[(int)p_margin]);
        if (n) {
            c = object_cast<Control>(n);
            ERR_FAIL_COND_V_MSG(!c, nullptr, String("Neighbor focus node is not a control: ") + n->get_name() + ".");
        } else {
            return nullptr;
        }
        bool valid = true;
        if (!c->is_visible())
            valid = false;
        if (c->get_focus_mode() == FOCUS_NONE)
            valid = false;
        if (valid)
            return c;

        c = c->_get_focus_neighbour(p_margin, p_count + 1);
        return c;
    }

    float dist = 1e7;
    Control *result = nullptr;

    Point2 points[4];

    Transform2D xform = get_global_transform();

    points[0] = xform.xform(Point2());
    points[1] = xform.xform(Point2(get_size().x, 0));
    points[2] = xform.xform(get_size());
    points[3] = xform.xform(Point2(0, get_size().y));

    const Vector2 dir[4] = {
        Vector2(-1, 0),
        Vector2(0, -1),
        Vector2(1, 0),
        Vector2(0, 1)
    };

    Vector2 vdir = dir[(int)p_margin];

    float maxd = -1e7;

    for (int i = 0; i < 4; i++) {

        float d = vdir.dot(points[i]);
        if (d > maxd)
            maxd = d;
    }

    Node *base = this;

    while (base) {

        Control *c = object_cast<Control>(base);
        if (c) {
            if (c->data.SI)
                break;
            if (c->data.RI)
                break;
        }
        base = base->get_parent();
    }

    if (!base)
        return nullptr;

    _window_find_focus_neighbour(vdir, base, points, maxd, dist, &result);

    return result;
}

void Control::_window_find_focus_neighbour(const Vector2 &p_dir, Node *p_at, const Point2 *p_points, float p_min, float &r_closest_dist, Control **r_closest) {

    if (object_cast<Viewport>(p_at))
        return; //bye

    Control *c = object_cast<Control>(p_at);

    if (c && c != this && c->get_focus_mode() == FOCUS_ALL && c->is_visible_in_tree()) {

        Point2 points[4];

        Transform2D xform = c->get_global_transform();

        points[0] = xform.xform(Point2());
        points[1] = xform.xform(Point2(c->get_size().x, 0));
        points[2] = xform.xform(c->get_size());
        points[3] = xform.xform(Point2(0, c->get_size().y));

        float min = 1e7;

        for (int i = 0; i < 4; i++) {

            float d = p_dir.dot(points[i]);
            if (d < min)
                min = d;
        }

        if (min > (p_min - CMP_EPSILON)) {

            for (int i = 0; i < 4; i++) {

                Vector2 la = p_points[i];
                Vector2 lb = p_points[(i + 1) % 4];

                for (int j = 0; j < 4; j++) {

                    Vector2 fa = points[j];
                    Vector2 fb = points[(j + 1) % 4];

                    Vector2 pa, pb;
                    float d = Geometry::get_closest_points_between_segments(la, lb, fa, fb, pa, pb);
                    //float d = Geometry::get_closest_distance_between_segments(Vector3(la.x,la.y,0),Vector3(lb.x,lb.y,0),Vector3(fa.x,fa.y,0),Vector3(fb.x,fb.y,0));
                    if (d < r_closest_dist) {
                        r_closest_dist = d;
                        *r_closest = c;
                    }
                }
            }
        }
    }

    for (int i = 0; i < p_at->get_child_count(); i++) {

        Node *child = p_at->get_child(i);
        Control *childc = object_cast<Control>(child);
        if (childc && childc->data.SI)
            continue; //subwindow, ignore
        _window_find_focus_neighbour(p_dir, p_at->get_child(i), p_points, p_min, r_closest_dist, r_closest);
    }
}

void Control::set_h_size_flags(int p_flags) {

    if (data.h_size_flags == p_flags)
        return;
    data.h_size_flags = p_flags;
    emit_signal(SceneStringNames::get_singleton()->size_flags_changed);
}

int Control::get_h_size_flags() const {
    return data.h_size_flags;
}
void Control::set_v_size_flags(int p_flags) {

    if (data.v_size_flags == p_flags)
        return;
    data.v_size_flags = p_flags;
    emit_signal(SceneStringNames::get_singleton()->size_flags_changed);
}

void Control::set_stretch_ratio(float p_ratio) {

    if (data.expand == p_ratio)
        return;

    data.expand = p_ratio;
    emit_signal(SceneStringNames::get_singleton()->size_flags_changed);
}

float Control::get_stretch_ratio() const {

    return data.expand;
}

void Control::grab_click_focus() {

    ERR_FAIL_COND(!is_inside_tree());

    get_viewport()->_gui_grab_click_focus(this);
}

void Control::minimum_size_changed() {

    if (!is_inside_tree() || data.block_minimum_size_adjust)
        return;

    Control *invalidate = this;

    //invalidate cache upwards
    while (invalidate && invalidate->data.minimum_size_valid) {
        invalidate->data.minimum_size_valid = false;
        if (invalidate->is_set_as_toplevel())
            break; // do not go further up
        invalidate = invalidate->data.parent;
    }

    if (!is_visible_in_tree())
        return;

    if (data.updating_last_minimum_size)
        return;

    data.updating_last_minimum_size = true;

    MessageQueue::get_singleton()->push_call(this, "_update_minimum_size");
}

int Control::get_v_size_flags() const {
    return data.v_size_flags;
}

void Control::set_mouse_filter(MouseFilter p_filter) {

    ERR_FAIL_INDEX(p_filter, 3);
    data.mouse_filter = p_filter;
    update_configuration_warning();
}

Control::MouseFilter Control::get_mouse_filter() const {

    return data.mouse_filter;
}

void Control::set_pass_on_modal_close_click(bool p_pass_on) {

    data.pass_on_modal_close_click = p_pass_on;
}

bool Control::pass_on_modal_close_click() const {

    return data.pass_on_modal_close_click;
}

Control *Control::get_focus_owner() const {

    ERR_FAIL_COND_V(!is_inside_tree(), nullptr);
    return get_viewport()->_gui_get_focus_owner();
}

void Control::warp_mouse(const Point2 &p_to_pos) {
    ERR_FAIL_COND(!is_inside_tree());
    get_viewport()->warp_mouse(get_global_transform().xform(p_to_pos));
}

bool Control::is_text_field() const {
    /*
    if (get_script_instance()) {
        Variant v=p_point;
        const Variant *p[2]={&v,&p_data};
        Callable::CallError ce;
        Variant ret = get_script_instance()->call("is_text_field",p,2,ce);
        if (ce.error==Callable::CallError::CALL_OK)
            return ret;
    }
  */
    return false;
}

void Control::set_rotation(float p_radians) {

    data.rotation = p_radians;
    update();
    _notify_transform();
    Object_change_notify(this,"rect_rotation");
}

float Control::get_rotation() const {

    return data.rotation;
}

void Control::set_rotation_degrees(float p_degrees) {
    set_rotation(Math::deg2rad(p_degrees));
}

float Control::get_rotation_degrees() const {
    return Math::rad2deg(get_rotation());
}

void Control::_override_changed() {

    notification(NOTIFICATION_THEME_CHANGED);
    minimum_size_changed(); // overrides are likely to affect minimum size
}

void Control::set_pivot_offset(const Vector2 &p_pivot) {

    data.pivot_offset = p_pivot;
    update();
    _notify_transform();
    Object_change_notify(this,"rect_pivot_offset");
}

Vector2 Control::get_pivot_offset() const {

    return data.pivot_offset;
}

void Control::set_scale(const Vector2 &p_scale) {

    data.scale = p_scale;
    // Avoid having 0 scale values, can lead to errors in physics and rendering.
    if (data.scale.x == 0)
        data.scale.x = CMP_EPSILON;
    if (data.scale.y == 0)
        data.scale.y = CMP_EPSILON;

    update();
    _notify_transform();
}
Vector2 Control::get_scale() const {

    return data.scale;
}

Control *Control::get_root_parent_control() const {

    const CanvasItem *ci = this;
    const Control *root = this;

    while (ci) {

        const Control *c = object_cast<Control>(ci);
        if (c) {
            root = c;

            if (c->data.RI || c->data.MI || c->is_toplevel_control())
                break;
        }

        ci = ci->get_parent_item();
    }

    return const_cast<Control *>(root);
}

void Control::set_block_minimum_size_adjust(bool p_block) {
    data.block_minimum_size_adjust = p_block;
}

bool Control::is_minimum_size_adjust_blocked() const {

    return data.block_minimum_size_adjust;
}

void Control::set_disable_visibility_clip(bool p_ignore) {

    data.disable_visibility_clip = p_ignore;
    update();
}

bool Control::is_visibility_clip_disabled() const {

    return data.disable_visibility_clip;
}


void Control::get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const {
    using namespace eastl;
#ifdef TOOLS_ENABLED
    const char * quote_style(EDITOR_DEF_T<bool>("text_editor/completion/use_single_quotes", false) ? "'" : "\"");
#else
    const char * quote_style = "\"";
#endif

    Node::get_argument_options(p_function, p_idx, r_options);

    if (p_idx == 0) {
        Vector<StringName> sn;
        StringView pf = p_function;
        if (pf == "add_color_override"_sv || pf == "has_color"_sv || pf == "has_color_override"_sv || pf == "get_color"_sv) {
            Theme::get_default()->get_color_list(get_class_name(), &sn);
        } else if (pf == "add_style_override"_sv || pf == "has_style"_sv || pf == "has_style_override"_sv || pf == "get_style"_sv) {
            sn = Theme::get_default()->get_stylebox_list(get_class_name());
        } else if (pf == "add_font_override"_sv || pf == "has_font"_sv || pf == "has_font_override"_sv || pf == "get_font"_sv) {
            Theme::get_default()->get_font_list(get_class_name(), &sn);
        } else if (pf == "add_constant_override"_sv || pf == "has_constant"_sv || pf == "has_constant_override"_sv || pf == "get_constant"_sv) {
            Theme::get_default()->get_constant_list(get_class_name(), &sn);
        }
        eastl::sort(sn.begin(),sn.end(),WrapAlphaCompare());
        for (const StringName &E : sn) {
            r_options->push_back(quote_style + String(E.asCString()) + quote_style);
        }
    }
}

StringName Control::get_configuration_warning() const {
    String warning(CanvasItem::get_configuration_warning());

    if (data.mouse_filter == MOUSE_FILTER_IGNORE && !data.tooltip.empty()) {
        if (!warning.empty()) {
            warning += ("\n\n");
        }
        warning += TTR(R"(The Hint Tooltip won't be displayed as the control's Mouse Filter is set to "Ignore". To solve this, set the Mouse Filter to "Stop" or "Pass".)");
    }

    return StringName(warning);
}

void Control::set_clip_contents(bool p_clip) {

    data.clip_contents = p_clip;
    update();
}

bool Control::is_clipping_contents() {

    return data.clip_contents;
}

void Control::set_h_grow_direction(GrowDirection p_direction) {

    ERR_FAIL_INDEX((int)p_direction, 3);

    data.h_grow = p_direction;
    _size_changed();
}

Control::GrowDirection Control::get_h_grow_direction() const {

    return data.h_grow;
}

void Control::set_v_grow_direction(GrowDirection p_direction) {
    ERR_FAIL_INDEX((int)p_direction, 3);

    data.v_grow = p_direction;
    _size_changed();
}
Control::GrowDirection Control::get_v_grow_direction() const {

    return data.v_grow;
}

void Control::_bind_methods() {

    //MethodBinder::bind_method(D_METHOD("_window_resize_event"),&Control::_window_resize_event);
    MethodBinder::bind_method(D_METHOD("_size_changed"), &Control::_size_changed);
    MethodBinder::bind_method(D_METHOD("_update_minimum_size"), &Control::_update_minimum_size);

    MethodBinder::bind_method(D_METHOD("accept_event"), &Control::accept_event);
    MethodBinder::bind_method(D_METHOD("get_minimum_size"), &Control::get_minimum_size);
    MethodBinder::bind_method(D_METHOD("get_combined_minimum_size"), &Control::get_combined_minimum_size);
    MethodBinder::bind_method(D_METHOD("set_anchors_preset", {"preset", "keep_margins"}), &Control::set_anchors_preset, {DEFVAL(false)});
    MethodBinder::bind_method(D_METHOD("set_margins_preset", {"preset", "resize_mode", "margin"}), &Control::set_margins_preset, {DEFVAL(PRESET_MODE_MINSIZE), DEFVAL(0)});
    MethodBinder::bind_method(D_METHOD("set_anchors_and_margins_preset", {"preset", "resize_mode", "margin"}), &Control::set_anchors_and_margins_preset, {DEFVAL(PRESET_MODE_MINSIZE), DEFVAL(0)});
    MethodBinder::bind_method(D_METHOD("_set_anchor", {"margin", "anchor"}), &Control::_set_anchor);
    MethodBinder::bind_method(D_METHOD("set_anchor", {"margin", "anchor", "keep_margin", "push_opposite_anchor"}), &Control::set_anchor, {DEFVAL(false), DEFVAL(true)});
    MethodBinder::bind_method(D_METHOD("get_anchor", {"margin"}), &Control::get_anchor);
    MethodBinder::bind_method(D_METHOD("set_margin", {"margin", "offset"}), &Control::set_margin);
    MethodBinder::bind_method(D_METHOD("set_anchor_and_margin", {"margin", "anchor", "offset", "push_opposite_anchor"}), &Control::set_anchor_and_margin, {DEFVAL(false)});
    MethodBinder::bind_method(D_METHOD("set_begin", {"position"}), &Control::set_begin);
    MethodBinder::bind_method(D_METHOD("set_end", {"position"}), &Control::set_end);
    MethodBinder::bind_method(D_METHOD("set_position", {"position", "keep_margins"}), &Control::set_position, {DEFVAL(false)});
    MethodBinder::bind_method(D_METHOD("_set_position", {"margin"}), &Control::_set_position);
    MethodBinder::bind_method(D_METHOD("set_size", {"size", "keep_margins"}), &Control::set_size, {DEFVAL(false)});
    MethodBinder::bind_method(D_METHOD("_set_size", {"size"}), &Control::_set_size);
    MethodBinder::bind_method(D_METHOD("set_custom_minimum_size", {"size"}), &Control::set_custom_minimum_size);
    MethodBinder::bind_method(D_METHOD("set_global_position", {"position", "keep_margins"}), &Control::set_global_position, {DEFVAL(false)});
    MethodBinder::bind_method(D_METHOD("_set_global_position", {"position"}), &Control::_set_global_position);
    MethodBinder::bind_method(D_METHOD("set_rotation", {"radians"}), &Control::set_rotation);
    MethodBinder::bind_method(D_METHOD("set_rotation_degrees", {"degrees"}), &Control::set_rotation_degrees);
    MethodBinder::bind_method(D_METHOD("set_scale", {"scale"}), &Control::set_scale);
    MethodBinder::bind_method(D_METHOD("set_pivot_offset", {"pivot_offset"}), &Control::set_pivot_offset);
    MethodBinder::bind_method(D_METHOD("get_margin", {"margin"}), &Control::get_margin);
    MethodBinder::bind_method(D_METHOD("get_begin"), &Control::get_begin);
    MethodBinder::bind_method(D_METHOD("get_end"), &Control::get_end);
    MethodBinder::bind_method(D_METHOD("get_position"), &Control::get_position);
    MethodBinder::bind_method(D_METHOD("get_size"), &Control::get_size);
    MethodBinder::bind_method(D_METHOD("get_rotation"), &Control::get_rotation);
    MethodBinder::bind_method(D_METHOD("get_rotation_degrees"), &Control::get_rotation_degrees);
    MethodBinder::bind_method(D_METHOD("get_scale"), &Control::get_scale);
    MethodBinder::bind_method(D_METHOD("get_pivot_offset"), &Control::get_pivot_offset);
    MethodBinder::bind_method(D_METHOD("get_custom_minimum_size"), &Control::get_custom_minimum_size);
    MethodBinder::bind_method(D_METHOD("get_parent_area_size"), &Control::get_parent_area_size);
    MethodBinder::bind_method(D_METHOD("get_global_position"), &Control::get_global_position);
    MethodBinder::bind_method(D_METHOD("get_rect"), &Control::get_rect);
    MethodBinder::bind_method(D_METHOD("get_global_rect"), &Control::get_global_rect);
    MethodBinder::bind_method(D_METHOD("show_modal", {"exclusive"}), &Control::show_modal, {DEFVAL(false)});
    MethodBinder::bind_method(D_METHOD("set_focus_mode", {"mode"}), &Control::set_focus_mode);
    MethodBinder::bind_method(D_METHOD("get_focus_mode"), &Control::get_focus_mode);
    MethodBinder::bind_method(D_METHOD("has_focus"), &Control::has_focus);
    MethodBinder::bind_method(D_METHOD("grab_focus"), &Control::grab_focus);
    MethodBinder::bind_method(D_METHOD("release_focus"), &Control::release_focus);
    MethodBinder::bind_method(D_METHOD("get_focus_owner"), &Control::get_focus_owner);

    MethodBinder::bind_method(D_METHOD("set_h_size_flags", {"flags"}), &Control::set_h_size_flags);
    MethodBinder::bind_method(D_METHOD("get_h_size_flags"), &Control::get_h_size_flags);

    MethodBinder::bind_method(D_METHOD("set_stretch_ratio", {"ratio"}), &Control::set_stretch_ratio);
    MethodBinder::bind_method(D_METHOD("get_stretch_ratio"), &Control::get_stretch_ratio);

    MethodBinder::bind_method(D_METHOD("set_v_size_flags", {"flags"}), &Control::set_v_size_flags);
    MethodBinder::bind_method(D_METHOD("get_v_size_flags"), &Control::get_v_size_flags);

    MethodBinder::bind_method(D_METHOD("set_theme", {"theme"}), &Control::set_theme);
    MethodBinder::bind_method(D_METHOD("get_theme"), &Control::get_theme);

    MethodBinder::bind_method(D_METHOD("add_icon_override", {"name", "texture"}), &Control::add_icon_override);
    MethodBinder::bind_method(D_METHOD("add_shader_override", {"name", "shader"}), &Control::add_shader_override);
    MethodBinder::bind_method(D_METHOD("add_style_override", {"name", "stylebox"}), &Control::add_style_override);
    MethodBinder::bind_method(D_METHOD("add_font_override", {"name", "font"}), &Control::add_font_override);
    MethodBinder::bind_method(D_METHOD("add_color_override", {"name", "color"}), &Control::add_color_override);
    MethodBinder::bind_method(D_METHOD("add_constant_override", {"name", "constant"}), &Control::add_constant_override);

    MethodBinder::bind_method(D_METHOD("get_icon", {"name", "type"}), &Control::get_icon, {DEFVAL("")});
    MethodBinder::bind_method(D_METHOD("get_stylebox", {"name", "type"}), &Control::get_stylebox, {DEFVAL(StringName())});
    MethodBinder::bind_method(D_METHOD("get_font", {"name", "type"}), &Control::get_font, {DEFVAL("")});
    MethodBinder::bind_method(D_METHOD("get_color", {"name", "type"}), &Control::get_color, {DEFVAL("")});
    MethodBinder::bind_method(D_METHOD("get_constant", {"name", "type"}), &Control::get_constant, {DEFVAL("")});

    MethodBinder::bind_method(D_METHOD("has_icon_override", {"name"}), &Control::has_icon_override);
    MethodBinder::bind_method(D_METHOD("has_shader_override", {"name"}), &Control::has_shader_override);
    MethodBinder::bind_method(D_METHOD("has_stylebox_override", {"name"}), &Control::has_stylebox_override);
    MethodBinder::bind_method(D_METHOD("has_font_override", {"name"}), &Control::has_font_override);
    MethodBinder::bind_method(D_METHOD("has_color_override", {"name"}), &Control::has_color_override);
    MethodBinder::bind_method(D_METHOD("has_constant_override", {"name"}), &Control::has_constant_override);

    MethodBinder::bind_method(D_METHOD("has_icon", {"name", "type"}), &Control::has_icon, {DEFVAL("")});
    MethodBinder::bind_method(D_METHOD("has_stylebox", {"name", "type"}), &Control::has_stylebox, {DEFVAL("")});
    MethodBinder::bind_method(D_METHOD("has_font", {"name", "type"}), &Control::has_font, {DEFVAL("")});
    MethodBinder::bind_method(D_METHOD("has_color", {"name", "type"}), &Control::has_color, {DEFVAL("")});
    MethodBinder::bind_method(D_METHOD("has_constant", {"name", "type"}), &Control::has_constant, {DEFVAL("")});

    MethodBinder::bind_method(D_METHOD("get_parent_control"), &Control::get_parent_control);

    MethodBinder::bind_method(D_METHOD("set_h_grow_direction", {"direction"}), &Control::set_h_grow_direction);
    MethodBinder::bind_method(D_METHOD("get_h_grow_direction"), &Control::get_h_grow_direction);

    MethodBinder::bind_method(D_METHOD("set_v_grow_direction", {"direction"}), &Control::set_v_grow_direction);
    MethodBinder::bind_method(D_METHOD("get_v_grow_direction"), &Control::get_v_grow_direction);

    MethodBinder::bind_method(D_METHOD("set_tooltip", {"tooltip"}), &Control::set_tooltip);
    MethodBinder::bind_method(D_METHOD("get_tooltip", {"at_position"}), &Control::get_tooltip, {DEFVAL(Point2())});
    MethodBinder::bind_method(D_METHOD("_get_tooltip"), &Control::_get_tooltip);

    MethodBinder::bind_method(D_METHOD("set_default_cursor_shape", {"shape"}), &Control::set_default_cursor_shape);
    MethodBinder::bind_method(D_METHOD("get_default_cursor_shape"), &Control::get_default_cursor_shape);
    MethodBinder::bind_method(D_METHOD("get_cursor_shape", {"position"}), &Control::get_cursor_shape, {DEFVAL(Point2())});

    MethodBinder::bind_method(D_METHOD("set_focus_neighbour", {"margin", "neighbour"}), &Control::set_focus_neighbour);
    MethodBinder::bind_method(D_METHOD("get_focus_neighbour", {"margin"}), &Control::get_focus_neighbour);

    MethodBinder::bind_method(D_METHOD("set_focus_next", {"next"}), &Control::set_focus_next);
    MethodBinder::bind_method(D_METHOD("get_focus_next"), &Control::get_focus_next);

    MethodBinder::bind_method(D_METHOD("set_focus_previous", {"previous"}), &Control::set_focus_previous);
    MethodBinder::bind_method(D_METHOD("get_focus_previous"), &Control::get_focus_previous);

    MethodBinder::bind_method(D_METHOD("force_drag", {"data", "preview"}), &Control::force_drag);

    MethodBinder::bind_method(D_METHOD("set_mouse_filter", {"filter"}), &Control::set_mouse_filter);
    MethodBinder::bind_method(D_METHOD("get_mouse_filter"), &Control::get_mouse_filter);

    MethodBinder::bind_method(D_METHOD("set_clip_contents", {"enable"}), &Control::set_clip_contents);
    MethodBinder::bind_method(D_METHOD("is_clipping_contents"), &Control::is_clipping_contents);

    MethodBinder::bind_method(D_METHOD("grab_click_focus"), &Control::grab_click_focus);

    MethodBinder::bind_method(D_METHOD("set_drag_forwarding", {"target"}), &Control::set_drag_forwarding);
    MethodBinder::bind_method(D_METHOD("set_drag_preview", {"control"}), &Control::set_drag_preview);

    MethodBinder::bind_method(D_METHOD("warp_mouse", {"to_position"}), &Control::warp_mouse);

    MethodBinder::bind_method(D_METHOD("minimum_size_changed"), &Control::minimum_size_changed);

    MethodBinder::bind_method(D_METHOD("_theme_changed"), &Control::_theme_changed);

    MethodBinder::bind_method(D_METHOD("_override_changed"), &Control::_override_changed);

    BIND_VMETHOD(MethodInfo("_gui_input", PropertyInfo(VariantType::OBJECT, "event", PropertyHint::ResourceType, "InputEvent")));
    BIND_VMETHOD(MethodInfo(VariantType::VECTOR2, "_get_minimum_size"));

    MethodInfo get_drag_data = MethodInfo("get_drag_data", PropertyInfo(VariantType::VECTOR2, "position"));
    get_drag_data.return_val.usage |= PROPERTY_USAGE_NIL_IS_VARIANT;
    BIND_VMETHOD(get_drag_data);
    BIND_VMETHOD(MethodInfo(VariantType::BOOL, "can_drop_data", PropertyInfo(VariantType::VECTOR2, "position"), PropertyInfo(VariantType::NIL, "data")));
    BIND_VMETHOD(MethodInfo("drop_data", PropertyInfo(VariantType::VECTOR2, "position"), PropertyInfo(VariantType::NIL, "data")));
    BIND_VMETHOD(MethodInfo(VariantType::OBJECT, "_make_custom_tooltip", PropertyInfo(VariantType::STRING, "for_text")));
    BIND_VMETHOD(MethodInfo(VariantType::BOOL, "_clips_input"));

    ADD_GROUP("Anchor", "anchor_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "anchor_left", PropertyHint::Range, "0,1,0.001,or_lesser,or_greater"), "_set_anchor", "get_anchor", (int)Margin::Left);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "anchor_top", PropertyHint::Range, "0,1,0.001,or_lesser,or_greater"), "_set_anchor", "get_anchor", (int)Margin::Top);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "anchor_right", PropertyHint::Range, "0,1,0.001,or_lesser,or_greater"), "_set_anchor", "get_anchor", (int)Margin::Right);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "anchor_bottom", PropertyHint::Range, "0,1,0.001,or_lesser,or_greater"), "_set_anchor", "get_anchor", (int)Margin::Bottom);

    ADD_GROUP("Margin", "margin_");
    ADD_PROPERTYI(PropertyInfo(VariantType::INT, "margin_left", PropertyHint::Range, "-4096,4096"), "set_margin", "get_margin", (int)Margin::Left);
    ADD_PROPERTYI(PropertyInfo(VariantType::INT, "margin_top", PropertyHint::Range, "-4096,4096"), "set_margin", "get_margin", (int)Margin::Top);
    ADD_PROPERTYI(PropertyInfo(VariantType::INT, "margin_right", PropertyHint::Range, "-4096,4096"), "set_margin", "get_margin", (int)Margin::Right);
    ADD_PROPERTYI(PropertyInfo(VariantType::INT, "margin_bottom", PropertyHint::Range, "-4096,4096"), "set_margin", "get_margin", (int)Margin::Bottom);

    ADD_GROUP("Grow Direction", "grow_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "grow_horizontal", PropertyHint::Enum, "Begin,End,Both"), "set_h_grow_direction", "get_h_grow_direction");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "grow_vertical", PropertyHint::Enum, "Begin,End,Both"), "set_v_grow_direction", "get_v_grow_direction");

    ADD_GROUP("Rect", "rect_");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "rect_position", PropertyHint::None, "", PROPERTY_USAGE_EDITOR), "_set_position", "get_position");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "rect_global_position", PropertyHint::None, "", 0), "_set_global_position", "get_global_position");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "rect_size", PropertyHint::None, "", PROPERTY_USAGE_EDITOR), "_set_size", "get_size");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "rect_min_size"), "set_custom_minimum_size", "get_custom_minimum_size");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "rect_rotation", PropertyHint::Range, "-360,360,0.1,or_lesser,or_greater"), "set_rotation_degrees", "get_rotation_degrees");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "rect_scale"), "set_scale", "get_scale");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "rect_pivot_offset"), "set_pivot_offset", "get_pivot_offset");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "rect_clip_content"), "set_clip_contents", "is_clipping_contents");

    ADD_GROUP("Hint", "hint_");
    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "hint_tooltip", PropertyHint::MultilineText), "set_tooltip", "_get_tooltip");

    ADD_GROUP("Focus", "focus_");
    ADD_PROPERTYI(PropertyInfo(VariantType::NODE_PATH, "focus_neighbour_left", PropertyHint::NodePathValidTypes, "Control"), "set_focus_neighbour", "get_focus_neighbour", (int)Margin::Left);
    ADD_PROPERTYI(PropertyInfo(VariantType::NODE_PATH, "focus_neighbour_top", PropertyHint::NodePathValidTypes, "Control"), "set_focus_neighbour", "get_focus_neighbour", (int)Margin::Top);
    ADD_PROPERTYI(PropertyInfo(VariantType::NODE_PATH, "focus_neighbour_right", PropertyHint::NodePathValidTypes, "Control"), "set_focus_neighbour", "get_focus_neighbour", (int)Margin::Right);
    ADD_PROPERTYI(PropertyInfo(VariantType::NODE_PATH, "focus_neighbour_bottom", PropertyHint::NodePathValidTypes, "Control"), "set_focus_neighbour", "get_focus_neighbour", (int)Margin::Bottom);
    ADD_PROPERTY(PropertyInfo(VariantType::NODE_PATH, "focus_next", PropertyHint::NodePathValidTypes, "Control"), "set_focus_next", "get_focus_next");
    ADD_PROPERTY(PropertyInfo(VariantType::NODE_PATH, "focus_previous", PropertyHint::NodePathValidTypes, "Control"), "set_focus_previous", "get_focus_previous");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "focus_mode", PropertyHint::Enum, "None,Click,All"), "set_focus_mode", "get_focus_mode");

    ADD_GROUP("Mouse", "mouse_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "mouse_filter", PropertyHint::Enum, "Stop,Pass,Ignore"), "set_mouse_filter", "get_mouse_filter");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "mouse_default_cursor_shape", PropertyHint::Enum, "Arrow,Ibeam,Pointing hand,Cross,Wait,Busy,Drag,Can drop,Forbidden,Vertical resize,Horizontal resize,Secondary diagonal resize,Main diagonal resize,Move,Vertical split,Horizontal split,Help"), "set_default_cursor_shape", "get_default_cursor_shape");

    ADD_GROUP("Size Flags", "size_flags_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "size_flags_horizontal", PropertyHint::Flags, "Fill,Expand,Shrink Center,Shrink End"), "set_h_size_flags", "get_h_size_flags");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "size_flags_vertical", PropertyHint::Flags, "Fill,Expand,Shrink Center,Shrink End"), "set_v_size_flags", "get_v_size_flags");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "size_flags_stretch_ratio", PropertyHint::Range, "0,20,0.01,or_greater"), "set_stretch_ratio", "get_stretch_ratio");
    ADD_GROUP("Theme", "");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "theme", PropertyHint::ResourceType, "Theme"), "set_theme", "get_theme");
    ADD_GROUP("", "");

    BIND_ENUM_CONSTANT(FOCUS_NONE);
    BIND_ENUM_CONSTANT(FOCUS_CLICK);
    BIND_ENUM_CONSTANT(FOCUS_ALL);

    BIND_CONSTANT(NOTIFICATION_RESIZED);
    BIND_CONSTANT(NOTIFICATION_MOUSE_ENTER);
    BIND_CONSTANT(NOTIFICATION_MOUSE_EXIT);
    BIND_CONSTANT(NOTIFICATION_FOCUS_ENTER);
    BIND_CONSTANT(NOTIFICATION_FOCUS_EXIT);
    BIND_CONSTANT(NOTIFICATION_THEME_CHANGED);
    BIND_CONSTANT(NOTIFICATION_MODAL_CLOSE);
    BIND_CONSTANT(NOTIFICATION_SCROLL_BEGIN);
    BIND_CONSTANT(NOTIFICATION_SCROLL_END);

    BIND_ENUM_CONSTANT(CURSOR_ARROW);
    BIND_ENUM_CONSTANT(CURSOR_IBEAM);
    BIND_ENUM_CONSTANT(CURSOR_POINTING_HAND);
    BIND_ENUM_CONSTANT(CURSOR_CROSS);
    BIND_ENUM_CONSTANT(CURSOR_WAIT);
    BIND_ENUM_CONSTANT(CURSOR_BUSY);
    BIND_ENUM_CONSTANT(CURSOR_DRAG);
    BIND_ENUM_CONSTANT(CURSOR_CAN_DROP);
    BIND_ENUM_CONSTANT(CURSOR_FORBIDDEN);
    BIND_ENUM_CONSTANT(CURSOR_VSIZE);
    BIND_ENUM_CONSTANT(CURSOR_HSIZE);
    BIND_ENUM_CONSTANT(CURSOR_BDIAGSIZE);
    BIND_ENUM_CONSTANT(CURSOR_FDIAGSIZE);
    BIND_ENUM_CONSTANT(CURSOR_MOVE);
    BIND_ENUM_CONSTANT(CURSOR_VSPLIT);
    BIND_ENUM_CONSTANT(CURSOR_HSPLIT);
    BIND_ENUM_CONSTANT(CURSOR_HELP);

    BIND_ENUM_CONSTANT(PRESET_TOP_LEFT);
    BIND_ENUM_CONSTANT(PRESET_TOP_RIGHT);
    BIND_ENUM_CONSTANT(PRESET_BOTTOM_LEFT);
    BIND_ENUM_CONSTANT(PRESET_BOTTOM_RIGHT);
    BIND_ENUM_CONSTANT(PRESET_CENTER_LEFT);
    BIND_ENUM_CONSTANT(PRESET_CENTER_TOP);
    BIND_ENUM_CONSTANT(PRESET_CENTER_RIGHT);
    BIND_ENUM_CONSTANT(PRESET_CENTER_BOTTOM);
    BIND_ENUM_CONSTANT(PRESET_CENTER);
    BIND_ENUM_CONSTANT(PRESET_LEFT_WIDE);
    BIND_ENUM_CONSTANT(PRESET_TOP_WIDE);
    BIND_ENUM_CONSTANT(PRESET_RIGHT_WIDE);
    BIND_ENUM_CONSTANT(PRESET_BOTTOM_WIDE);
    BIND_ENUM_CONSTANT(PRESET_VCENTER_WIDE);
    BIND_ENUM_CONSTANT(PRESET_HCENTER_WIDE);
    BIND_ENUM_CONSTANT(PRESET_WIDE);

    BIND_ENUM_CONSTANT(PRESET_MODE_MINSIZE);
    BIND_ENUM_CONSTANT(PRESET_MODE_KEEP_WIDTH);
    BIND_ENUM_CONSTANT(PRESET_MODE_KEEP_HEIGHT);
    BIND_ENUM_CONSTANT(PRESET_MODE_KEEP_SIZE);

    BIND_ENUM_CONSTANT(SIZE_FILL);
    BIND_ENUM_CONSTANT(SIZE_EXPAND);
    BIND_ENUM_CONSTANT(SIZE_EXPAND_FILL);
    BIND_ENUM_CONSTANT(SIZE_SHRINK_CENTER);
    BIND_ENUM_CONSTANT(SIZE_SHRINK_END);

    BIND_ENUM_CONSTANT(MOUSE_FILTER_STOP);
    BIND_ENUM_CONSTANT(MOUSE_FILTER_PASS);
    BIND_ENUM_CONSTANT(MOUSE_FILTER_IGNORE);

    BIND_ENUM_CONSTANT(GROW_DIRECTION_BEGIN);
    BIND_ENUM_CONSTANT(GROW_DIRECTION_END);
    BIND_ENUM_CONSTANT(GROW_DIRECTION_BOTH);

    BIND_ENUM_CONSTANT(ANCHOR_BEGIN);
    BIND_ENUM_CONSTANT(ANCHOR_END);

    ADD_SIGNAL(MethodInfo("resized"));
    ADD_SIGNAL(MethodInfo("gui_input", PropertyInfo(VariantType::OBJECT, "event", PropertyHint::ResourceType, "InputEvent")));
    ADD_SIGNAL(MethodInfo("mouse_entered"));
    ADD_SIGNAL(MethodInfo("mouse_exited"));
    ADD_SIGNAL(MethodInfo("focus_entered"));
    ADD_SIGNAL(MethodInfo("focus_exited"));
    ADD_SIGNAL(MethodInfo("size_flags_changed"));
    ADD_SIGNAL(MethodInfo("minimum_size_changed"));
    ADD_SIGNAL(MethodInfo("modal_closed"));

    BIND_VMETHOD(MethodInfo(VariantType::BOOL, "has_point", PropertyInfo(VariantType::VECTOR2, "point")));
}
Control::Control() {

    data.parent = nullptr;

    data.mouse_filter = MOUSE_FILTER_STOP;
    data.pass_on_modal_close_click = true;

    data.SI = nullptr;
    data.MI = nullptr;
    data.RI = nullptr;
    data.theme_owner = nullptr;
    data.modal_exclusive = false;
    data.default_cursor = CURSOR_ARROW;
    data.h_size_flags = SIZE_FILL;
    data.v_size_flags = SIZE_FILL;
    data.expand = 1;
    data.rotation = 0;
    data.parent_canvas_item = nullptr;
    data.scale = Vector2(1, 1);
    data.drag_owner = 0;
    data.modal_frame = 0;
    data.block_minimum_size_adjust = false;
    data.disable_visibility_clip = false;
    data.h_grow = GROW_DIRECTION_END;
    data.v_grow = GROW_DIRECTION_END;
    data.minimum_size_valid = false;
    data.updating_last_minimum_size = false;

    data.clip_contents = false;
    for (int i = 0; i < 4; i++) {
        data.anchor[i] = ANCHOR_BEGIN;
        data.margin[i] = 0;
    }
    data.focus_mode = FOCUS_NONE;
    data.modal_prev_focus_owner = 0;
}

Control::~Control() {
}
