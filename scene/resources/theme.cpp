/*************************************************************************/
/*  theme.cpp                                                            */
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

#include "theme.h"

#include "core/callable_method_pointer.h"
#include "core/object_tooling.h"
#include "core/os/file_access.h"
#include "core/print_string.h"
#include "core/method_bind.h"
#include "scene/resources/font.h"

#include "EASTL/sort.h"
#include "EASTL/deque.h"
#include <cassert>

IMPL_GDCLASS(Theme)
RES_BASE_EXTENSION_IMPL(Theme,"theme")

void Theme::_emit_theme_changed() {

    emit_changed();
}
PoolVector<String> Theme::_get_icon_list(const String &p_type) const {
    PoolVector<String> ilret;
    Vector<StringName> il;
    get_icon_list(StringName(p_type), &il);
    for (const StringName &E : il) {
        ilret.push_back(String(E));
    }
    return ilret;
}

PoolVector<String> Theme::_get_stylebox_list(const String &p_type) const {
    PoolVector<String> ilret;
    Vector<StringName> il = get_stylebox_list(StringName(p_type));
    for (const StringName &E : il) {
        ilret.push_back(String(E));
    }
    return ilret;
}

PoolVector<String> Theme::_get_font_list(const String &p_type) const {
    PoolVector<String> ilret;
    Vector<StringName> il;
    get_font_list(StringName(p_type), &il);
    for (const StringName &E : il) {
        ilret.push_back(String(E));
    }
    return ilret;
}

PoolVector<String> Theme::_get_color_list(const String &p_type) const {
    PoolVector<String> ilret;
    Vector<StringName> il;
    get_color_list(StringName(p_type), &il);
    for (const StringName &E : il) {
        ilret.push_back(String(E));
    }
    return ilret;
}

PoolVector<String> Theme::_get_constant_list(const String &p_type) const {
    PoolVector<String> ilret;
    Vector<StringName> il;
    get_constant_list(StringName(p_type), &il);
    for (const StringName &E : il) {
        ilret.push_back(String(E));
    }
    return ilret;
}

PoolVector<String> Theme::_get_type_list(StringView p_type) const {
    PoolVector<String> ilret;
    Vector<StringName> il;
    get_type_list(&il);
    for (const StringName &E : il) {
        ilret.push_back(String(E));
    }
    return ilret;
}
bool Theme::_set(const StringName &p_name, const Variant &p_value) {
    using namespace eastl;

    if (StringUtils::contains(p_name,'/') ) {

        StringView type = StringUtils::get_slice(p_name,'/', 1);
        StringName node_type(StringUtils::get_slice(p_name,'/', 0));
        StringName name(StringUtils::get_slice(p_name,'/', 2));

        if (type == "icons"_sv) {

            set_icon(name, node_type, refFromVariant<Texture>(p_value));
        } else if (type == "styles"_sv) {

            set_stylebox(name, node_type, refFromVariant<StyleBox>(p_value));
        } else if (type == "fonts"_sv) {

            set_font(name, node_type, refFromVariant<Font>(p_value));
        } else if (type == "colors"_sv) {

            set_color(name, node_type, p_value.as<Color>());
        } else if (type == "constants"_sv) {

            set_constant(name, node_type, p_value.as<int>());
        } else
            return false;

        return true;
    }

    return false;
}

bool Theme::_get(const StringName &p_name, Variant &r_ret) const {
    using namespace eastl;

    if (StringUtils::contains(p_name,'/') ) {

        StringView type = StringUtils::get_slice(p_name,'/', 1);
        StringName node_type(StringUtils::get_slice(p_name,'/', 0));
        StringName name(StringUtils::get_slice(p_name,'/', 2));

        if (type == "icons"_sv) {

            if (!has_icon(name, node_type))
                r_ret = Ref<Texture>();
            else
                r_ret = get_icon(name, node_type);
        } else if (type == "styles"_sv) {

            if (!has_stylebox(name, node_type))
                r_ret = Ref<StyleBox>();
            else
                r_ret = get_stylebox(name, node_type);
        } else if (type == "fonts"_sv) {

            if (!has_font(name, node_type))
                r_ret = Ref<Font>();
            else
                r_ret = get_font(name, node_type);
        } else if (type == "colors"_sv) {

            r_ret = get_color(name, node_type);
        } else if (type == "constants"_sv) {

            r_ret = get_constant(name, node_type);
        } else
            return false;

        return true;
    }

    return false;
}

