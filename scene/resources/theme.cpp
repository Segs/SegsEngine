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
#include "core/fixed_string.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/os/file_access.h"
#include "core/print_string.h"
#include "core/set.h"
#include "scene/resources/font.h"

#include "EASTL/deque.h"
#include "EASTL/sort.h"
#include <cassert>

IMPL_GDCLASS(Theme)
RES_BASE_EXTENSION_IMPL(Theme, "theme")
VARIANT_ENUM_CAST(Theme::DataType);

// Universal Theme resources used when no other theme has the item.
Ref<Theme> Theme::default_theme;
Ref<Theme> Theme::project_default_theme;
// Universal default values, final fallback for every theme.
Ref<Texture> Theme::default_icon;
Ref<StyleBox> Theme::default_style;
Ref<Font> Theme::default_font;


bool Theme::_set(const StringName &p_name, const Variant &p_value) {
    using namespace eastl;

    if (StringUtils::contains(p_name, '/')) {
        StringView type = StringUtils::get_slice(p_name, '/', 1);
        StringName theme_type(StringUtils::get_slice(p_name, '/', 0));
        StringName name(StringUtils::get_slice(p_name, '/', 2));

        if (type == "icons"_sv) {
            set_icon(name, theme_type, refFromVariant<Texture>(p_value));
        } else if (type == "styles"_sv) {
            set_stylebox(name, theme_type, refFromVariant<StyleBox>(p_value));
        } else if (type == "fonts"_sv) {
            set_font(name, theme_type, refFromVariant<Font>(p_value));
        } else if (type == "colors"_sv) {
            set_color(name, theme_type, p_value.as<Color>());
        } else if (type == "constants"_sv) {
            set_constant(name, theme_type, p_value.as<int>());
        } else if (type == "base_type") {
            set_type_variation(theme_type, p_value.as<StringName>());
        } else {
            return false;
        }

        return true;
    }

    return false;
}

bool Theme::_get(const StringName &p_name, Variant &r_ret) const {
    using namespace eastl;

    if (StringUtils::contains(p_name, '/')) {
        StringView type = StringUtils::get_slice(p_name, '/', 1);
        StringName theme_type(StringUtils::get_slice(p_name, '/', 0));
        StringName name(StringUtils::get_slice(p_name, '/', 2));

        if (type == "icons"_sv) {
            if (!has_icon(name, theme_type)) {
                r_ret = Ref<Texture>();
            } else {
                r_ret = get_icon(name, theme_type);
            }
        } else if (type == "styles"_sv) {
            if (!has_stylebox(name, theme_type)) {
                r_ret = Ref<StyleBox>();
            } else {
                r_ret = get_stylebox(name, theme_type);
            }
        } else if (type == "fonts"_sv) {
            if (!has_font(name, theme_type)) {
                r_ret = Ref<Font>();
            } else {
                r_ret = get_font(name, theme_type);
            }
        } else if (type == "colors"_sv) {
            r_ret = get_color(name, theme_type);
        } else if (type == "constants"_sv) {
            r_ret = get_constant(name, theme_type);
        } else if (type == "base_type") {
            r_ret = get_type_variation_base(theme_type);
        } else {
            return false;
        }

        return true;
    }

    return false;
}

void Theme::_get_property_list(Vector<PropertyInfo> *p_tgt) const {
    Vector<PropertyInfo> store;

    // Type variations.
    for(const auto & v : variation_map) {
        p_tgt->push_back(PropertyInfo(VariantType::STRING, v.first + "/base_type"));
    }

    // Icons.

    for (const eastl::pair<const StringName, HashMap<StringName, Ref<Texture>>> &kv : icon_map) {
        for (const auto &kv2 : kv.second) {
            store.emplace_back(VariantType::OBJECT, kv.first + "/icons/" + kv2.first, PropertyHint::ResourceType,
                    "Texture", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_STORE_IF_NULL);
        }
    }
    // Styles.
    TmpString<1024, false> tmp_str;
    for (const auto &e : style_map) {
        tmp_str.assign(e.first.asCString());
        tmp_str.append("/styles/");
        for (const auto &f : e.second) {
            store.emplace_back(VariantType::OBJECT, StringName(tmp_str.append(f.first.asCString())),
                    PropertyHint::ResourceType, "StyleBox", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_STORE_IF_NULL);
        }
    }
    // Fonts.
    for (const auto &e : font_map) {
        tmp_str.assign(e.first.asCString());
        tmp_str.append("/fonts/");
        for (const auto &f : e.second) {
            store.emplace_back(VariantType::OBJECT, StringName(tmp_str.append(f.first.asCString())),
                    PropertyHint::ResourceType, "Font", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_STORE_IF_NULL);
        }
    }
    // Colors.
    for (const auto &e : color_map) {
        tmp_str.assign(e.first.asCString());
        tmp_str.append("/colors/");
        for (const auto &f : e.second) {
            store.emplace_back(VariantType::COLOR, StringName(tmp_str.append(f.first.asCString())));
        }
    }
    // Constants.
    for (const auto &e : constant_map) {
        tmp_str.assign(e.first.asCString());
        tmp_str.append("/constants/");
        for (const auto &f : e.second) {
            store.emplace_back(VariantType::INT, StringName(tmp_str.append(f.first.asCString())));
        }
    }
    eastl::sort(store.begin(), store.end());
    p_tgt->insert(p_tgt->end(), eastl::make_move_iterator(store.begin()), eastl::make_move_iterator(store.end()));
}



void Theme::set_icons(Span<const ThemeIcon> icon_defs, const StringName &p_theme_type) {
    for (const ThemeIcon &ic : icon_defs) {
        set_icon(StaticCString(ic.name, true), p_theme_type,
                get_icon(StaticCString(ic.icon_name, true), StaticCString(ic.icon_type, true)));
    }
}

