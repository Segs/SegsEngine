/*************************************************************************/
/*  theme.cpp                                                            */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
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

#include "theme.h"
#include "core/os/file_access.h"
#include "core/print_string.h"
#include "core/method_bind.h"

#include <cassert>

IMPL_GDCLASS(Theme)
RES_BASE_EXTENSION_IMPL(Theme,"theme")

void Theme::_emit_theme_changed() {

    emit_changed();
}

bool Theme::_set(const StringName &p_name, const Variant &p_value) {

    String sname = p_name;

    if (StringUtils::contains(sname,'/') ) {

        String type = StringUtils::get_slice(sname,'/', 1);
        String node_type = StringUtils::get_slice(sname,'/', 0);
        String name = StringUtils::get_slice(sname,'/', 2);

        if (type == "icons") {

            set_icon(name, node_type, refFromRefPtr<Texture>(p_value));
        } else if (type == "styles") {

            set_stylebox(name, node_type, refFromRefPtr<StyleBox>(p_value));
        } else if (type == "fonts") {

            set_font(name, node_type, refFromRefPtr<Font>(p_value));
        } else if (type == "colors") {

            set_color(name, node_type, p_value);
        } else if (type == "constants") {

            set_constant(name, node_type, p_value);
        } else
            return false;

        return true;
    }

    return false;
}

bool Theme::_get(const StringName &p_name, Variant &r_ret) const {

    String sname = p_name;

    if (StringUtils::contains(sname,'/') ) {

        String type = StringUtils::get_slice(sname,'/', 1);
        String node_type = StringUtils::get_slice(sname,'/', 0);
        String name = StringUtils::get_slice(sname,'/', 2);

        if (type == "icons") {

            if (!has_icon(name, node_type))
                r_ret = Ref<Texture>();
            else
                r_ret = get_icon(name, node_type);
        } else if (type == "styles") {

            if (!has_stylebox(name, node_type))
                r_ret = Ref<StyleBox>();
            else
                r_ret = get_stylebox(name, node_type);
        } else if (type == "fonts") {

            if (!has_font(name, node_type))
                r_ret = Ref<Font>();
            else
                r_ret = get_font(name, node_type);
        } else if (type == "colors") {

            r_ret = get_color(name, node_type);
        } else if (type == "constants") {

            r_ret = get_constant(name, node_type);
        } else
            return false;

        return true;
    }

    return false;
}

void Theme::_get_property_list(ListPOD<PropertyInfo> *p_list) const {

    List<PropertyInfo> list;

    for(const eastl::pair<const StringName, DefHashMap<StringName, Ref<Texture> >> &kv : icon_map) {
        for(const auto &kv2 : kv.second) {
            list.push_back(PropertyInfo(VariantType::OBJECT, String() + kv.first + "/icons/" + kv2.first,
                    PROPERTY_HINT_RESOURCE_TYPE, "Texture", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_STORE_IF_NULL));
        }
    }
    const StringName *key = nullptr;
    const StringName *key2 = nullptr;

    key = nullptr;

    while ((key = style_map.next(key))) {

        const StringName *key2 = nullptr;

        while ((key2 = style_map[*key].next(key2))) {

            list.push_back(PropertyInfo(VariantType::OBJECT, String() + *key + "/styles/" + *key2, PROPERTY_HINT_RESOURCE_TYPE, "StyleBox", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_STORE_IF_NULL));
        }
    }

    key = nullptr;

    while ((key = font_map.next(key))) {

        const StringName *key2 = nullptr;

        while ((key2 = font_map[*key].next(key2))) {

            list.push_back(PropertyInfo(VariantType::OBJECT, String() + *key + "/fonts/" + *key2, PROPERTY_HINT_RESOURCE_TYPE, "Font", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_STORE_IF_NULL));
        }
    }

    key = nullptr;

    while ((key = color_map.next(key))) {

        const StringName *key2 = nullptr;

        while ((key2 = color_map[*key].next(key2))) {

            list.push_back(PropertyInfo(VariantType::COLOR, String() + *key + "/colors/" + *key2));
        }
    }

    key = nullptr;

    while ((key = constant_map.next(key))) {

        const StringName *key2 = nullptr;

        while ((key2 = constant_map[*key].next(key2))) {

            list.push_back(PropertyInfo(VariantType::INT, String() + *key + "/constants/" + *key2));
        }
    }

    list.sort();
    for (List<PropertyInfo>::Element *E = list.front(); E; E = E->next()) {
        p_list->push_back(E->deref());
    }
}