void Theme::_get_property_list(Vector<PropertyInfo> *p_tgt) const {

    Vector<PropertyInfo> store;

    for(const eastl::pair<const StringName, HashMap<StringName, Ref<Texture> >> &kv : icon_map) {
        for(const auto &kv2 : kv.second) {
            store.emplace_back(VariantType::OBJECT, kv.first + "/icons/" + kv2.first,
                    PropertyHint::ResourceType, "Texture", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_STORE_IF_NULL);
        }
    }

    TmpString<1024,false> tmp_str;
    for (const auto &e : style_map) {
        tmp_str.assign(e.first.asCString());
        tmp_str.append("/styles/");
        for (const auto &f : e.second) {
            store.emplace_back(VariantType::OBJECT,  StringName(tmp_str.append(f.first.asCString())), PropertyHint::ResourceType, "StyleBox", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_STORE_IF_NULL);
        }
    }

    for (const auto &e : font_map) {
        tmp_str.assign(e.first.asCString());
        tmp_str.append("/fonts/");
        for (const auto &f : e.second) {

            store.emplace_back(VariantType::OBJECT,  StringName(tmp_str.append(f.first.asCString())), PropertyHint::ResourceType, "Font", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_STORE_IF_NULL);
        }
    }

    for(const auto &e : color_map) {
        tmp_str.assign(e.first.asCString());
        tmp_str.append("/colors/");
        for(const auto &f : e.second) {

            store.emplace_back(VariantType::COLOR, StringName(tmp_str.append(f.first.asCString())));
        }
    }

    for(const auto &e : constant_map) {
        tmp_str.assign(e.first.asCString());
        tmp_str.append("/constants/");
        for(const auto &f : e.second) {

            store.emplace_back(VariantType::INT, StringName(tmp_str.append(f.first.asCString())));
        }
    }
    eastl::sort(store.begin(),store.end());
    p_tgt->insert(p_tgt->end(),eastl::make_move_iterator(store.begin()),eastl::make_move_iterator(store.end()));
}



void Theme::set_default_theme_font(const Ref<Font> &p_default_font) {

    if (default_theme_font == p_default_font)
        return;

    if (default_theme_font) {
        default_theme_font->disconnect("changed",callable_mp(this, &ClassName::_emit_theme_changed));
    }

    default_theme_font = p_default_font;

    if (default_theme_font) {
        default_theme_font->connect("changed",callable_mp(this, &ClassName::_emit_theme_changed), varray(), ObjectNS::CONNECT_REFERENCE_COUNTED);
    }

    Object_change_notify(this);
    emit_changed();
}

Ref<Font> Theme::get_default_theme_font() const {

    return default_theme_font;
}