const Ref<Theme> &Theme::get_default() {
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
// Universal fallback values for theme item types.
void Theme::set_default_icon(const Ref<Texture> &p_icon) {
    default_icon = p_icon;
}
void Theme::set_default_style(const Ref<StyleBox> &p_style) {
    default_style = p_style;
}
void Theme::set_default_font(const Ref<Font> &p_font) {
    default_font = p_font;
}
// Fallback values for theme item types, configurable per theme.
void Theme::set_default_theme_font(const Ref<Font> &p_default_font) {
    if (default_theme_font == p_default_font) {
        return;
    }

    if (default_theme_font) {
        default_theme_font->disconnect("changed", cb_theme_changed);
    }

    default_theme_font = p_default_font;

    if (default_theme_font) {
        default_theme_font->connect(
                "changed", cb_theme_changed, ObjectNS::CONNECT_REFERENCE_COUNTED);
    }

    _emit_theme_changed();
}
Ref<Font> Theme::get_default_theme_font() const {
    return default_theme_font;
}

bool Theme::has_default_theme_font() const {
    return default_theme_font!=nullptr;
}

// Icons.
void Theme::set_icon(const StringName &p_name, const StringName &p_theme_type, const Ref<Texture> &p_icon) {
    bool existing = false;
    if (icon_map[p_theme_type].contains(p_name) && icon_map[p_theme_type][p_name]) {
        existing = true;
        icon_map[p_theme_type][p_name]->disconnect("changed", cb_theme_changed);
    }

    icon_map[p_theme_type][p_name] = p_icon;

    if (p_icon) {
        icon_map[p_theme_type][p_name]->connect(
                "changed", cb_theme_changed, ObjectNS::CONNECT_REFERENCE_COUNTED);
    }

    _emit_theme_changed(!existing);
}

Ref<Texture> Theme::get_icon(const StringName &p_name, const StringName &p_theme_type) const {
    if (icon_map.contains(p_theme_type) && icon_map.at(p_theme_type).contains(p_name) &&
            icon_map.at(p_theme_type).at(p_name)) {
        return icon_map.at(p_theme_type).at(p_name);
    } else {
        return default_icon;
    }
}

bool Theme::has_icon(const StringName &p_name, const StringName &p_theme_type) const {
    bool has_type = icon_map.contains(p_theme_type);

    return has_type && icon_map.at(p_theme_type).contains(p_name) && icon_map.at(p_theme_type).at(p_name);
}
bool Theme::has_icon_nocheck(const StringName &p_name, const StringName &p_theme_type) const {
    return (icon_map.contains(p_theme_type) && icon_map.at(p_theme_type).contains(p_name));
}

void Theme::rename_icon(const StringName &p_old_name, const StringName &p_name, const StringName &p_theme_type) {
    ERR_FAIL_COND_MSG(!icon_map.contains(p_theme_type), "Cannot rename the icon '" + String(p_old_name) + "' because the node type '" + String(p_theme_type) + "' does not exist.");
    ERR_FAIL_COND_MSG(icon_map[p_theme_type].contains(p_name), "Cannot rename the icon '" + String(p_old_name) + "' because the new name '" + String(p_name) + "' already exists.");
    ERR_FAIL_COND_MSG(!icon_map[p_theme_type].contains(p_old_name), "Cannot rename the icon '" + String(p_old_name) + "' because it does not exist.");

    icon_map[p_theme_type][p_name] = icon_map[p_theme_type][p_old_name];
    icon_map[p_theme_type].erase(p_old_name);

    _emit_theme_changed(true);
}
void Theme::clear_icon(const StringName &p_name, const StringName &p_theme_type) {
    ERR_FAIL_COND_MSG(!icon_map.contains(p_theme_type), "Cannot clear the icon '" + String(p_name) + "' because the node type '" + String(p_theme_type) + "' does not exist.");
    ERR_FAIL_COND_MSG(!icon_map[p_theme_type].contains(p_name), "Cannot clear the icon '" + String(p_name) + "' because it does not exist.");

    if (icon_map[p_theme_type][p_name]) {
        icon_map[p_theme_type][p_name]->disconnect("changed", cb_theme_changed);
    }

    icon_map[p_theme_type].erase(p_name);

    _emit_theme_changed(true);
}

void Theme::get_icon_list(const StringName &p_theme_type, Vector<StringName> &p_list) const {
    if (!icon_map.contains(p_theme_type)) {
        return;
    }
    const auto &vals(icon_map.at(p_theme_type));
    vals.keys_into(p_list);
}
void Theme::add_icon_type(const StringName &p_theme_type) {
    if (icon_map.contains(p_theme_type)) {
        return;
    }
    icon_map[p_theme_type] = {};
}

void Theme::remove_icon_type(const StringName &p_theme_type) {
    if (!icon_map.contains(p_theme_type)) {
        return;
    }

    _freeze_change_propagation();

    for (const auto & E : icon_map[p_theme_type]) {
        if (E.second) {
            E.second->disconnect("changed", callable_mp(this, &Theme::_emit_theme_changed));
        }
    }

    icon_map.erase(p_theme_type);

    _unfreeze_and_propagate_changes();
}
void Theme::get_icon_types(Vector<StringName> &p_list) const {
    icon_map.keys_into(p_list);
}

// Shaders.
void Theme::set_shader(const StringName &p_name, const StringName &p_theme_type, const Ref<Shader> &p_shader) {
    bool existing = (shader_map.contains(p_theme_type) && shader_map[p_theme_type].contains(p_name));
    shader_map[p_theme_type][p_name] = p_shader;

    _emit_theme_changed(!existing);
}

Ref<Shader> Theme::get_shader(const StringName &p_name, const StringName &p_theme_type) const {
    if (shader_map.contains(p_theme_type) && shader_map.at(p_theme_type).contains(p_name)) {
        return shader_map.at(p_theme_type).at(p_name);
    }

    return {};
}

bool Theme::has_shader(const StringName &p_name, const StringName &p_theme_type) const {
    return shader_map.contains(p_theme_type) && shader_map.at(p_theme_type).contains(p_name) &&
           shader_map.at(p_theme_type).at(p_name);
}

void Theme::clear_shader(const StringName &p_name, const StringName &p_theme_type) {
    ERR_FAIL_COND(!shader_map.contains(p_theme_type));
    ERR_FAIL_COND(!shader_map[p_theme_type].contains(p_name));

    shader_map[p_theme_type].erase(p_name);
    _emit_theme_changed(true);
}


void Theme::get_shader_list(const StringName &p_theme_type, Vector<StringName> *p_list) const {
    if (!shader_map.contains(p_theme_type)) {
        return;
    }

    for (const auto &v : shader_map.at(p_theme_type)) {
        p_list->push_back(v.first);
    }
}
// Styleboxes.
void Theme::set_stylebox(const StringName &p_name, const StringName &p_theme_type, const Ref<StyleBox> &p_style) {
    // ERR_FAIL_COND();

    bool existing = false;

    if (style_map[p_theme_type].contains(p_name) && style_map[p_theme_type][p_name]) {
        existing = true;
        style_map[p_theme_type][p_name]->disconnect("changed", cb_theme_changed);
    }

    style_map[p_theme_type][p_name] = p_style;

    if (p_style) {
        style_map[p_theme_type][p_name]->connect(
                "changed", cb_theme_changed, ObjectNS::CONNECT_REFERENCE_COUNTED);
    }

    _emit_theme_changed(!existing);
}

Ref<StyleBox> Theme::get_stylebox(const StringName &p_name, const StringName &p_theme_type) const {
    if (style_map.contains(p_theme_type) && style_map.at(p_theme_type).contains(p_name) &&
            style_map.at(p_theme_type).at(p_name)) {
        return style_map.at(p_theme_type).at(p_name);
    }
    return default_style;
}

bool Theme::has_stylebox(const StringName &p_name, const StringName &p_theme_type) const {
    return style_map.contains(p_theme_type) && style_map.at(p_theme_type).contains(p_name) &&
           style_map.at(p_theme_type).at(p_name);
}
bool Theme::has_stylebox_nocheck(const StringName &p_name, const StringName &p_theme_type) const {
    return (style_map.contains(p_theme_type) && style_map.at(p_theme_type).contains(p_name));
}
void Theme::rename_stylebox(const StringName &p_old_name, const StringName &p_name, const StringName &p_theme_type) {
    ERR_FAIL_COND_MSG(!style_map.contains(p_theme_type), "Cannot rename the stylebox '" + String(p_old_name) + "' because the node type '" + String(p_theme_type) + "' does not exist.");
    ERR_FAIL_COND_MSG(style_map[p_theme_type].contains(p_name), "Cannot rename the stylebox '" + String(p_old_name) + "' because the new name '" + String(p_name) + "' already exists.");
    ERR_FAIL_COND_MSG(!style_map[p_theme_type].contains(p_old_name), "Cannot rename the stylebox '" + String(p_old_name) + "' because it does not exist.");

    style_map[p_theme_type][p_name] = style_map[p_theme_type][p_old_name];
    style_map[p_theme_type].erase(p_old_name);

    _emit_theme_changed(true);
}

void Theme::clear_stylebox(const StringName &p_name, const StringName &p_theme_type) {
    ERR_FAIL_COND_MSG(!style_map.contains(p_theme_type), "Cannot clear the stylebox '" + String(p_name) + "' because the node type '" + String(p_theme_type) + "' does not exist.");
    ERR_FAIL_COND_MSG(!style_map[p_theme_type].contains(p_name), "Cannot clear the stylebox '" + String(p_name) + "' because it does not exist.");


    if (style_map[p_theme_type][p_name]) {
        style_map[p_theme_type][p_name]->disconnect("changed", cb_theme_changed);
    }

    style_map[p_theme_type].erase(p_name);

    _emit_theme_changed(true);
}

Vector<StringName> Theme::get_stylebox_list(const StringName &p_theme_type) const {
    if (!style_map.contains(p_theme_type)) {
        return {};
    }
    Vector<StringName> res;
    const HashMap<StringName, Ref<StyleBox>> &smap(style_map.at(p_theme_type));
    res.reserve(smap.size());
    for (const auto &v : smap) {
        res.emplace_back(v.first);
    }
    return res;
}

void Theme::add_stylebox_type(const StringName &p_theme_type) {
    if (style_map.contains(p_theme_type)) {
        return;
    }
    style_map[p_theme_type] = HashMap<StringName, Ref<StyleBox>>();
}

void Theme::remove_stylebox_type(const StringName &p_theme_type) {
    if (!style_map.contains(p_theme_type)) {
        return;
    }

    _freeze_change_propagation();

    for(const auto & e : style_map[p_theme_type]) {
        if (e.second) {
            e.second->disconnect("changed", callable_mp(this, &Theme::_emit_theme_changed));
        }
    }

    style_map.erase(p_theme_type);

    _unfreeze_and_propagate_changes();
}

Vector<StringName> Theme::get_stylebox_types() const {
    Vector<StringName> res;
    style_map.keys_into(res);
    return res;
}

// Fonts.
void Theme::set_font(const StringName &p_name, const StringName &p_theme_type, const Ref<Font> &p_font) {
    // ERR_FAIL_COND();

    bool existing = false;

    if (font_map[p_theme_type][p_name]) {
        existing = true;
        font_map[p_theme_type][p_name]->disconnect("changed", cb_theme_changed);
    }

    font_map[p_theme_type][p_name] = p_font;

    if (p_font) {
        font_map[p_theme_type][p_name]->connect(
                "changed", cb_theme_changed, ObjectNS::CONNECT_REFERENCE_COUNTED);
    }

    _emit_theme_changed(!existing);
}

Ref<Font> Theme::get_font(const StringName &p_name, const StringName &p_theme_type) const {
    const Ref<Font> dummy;
    if (font_map.contains(p_theme_type) && font_map.at(p_theme_type).at(p_name, dummy)) {
        return font_map.at(p_theme_type).at(p_name);
    }
    if (has_default_theme_font()) {
        return default_theme_font;
    }
    return default_font;
}

bool Theme::has_font(const StringName &p_name, const StringName &p_theme_type) const {
    return ((font_map.contains(p_theme_type) && font_map.at(p_theme_type).contains(p_name) &&
                    font_map.at(p_theme_type).at(p_name)) ||
            has_default_theme_font());
}
bool Theme::has_font_nocheck(const StringName &p_name, const StringName &p_theme_type) const {
    return (font_map.contains(p_theme_type) && font_map.at(p_theme_type).contains(p_name));
}

void Theme::rename_font(const StringName &p_old_name, const StringName &p_name, const StringName &p_theme_type) {
    ERR_FAIL_COND_MSG(!font_map.contains(p_theme_type), "Cannot rename the font '" + String(p_old_name) + "' because the node type '" + String(p_theme_type) + "' does not exist.");
    ERR_FAIL_COND_MSG(font_map[p_theme_type].contains(p_name), "Cannot rename the font '" + String(p_old_name) + "' because the new name '" + String(p_name) + "' already exists.");
    ERR_FAIL_COND_MSG(!font_map[p_theme_type].contains(p_old_name), "Cannot rename the font '" + String(p_old_name) + "' because it does not exist.");

    font_map[p_theme_type][p_name] = font_map[p_theme_type][p_old_name];
    font_map[p_theme_type].erase(p_old_name);

    _emit_theme_changed(true);
}
void Theme::clear_font(const StringName &p_name, const StringName &p_theme_type) {
    ERR_FAIL_COND_MSG(!font_map.contains(p_theme_type), "Cannot clear the font '" + String(p_name) + "' because the node type '" + String(p_theme_type) + "' does not exist.");
    ERR_FAIL_COND_MSG(!font_map[p_theme_type].contains(p_name), "Cannot clear the font '" + String(p_name) + "' because it does not exist.");

    if (font_map[p_theme_type][p_name]) {
        font_map[p_theme_type][p_name]->disconnect("changed", cb_theme_changed);
    }

    font_map[p_theme_type].erase(p_name);
    _emit_theme_changed(true);
}


void Theme::get_font_list(const StringName &p_theme_type, Vector<StringName> *p_list) const {
    if (!font_map.contains(p_theme_type)) {
        return;
    }
    font_map.at(p_theme_type).keys_into(*p_list);
}
void Theme::add_font_type(const StringName &p_theme_type) {
    if (font_map.contains(p_theme_type)) {
        return;
    }
    font_map[p_theme_type] = HashMap<StringName, Ref<Font>>();
}

void Theme::remove_font_type(const StringName &p_theme_type) {
    if (!font_map.contains(p_theme_type)) {
        return;
    }

    _freeze_change_propagation();

    for (auto & e : font_map[p_theme_type]) {
        if (e.second) {
            e.second->disconnect("changed", callable_mp(this, &Theme::_emit_theme_changed));
        }
    }

    font_map.erase(p_theme_type);

    _unfreeze_and_propagate_changes();
}
void Theme::get_font_types(Vector<StringName> *p_list) const {
    ERR_FAIL_NULL(p_list);
    font_map.keys_into(*p_list);
}

void Theme::set_colors(Span<const Theme::ThemeColor> colors) {
    bool need_notify = false;
    for (const ThemeColor &v : colors) {
        auto iter = color_map.find_as(v.type);
        if (iter == color_map.end()) {
            need_notify = true;
            iter = color_map.emplace(eastl::make_pair(StaticCString(v.type, true), HashMap<StringName, Color>())).first;
        }
        auto n_iter = iter->second.find_as(v.name);
        if (n_iter == iter->second.end()) {
            need_notify = true;
            n_iter = iter->second.emplace(eastl::make_pair(StaticCString(v.name, true), Color())).first;
        }
        need_notify |= n_iter->second != v.color;
        n_iter->second = v.color;
    }
    if (need_notify) {
        Object_change_notify(this);
        emit_changed();
    }
}

// Colors.
void Theme::set_color(const StringName &p_name, const StringName &p_theme_type, const Color &p_color) {
    bool existing = has_color_nocheck(p_name, p_theme_type);
    color_map[p_theme_type][p_name] = p_color;

    _emit_theme_changed(!existing);
}

Color Theme::get_color(const StringName &p_name, const StringName &p_theme_type) const {
    if (color_map.contains(p_theme_type) && color_map.at(p_theme_type).contains(p_name)) {
        return color_map.at(p_theme_type).at(p_name);
    } else {
        return Color();
    }
}

bool Theme::has_color(const StringName &p_name, const StringName &p_theme_type) const {
    return (color_map.contains(p_theme_type) && color_map.at(p_theme_type).contains(p_name));
}

bool Theme::has_color_nocheck(const StringName &p_name, const StringName &p_theme_type) const {
    return (color_map.contains(p_theme_type) && color_map.at(p_theme_type).contains(p_name));
}

void Theme::rename_color(const StringName &p_old_name, const StringName &p_name, const StringName &p_theme_type) {
    ERR_FAIL_COND_MSG(!color_map.contains(p_theme_type), "Cannot rename the color '" + String(p_old_name) + "' because the node type '" + String(p_theme_type) + "' does not exist.");
    ERR_FAIL_COND_MSG(color_map[p_theme_type].contains(p_name), "Cannot rename the color '" + String(p_old_name) + "' because the new name '" + String(p_name) + "' already exists.");
    ERR_FAIL_COND_MSG(!color_map[p_theme_type].contains(p_old_name), "Cannot rename the color '" + String(p_old_name) + "' because it does not exist.");

    color_map[p_theme_type][p_name] = color_map[p_theme_type][p_old_name];
    color_map[p_theme_type].erase(p_old_name);

    _emit_theme_changed(true);
}

void Theme::clear_color(const StringName &p_name, const StringName &p_theme_type) {
    ERR_FAIL_COND_MSG(!color_map.contains(p_theme_type), "Cannot clear the color '" + String(p_name) + "' because the node type '" + String(p_theme_type) + "' does not exist.");
    ERR_FAIL_COND_MSG(!color_map[p_theme_type].contains(p_name), "Cannot clear the color '" + String(p_name) + "' because it does not exist.");

    color_map[p_theme_type].erase(p_name);
    _emit_theme_changed(true);
}


void Theme::get_color_list(const StringName &p_theme_type, Vector<StringName> *p_list) const {
    if (!color_map.contains(p_theme_type)) {
        return;
    }

    for (const auto &c : color_map.at(p_theme_type)) {
        p_list->push_back(c.first);
    }
}
void Theme::add_color_type(const StringName &p_theme_type) {
    if (color_map.contains(p_theme_type)) {
        return;
    }
    color_map[p_theme_type] = HashMap<StringName, Color>();
}

void Theme::remove_color_type(const StringName &p_theme_type) {
    if (!color_map.contains(p_theme_type)) {
        return;
    }

    color_map.erase(p_theme_type);
}

void Theme::get_color_types(Vector<StringName> *p_list) const {
    ERR_FAIL_NULL(p_list);
    color_map.keys_into(*p_list);
}

void Theme::set_constants(Span<const ThemeConstant> vals) {
    bool need_notify = false;
    for (const ThemeConstant &v : vals) {
        auto iter = constant_map.find_as(v.type);
        if (iter == constant_map.end()) {
            need_notify = true;
            iter = constant_map.emplace(eastl::make_pair(StaticCString(v.type, true), HashMap<StringName, int>()))
                           .first;
        }
        auto n_iter = iter->second.find_as(v.name);
        if (n_iter == iter->second.end()) {
            need_notify = true;
            n_iter = iter->second.emplace(eastl::make_pair(StaticCString(v.name, true), 0)).first;
        }
        need_notify |= n_iter->second != v.value;
        n_iter->second = v.value;
    }
    if (need_notify) {
        Object_change_notify(this);
        emit_changed();
    }
}

// Theme constants.
void Theme::set_constant(const StringName &p_name, const StringName &p_theme_type, int p_constant) {
    bool existing = has_constant_nocheck(p_name, p_theme_type);
    constant_map[p_theme_type][p_name] = p_constant;

    _emit_theme_changed(!existing);
}

int Theme::get_constant(const StringName &p_name, const StringName &p_theme_type) const {
    if (constant_map.contains(p_theme_type) && constant_map.at(p_theme_type).contains(p_name)) {
        return constant_map.at(p_theme_type).at(p_name);
    } else {
        return 0;
    }
}

bool Theme::has_constant(const StringName &p_name, const StringName &p_theme_type) const {
    return (constant_map.contains(p_theme_type) && constant_map.at(p_theme_type).contains(p_name));
}

bool Theme::has_constant_nocheck(const StringName &p_name, const StringName &p_theme_type) const {
    return (constant_map.contains(p_theme_type) && constant_map.at(p_theme_type).contains(p_name));
}
void Theme::rename_constant(const StringName &p_old_name, const StringName &p_name, const StringName &p_theme_type) {
    ERR_FAIL_COND_MSG(!constant_map.contains(p_theme_type), "Cannot rename the constant '" + String(p_old_name) + "' because the node type '" + String(p_theme_type) + "' does not exist.");
    ERR_FAIL_COND_MSG(constant_map[p_theme_type].contains(p_name), "Cannot rename the constant '" + String(p_old_name) + "' because the new name '" + String(p_name) + "' already exists.");
    ERR_FAIL_COND_MSG(!constant_map[p_theme_type].contains(p_old_name), "Cannot rename the constant '" + String(p_old_name) + "' because it does not exist.");

    constant_map[p_theme_type][p_name] = constant_map[p_theme_type][p_old_name];
    constant_map[p_theme_type].erase(p_old_name);

    _emit_theme_changed(true);
}

void Theme::clear_constant(const StringName &p_name, const StringName &p_theme_type) {
    ERR_FAIL_COND_MSG(!constant_map.contains(p_theme_type), "Cannot clear the constant '" + String(p_name) + "' because the node type '" + String(p_theme_type) + "' does not exist.");
    ERR_FAIL_COND_MSG(!constant_map[p_theme_type].contains(p_name), "Cannot clear the constant '" + String(p_name) + "' because it does not exist.");

    constant_map[p_theme_type].erase(p_name);

    _emit_theme_changed(true);
}
void Theme::get_constant_list(const StringName &p_theme_type, Vector<StringName> *p_list) const {
    if (!constant_map.contains(p_theme_type)) {
        return;
    }

    for (const auto &v : constant_map.at(p_theme_type)) {
        p_list->push_back(v.first);
    }
}
void Theme::add_constant_type(const StringName &p_theme_type) {
    if (constant_map.contains(p_theme_type)) {
        return;
    }
    constant_map[p_theme_type] = HashMap<StringName, int>();
}

void Theme::remove_constant_type(const StringName &p_theme_type) {
    if (!constant_map.contains(p_theme_type)) {
        return;
    }

    constant_map.erase(p_theme_type);
}
void Theme::get_constant_types(Vector<StringName> *p_list) const {
    ERR_FAIL_NULL(p_list);
    constant_map.keys_into(*p_list);
}

// Generic methods for managing theme items.
void Theme::set_theme_item(DataType p_data_type, const StringName &p_name, const StringName &p_theme_type, const Variant &p_value) {
    switch (p_data_type) {
        case DATA_TYPE_COLOR: {
            ERR_FAIL_COND_MSG(p_value.get_type() != VariantType::COLOR, String("Theme item's data type (Color) does not match Variant's type (") + Variant::get_type_name(p_value.get_type()) + ").");
            set_color(p_name, p_theme_type, p_value.as<Color>());
        } break;
        case DATA_TYPE_CONSTANT: {
            ERR_FAIL_COND_MSG(p_value.get_type() != VariantType::INT, String("Theme item's data type (int) does not match Variant's type (") + Variant::get_type_name(p_value.get_type()) + ").");
            set_constant(p_name, p_theme_type, p_value.as<int>());
        } break;
        case DATA_TYPE_FONT: {
            ERR_FAIL_COND_MSG(p_value.get_type() != VariantType::OBJECT, String("Theme item's data type (Object) does not match Variant's type (") + Variant::get_type_name(p_value.get_type()) + ").");

            Ref<Font> font_value(p_value.asT<Font>());
            set_font(p_name, p_theme_type, font_value);
        } break;
        case DATA_TYPE_ICON: {
            ERR_FAIL_COND_MSG(p_value.get_type() != VariantType::OBJECT, String("Theme item's data type (Object) does not match Variant's type (") + Variant::get_type_name(p_value.get_type()) + ").");

            Ref<Texture> icon_value(p_value.asT<Texture>());
            set_icon(p_name, p_theme_type, icon_value);
        } break;
        case DATA_TYPE_STYLEBOX: {
            ERR_FAIL_COND_MSG(p_value.get_type() != VariantType::OBJECT, String("Theme item's data type (Object) does not match Variant's type (") + Variant::get_type_name(p_value.get_type()) + ").");

            Ref<StyleBox> stylebox_value(p_value.asT<StyleBox>());
            set_stylebox(p_name, p_theme_type, stylebox_value);
        } break;
        case DATA_TYPE_MAX:
            break; // Can't happen, but silences warning.
    }
}

Variant Theme::get_theme_item(DataType p_data_type, const StringName &p_name, const StringName &p_theme_type) const {
    switch (p_data_type) {
        case DATA_TYPE_COLOR:
            return get_color(p_name, p_theme_type);
        case DATA_TYPE_CONSTANT:
            return get_constant(p_name, p_theme_type);
        case DATA_TYPE_FONT:
            return get_font(p_name, p_theme_type);
        case DATA_TYPE_ICON:
            return get_icon(p_name, p_theme_type);
        case DATA_TYPE_STYLEBOX:
            return get_stylebox(p_name, p_theme_type);
        case DATA_TYPE_MAX:
            break; // Can't happen, but silences warning.
    }

    return Variant();
}

bool Theme::has_theme_item(DataType p_data_type, const StringName &p_name, const StringName &p_theme_type) const {
    switch (p_data_type) {
        case DATA_TYPE_COLOR:
            return has_color(p_name, p_theme_type);
        case DATA_TYPE_CONSTANT:
            return has_constant(p_name, p_theme_type);
        case DATA_TYPE_FONT:
            return has_font(p_name, p_theme_type);
        case DATA_TYPE_ICON:
            return has_icon(p_name, p_theme_type);
        case DATA_TYPE_STYLEBOX:
            return has_stylebox(p_name, p_theme_type);
        case DATA_TYPE_MAX:
            break; // Can't happen, but silences warning.
    }

    return false;
}

bool Theme::has_theme_item_nocheck(DataType p_data_type, const StringName &p_name, const StringName &p_theme_type) const {
    switch (p_data_type) {
        case DATA_TYPE_COLOR:
            return has_color_nocheck(p_name, p_theme_type);
        case DATA_TYPE_CONSTANT:
            return has_constant_nocheck(p_name, p_theme_type);
        case DATA_TYPE_FONT:
            return has_font_nocheck(p_name, p_theme_type);
        case DATA_TYPE_ICON:
            return has_icon_nocheck(p_name, p_theme_type);
        case DATA_TYPE_STYLEBOX:
            return has_stylebox_nocheck(p_name, p_theme_type);
        case DATA_TYPE_MAX:
            break; // Can't happen, but silences warning.
    }

    return false;
}
void Theme::rename_theme_item(DataType p_data_type, const StringName &p_old_name, const StringName &p_name, const StringName &p_theme_type) {
    switch (p_data_type) {
        case DATA_TYPE_COLOR:
            rename_color(p_old_name, p_name, p_theme_type);
            break;
        case DATA_TYPE_CONSTANT:
            rename_constant(p_old_name, p_name, p_theme_type);
            break;
        case DATA_TYPE_FONT:
            rename_font(p_old_name, p_name, p_theme_type);
            break;
        case DATA_TYPE_ICON:
            rename_icon(p_old_name, p_name, p_theme_type);
            break;
        case DATA_TYPE_STYLEBOX:
            rename_stylebox(p_old_name, p_name, p_theme_type);
            break;
        case DATA_TYPE_MAX:
            break; // Can't happen, but silences warning.
    }
}

void Theme::clear_theme_item(DataType p_data_type, const StringName &p_name, const StringName &p_theme_type) {
    switch (p_data_type) {
        case DATA_TYPE_COLOR:
            clear_color(p_name, p_theme_type);
            break;
        case DATA_TYPE_CONSTANT:
            clear_constant(p_name, p_theme_type);
            break;
        case DATA_TYPE_FONT:
            clear_font(p_name, p_theme_type);
            break;
        case DATA_TYPE_ICON:
            clear_icon(p_name, p_theme_type);
            break;
        case DATA_TYPE_STYLEBOX:
            clear_stylebox(p_name, p_theme_type);
            break;
        case DATA_TYPE_MAX:
            break; // Can't happen, but silences warning.
    }
}

void Theme::get_theme_item_list(DataType p_data_type, StringName p_theme_type, Vector<StringName> &p_list) const {
    switch (p_data_type) {
        case DATA_TYPE_COLOR:
            get_color_list(p_theme_type, &p_list);
            break;
        case DATA_TYPE_CONSTANT:
            get_constant_list(p_theme_type, &p_list);
            break;
        case DATA_TYPE_FONT:
            get_font_list(p_theme_type, &p_list);
            break;
        case DATA_TYPE_ICON:
            get_icon_list(p_theme_type, p_list);
            break;
        case DATA_TYPE_STYLEBOX:
            p_list = get_stylebox_list(p_theme_type);
            break;
        case DATA_TYPE_MAX:
            break; // Can't happen, but silences warning.
    }
}

void Theme::add_theme_item_type(DataType p_data_type, const StringName &p_theme_type) {
    switch (p_data_type) {
        case DATA_TYPE_COLOR:
            add_color_type(p_theme_type);
            break;
        case DATA_TYPE_CONSTANT:
            add_constant_type(p_theme_type);
            break;
        case DATA_TYPE_FONT:
            add_font_type(p_theme_type);
            break;
        case DATA_TYPE_ICON:
            add_icon_type(p_theme_type);
            break;
        case DATA_TYPE_STYLEBOX:
            add_stylebox_type(p_theme_type);
            break;
        case DATA_TYPE_MAX:
            break; // Can't happen, but silences warning.
    }
}

void Theme::get_theme_item_types(DataType p_data_type, Vector<StringName> &p_list) const {
    switch (p_data_type) {
        case DATA_TYPE_COLOR:
            get_color_types(&p_list);
            break;
        case DATA_TYPE_CONSTANT:
            get_constant_types(&p_list);
            break;
        case DATA_TYPE_FONT:
            get_font_types(&p_list);
            break;
        case DATA_TYPE_ICON:
            get_icon_types(p_list);
            break;
        case DATA_TYPE_STYLEBOX:
            p_list = get_stylebox_types();
            break;
        case DATA_TYPE_MAX:
            break; // Can't happen, but silences warning.
    }
}
// Theme type variations.
void Theme::set_type_variation(const StringName &p_theme_type, const StringName &p_base_type) {
    ERR_FAIL_COND_MSG(
            p_theme_type == StringName(), "An empty theme type cannot be marked as a variation of another type.");
    ERR_FAIL_COND_MSG(ClassDB::class_exists(p_theme_type),
            "A type associated with a built-in class cannot be marked as a variation of another type.");
    ERR_FAIL_COND_MSG(p_base_type == StringName(), "An empty theme type cannot be the base type of a variation. Use "
                                                   "clear_type_variation() instead if you want to unmark '" +
                                                           String(p_theme_type) + "' as a variation.");

    if (variation_map.contains(p_theme_type)) {
        StringName old_base = variation_map[p_theme_type];
        variation_base_map[old_base].erase_first(p_theme_type);
    }

    variation_map[p_theme_type] = p_base_type;
    variation_base_map[p_base_type].push_back(p_theme_type);

    _emit_theme_changed(true);
}
bool Theme::is_type_variation(const StringName &p_theme_type, const StringName &p_base_type) const {
    return (variation_map.contains(p_theme_type) && variation_map.at(p_theme_type,StringName()) == p_base_type);
}

void Theme::clear_type_variation(const StringName &p_theme_type) {
    ERR_FAIL_COND_MSG(!variation_map.contains(p_theme_type),
            "Cannot clear the type variation '" + String(p_theme_type) + "' because it does not exist.");

    StringName base_type = variation_map[p_theme_type];
    variation_base_map[base_type].erase_first(p_theme_type);
    variation_map.erase(p_theme_type);

    _emit_theme_changed(true);
}

StringName Theme::get_type_variation_base(const StringName &p_theme_type) const {
    if (!variation_map.contains(p_theme_type)) {
        return StringName();
    }

    return variation_map.at(p_theme_type,StringName());
}
void Theme::get_type_variation_list(const StringName &p_base_type, Vector<StringName> *p_list) const {
    ERR_FAIL_NULL(p_list);

    if (!variation_base_map.contains(p_base_type)) {
        return;
    }

    for (const StringName &E : variation_base_map.at(p_base_type,{})) {
        // Prevent infinite loops if variants were set to be cross-dependent (that's still invalid usage, but handling
        // for stability sake).
        if (p_list->contains(E)) {
            continue;
        }

        p_list->emplace_back(E);
        // Continue looking for sub-variations.
        get_type_variation_list(E, p_list);
    }
}

// Theme types.
void Theme::add_type(const StringName &p_theme_type) {
    // Add a record to every data type map.
    for (int i = 0; i < Theme::DATA_TYPE_MAX; i++) {
        Theme::DataType dt = (Theme::DataType)i;
        add_theme_item_type(dt, p_theme_type);
    }

    _emit_theme_changed(true);
}

void Theme::remove_type(const StringName &p_theme_type)
{
    // Gracefully remove the record from every data type map.
    for (int i = 0; i < Theme::DATA_TYPE_MAX; i++) {
        Theme::DataType dt = (Theme::DataType)i;
        remove_theme_item_type(dt, p_theme_type);
    }

         // If type is a variation, remove that connection.
    if (get_type_variation_base(p_theme_type) != StringName()) {
        clear_type_variation(p_theme_type);
    }

         // If type is a variation base, remove all those connections.
    Vector<StringName> names;
    get_type_variation_list(p_theme_type, &names);
    for (const StringName &E : names) {
        clear_type_variation(E);
    }

    _emit_theme_changed(true);
}
void Theme::get_type_list(Vector<StringName> *p_list) const {
    ERR_FAIL_NULL(p_list);

    Set<StringName> types;
    icon_map.keys_into_set(types);
    style_map.keys_into_set(types);
    font_map.keys_into_set(types);
    color_map.keys_into_set(types);
    constant_map.keys_into_set(types);
    p_list->assign(types.begin(),types.end());
}
void Theme::get_type_dependencies(const StringName &p_base_type, const StringName &p_type_variation, Vector<StringName> *p_list) {
    ERR_FAIL_NULL(p_list);

    // Build the dependency chain for type variations.
    if (!p_type_variation.empty()) {
        StringName variation_name = p_type_variation;
        while (!variation_name.empty()) {
            p_list->push_back(variation_name);
            variation_name = get_type_variation_base(variation_name);

                 // If we have reached the base type dependency, it's safe to stop (assuming no funny business was done to the Theme).
            if (variation_name == p_base_type) {
                break;
            }
        }
    }

         // Continue building the chain using native class hierarchy.
    StringName class_name = p_base_type;
    while (!class_name.empty()) {
        p_list->push_back(class_name);
        class_name = ClassDB::get_parent_class_nocheck(class_name);
    }
}
// Internal methods for getting lists as a Vector of String (compatible with public API).
PoolVector<String> Theme::_get_icon_list(const StringName &p_theme_type) const {
    PoolVector<String> ilret;
    Vector<StringName> il;
    get_icon_list(StringName(p_theme_type), il);
    for (const StringName &E : il) {
        ilret.push_back(String(E));
    }
    return ilret;
}

PoolVector<String> Theme::_get_icon_types() const {
    PoolVector<String> ilret;
    Vector<StringName> il;

    get_icon_types(il);
    for (const StringName &E : il) {
        ilret.push_back(String(E));
    }
    return ilret;
}

PoolVector<String> Theme::_get_stylebox_list(const StringName &p_theme_type) const {
    PoolVector<String> ilret;
    Vector<StringName> il = get_stylebox_list(StringName(p_theme_type));
    for (const StringName &E : il) {
        ilret.push_back(String(E));
    }
    return ilret;
}

PoolVector<String> Theme::_get_stylebox_types() const {
    PoolVector<String> ilret;
    Vector<StringName> il=get_stylebox_types();
    for (const StringName &E : il) {
        ilret.push_back(String(E));
    }
    return ilret;
}

PoolVector<String> Theme::_get_font_list(const StringName &p_theme_type) const {
    PoolVector<String> ilret;
    Vector<StringName> il;
    get_font_list(p_theme_type, &il);
    for (const StringName &E : il) {
        ilret.push_back(String(E));
    }
    return ilret;
}

PoolVector<String> Theme::_get_font_types() const {
    PoolVector<String> ilret;
    Vector<StringName> il;

    get_font_types(&il);
    for (const StringName &E : il) {
        ilret.push_back(String(E));
    }
    return ilret;
}

PoolVector<String> Theme::_get_color_list(const StringName &p_theme_type) const {
    PoolVector<String> ilret;
    Vector<StringName> il;
    get_color_list(p_theme_type, &il);
    for (const StringName &E : il) {
        ilret.push_back(String(E));
    }
    return ilret;
}

PoolVector<String> Theme::_get_color_types() const {
    PoolVector<String> ilret;
    Vector<StringName> il;

    get_color_types(&il);
    ilret.resize(il.size());
    for (const StringName &E : il) {
        ilret.push_back(String(E));
    }
    return ilret;
}

PoolVector<String> Theme::_get_constant_list(const StringName &p_theme_type) const {
    PoolVector<String> ilret;
    Vector<StringName> il;
    get_constant_list(p_theme_type, &il);
    for (const StringName &E : il) {
        ilret.push_back(String(E));
    }
    return ilret;
}

PoolVector<String> Theme::_get_constant_types() const {
    PoolVector<String> ilret;
    Vector<StringName> il;

    get_constant_types(&il);
    for (const StringName &E : il) {
        ilret.push_back(String(E));
    }
    return ilret;
}

PoolVector<String> Theme::_get_theme_item_list(DataType p_data_type, const StringName &p_theme_type) const {
    switch (p_data_type) {
        case DATA_TYPE_COLOR:
            return _get_color_list(p_theme_type);
        case DATA_TYPE_CONSTANT:
            return _get_constant_list(p_theme_type);
        case DATA_TYPE_FONT:
            return _get_font_list(p_theme_type);
        case DATA_TYPE_ICON:
            return _get_icon_list(p_theme_type);
        case DATA_TYPE_STYLEBOX:
            return _get_stylebox_list(p_theme_type);
        case DATA_TYPE_MAX:
            break; // Can't happen, but silences warning.
    }

    return PoolVector<String>();
}

void Theme::remove_theme_item_type(DataType p_data_type, const StringName &p_theme_type) {
    switch (p_data_type) {
        case DATA_TYPE_COLOR:
            remove_color_type(p_theme_type);
            break;
        case DATA_TYPE_CONSTANT:
            remove_constant_type(p_theme_type);
            break;
        case DATA_TYPE_FONT:
            remove_font_type(p_theme_type);
            break;
        case DATA_TYPE_ICON:
            remove_icon_type(p_theme_type);
            break;
        case DATA_TYPE_STYLEBOX:
            remove_stylebox_type(p_theme_type);
            break;
        case DATA_TYPE_MAX:
            break; // Can't happen, but silences warning.
    }
}

PoolVector<String> Theme::_get_theme_item_types(DataType p_data_type) const {
    switch (p_data_type) {
        case DATA_TYPE_COLOR:
            return _get_color_types();
        case DATA_TYPE_CONSTANT:
            return _get_constant_types();
        case DATA_TYPE_FONT:
            return _get_font_types();
        case DATA_TYPE_ICON:
            return _get_icon_types();
        case DATA_TYPE_STYLEBOX:
            return _get_stylebox_types();
        case DATA_TYPE_MAX:
            break; // Can't happen, but silences warning.
    }

    return PoolVector<String>();
}

//Vector<String> Theme::_get_type_variation_list(const StringName &p_theme_type) const {
//    Vector<String> ilret;
//    Vector<String> il;

//    get_type_variation_list(p_theme_type, &ilret);
//    ilret.resize(il.size());

//    int i = 0;
//    String *w = ilret.ptrw();
//    for (List<StringName>::Element *E = il.front(); E; E = E->next(), i++) {
//        w[i] = E->get();
//    }
//    return ilret;
//}
PoolVector<String> Theme::_get_type_list(StringView p_theme_type) const {
    PoolVector<String> ilret;
    Vector<StringName> il;
    get_type_list(&il);
    for (const StringName &E : il) {
        ilret.push_back(String(E));
    }
    return ilret;
}

// Theme bulk manipulations.
void Theme::_emit_theme_changed(bool p_notify_list_changed) {
    if (no_change_propagation) {
        return;
    }

    if (p_notify_list_changed) {
        Object_change_notify(this);
    }
    emit_changed();
}

void Theme::_freeze_change_propagation() {
    no_change_propagation = true;
}

void Theme::_unfreeze_and_propagate_changes() {
    no_change_propagation = false;
    _emit_theme_changed(true);
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

    _freeze_change_propagation();

    // These items need reconnecting, so add them normally.
    for (auto &kv : icon_map) {
        for (auto &L : kv.second) {
            set_icon(L.first, kv.first, L.second);
        }
    }

    for (const auto &e : style_map) {
        for (const auto &f : e.second) {
            set_stylebox(f.first, e.first, f.second);
        }
    }

    for (const auto &e : font_map) {
        for (const auto &f : e.second) {
            set_font(f.first, e.first, f.second);
        }
    }

    // These items can be simply copied.
    color_map = p_other->color_map;
    constant_map = p_other->constant_map;
    shader_map = p_other->shader_map;

    _unfreeze_and_propagate_changes();
}

void Theme::merge_with(const Ref<Theme> &p_other) {
    if (!p_other) {
        return;
    }

    _freeze_change_propagation();

    // Colors.
    {
        for(const auto & K : p_other->color_map) {
            for(const auto & L : K.second) {
                set_color(K.first, L.first, L.second);
            }
        }
    }

    // Constants.
    {
        for(const auto & K : p_other->constant_map) {
            for(const auto & L : K.second) {
                set_constant(K.first, L.first, L.second);
            }
        }
    }

    // Fonts.
    {
        for(const auto & K : p_other->font_map) {
            for(const auto & L : K.second) {
                set_font(K.first, L.first, L.second);
            }
        }
    }

    // Icons.
    {
        for(const auto & K : p_other->icon_map) {
            for(const auto & L : K.second) {
                set_icon(K.first, L.first, L.second);
            }
        }
    }

    // Shaders.
    {
        for(const auto & K : p_other->shader_map) {
            for(const auto & L : K.second) {
                set_shader(K.first, L.first, L.second);
            }
        }
    }

    // Styleboxes.
    {
        for(const auto & K : p_other->style_map) {
            for(const auto & L : K.second) {
                set_stylebox(K.first, L.first, L.second);
            }
        }
    }
    // Type variations.
    {
        for(const auto & K : p_other->variation_map) {
            set_type_variation(K.first, p_other->variation_map[K.first]);
        }
    }

    _unfreeze_and_propagate_changes();
}
void Theme::clear() {
    // these need disconnecting
    for (auto &kv : icon_map) {
        for (auto &L : kv.second) {
            if (L.second) {
                L.second->disconnect("changed", cb_theme_changed);
            }
        }
    }

    for (const auto &e : style_map) {
        for (const auto &f : e.second) {
            auto style(f.second);
            if (style) {
                style->disconnect("changed", cb_theme_changed);
            }
        }
    }

    for (const auto &e : font_map) {
        for (const auto &f : e.second) {
            auto font(f.second);
            if (font) {
                font->disconnect("changed", cb_theme_changed);
            }
        }
    }

    icon_map.clear();
    style_map.clear();
    font_map.clear();
    shader_map.clear();
    color_map.clear();
    constant_map.clear();
    variation_map.clear();
    variation_base_map.clear();

    _emit_theme_changed(true);
}

void Theme::_bind_methods() {
    SE_BIND_METHOD(Theme,set_icon);
    SE_BIND_METHOD(Theme,get_icon);
    SE_BIND_METHOD(Theme,has_icon);
    SE_BIND_METHOD(Theme,rename_icon);
    SE_BIND_METHOD(Theme,clear_icon);
    MethodBinder::bind_method(D_METHOD("get_icon_list", { "node_type" }), &Theme::_get_icon_list);
    MethodBinder::bind_method(D_METHOD("get_icon_types"), &Theme::_get_icon_types);

    SE_BIND_METHOD(Theme,set_stylebox);
    SE_BIND_METHOD(Theme,get_stylebox);
    SE_BIND_METHOD(Theme,has_stylebox);
    SE_BIND_METHOD(Theme,rename_stylebox);
    SE_BIND_METHOD(Theme,clear_stylebox);
    MethodBinder::bind_method(D_METHOD("get_stylebox_list", { "node_type" }), &Theme::_get_stylebox_list);
    SE_BIND_METHOD(Theme,get_stylebox_types);

    SE_BIND_METHOD(Theme,set_font);
    SE_BIND_METHOD(Theme,get_font);
    SE_BIND_METHOD(Theme,has_font);
    SE_BIND_METHOD(Theme,rename_font);
    SE_BIND_METHOD(Theme,clear_font);
    MethodBinder::bind_method(D_METHOD("get_font_list", { "node_type" }), &Theme::_get_font_list);
    MethodBinder::bind_method(D_METHOD("get_font_types"), &Theme::_get_font_types);

    SE_BIND_METHOD(Theme,set_color);
    SE_BIND_METHOD(Theme,get_color);
    SE_BIND_METHOD(Theme,has_color);
    SE_BIND_METHOD(Theme,rename_color);
    SE_BIND_METHOD(Theme,clear_color);
    MethodBinder::bind_method(D_METHOD("get_color_list", { "node_type" }), &Theme::_get_color_list);
    MethodBinder::bind_method(D_METHOD("get_color_types"), &Theme::_get_color_types);

    SE_BIND_METHOD(Theme,set_constant);
    SE_BIND_METHOD(Theme,get_constant);
    SE_BIND_METHOD(Theme,has_constant);
    SE_BIND_METHOD(Theme,rename_constant);
    SE_BIND_METHOD(Theme,clear_constant);
    MethodBinder::bind_method(D_METHOD("get_constant_list", { "node_type" }), &Theme::_get_constant_list);
    MethodBinder::bind_method(D_METHOD("get_constant_types"), &Theme::_get_constant_types);

    MethodBinder::bind_method(D_METHOD("set_default_font", { "font" }), &Theme::set_default_theme_font);
    MethodBinder::bind_method(D_METHOD("get_default_font"), &Theme::get_default_theme_font);
    MethodBinder::bind_method(D_METHOD("has_default_font"), &Theme::has_default_theme_font);

    SE_BIND_METHOD(Theme,set_theme_item);
    SE_BIND_METHOD(Theme,get_theme_item);
    SE_BIND_METHOD(Theme,has_theme_item);
    SE_BIND_METHOD(Theme,rename_theme_item);
    SE_BIND_METHOD(Theme,clear_theme_item);
    MethodBinder::bind_method(D_METHOD("get_theme_item_list", {"data_type", "node_type"}), &Theme::_get_theme_item_list);
    MethodBinder::bind_method(D_METHOD("get_theme_item_types", {"data_type"}), &Theme::_get_theme_item_types);

    SE_BIND_METHOD(Theme,set_type_variation);
    SE_BIND_METHOD(Theme,is_type_variation);
    SE_BIND_METHOD(Theme,clear_type_variation);
    SE_BIND_METHOD(Theme,get_type_variation_base);
    //MethodBinder::bind_method(D_METHOD("get_type_variation_list", {"base_type"}), &Theme::_get_type_variation_list);

    SE_BIND_METHOD(Theme,add_type);
    SE_BIND_METHOD(Theme,remove_type);
    MethodBinder::bind_method(D_METHOD("get_type_list", { "node_type" }), &Theme::_get_type_list);

    MethodBinder::bind_method("copy_default_theme", &Theme::copy_default_theme);
    SE_BIND_METHOD(Theme,copy_theme);
    SE_BIND_METHOD(Theme,merge_with);
    SE_BIND_METHOD(Theme,clear);

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "default_font", PropertyHint::ResourceType, "Font"),
            "set_default_font", "get_default_font");

    BIND_ENUM_CONSTANT(DATA_TYPE_COLOR);
    BIND_ENUM_CONSTANT(DATA_TYPE_CONSTANT);
    BIND_ENUM_CONSTANT(DATA_TYPE_FONT);
    BIND_ENUM_CONSTANT(DATA_TYPE_ICON);
    BIND_ENUM_CONSTANT(DATA_TYPE_STYLEBOX);
    BIND_ENUM_CONSTANT(DATA_TYPE_MAX);
}

Theme::Theme() {
    cb_theme_changed = callable_gen(this,[this] { _emit_theme_changed(); });
}

Theme::~Theme() {}