void Theme::set_default_theme_font(const Ref<Font> &p_default_font) {

    if (default_theme_font == p_default_font)
        return;

    if (default_theme_font) {
        default_theme_font->disconnect("changed", this, "_emit_theme_changed");
    }

    default_theme_font = p_default_font;

    if (default_theme_font) {
        default_theme_font->connect("changed", this, "_emit_theme_changed", varray(), ObjectNS::CONNECT_REFERENCE_COUNTED);
    }

    _change_notify();
    emit_changed();
}

Ref<Font> Theme::get_default_theme_font() const {

    return default_theme_font;
}

Ref<Theme> Theme::project_default_theme;
Ref<Theme> Theme::default_theme;
Ref<Texture> Theme::default_icon;
Ref<StyleBox> Theme::default_style;
Ref<Font> Theme::default_font;

Ref<Theme> Theme::get_default() {

    return default_theme;
}

void Theme::set_default(const Ref<Theme> &p_default) {

    default_theme = p_default;
}

Ref<Theme> Theme::get_project_default() {

    return project_default_theme;
}

void Theme::set_project_default(const Ref<Theme> &p_project_default) {

    project_default_theme = p_project_default;
}

void Theme::set_default_icon(const Ref<Texture> &p_icon) {

    default_icon = p_icon;
}
void Theme::set_default_style(const Ref<StyleBox> &p_style) {

    default_style = p_style;
}
void Theme::set_default_font(const Ref<Font> &p_font) {

    default_font = p_font;
}

void Theme::set_icon(const StringName &p_name, const StringName &p_type, const Ref<Texture> &p_icon) {

    //ERR_FAIL_COND(p_icon.is_null())

    bool new_value = !icon_map.contains(p_type) || !icon_map[p_type].contains(p_name);

    if (icon_map[p_type].contains(p_name) && icon_map[p_type][p_name]) {
        icon_map[p_type][p_name]->disconnect("changed", this, "_emit_theme_changed");
    }

    icon_map[p_type][p_name] = p_icon;

    if (p_icon) {
        icon_map[p_type][p_name]->connect("changed", this, "_emit_theme_changed", varray(), ObjectNS::CONNECT_REFERENCE_COUNTED);
    }

    if (new_value) {
        _change_notify();
        emit_changed();
    }
}
Ref<Texture> Theme::get_icon(const StringName &p_name, const StringName &p_type) const {

    if (icon_map.contains(p_type) && icon_map.at(p_type).contains(p_name) && icon_map.at(p_type).at(p_name)) {

        return icon_map.at(p_type).at(p_name);
    } else {
        return default_icon;
    }
}

bool Theme::has_icon(const StringName &p_name, const StringName &p_type) const {
    bool has_type = icon_map.contains(p_type);

    return has_type && icon_map.at(p_type).contains(p_name) && icon_map.at(p_type).at(p_name);
}

void Theme::clear_icon(const StringName &p_name, const StringName &p_type) {

    ERR_FAIL_COND(!icon_map.contains(p_type))
    ERR_FAIL_COND(!icon_map[p_type].contains(p_name))

    if (icon_map[p_type][p_name]) {
        icon_map[p_type][p_name]->disconnect("changed", this, "_emit_theme_changed");
    }

    icon_map[p_type].erase(p_name);

    _change_notify();
    emit_changed();
}

void Theme::get_icon_list(const StringName& p_type, List<StringName> *p_list) const {

    if (!icon_map.contains(p_type))
        return;

    for(const auto &key : icon_map.at(p_type)) {

        p_list->push_back(key.first);
    }
}

void Theme::set_shader(const StringName &p_name, const StringName &p_type, const Ref<Shader> &p_shader) {
    bool new_value = !shader_map.contains(p_type) || !shader_map[p_type].contains(p_name);

    shader_map[p_type][p_name] = p_shader;

    if (new_value) {
        _change_notify();
        emit_changed();
    }
}

Ref<Shader> Theme::get_shader(const StringName &p_name, const StringName &p_type) const {
    if (shader_map.contains(p_type) && shader_map[p_type].contains(p_name) && shader_map[p_type][p_name]) {
        return shader_map[p_type][p_name];
    } else {
        return {};
    }
}

bool Theme::has_shader(const StringName &p_name, const StringName &p_type) const {
    return (shader_map.contains(p_type) && shader_map[p_type].contains(p_name) && shader_map[p_type][p_name]);
}

void Theme::clear_shader(const StringName &p_name, const StringName &p_type) {
    ERR_FAIL_COND(!shader_map.contains(p_type))
    ERR_FAIL_COND(!shader_map[p_type].contains(p_name))

    shader_map[p_type].erase(p_name);
    _change_notify();
    emit_changed();
}