void Theme::set_icons(Span<const ThemeIcon> icon_defs, const StringName &p_type)
{
    for(const ThemeIcon & ic : icon_defs) {
        set_icon(StaticCString(ic.name,true), p_type, get_icon(StaticCString(ic.icon_name,true), StaticCString(ic.icon_type,true)));
    }
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

    //ERR_FAIL_COND();

    bool new_value = !icon_map.contains(p_type) || !icon_map[p_type].contains(p_name);

    if (icon_map[p_type].contains(p_name) && icon_map[p_type][p_name]) {
        icon_map[p_type][p_name]->disconnect("changed",callable_mp(this, &ClassName::_emit_theme_changed));
    }

    icon_map[p_type][p_name] = p_icon;

    if (p_icon) {
        icon_map[p_type][p_name]->connect("changed",callable_mp(this, &ClassName::_emit_theme_changed), varray(), ObjectNS::CONNECT_REFERENCE_COUNTED);
    }

    if (new_value) {
        Object_change_notify(this);
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

    ERR_FAIL_COND(!icon_map.contains(p_type));
    ERR_FAIL_COND(!icon_map[p_type].contains(p_name));

    if (icon_map[p_type][p_name]) {
        icon_map[p_type][p_name]->disconnect("changed",callable_mp(this, &ClassName::_emit_theme_changed));
    }

    icon_map[p_type].erase(p_name);

    Object_change_notify(this);
    emit_changed();
}

void Theme::get_icon_list(const StringName& p_type, Vector<StringName> *p_list) const {

    if (!icon_map.contains(p_type))
        return;
    const auto &vals(icon_map.at(p_type));
    p_list->reserve(p_list->size()+vals.size());

    for(const auto &key : vals) {
        p_list->push_back(key.first);
    }
}

void Theme::set_shader(const StringName &p_name, const StringName &p_type, const Ref<Shader> &p_shader) {
    bool new_value = !shader_map.contains(p_type) || !shader_map[p_type].contains(p_name);

    shader_map[p_type][p_name] = p_shader;

    if (new_value) {
        Object_change_notify(this);
        emit_changed();
    }
}

Ref<Shader> Theme::get_shader(const StringName &p_name, const StringName &p_type) const {
    if (shader_map.contains(p_type) && shader_map.at(p_type).contains(p_name)) {
        return shader_map.at(p_type).at(p_name);
    }

    return {};
}

bool Theme::has_shader(const StringName &p_name, const StringName &p_type) const {
    return shader_map.contains(p_type) && shader_map.at(p_type).contains(p_name) && shader_map.at(p_type).at(p_name);
}

void Theme::clear_shader(const StringName &p_name, const StringName &p_type) {
    ERR_FAIL_COND(!shader_map.contains(p_type));
    ERR_FAIL_COND(!shader_map[p_type].contains(p_name));

    shader_map[p_type].erase(p_name);
    Object_change_notify(this);
    emit_changed();
}

void Theme::get_shader_list(const StringName &p_type, Vector<StringName> *p_list) const {
    if (!shader_map.contains(p_type))
        return;

    for(const auto & v : shader_map.at(p_type)) {

        p_list->push_back(v.first);
    }
}

void Theme::set_stylebox(const StringName &p_name, const StringName &p_type, const Ref<StyleBox> &p_style) {

    //ERR_FAIL_COND();

    bool new_value = !style_map.contains(p_type) || !style_map[p_type].contains(p_name);

    if (style_map[p_type].contains(p_name) && style_map[p_type][p_name]) {
        style_map[p_type][p_name]->disconnect("changed",callable_mp(this, &ClassName::_emit_theme_changed));
    }

    style_map[p_type][p_name] = p_style;

    if (p_style) {
        style_map[p_type][p_name]->connect("changed",callable_mp(this, &ClassName::_emit_theme_changed), varray(), ObjectNS::CONNECT_REFERENCE_COUNTED);
    }

    if (new_value)
        Object_change_notify(this);
    emit_changed();
}

Ref<StyleBox> Theme::get_stylebox(const StringName &p_name, const StringName &p_type) const {

    if (style_map.contains(p_type) && style_map.at(p_type).contains(p_name) && style_map.at(p_type).at(p_name)) {

        return style_map.at(p_type).at(p_name);
    }
    return default_style;
}

bool Theme::has_stylebox(const StringName &p_name, const StringName &p_type) const {

    return style_map.contains(p_type) && style_map.at(p_type).contains(p_name) && style_map.at(p_type).at(p_name);
}

void Theme::clear_stylebox(const StringName &p_name, const StringName &p_type) {

    ERR_FAIL_COND(!style_map.contains(p_type));
    ERR_FAIL_COND(!style_map[p_type].contains(p_name));

    if (style_map[p_type][p_name]) {
        style_map[p_type][p_name]->disconnect("changed",callable_mp(this, &ClassName::_emit_theme_changed));
    }

    style_map[p_type].erase(p_name);

    Object_change_notify(this);
    emit_changed();
}

Vector<StringName> Theme::get_stylebox_list(const StringName& p_type) const {

    if (!style_map.contains(p_type))
        return {};
    Vector<StringName> res;
    const  HashMap<StringName, Ref<StyleBox> > &smap(style_map.at(p_type));
    res.reserve(smap.size());
    for(const auto & v : smap) {
        res.emplace_back(v.first);
    }
    return res;
}

Vector<StringName> Theme::get_stylebox_types() const {
    Vector<StringName> res;
    res.reserve(style_map.size());
    for (const auto & v : style_map) {
        res.emplace_back(v.first);
    }
    return res;
}

void Theme::set_font(const StringName &p_name, const StringName &p_type, const Ref<Font> &p_font) {

    //ERR_FAIL_COND();

    bool new_value = !font_map.contains(p_type) || !font_map[p_type].contains(p_name);

    if (font_map[p_type][p_name]) {
        font_map[p_type][p_name]->disconnect("changed",callable_mp(this, &ClassName::_emit_theme_changed));
    }

    font_map[p_type][p_name] = p_font;

    if (p_font) {
        font_map[p_type][p_name]->connect("changed",callable_mp(this, &ClassName::_emit_theme_changed), varray(), ObjectNS::CONNECT_REFERENCE_COUNTED);
    }

    if (new_value) {
        Object_change_notify(this);
        emit_changed();
    }
}
Ref<Font> Theme::get_font(const StringName &p_name, const StringName &p_type) const {
    const Ref<Font> dummy;
    if (font_map.contains(p_type) && font_map.at(p_type).at(p_name, dummy))
        return font_map.at(p_type).at(p_name);
    if (default_theme_font)
        return default_theme_font;
    return default_font;
}

bool Theme::has_font(const StringName &p_name, const StringName &p_type) const {

    return font_map.contains(p_type) && (font_map.at(p_type).at(p_name,Ref<Font>()) != nullptr);
}

void Theme::clear_font(const StringName &p_name, const StringName &p_type) {

    ERR_FAIL_COND(!font_map.contains(p_type));
    ERR_FAIL_COND(!font_map[p_type].contains(p_name));

    if (font_map[p_type][p_name]) {
        font_map[p_type][p_name]->disconnect("changed",callable_mp(this, &ClassName::_emit_theme_changed));
    }

    font_map[p_type].erase(p_name);
    Object_change_notify(this);
    emit_changed();
}

void Theme::get_font_list(const StringName& p_type, Vector<StringName> *p_list) const {

    if (!font_map.contains(p_type))
        return;

    for(const auto &v : font_map.at(p_type)) {

        p_list->push_back(v.first);
    }
}

void Theme::set_colors(Span<const Theme::ThemeColor> colors)
{
    bool need_notify = false;
    for(const ThemeColor & v : colors)
    {
        auto iter = color_map.find_as(v.type);
        if(iter==color_map.end()) {
            need_notify = true;
            iter = color_map.emplace(eastl::make_pair(StaticCString(v.type,true), HashMap<StringName, Color>())).first;
        }
        auto n_iter = iter->second.find_as(v.name);
        if(n_iter==iter->second.end()) {
            need_notify = true;
            n_iter = iter->second.emplace(eastl::make_pair(StaticCString(v.name,true),Color())).first;
        }
        need_notify |= n_iter->second!=v.color;
        n_iter->second = v.color;

    }
    if (need_notify) {
        Object_change_notify(this);
        emit_changed();
    }
}

void Theme::set_color(const StringName &p_name, const StringName &p_type, const Color &p_color) {

    bool new_value = !color_map.contains(p_type) || !color_map[p_type].contains(p_name);

    color_map[p_type][p_name] = p_color;

    if (new_value) {
        Object_change_notify(this);
        emit_changed();
    }
}

Color Theme::get_color(const StringName &p_name, const StringName &p_type) const {

    if (color_map.contains(p_type) && color_map.at(p_type).contains(p_name))
        return color_map.at(p_type).at(p_name);
    else
        return Color();
}

bool Theme::has_color(const StringName &p_name, const StringName &p_type) const {

    return color_map.contains(p_type) && color_map.at(p_type).contains(p_name);
}

void Theme::clear_color(const StringName &p_name, const StringName &p_type) {

    ERR_FAIL_COND(!color_map.contains(p_type));
    ERR_FAIL_COND(!color_map[p_type].contains(p_name));

    color_map[p_type].erase(p_name);
    Object_change_notify(this);
    emit_changed();
}

void Theme::get_color_list(const StringName& p_type, Vector<StringName> *p_list) const {

    if (!color_map.contains(p_type))
        return;

    for(const auto & c : color_map.at(p_type) ) {

        p_list->push_back(c.first);
    }
}

void Theme::set_constants(Span<const ThemeConstant> vals)
{
    bool need_notify = false;
    for(const ThemeConstant & v : vals)
    {
        auto iter = constant_map.find_as(v.type);
        if(iter==constant_map.end()) {
            need_notify = true;
            iter = constant_map.emplace(eastl::make_pair(StaticCString(v.type,true), HashMap<StringName, int>())).first;
        }
        auto n_iter = iter->second.find_as(v.name);
        if(n_iter==iter->second.end()) {
            need_notify = true;
            n_iter = iter->second.emplace(eastl::make_pair(StaticCString(v.name,true),0)).first;
        }
        need_notify |= n_iter->second!=v.value;
        n_iter->second = v.value;

    }
    if (need_notify) {
        Object_change_notify(this);
        emit_changed();
    }

}

void Theme::set_constant(const StringName &p_name, const StringName &p_type, int p_constant) {

    bool new_value = !constant_map.contains(p_type) || !constant_map[p_type].contains(p_name);
    constant_map[p_type][p_name] = p_constant;

    if (new_value) {
        Object_change_notify(this);
        emit_changed();
    }
}

int Theme::get_constant(const StringName &p_name, const StringName &p_type) const {

    if (constant_map.contains(p_type) && constant_map.at(p_type).contains(p_name))
        return constant_map.at(p_type).at(p_name);
    else {
        return 0;
    }
}

bool Theme::has_constant(const StringName &p_name, const StringName &p_type) const {

    return constant_map.contains(p_type) && constant_map.at(p_type).contains(p_name);
}

void Theme::clear_constant(const StringName &p_name, const StringName &p_type) {

    ERR_FAIL_COND(!constant_map.contains(p_type));
    ERR_FAIL_COND(!constant_map[p_type].contains(p_name));

    constant_map[p_type].erase(p_name);
    Object_change_notify(this);
    emit_changed();
}

void Theme::get_constant_list(const StringName& p_type, Vector<StringName> *p_list) const {

    if (!constant_map.contains(p_type))
        return;

    for(const auto & v : constant_map.at(p_type)) {

        p_list->push_back(v.first);
    }
}

void Theme::clear() {

    //these need disconnecting
    for(auto & kv : icon_map ) {
        for( auto &L : kv.second) {
            if(L.second)
                L.second->disconnect("changed",callable_mp(this, &ClassName::_emit_theme_changed));
        }
    }

    for(const auto & e : style_map) {
        for (const auto & f : e.second) {
            auto style(f.second);
            if(style)
                style->disconnect("changed",callable_mp(this, &ClassName::_emit_theme_changed));
        }
    }

    for (const auto & e : font_map) {
        for (const auto & f : e.second) {
            auto font(f.second);
            if(font)
                font->disconnect("changed",callable_mp(this, &ClassName::_emit_theme_changed));
        }
    }

    icon_map.clear();
    style_map.clear();
    font_map.clear();
    shader_map.clear();
    color_map.clear();
    constant_map.clear();

    Object_change_notify(this);
    emit_changed();
}

void Theme::copy_default_theme() {

    Ref<Theme> default_theme2 = get_default();
    copy_theme(default_theme2);
}

void Theme::copy_theme(const Ref<Theme> &p_other) {

    if (not p_other) {
        clear();
        return;
    }

    //these need reconnecting, so add normally
    for(auto & kv : icon_map ) {
        for( auto &L : kv.second) {
            set_icon(L.first, kv.first, L.second);
        }
    }

    for (const auto & e : style_map) {
        for (const auto & f : e.second) {
            set_stylebox(f.first, e.first, f.second);
        }
    }

    for (const auto & e : font_map) {
        for (const auto & f : e.second) {
            set_font(f.first, e.first, f.second);
        }
    }

    //these are ok to just copy

    color_map = p_other->color_map;
    constant_map = p_other->constant_map;
    shader_map = p_other->shader_map;

    Object_change_notify(this);
    emit_changed();
}

void Theme::get_type_list(Vector<StringName> *p_list) const {

    HashSet<StringName> types;

    for(auto & kv : icon_map ) {
        types.insert(kv.first);
    }

    for (const auto & e : style_map) {

        types.insert(e.first);
    }

    for (const auto & e : font_map) {

        types.insert(e.first);
    }

    for(const auto & cm : color_map) {

        types.insert(cm.first);
    }

    for (const auto & cm : constant_map) {
        types.insert(cm.first);
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
    MethodBinder::bind_method(D_METHOD("get_stylebox_types"), &Theme::get_stylebox_types);

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

    MethodBinder::bind_method("copy_default_theme", &Theme::copy_default_theme);
    MethodBinder::bind_method(D_METHOD("copy_theme", {"other"}), &Theme::copy_theme);

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "default_font", PropertyHint::ResourceType, "Font"), "set_default_font", "get_default_font");
}

Theme::Theme() {
}

Theme::~Theme() {
}