void Theme::get_shader_list(const StringName &p_type, List<StringName> *p_list) const {
    if (!shader_map.contains(p_type))
        return;

    const StringName *key = nullptr;

    while ((key = shader_map[p_type].next(key))) {

        p_list->push_back(*key);
    }
}

void Theme::set_stylebox(const StringName &p_name, const StringName &p_type, const Ref<StyleBox> &p_style) {

    //ERR_FAIL_COND(p_style.is_null())

    bool new_value = !style_map.contains(p_type) || !style_map[p_type].contains(p_name);

    if (style_map[p_type].contains(p_name) && style_map[p_type][p_name]) {
        style_map[p_type][p_name]->disconnect("changed", this, "_emit_theme_changed");
    }

    style_map[p_type][p_name] = p_style;

    if (p_style) {
        style_map[p_type][p_name]->connect("changed", this, "_emit_theme_changed", varray(), ObjectNS::CONNECT_REFERENCE_COUNTED);
    }

    if (new_value)
        _change_notify();
    emit_changed();
}

Ref<StyleBox> Theme::get_stylebox(const StringName &p_name, const StringName &p_type) const {

    if (style_map.contains(p_type) && style_map[p_type].contains(p_name) && style_map[p_type][p_name]) {

        return style_map[p_type][p_name];
    } else {
        return default_style;
    }
}

bool Theme::has_stylebox(const StringName &p_name, const StringName &p_type) const {

    return (style_map.contains(p_type) && style_map[p_type].contains(p_name) && style_map[p_type][p_name]);
}

void Theme::clear_stylebox(const StringName &p_name, const StringName &p_type) {

    ERR_FAIL_COND(!style_map.contains(p_type))
    ERR_FAIL_COND(!style_map[p_type].contains(p_name))

    if (style_map[p_type][p_name]) {
        style_map[p_type][p_name]->disconnect("changed", this, "_emit_theme_changed");
    }

    style_map[p_type].erase(p_name);

    _change_notify();
    emit_changed();
}

void Theme::get_stylebox_list(const StringName& p_type, List<StringName> *p_list) const {

    if (!style_map.contains(p_type))
        return;

    const StringName *key = nullptr;

    while ((key = style_map[p_type].next(key))) {

        p_list->push_back(*key);
    }
}

void Theme::get_stylebox_types(List<StringName> *p_list) const {
    const StringName *key = nullptr;
    while ((key = style_map.next(key))) {
        p_list->push_back(*key);
    }
}

void Theme::set_font(const StringName &p_name, const StringName &p_type, const Ref<Font> &p_font) {

    //ERR_FAIL_COND(p_font.is_null())

    bool new_value = !font_map.contains(p_type) || !font_map[p_type].contains(p_name);

    if (font_map[p_type][p_name]) {
        font_map[p_type][p_name]->disconnect("changed", this, "_emit_theme_changed");
    }

    font_map[p_type][p_name] = p_font;

    if (p_font) {
        font_map[p_type][p_name]->connect("changed", this, "_emit_theme_changed", varray(), ObjectNS::CONNECT_REFERENCE_COUNTED);
    }

    if (new_value) {
        _change_notify();
        emit_changed();
    }
}
Ref<Font> Theme::get_font(const StringName &p_name, const StringName &p_type) const {

    if (font_map.contains(p_type) && font_map[p_type].contains(p_name) && font_map[p_type][p_name])
        return font_map[p_type][p_name];
    else if (default_theme_font)
        return default_theme_font;
    else
        return default_font;
}

bool Theme::has_font(const StringName &p_name, const StringName &p_type) const {

    return (font_map.contains(p_type) && font_map[p_type].contains(p_name) && font_map[p_type][p_name]);
}

void Theme::clear_font(const StringName &p_name, const StringName &p_type) {

    ERR_FAIL_COND(!font_map.contains(p_type))
    ERR_FAIL_COND(!font_map[p_type].contains(p_name))

    if (font_map[p_type][p_name]) {
        font_map[p_type][p_name]->disconnect("changed", this, "_emit_theme_changed");
    }

    font_map[p_type].erase(p_name);
    _change_notify();
    emit_changed();
}

void Theme::get_font_list(const StringName& p_type, List<StringName> *p_list) const {

    if (!font_map.contains(p_type))
        return;

    const StringName *key = nullptr;

    while ((key = font_map[p_type].next(key))) {

        p_list->push_back(*key);
    }
}

void Theme::set_color(const StringName &p_name, const StringName &p_type, const Color &p_color) {

    bool new_value = !color_map.contains(p_type) || !color_map[p_type].contains(p_name);

    color_map[p_type][p_name] = p_color;

    if (new_value) {
        _change_notify();
        emit_changed();
    }
}

Color Theme::get_color(const StringName &p_name, const StringName &p_type) const {

    if (color_map.contains(p_type) && color_map[p_type].contains(p_name))
        return color_map[p_type][p_name];
    else
        return Color();
}

bool Theme::has_color(const StringName &p_name, const StringName &p_type) const {

    return (color_map.contains(p_type) && color_map[p_type].contains(p_name));
}

void Theme::clear_color(const StringName &p_name, const StringName &p_type) {

    ERR_FAIL_COND(!color_map.contains(p_type))
    ERR_FAIL_COND(!color_map[p_type].contains(p_name))

    color_map[p_type].erase(p_name);
    _change_notify();
    emit_changed();
}

void Theme::get_color_list(const StringName& p_type, List<StringName> *p_list) const {

    if (!color_map.contains(p_type))
        return;

    const StringName *key = nullptr;

    while ((key = color_map[p_type].next(key))) {

        p_list->push_back(*key);
    }
}

void Theme::set_constant(const StringName &p_name, const StringName &p_type, int p_constant) {

    bool new_value = !constant_map.contains(p_type) || !constant_map[p_type].contains(p_name);
    constant_map[p_type][p_name] = p_constant;

    if (new_value) {
        _change_notify();
        emit_changed();
    }
}

int Theme::get_constant(const StringName &p_name, const StringName &p_type) const {

    if (constant_map.contains(p_type) && constant_map[p_type].contains(p_name))
        return constant_map[p_type][p_name];
    else {
        return 0;
    }
}

bool Theme::has_constant(const StringName &p_name, const StringName &p_type) const {

    return (constant_map.contains(p_type) && constant_map[p_type].contains(p_name));
}

void Theme::clear_constant(const StringName &p_name, const StringName &p_type) {

    ERR_FAIL_COND(!constant_map.contains(p_type))
    ERR_FAIL_COND(!constant_map[p_type].contains(p_name))

    constant_map[p_type].erase(p_name);
    _change_notify();
    emit_changed();
}

void Theme::get_constant_list(const StringName& p_type, List<StringName> *p_list) const {

    if (!constant_map.contains(p_type))
        return;

    const StringName *key = nullptr;

    while ((key = constant_map[p_type].next(key))) {

        p_list->push_back(*key);
    }
}

void Theme::clear() {

    //these need disconnecting
    {
        for(auto & kv : icon_map ) {
            for( auto &L : kv.second) {
                L.second->disconnect("changed", this, "_emit_theme_changed");
            }
        }
    }

    {
        const StringName *K = nullptr;
        while ((K = style_map.next(K))) {
            const StringName *L = nullptr;
            while ((L = style_map[*K].next(L))) {
                style_map[*K][*L]->disconnect("changed", this, "_emit_theme_changed");
            }
        }
    }

    {
        const StringName *K = nullptr;
        while ((K = font_map.next(K))) {
            const StringName *L = nullptr;
            while ((L = font_map[*K].next(L))) {
                font_map[*K][*L]->disconnect("changed", this, "_emit_theme_changed");
            }
        }
    }

    icon_map.clear();
    style_map.clear();
    font_map.clear();
    shader_map.clear();
    color_map.clear();
    constant_map.clear();

    _change_notify();
    emit_changed();
}

void Theme::copy_default_theme() {

    Ref<Theme> default_theme2 = get_default();
    copy_theme(default_theme2);
}

void Theme::copy_theme(const Ref<Theme> &p_other) {

    //these need reconnecting, so add normally
    {
        for(auto & kv : icon_map ) {
            for( auto &L : kv.second) {
                set_icon(L.first, kv.first, L.second);
            }
        }
    }

    {
        const StringName *K = nullptr;
        while ((K = p_other->style_map.next(K))) {
            const StringName *L = nullptr;
            while ((L = p_other->style_map[*K].next(L))) {
                set_stylebox(*L, *K, p_other->style_map[*K][*L]);
            }
        }
    }

    {
        const StringName *K = nullptr;
        while ((K = p_other->font_map.next(K))) {
            const StringName *L = nullptr;
            while ((L = p_other->font_map[*K].next(L))) {
                set_font(*L, *K, p_other->font_map[*K][*L]);
            }
        }
    }

    //these are ok to just copy

    color_map = p_other->color_map;
    constant_map = p_other->constant_map;
    shader_map = p_other->shader_map;

    _change_notify();
    emit_changed();
}

void Theme::get_type_list(List<StringName> *p_list) const {

    Set<StringName> types;

    const StringName *key = nullptr;

    for(auto & kv : icon_map ) {
        types.insert(kv.first);
    }

    key = nullptr;

    while ((key = style_map.next(key))) {

        types.insert(*key);
    }

    key = nullptr;

    while ((key = font_map.next(key))) {

        types.insert(*key);
    }

    key = nullptr;

    while ((key = color_map.next(key))) {

        types.insert(*key);
    }

    key = nullptr;

    while ((key = constant_map.next(key))) {

        types.insert(*key);
    }

    for (const StringName &E : types) {

        p_list->push_back(E);
    }
}

void Theme::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_icon", {"name", "type", "texture"}), &Theme::set_icon);
    MethodBinder::bind_method(D_METHOD("get_icon", {"name", "type"}), &Theme::get_icon);
    MethodBinder::bind_method(D_METHOD("has_icon", {"name", "type"}), &Theme::has_icon);
    MethodBinder::bind_method(D_METHOD("clear_icon", {"name", "type"}), &Theme::clear_icon);
    MethodBinder::bind_method(D_METHOD("get_icon_list", {"type"}), &Theme::_get_icon_list);

    MethodBinder::bind_method(D_METHOD("set_stylebox", {"name", "type", "texture"}), &Theme::set_stylebox);
    MethodBinder::bind_method(D_METHOD("get_stylebox", {"name", "type"}), &Theme::get_stylebox);
    MethodBinder::bind_method(D_METHOD("has_stylebox", {"name", "type"}), &Theme::has_stylebox);
    MethodBinder::bind_method(D_METHOD("clear_stylebox", {"name", "type"}), &Theme::clear_stylebox);
    MethodBinder::bind_method(D_METHOD("get_stylebox_list", {"type"}), &Theme::_get_stylebox_list);
    MethodBinder::bind_method(D_METHOD("get_stylebox_types"), &Theme::_get_stylebox_types);

    MethodBinder::bind_method(D_METHOD("set_font", {"name", "type", "font"}), &Theme::set_font);
    MethodBinder::bind_method(D_METHOD("get_font", {"name", "type"}), &Theme::get_font);
    MethodBinder::bind_method(D_METHOD("has_font", {"name", "type"}), &Theme::has_font);
    MethodBinder::bind_method(D_METHOD("clear_font", {"name", "type"}), &Theme::clear_font);
    MethodBinder::bind_method(D_METHOD("get_font_list", {"type"}), &Theme::_get_font_list);

    MethodBinder::bind_method(D_METHOD("set_color", {"name", "type", "color"}), &Theme::set_color);
    MethodBinder::bind_method(D_METHOD("get_color", {"name", "type"}), &Theme::get_color);
    MethodBinder::bind_method(D_METHOD("has_color", {"name", "type"}), &Theme::has_color);
    MethodBinder::bind_method(D_METHOD("clear_color", {"name", "type"}), &Theme::clear_color);
    MethodBinder::bind_method(D_METHOD("get_color_list", {"type"}), &Theme::_get_color_list);

    MethodBinder::bind_method(D_METHOD("set_constant", {"name", "type", "constant"}), &Theme::set_constant);
    MethodBinder::bind_method(D_METHOD("get_constant", {"name", "type"}), &Theme::get_constant);
    MethodBinder::bind_method(D_METHOD("has_constant", {"name", "type"}), &Theme::has_constant);
    MethodBinder::bind_method(D_METHOD("clear_constant", {"name", "type"}), &Theme::clear_constant);
    MethodBinder::bind_method(D_METHOD("get_constant_list", {"type"}), &Theme::_get_constant_list);

    MethodBinder::bind_method(D_METHOD("clear"), &Theme::clear);

    MethodBinder::bind_method(D_METHOD("set_default_font", {"font"}), &Theme::set_default_theme_font);
    MethodBinder::bind_method(D_METHOD("get_default_font"), &Theme::get_default_theme_font);

    MethodBinder::bind_method(D_METHOD("get_type_list", {"type"}), &Theme::_get_type_list);

    MethodBinder::bind_method(D_METHOD("_emit_theme_changed"), &Theme::_emit_theme_changed);

    MethodBinder::bind_method("copy_default_theme", &Theme::copy_default_theme);
    MethodBinder::bind_method(D_METHOD("copy_theme", {"other"}), &Theme::copy_theme);

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "default_font", PROPERTY_HINT_RESOURCE_TYPE, "Font"), "set_default_font", "get_default_font");
}

Theme::Theme() {
}

Theme::~Theme() {
}
