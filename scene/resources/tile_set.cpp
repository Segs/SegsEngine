/*************************************************************************/
/*  tile_set.cpp                                                         */
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

#include "tile_set.h"

#include "core/array.h"
#include "core/engine.h"
#include "core/math/geometry.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/script_language.h"
#include "core/string_formatter.h"

IMPL_GDCLASS(TileSet)
VARIANT_ENUM_CAST(TileSet::AutotileBindings);
VARIANT_ENUM_CAST(TileSet::BitmaskMode);
VARIANT_ENUM_CAST(TileSet::TileMode);

//Static values returned by reference from functions, hopefully compiler will put those in non-writeable memory :)
static const Vector<TileSet::ShapeData> null_shape_vec;
static const HashMap<Vector2, int> s_null_map_vec2_int;
static const HashMap<Vector2, uint32_t> s_null_map_vec2_uint;

bool TileSet::_set(const StringName &p_name, const Variant &p_value) {
    using namespace eastl;

    auto slash = StringUtils::find(p_name,"/");
    if (slash == String::npos)
        return false;

    int id = StringUtils::to_int(p_name.asCString(), slash);

    if (!tile_map.contains(id))
        create_tile(id);
    StringView what(StringUtils::substr(p_name,slash + 1));

    if (what == "name"_sv)
        tile_set_name(id, p_value.as<String>());
    else if (what == "texture"_sv)
        tile_set_texture(id, refFromVariant<Texture>(p_value));
    else if (what == "normal_map"_sv)
        tile_set_normal_map(id, refFromVariant<Texture>(p_value));
    else if (what == "tex_offset"_sv)
        tile_set_texture_offset(id, p_value.as<Vector2>());
    else if (what == "material"_sv)
        tile_set_material(id, refFromVariant<ShaderMaterial>(p_value));
    else if (what == "modulate"_sv)
        tile_set_modulate(id, p_value.as<Color>());
    else if (what == "region"_sv)
        tile_set_region(id, p_value.as<Rect2>());
    else if (what == "tile_mode"_sv)
        tile_set_tile_mode(id, p_value.as<TileMode>());
    else if (what == "is_autotile"_sv) {
        // backward compatibility for Godot 3.0.x
        // autotile used to be a bool, it's now an enum
        bool is_autotile = p_value.as<bool>();
        if (is_autotile)
            tile_set_tile_mode(id, AUTO_TILE);
    } else if (StringUtils::left(what,9) == StringView("autotile/")) {
        what = StringName(StringUtils::right(what,9));
        if (what == "bitmask_mode"_sv)
            autotile_set_bitmask_mode(id, p_value.as< BitmaskMode>());
        else if (what == "icon_coordinate"_sv)
            autotile_set_icon_coordinate(id, p_value.as<Vector2>());
        else if (what == "tile_size"_sv)
            autotile_set_size(id, p_value.as<Vector2>());
        else if (what == "spacing"_sv)
            autotile_set_spacing(id, p_value.as<int>());
        else if (what == "bitmask_flags"_sv) {
            tile_map[id].autotile_data.flags.clear();
            if (p_value.is_array()) {
                Array p = p_value.as<Array>();
                Vector2 last_coord;
                for(int i=0; i<p.size(); ++i) {
                    if (p[i].get_type() == VariantType::VECTOR2) {
                        last_coord = p[i].as<Vector2>();
                    }
                    else if (p[i].get_type() == VariantType::INT) {
                        autotile_set_bitmask(id, last_coord, p[i].as<uint32_t>());
                    }
                }
            }
        } else if (what == "occluder_map"_sv) {
            tile_map[id].autotile_data.occluder_map.clear();
            Array p = p_value.as<Array>();
            Vector2 last_coord;
            for (int i = 0; i < p.size(); ++i) {
                if (p[i].get_type() == VariantType::VECTOR2) {
                    last_coord = p[i].as<Vector2>();
                }
                else if (p[i].get_type() == VariantType::OBJECT) {
                    autotile_set_light_occluder(id, refFromVariant<OccluderPolygon2D>(p[i]), last_coord);
                }
            }
        } else if (what == "navpoly_map"_sv) {
            tile_map[id].autotile_data.navpoly_map.clear();
            Array p = p_value.as<Array>();
            Vector2 last_coord;
            for (int i = 0; i < p.size(); ++i) {
                if (p[i].get_type() == VariantType::VECTOR2) {
                    last_coord = p[i].as<Vector2>();
                }
                else if (p[i].get_type() == VariantType::OBJECT) {
                    autotile_set_navigation_polygon(id, refFromVariant<NavigationPolygon>(p[i]), last_coord);
                }
            }

        } else if (what == "priority_map"_sv) {
            tile_map[id].autotile_data.priority_map.clear();
            Array p = p_value.as<Array>();
            Vector2 v;
            for (int i = 0; i < p.size(); ++i) {
                Vector3 val = p[i].as<Vector3>();
                if (val.z > 1) {
                    v.x = val.x;
                    v.y = val.y;
                    int priority = (int)val.z;
                    tile_map[id].autotile_data.priority_map[v] = priority;
                }
            }
        } else if (what == "z_index_map"_sv) {
            tile_map[id].autotile_data.z_index_map.clear();
            Array p = p_value.as<Array>();
            Vector2 v;
            for (int i = 0; i < p.size(); ++i) {
                Vector3 val = p[i].as<Vector3>();
                if (val.z != 0.0f) {
                    v.x = val.x;
                    v.y = val.y;
                    int z_index = (int)val.z;
                    tile_map[id].autotile_data.z_index_map[v] = z_index;
                }
                p.pop_front();
            }
        }
    } else if (what == "shape"_sv) {
        if (tile_get_shape_count(id) > 0) {
            for (int i = 0; i < tile_get_shape_count(id); i++) {
                tile_set_shape(id, i, refFromVariant<Shape2D>(p_value));
            }
        } else {
            tile_set_shape(id, 0, refFromVariant<Shape2D>(p_value));
        }
    }
    else if (what == "shape_offset"_sv) {
        if (tile_get_shape_count(id) > 0) {
            for (int i = 0; i < tile_get_shape_count(id); i++) {
                tile_set_shape_offset(id, i, p_value.as<Vector2>());
            }
        } else {
            tile_set_shape_offset(id, 0, p_value.as<Vector2>());
        }
    }
    else if (what == "shape_transform"_sv) {
        if (tile_get_shape_count(id) > 0) {
            for (int i = 0; i < tile_get_shape_count(id); i++) {
                tile_set_shape_transform(id, i, p_value.as<Transform2D>());
            }
        } else {
            tile_set_shape_transform(id, 0, p_value.as<Transform2D>());
        }
    }
    else if (what == "shape_one_way"_sv) {
        if (tile_get_shape_count(id) > 0) {
            for (int i = 0; i < tile_get_shape_count(id); i++) {
                tile_set_shape_one_way(id, i, p_value.as<bool>());
            }
        } else {
            tile_set_shape_one_way(id, 0, p_value.as<bool>());
        }
    }
    else if (what == "shape_one_way_margin"_sv) {
        if (tile_get_shape_count(id) > 0) {
            for (int i = 0; i < tile_get_shape_count(id); i++) {
                tile_set_shape_one_way_margin(id, i, p_value.as<float>());
            }
        } else {
            tile_set_shape_one_way_margin(id, 0, p_value.as<float>());
        }
    }
    else if (what == "shapes"_sv)
        _tile_set_shapes(id, p_value.as<Array>());
    else if (what == "occluder"_sv)
        tile_set_light_occluder(id, refFromVariant<OccluderPolygon2D>(p_value));
    else if (what == "occluder_offset"_sv)
        tile_set_occluder_offset(id, p_value.as<Vector2>());
    else if (what == "navigation"_sv)
        tile_set_navigation_polygon(id, refFromVariant<NavigationPolygon>(p_value));
    else if (what == "navigation_offset"_sv)
        tile_set_navigation_polygon_offset(id, p_value.as<Vector2>());
    else if (what == "z_index"_sv)
        tile_set_z_index(id, p_value.as<int>());
    else
        return false;

    return true;
}

bool TileSet::_get(const StringName &p_name, Variant &r_ret) const {
    using namespace eastl;

    StringName n(p_name);
    auto slash = StringUtils::find(p_name,"/");
    if (slash == String::npos)
        return false;
    int id = StringUtils::to_int(StringUtils::substr(n,0,slash));

    ERR_FAIL_COND_V(!tile_map.contains(id), false);

    StringView what = StringUtils::substr(n,slash + 1);

    if (what == "name"_sv)
        r_ret = tile_get_name(id);
    else if (what == "texture"_sv)
        r_ret = tile_get_texture(id);
    else if (what == "normal_map"_sv)
        r_ret = tile_get_normal_map(id);
    else if (what == "tex_offset"_sv)
        r_ret = tile_get_texture_offset(id);
    else if (what == "material"_sv)
        r_ret = tile_get_material(id);
    else if (what == "modulate"_sv)
        r_ret = tile_get_modulate(id);
    else if (what == "region"_sv)
        r_ret = tile_get_region(id);
    else if (what == "tile_mode"_sv)
        r_ret = tile_get_tile_mode(id);
    else if (StringUtils::left(what,9) == "autotile/"_sv) {
        what = StringUtils::right(what,9);
        if (what == "bitmask_mode"_sv)
            r_ret = autotile_get_bitmask_mode(id);
        else if (what == "icon_coordinate"_sv)
            r_ret = autotile_get_icon_coordinate(id);
        else if (what == "tile_size"_sv)
            r_ret = autotile_get_size(id);
        else if (what == "spacing"_sv)
            r_ret = autotile_get_spacing(id);
        else if (what == "bitmask_flags"_sv) {
            Array p;
            for (auto & E : tile_map.at(id).autotile_data.flags) {
                p.push_back(E.first);
                p.push_back(E.second);
            }
            r_ret = p;
        } else if (what == "occluder_map"_sv) {
            Array p;
            for (auto &E : tile_map.at(id).autotile_data.occluder_map) {
                p.push_back(E.first);
                p.push_back(E.second);
            }
            r_ret = p;
        } else if (what == "navpoly_map"_sv) {
            Array p;
            for (auto &E : tile_map.at(id).autotile_data.navpoly_map) {
                p.push_back(E.first);
                p.push_back(E.second);
            }
            r_ret = p;
        } else if (what == "priority_map"_sv) {
            Array p;
            Vector3 v;
            for (auto &E : tile_map.at(id).autotile_data.priority_map) {
                if (E.second > 1) {
                    //Don't save default value
                    v.x = E.first.x;
                    v.y = E.first.y;
                    v.z = E.second;
                    p.push_back(v);
                }
            }
            r_ret = p;
        } else if (what == "z_index_map"_sv) {
            Array p;
            Vector3 v;
            for (auto & E:tile_map.at(id).autotile_data.z_index_map) {
                if (E.second != 0) {
                    //Don't save default value
                    v.x = E.first.x;
                    v.y = E.first.y;
                    v.z = E.second;
                    p.push_back(v);
                }
            }
            r_ret = p;
        }
    } else if (what == "shape"_sv)
        r_ret = tile_get_shape(id, 0);
    else if (what == "shape_offset"_sv)
        r_ret = tile_get_shape_offset(id, 0);
    else if (what == "shape_transform"_sv)
        r_ret = tile_get_shape_transform(id, 0);
    else if (what == "shape_one_way"_sv)
        r_ret = tile_get_shape_one_way(id, 0);
    else if (what == "shape_one_way_margin"_sv)
        r_ret = tile_get_shape_one_way_margin(id, 0);
    else if (what == "shapes"_sv)
        r_ret = _tile_get_shapes(id);
    else if (what == "occluder"_sv)
        r_ret = tile_get_light_occluder(id);
    else if (what == "occluder_offset"_sv)
        r_ret = tile_get_occluder_offset(id);
    else if (what == "navigation"_sv)
        r_ret = tile_get_navigation_polygon(id);
    else if (what == "navigation_offset"_sv)
        r_ret = tile_get_navigation_polygon_offset(id);
    else if (what == "z_index"_sv)
        r_ret = tile_get_z_index(id);
    else
        return false;

    return true;
}

void TileSet::_get_property_list(Vector<PropertyInfo> *p_list) const {

    for (const eastl::pair<const int,TileData> &E : tile_map) {

        int id = E.first;
        String pre = itos(id) + "/";
        p_list->emplace_back(VariantType::STRING, StringName(pre + "name"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR);
        p_list->emplace_back(VariantType::OBJECT, StringName(pre + "texture"), PropertyHint::ResourceType, "Texture", PROPERTY_USAGE_NOEDITOR);
        p_list->emplace_back(VariantType::OBJECT, StringName(pre + "normal_map"), PropertyHint::ResourceType, "Texture", PROPERTY_USAGE_NOEDITOR);
        p_list->emplace_back(VariantType::VECTOR2, StringName(pre + "tex_offset"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR);
        p_list->emplace_back(VariantType::OBJECT, StringName(pre + "material"), PropertyHint::ResourceType, "ShaderMaterial", PROPERTY_USAGE_NOEDITOR);
        p_list->emplace_back(VariantType::COLOR, StringName(pre + "modulate"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR);
        p_list->emplace_back(VariantType::RECT2, StringName(pre + "region"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR);
        p_list->emplace_back(VariantType::INT, StringName(pre + "tile_mode"), PropertyHint::Enum, "SINGLE_TILE,AUTO_TILE,ATLAS_TILE", PROPERTY_USAGE_NOEDITOR);
        if (tile_get_tile_mode(id) == AUTO_TILE) {
            p_list->emplace_back(VariantType::INT, StringName(pre + "autotile/bitmask_mode"), PropertyHint::Enum, "2X2,3X3 (minimal),3X3", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL);
            p_list->emplace_back(VariantType::ARRAY, StringName(pre + "autotile/bitmask_flags"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL);
            p_list->emplace_back(VariantType::VECTOR2, StringName(pre + "autotile/icon_coordinate"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL);
            p_list->emplace_back(VariantType::VECTOR2, StringName(pre + "autotile/tile_size"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL);
            p_list->emplace_back(VariantType::INT, StringName(pre + "autotile/spacing"), PropertyHint::Range, "0,256,1", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL);
            p_list->emplace_back(VariantType::ARRAY, StringName(pre + "autotile/occluder_map"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL);
            p_list->emplace_back(VariantType::ARRAY, StringName(pre + "autotile/navpoly_map"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL);
            p_list->emplace_back(VariantType::ARRAY, StringName(pre + "autotile/priority_map"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL);
            p_list->emplace_back(VariantType::ARRAY, StringName(pre + "autotile/z_index_map"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL);
        } else if (tile_get_tile_mode(id) == ATLAS_TILE) {
            p_list->emplace_back(VariantType::VECTOR2, StringName(pre + "autotile/icon_coordinate"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL);
            p_list->emplace_back(VariantType::VECTOR2, StringName(pre + "autotile/tile_size"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL);
            p_list->emplace_back(VariantType::INT, StringName(pre + "autotile/spacing"), PropertyHint::Range, "0,256,1", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL);
            p_list->emplace_back(VariantType::ARRAY, StringName(pre + "autotile/occluder_map"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL);
            p_list->emplace_back(VariantType::ARRAY, StringName(pre + "autotile/navpoly_map"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL);
            p_list->emplace_back(VariantType::ARRAY, StringName(pre + "autotile/priority_map"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL);
            p_list->emplace_back(VariantType::ARRAY, StringName(pre + "autotile/z_index_map"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL);
        }
        p_list->emplace_back(VariantType::VECTOR2, StringName(pre + "occluder_offset"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR);
        p_list->emplace_back(VariantType::OBJECT, StringName(pre + "occluder"), PropertyHint::ResourceType, "OccluderPolygon2D", PROPERTY_USAGE_NOEDITOR);
        p_list->emplace_back(VariantType::VECTOR2, StringName(pre + "navigation_offset"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR);
        p_list->emplace_back(VariantType::OBJECT, StringName(pre + "navigation"), PropertyHint::ResourceType, "NavigationPolygon", PROPERTY_USAGE_NOEDITOR);
        p_list->emplace_back(VariantType::VECTOR2, StringName(pre + "shape_offset"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR);
        p_list->emplace_back(VariantType::VECTOR2, StringName(pre + "shape_transform"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR);
        p_list->emplace_back(VariantType::OBJECT, StringName(pre + "shape"), PropertyHint::ResourceType, "Shape2D", PROPERTY_USAGE_NOEDITOR);
        p_list->emplace_back(VariantType::BOOL, StringName(pre + "shape_one_way"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR);
        p_list->emplace_back(VariantType::FLOAT,StringName( pre + "shape_one_way_margin"), PropertyHint::Range, "0,128,0.01", PROPERTY_USAGE_NOEDITOR);
        p_list->emplace_back(VariantType::ARRAY, StringName(pre + "shapes"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR);
        p_list->emplace_back(VariantType::INT, StringName(pre + "z_index"), PropertyHint::Range, itos(RS::CANVAS_ITEM_Z_MIN) + "," + itos(RS::CANVAS_ITEM_Z_MAX) + ",1",PROPERTY_USAGE_NOEDITOR);
    }
}

void TileSet::create_tile(int p_id) {
    ERR_FAIL_COND_MSG(tile_map.contains(p_id), FormatVE("The TileSet already has a tile with ID '%d'.", p_id));
    tile_map[p_id] = TileData();
    tile_map[p_id].autotile_data = AutotileData();
    Object_change_notify(this,"");
    emit_changed();
}

void TileSet::autotile_set_bitmask_mode(int p_id, BitmaskMode p_mode) {
    ERR_FAIL_COND_MSG(!tile_map.contains(p_id), FormatVE("The TileSet doesn't have a tile with ID '%d'.", p_id));
    tile_map[p_id].autotile_data.bitmask_mode = p_mode;
    Object_change_notify(this,"");
    emit_changed();
}

TileSet::BitmaskMode TileSet::autotile_get_bitmask_mode(int p_id) const {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), BITMASK_2X2);
    return tile_map.at(p_id).autotile_data.bitmask_mode;
}

void TileSet::tile_set_texture(int p_id, const Ref<Texture> &p_texture) {

    ERR_FAIL_COND(!tile_map.contains(p_id));
    tile_map[p_id].texture = p_texture;
    emit_changed();
    Object_change_notify(this,"texture");
}

Ref<Texture> TileSet::tile_get_texture(int p_id) const {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), Ref<Texture>());
    return tile_map.at(p_id).texture;
}

void TileSet::tile_set_normal_map(int p_id, const Ref<Texture> &p_normal_map) {

    ERR_FAIL_COND(!tile_map.contains(p_id));
    tile_map[p_id].normal_map = p_normal_map;
    emit_changed();
}

Ref<Texture> TileSet::tile_get_normal_map(int p_id) const {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), Ref<Texture>());
    return tile_map.at(p_id).normal_map;
}

void TileSet::tile_set_material(int p_id, const Ref<ShaderMaterial> &p_material) {

    ERR_FAIL_COND(!tile_map.contains(p_id));
    tile_map[p_id].material = p_material;
    emit_changed();
}

Ref<ShaderMaterial> TileSet::tile_get_material(int p_id) const {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), Ref<ShaderMaterial>());
    return tile_map.at(p_id).material;
}

void TileSet::tile_set_modulate(int p_id, const Color &p_modulate) {

    ERR_FAIL_COND(!tile_map.contains(p_id));
    tile_map[p_id].modulate = p_modulate;
    emit_changed();
    Object_change_notify(this,"modulate");
}

Color TileSet::tile_get_modulate(int p_id) const {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), Color(1, 1, 1));
    return tile_map.at(p_id).modulate;
}

void TileSet::tile_set_texture_offset(int p_id, const Vector2 &p_offset) {

    ERR_FAIL_COND(!tile_map.contains(p_id));
    tile_map[p_id].offset = p_offset;
    emit_changed();
}

Vector2 TileSet::tile_get_texture_offset(int p_id) const {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), Vector2());
    return tile_map.at(p_id).offset;
}

void TileSet::tile_set_region(int p_id, const Rect2 &p_region) {

    ERR_FAIL_COND(!tile_map.contains(p_id));
    tile_map[p_id].region = p_region;
    emit_changed();
    Object_change_notify(this,"region");
}

Rect2 TileSet::tile_get_region(int p_id) const {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), Rect2());
    return tile_map.at(p_id).region;
}

void TileSet::tile_set_tile_mode(int p_id, TileMode p_tile_mode) {
    ERR_FAIL_COND(!tile_map.contains(p_id));
    tile_map[p_id].tile_mode = p_tile_mode;
    emit_changed();
    Object_change_notify(this,"tile_mode");
}

TileSet::TileMode TileSet::tile_get_tile_mode(int p_id) const {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), SINGLE_TILE);
    return tile_map.at(p_id).tile_mode;
}

void TileSet::autotile_set_icon_coordinate(int p_id, Vector2 coord) {

    ERR_FAIL_COND(!tile_map.contains(p_id));
    tile_map[p_id].autotile_data.icon_coord = coord;
    emit_changed();
}

Vector2 TileSet::autotile_get_icon_coordinate(int p_id) const {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), Vector2());
    return tile_map.at(p_id).autotile_data.icon_coord;
}

void TileSet::autotile_set_spacing(int p_id, int p_spacing) {

    ERR_FAIL_COND(!tile_map.contains(p_id));
    ERR_FAIL_COND(p_spacing < 0);
    tile_map[p_id].autotile_data.spacing = p_spacing;
    emit_changed();
}

int TileSet::autotile_get_spacing(int p_id) const {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), 0);
    return tile_map.at(p_id).autotile_data.spacing;
}

void TileSet::autotile_set_size(int p_id, Size2 p_size) {

    ERR_FAIL_COND(!tile_map.contains(p_id));
    ERR_FAIL_COND(p_size.x <= 0 || p_size.y <= 0);
    tile_map[p_id].autotile_data.size = p_size;
}

Size2 TileSet::autotile_get_size(int p_id) const {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), Size2());
    return tile_map.at(p_id).autotile_data.size;
}

void TileSet::autotile_clear_bitmask_map(int p_id) {

    ERR_FAIL_COND(!tile_map.contains(p_id));
    tile_map[p_id].autotile_data.flags.clear();
}

void TileSet::autotile_set_subtile_priority(int p_id, const Vector2 &p_coord, int p_priority) {

    ERR_FAIL_COND(!tile_map.contains(p_id));
    ERR_FAIL_COND(p_priority <= 0);
    tile_map[p_id].autotile_data.priority_map[p_coord] = p_priority;
}

int TileSet::autotile_get_subtile_priority(int p_id, const Vector2 &p_coord) {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), 1);
    if (tile_map.at(p_id).autotile_data.priority_map.contains(p_coord)) {
        return tile_map[p_id].autotile_data.priority_map[p_coord];
    }
    //When not custom priority set return the default value
    return 1;
}


const HashMap<Vector2, int> &TileSet::autotile_get_priority_map(int p_id) const {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), s_null_map_vec2_int);
    return tile_map.at(p_id).autotile_data.priority_map;
}

void TileSet::autotile_set_z_index(int p_id, const Vector2 &p_coord, int p_z_index) {

    ERR_FAIL_COND(!tile_map.contains(p_id));
    tile_map[p_id].autotile_data.z_index_map[p_coord] = p_z_index;
    emit_changed();
}

int TileSet::autotile_get_z_index(int p_id, const Vector2 &p_coord) {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), 1);
    if (tile_map[p_id].autotile_data.z_index_map.contains(p_coord)) {
        return tile_map[p_id].autotile_data.z_index_map[p_coord];
    }
    //When not custom z index set return the default value
    return 0;
}

const HashMap<Vector2, int> &TileSet::autotile_get_z_index_map(int p_id) const {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), s_null_map_vec2_int);
    return tile_map.at(p_id).autotile_data.z_index_map;
}

void TileSet::autotile_set_bitmask(int p_id, Vector2 p_coord, uint32_t p_flag) {

    ERR_FAIL_COND(!tile_map.contains(p_id));
    if (p_flag == 0) {
        if (tile_map[p_id].autotile_data.flags.contains(p_coord))
            tile_map[p_id].autotile_data.flags.erase(p_coord);
    } else {
        tile_map[p_id].autotile_data.flags[p_coord] = p_flag;
    }
}

uint32_t TileSet::autotile_get_bitmask(int p_id, Vector2 p_coord) {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), 0);
    if (!tile_map[p_id].autotile_data.flags.contains(p_coord)) {
        return 0;
    }
    return tile_map[p_id].autotile_data.flags[p_coord];
}

const HashMap<Vector2, uint32_t> &TileSet::autotile_get_bitmask_map(int p_id) {

    static HashMap<Vector2, uint32_t> dummy_atlas;
    ERR_FAIL_COND_V(!tile_map.contains(p_id), s_null_map_vec2_uint);
    if (tile_get_tile_mode(p_id) == ATLAS_TILE) {
        dummy_atlas.clear();
        Rect2 region = tile_get_region(p_id);
        Size2 size = autotile_get_size(p_id);
        float spacing = autotile_get_spacing(p_id);
        for (int x = 0; x < (region.size.x / (size.x + spacing)); x++) {
            for (int y = 0; y < (region.size.y / (size.y + spacing)); y++) {
                dummy_atlas.emplace(Vector2(x, y), 0);
            }
        }
        return dummy_atlas;
    } else
        return tile_map[p_id].autotile_data.flags;
}

Vector2 TileSet::autotile_get_subtile_for_bitmask(int p_id, uint16_t p_bitmask, const Node *p_tilemap_node, const Vector2 &p_tile_location) {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), Vector2());
    //First try to forward selection to script
    if (p_tilemap_node->get_class_name() == "TileMap") {
        if (get_script_instance() != nullptr) {
            if (get_script_instance()->has_method("_forward_subtile_selection")) {
                Variant ret = get_script_instance()->call("_forward_subtile_selection", p_id, p_bitmask, Variant(p_tilemap_node), p_tile_location);
                if (ret.get_type() == VariantType::VECTOR2) {
                    return ret.as<Vector2>();
                }
            }
        }
    }

    Vector<Vector2> coords;
    Vector<uint32_t> priorities;
    uint32_t priority_sum = 0;
    uint32_t mask;
    uint16_t mask_;
    uint16_t mask_ignore;
    for (auto &E : tile_map.at(p_id).autotile_data.flags) {
        mask = E.second;
        if (tile_map[p_id].autotile_data.bitmask_mode == BITMASK_2X2) {
            mask |= (BIND_IGNORE_TOP | BIND_IGNORE_LEFT | BIND_IGNORE_CENTER | BIND_IGNORE_RIGHT | BIND_IGNORE_BOTTOM);
        }

        mask_ = mask & 0xFFFF;
        mask_ignore = mask >> 16;

        if (((mask_ & (~mask_ignore)) == (p_bitmask & (~mask_ignore))) && (((~mask_) | mask_ignore) == ((~p_bitmask) | mask_ignore))) {
            uint32_t priority = autotile_get_subtile_priority(p_id, E.first);
            priority_sum += priority;
            priorities.push_back(priority);
            coords.push_back(E.first);
        }
    }

    if (coords.empty()) {
        return autotile_get_icon_coordinate(p_id);
    } else {
        uint32_t picked_value = Math::rand() % priority_sum;
        uint32_t upper_bound;
        uint32_t lower_bound = 0;
        Vector2 result = coords.front();
        for(size_t idx = 0,fin=priorities.size(); idx<fin; ++idx) {
            upper_bound = lower_bound + priorities[idx];
            if (lower_bound <= picked_value && picked_value < upper_bound) {
                result = coords[idx];
                break;
            }
            lower_bound = upper_bound;
        }

        return result;
    }
}

Vector2 TileSet::atlastile_get_subtile_by_priority(int p_id, const Node *p_tilemap_node, const Vector2 &p_tile_location) {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), Vector2());
    //First try to forward selection to script
    if (get_script_instance() != nullptr) {
        if (get_script_instance()->has_method("_forward_atlas_subtile_selection")) {
            Variant ret = get_script_instance()->call("_forward_atlas_subtile_selection", p_id, Variant(p_tilemap_node), p_tile_location);
            if (ret.get_type() == VariantType::VECTOR2) {
                return ret.as<Vector2>();
            }
        }
    }

    const Vector2 spacing(autotile_get_spacing(p_id), autotile_get_spacing(p_id));
    const Vector2 coord = tile_get_region(p_id).size / (autotile_get_size(p_id) + spacing);

    Vector<Vector2> coords;
    coords.reserve(size_t(int(coord.x)*int(coord.y)));
    for (int x = 0; x < coord.x; x++) {
        for (int y = 0; y < coord.y; y++) {
            for (int i = 0; i < autotile_get_subtile_priority(p_id, Vector2(x, y)); i++) {
                coords.emplace_back(Vector2(x, y));
            }
        }
    }
    if (coords.empty()) {
        return autotile_get_icon_coordinate(p_id);
    } else {
        return coords[Math::random(0, (int)coords.size())];
    }
}

void TileSet::tile_set_name(int p_id, StringView p_name) {

    ERR_FAIL_COND(!tile_map.contains(p_id));
    tile_map[p_id].name = p_name;
    emit_changed();
    Object_change_notify(this,"name");
}

const String &TileSet::tile_get_name(int p_id) const {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), null_string);
    return tile_map.at(p_id).name;
}

void TileSet::tile_clear_shapes(int p_id) {

    ERR_FAIL_COND(!tile_map.contains(p_id));
    tile_map[p_id].shapes_data.clear();
}

void TileSet::tile_add_shape(int p_id, const Ref<Shape2D> &p_shape, const Transform2D &p_transform, bool p_one_way, const Vector2 &p_autotile_coord) {

    ERR_FAIL_COND(!tile_map.contains(p_id));

    ShapeData new_data = ShapeData();
    new_data.shape = p_shape;
    new_data.shape_transform = p_transform;
    new_data.one_way_collision = p_one_way;
    new_data.autotile_coord = p_autotile_coord;

    tile_map[p_id].shapes_data.push_back(new_data);
}

int TileSet::tile_get_shape_count(int p_id) const {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), 0);
    return tile_map.at(p_id).shapes_data.size();
}

void TileSet::tile_set_shape(int p_id, int p_shape_id, const Ref<Shape2D> &p_shape) {

    ERR_FAIL_COND(!tile_map.contains(p_id));
    ERR_FAIL_COND(p_shape_id < 0);

    if (p_shape_id >= tile_map[p_id].shapes_data.size())
        tile_map[p_id].shapes_data.resize(p_shape_id + 1);
    tile_map[p_id].shapes_data[p_shape_id].shape = p_shape;
    _decompose_convex_shape(p_shape);
    emit_changed();
}

Ref<Shape2D> TileSet::tile_get_shape(int p_id, int p_shape_id) const {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), Ref<Shape2D>());
    ERR_FAIL_COND_V(p_shape_id < 0, Ref<Shape2D>());

    if (p_shape_id < tile_map.at(p_id).shapes_data.size())
        return tile_map.at(p_id).shapes_data[p_shape_id].shape;

    return Ref<Shape2D>();
}

void TileSet::tile_set_shape_transform(int p_id, int p_shape_id, const Transform2D &p_offset) {

    ERR_FAIL_COND(!tile_map.contains(p_id));
    ERR_FAIL_COND(p_shape_id < 0);

    if (p_shape_id >= tile_map[p_id].shapes_data.size())
        tile_map[p_id].shapes_data.resize(p_shape_id + 1);
    tile_map[p_id].shapes_data[p_shape_id].shape_transform = p_offset;
    emit_changed();
}

Transform2D TileSet::tile_get_shape_transform(int p_id, int p_shape_id) const {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), Transform2D());
    ERR_FAIL_COND_V(p_shape_id < 0, Transform2D());

    if (p_shape_id < tile_map.at(p_id).shapes_data.size())
        return tile_map.at(p_id).shapes_data[p_shape_id].shape_transform;

    return Transform2D();
}

void TileSet::tile_set_shape_offset(int p_id, int p_shape_id, const Vector2 &p_offset) {
    Transform2D transform = tile_get_shape_transform(p_id, p_shape_id);
    transform.set_origin(p_offset);
    tile_set_shape_transform(p_id, p_shape_id, transform);
}

Vector2 TileSet::tile_get_shape_offset(int p_id, int p_shape_id) const {
    return tile_get_shape_transform(p_id, p_shape_id).get_origin();
}

void TileSet::tile_set_shape_one_way(int p_id, int p_shape_id, const bool p_one_way) {

    ERR_FAIL_COND(!tile_map.contains(p_id));
    ERR_FAIL_COND(p_shape_id < 0);

    if (p_shape_id >= tile_map[p_id].shapes_data.size())
        tile_map[p_id].shapes_data.resize(p_shape_id + 1);
    tile_map[p_id].shapes_data[p_shape_id].one_way_collision = p_one_way;
    emit_changed();
}

bool TileSet::tile_get_shape_one_way(int p_id, int p_shape_id) const {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), false);
    ERR_FAIL_COND_V(p_shape_id < 0, false);

    if (p_shape_id < tile_map.at(p_id).shapes_data.size())
        return tile_map.at(p_id).shapes_data[p_shape_id].one_way_collision;

    return false;
}

void TileSet::tile_set_shape_one_way_margin(int p_id, int p_shape_id, float p_margin) {

    ERR_FAIL_COND(!tile_map.contains(p_id));
    ERR_FAIL_COND(p_shape_id < 0);

    if (p_shape_id >= tile_map[p_id].shapes_data.size())
        tile_map[p_id].shapes_data.resize(p_shape_id + 1);
    tile_map[p_id].shapes_data[p_shape_id].one_way_collision_margin = p_margin;
    emit_changed();
}

float TileSet::tile_get_shape_one_way_margin(int p_id, int p_shape_id) const {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), 0);
    ERR_FAIL_COND_V(p_shape_id < 0, 0);

    if (p_shape_id < tile_map.at(p_id).shapes_data.size())
        return tile_map.at(p_id).shapes_data[p_shape_id].one_way_collision_margin;

    return 0;
}

void TileSet::tile_set_light_occluder(int p_id, const Ref<OccluderPolygon2D> &p_light_occluder) {

    ERR_FAIL_COND(!tile_map.contains(p_id));
    tile_map[p_id].occluder = p_light_occluder;
}

Ref<OccluderPolygon2D> TileSet::tile_get_light_occluder(int p_id) const {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), Ref<OccluderPolygon2D>());
    return tile_map.at(p_id).occluder;
}

void TileSet::autotile_set_light_occluder(int p_id, const Ref<OccluderPolygon2D> &p_light_occluder, const Vector2 &p_coord) {
    ERR_FAIL_COND(!tile_map.contains(p_id));
    if (not p_light_occluder) {
        if (tile_map[p_id].autotile_data.occluder_map.contains(p_coord)) {
            tile_map[p_id].autotile_data.occluder_map.erase(p_coord);
        }
    } else {
        tile_map[p_id].autotile_data.occluder_map[p_coord] = p_light_occluder;
    }
}

Ref<OccluderPolygon2D> TileSet::autotile_get_light_occluder(int p_id, const Vector2 &p_coord) const {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), Ref<OccluderPolygon2D>());

    if (!tile_map.at(p_id).autotile_data.occluder_map.contains(p_coord)) {
        return Ref<OccluderPolygon2D>();
    } else {
        return tile_map.at(p_id).autotile_data.occluder_map.at(p_coord);
    }
}

void TileSet::tile_set_navigation_polygon_offset(int p_id, const Vector2 &p_offset) {

    ERR_FAIL_COND(!tile_map.contains(p_id));
    tile_map[p_id].navigation_polygon_offset = p_offset;
}

Vector2 TileSet::tile_get_navigation_polygon_offset(int p_id) const {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), Vector2());
    return tile_map.at(p_id).navigation_polygon_offset;
}

void TileSet::tile_set_navigation_polygon(int p_id, const Ref<NavigationPolygon> &p_navigation_polygon) {

    ERR_FAIL_COND(!tile_map.contains(p_id));
    tile_map[p_id].navigation_polygon = p_navigation_polygon;
}

Ref<NavigationPolygon> TileSet::tile_get_navigation_polygon(int p_id) const {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), Ref<NavigationPolygon>());
    return tile_map.at(p_id).navigation_polygon;
}

const HashMap<Vector2, Ref<OccluderPolygon2D> > &TileSet::autotile_get_light_oclusion_map(int p_id) const {

    static HashMap<Vector2, Ref<OccluderPolygon2D> > dummy;
    ERR_FAIL_COND_V(!tile_map.contains(p_id), dummy);
    return tile_map.at(p_id).autotile_data.occluder_map;
}

void TileSet::autotile_set_navigation_polygon(int p_id, const Ref<NavigationPolygon> &p_navigation_polygon, const Vector2 &p_coord) {

    ERR_FAIL_COND(!tile_map.contains(p_id));
    if (not p_navigation_polygon) {
        if (tile_map[p_id].autotile_data.navpoly_map.contains(p_coord)) {
            tile_map[p_id].autotile_data.navpoly_map.erase(p_coord);
        }
    } else {
        tile_map[p_id].autotile_data.navpoly_map[p_coord] = p_navigation_polygon;
    }
}

Ref<NavigationPolygon> TileSet::autotile_get_navigation_polygon(int p_id, const Vector2 &p_coord) const {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), Ref<NavigationPolygon>());
    if (!tile_map.at(p_id).autotile_data.navpoly_map.contains(p_coord)) {
        return Ref<NavigationPolygon>();
    } else {
        return tile_map.at(p_id).autotile_data.navpoly_map.at(p_coord);
    }
}

const HashMap<Vector2, Ref<NavigationPolygon> > &TileSet::autotile_get_navigation_map(int p_id) const {

    static HashMap<Vector2, Ref<NavigationPolygon> > dummy;
    ERR_FAIL_COND_V(!tile_map.contains(p_id), dummy);
    return tile_map.at(p_id).autotile_data.navpoly_map;
}

void TileSet::tile_set_occluder_offset(int p_id, const Vector2 &p_offset) {

    ERR_FAIL_COND(!tile_map.contains(p_id));
    tile_map[p_id].occluder_offset = p_offset;
}

Vector2 TileSet::tile_get_occluder_offset(int p_id) const {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), Vector2());
    return tile_map.at(p_id).occluder_offset;
}

void TileSet::tile_set_shapes(int p_id, const Vector<TileSet::ShapeData> &p_shapes) {

    ERR_FAIL_COND(!tile_map.contains(p_id));
    tile_map[p_id].shapes_data = p_shapes;
    for (int i = 0; i < p_shapes.size(); i++) {
        _decompose_convex_shape(p_shapes[i].shape);
    }
    emit_changed();
}

const Vector<TileSet::ShapeData> &TileSet::tile_get_shapes(int p_id) const {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), null_shape_vec);

    return tile_map.at(p_id).shapes_data;
}

int TileSet::tile_get_z_index(int p_id) const {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), 0);
    return tile_map.at(p_id).z_index;
}

void TileSet::tile_set_z_index(int p_id, int p_z_index) {

    ERR_FAIL_COND(!tile_map.contains(p_id));
    tile_map[p_id].z_index = p_z_index;
    emit_changed();
}

void TileSet::_tile_set_shapes(int p_id, const Array &p_shapes) {

    ERR_FAIL_COND(!tile_map.contains(p_id));
    Vector<ShapeData> shapes_data;
    Transform2D default_transform = tile_get_shape_transform(p_id, 0);
    bool default_one_way = tile_get_shape_one_way(p_id, 0);
    Vector2 default_autotile_coord = Vector2();
    for (int i = 0; i < p_shapes.size(); i++) {
        ShapeData s = ShapeData();

        if (p_shapes[i].get_type() == VariantType::OBJECT) {
            Ref<Shape2D> shape(p_shapes[i]);
            if (not shape) continue;

            s.shape = shape;
            s.shape_transform = default_transform;
            s.one_way_collision = default_one_way;
            s.autotile_coord = default_autotile_coord;
        } else if (p_shapes[i].get_type() == VariantType::DICTIONARY) {
            Dictionary d = p_shapes[i].as<Dictionary>();

            if (d.has("shape") && d["shape"].get_type() == VariantType::OBJECT) {
                s.shape = refFromVariant<Shape2D>(d["shape"]);
                _decompose_convex_shape(s.shape);
            } else
                continue;

            if (d.has("shape_transform") && d["shape_transform"].get_type() == VariantType::TRANSFORM2D)
                s.shape_transform = d["shape_transform"].as<Transform2D>();
            else if (d.has("shape_offset") && d["shape_offset"].get_type() == VariantType::VECTOR2)
                s.shape_transform = Transform2D(0, d["shape_offset"].as<Vector2>());
            else
                s.shape_transform = default_transform;

            if (d.has("one_way") && d["one_way"].get_type() == VariantType::BOOL)
                s.one_way_collision = d["one_way"].as<bool>();
            else
                s.one_way_collision = default_one_way;

            if (d.has("one_way_margin") && d["one_way_margin"].is_num())
                s.one_way_collision_margin = d["one_way_margin"].as<float>();
            else
                s.one_way_collision_margin = 1.0;

            if (d.has("autotile_coord") && d["autotile_coord"].get_type() == VariantType::VECTOR2)
                s.autotile_coord = d["autotile_coord"].as<Vector2>();
            else
                s.autotile_coord = default_autotile_coord;

        } else {
            ERR_CONTINUE_MSG(true, "Expected an array of objects or dictionaries for tile_set_shapes.");
        }

        shapes_data.push_back(s);
    }

    tile_map[p_id].shapes_data = eastl::move(shapes_data);
    emit_changed();
}

Array TileSet::_tile_get_shapes(int p_id) const {

    ERR_FAIL_COND_V(!tile_map.contains(p_id), Array());
    Array arr;

    const Vector<ShapeData> &data = tile_map.at(p_id).shapes_data;
    for (int i = 0; i < data.size(); i++) {
        Dictionary shape_data;
        shape_data["shape"] = data[i].shape;
        shape_data["shape_transform"] = data[i].shape_transform;
        shape_data["one_way"] = data[i].one_way_collision;
        shape_data["one_way_margin"] = data[i].one_way_collision_margin;
        shape_data["autotile_coord"] = data[i].autotile_coord;
        arr.push_back(shape_data);
    }

    return arr;
}

Array TileSet::_get_tiles_ids() const {

    Array arr;

    for (const eastl::pair<const int,TileData> &E : tile_map) {
        arr.push_back(E.first);
    }

    return arr;
}

void TileSet::_decompose_convex_shape(Ref<Shape2D> p_shape) {
    if (Engine::get_singleton()->is_editor_hint())
        return;
    Ref<ConvexPolygonShape2D> convex = dynamic_ref_cast<ConvexPolygonShape2D>(p_shape);
    if (not convex)
        return;
    Vector<Vector<Vector2> > decomp = Geometry::decompose_polygon_in_convex(convex->get_points());
    if (decomp.size() > 1) {
        Array sub_shapes;
        for (size_t i = 0; i < decomp.size(); i++) {
            Ref<ConvexPolygonShape2D> _convex(make_ref_counted<ConvexPolygonShape2D>());
            _convex->set_points(decomp[i]);
            sub_shapes.append(_convex);
        }
        convex->set_meta("decomposed", sub_shapes);
    } else {
        convex->set_meta("decomposed", Variant());
    }
}

void TileSet::get_tile_list(Vector<int> *p_tiles) const {
    p_tiles->reserve(tile_map.size());
    for (const eastl::pair<const int,TileData> &E : tile_map) {

        p_tiles->push_back(E.first);
    }
}

bool TileSet::has_tile(int p_id) const {

    return tile_map.contains(p_id);
}

bool TileSet::is_tile_bound(int p_drawn_id, int p_neighbor_id) {

    if (p_drawn_id == p_neighbor_id) {
        return true;
    } else if (get_script_instance() != nullptr) {
        if (get_script_instance()->has_method("_is_tile_bound")) {
            Variant ret = get_script_instance()->call("_is_tile_bound", p_drawn_id, p_neighbor_id);
            if (ret.get_type() == VariantType::BOOL) {
                return ret.as<bool>();
            }
        }
    }
    return false;
}

void TileSet::remove_tile(int p_id) {

    ERR_FAIL_COND(!tile_map.contains(p_id));
    tile_map.erase(p_id);
    Object_change_notify(this,"");
    emit_changed();
}

int TileSet::get_last_unused_tile_id() const {

    if (!tile_map.empty())
        return tile_map.rbegin()->first + 1;
    else
        return 0;
}

int TileSet::find_tile_by_name(StringView p_name) const {

    for (const eastl::pair<const int,TileData> &E : tile_map) {

        if (p_name == StringView(E.second.name))
            return E.first;
    }
    return -1;
}

void TileSet::clear() {

    tile_map.clear();
    Object_change_notify(this,"");
    emit_changed();
}

void TileSet::_bind_methods() {

    SE_BIND_METHOD(TileSet,create_tile);
    SE_BIND_METHOD(TileSet,autotile_clear_bitmask_map);
    SE_BIND_METHOD(TileSet,autotile_set_icon_coordinate);
    SE_BIND_METHOD(TileSet,autotile_get_icon_coordinate);
    SE_BIND_METHOD(TileSet,autotile_set_subtile_priority);
    SE_BIND_METHOD(TileSet,autotile_get_subtile_priority);
    SE_BIND_METHOD(TileSet,autotile_set_z_index);
    SE_BIND_METHOD(TileSet,autotile_get_z_index);
    SE_BIND_METHOD(TileSet,autotile_set_light_occluder);
    SE_BIND_METHOD(TileSet,autotile_get_light_occluder);
    SE_BIND_METHOD(TileSet,autotile_set_navigation_polygon);
    SE_BIND_METHOD(TileSet,autotile_get_navigation_polygon);
    SE_BIND_METHOD(TileSet,autotile_set_bitmask);
    SE_BIND_METHOD(TileSet,autotile_get_bitmask);
    SE_BIND_METHOD(TileSet,autotile_set_bitmask_mode);
    SE_BIND_METHOD(TileSet,autotile_get_bitmask_mode);
    SE_BIND_METHOD(TileSet,autotile_set_spacing);
    SE_BIND_METHOD(TileSet,autotile_get_spacing);
    SE_BIND_METHOD(TileSet,autotile_set_size);
    SE_BIND_METHOD(TileSet,autotile_get_size);
    SE_BIND_METHOD(TileSet,tile_set_name);
    SE_BIND_METHOD(TileSet,tile_get_name);
    SE_BIND_METHOD(TileSet,tile_set_texture);
    SE_BIND_METHOD(TileSet,tile_get_texture);
    SE_BIND_METHOD(TileSet,tile_set_normal_map);
    SE_BIND_METHOD(TileSet,tile_get_normal_map);
    SE_BIND_METHOD(TileSet,tile_set_material);
    SE_BIND_METHOD(TileSet,tile_get_material);
    SE_BIND_METHOD(TileSet,tile_set_modulate);
    SE_BIND_METHOD(TileSet,tile_get_modulate);
    SE_BIND_METHOD(TileSet,tile_set_texture_offset);
    SE_BIND_METHOD(TileSet,tile_get_texture_offset);
    SE_BIND_METHOD(TileSet,tile_set_region);
    SE_BIND_METHOD(TileSet,tile_get_region);
    SE_BIND_METHOD(TileSet,tile_set_shape);
    SE_BIND_METHOD(TileSet,tile_get_shape);
    SE_BIND_METHOD(TileSet,tile_set_shape_offset);
    SE_BIND_METHOD(TileSet,tile_get_shape_offset);
    SE_BIND_METHOD(TileSet,tile_set_shape_transform);
    SE_BIND_METHOD(TileSet,tile_get_shape_transform);
    SE_BIND_METHOD(TileSet,tile_set_shape_one_way);
    SE_BIND_METHOD(TileSet,tile_get_shape_one_way);
    SE_BIND_METHOD(TileSet,tile_set_shape_one_way_margin);
    SE_BIND_METHOD(TileSet,tile_get_shape_one_way_margin);
    MethodBinder::bind_method(D_METHOD("tile_add_shape", {"id", "shape", "shape_transform", "one_way", "autotile_coord"}), &TileSet::tile_add_shape, {DEFVAL(false), DEFVAL(Vector2())});
    SE_BIND_METHOD(TileSet,tile_get_shape_count);
    MethodBinder::bind_method(D_METHOD("tile_set_shapes", {"id", "shapes"}), &TileSet::_tile_set_shapes);
    MethodBinder::bind_method(D_METHOD("tile_get_shapes", {"id"}), &TileSet::_tile_get_shapes);
    SE_BIND_METHOD(TileSet,tile_set_tile_mode);
    SE_BIND_METHOD(TileSet,tile_get_tile_mode);
    SE_BIND_METHOD(TileSet,tile_set_navigation_polygon);
    SE_BIND_METHOD(TileSet,tile_get_navigation_polygon);
    SE_BIND_METHOD(TileSet,tile_set_navigation_polygon_offset);
    SE_BIND_METHOD(TileSet,tile_get_navigation_polygon_offset);
    SE_BIND_METHOD(TileSet,tile_set_light_occluder);
    SE_BIND_METHOD(TileSet,tile_get_light_occluder);
    SE_BIND_METHOD(TileSet,tile_set_occluder_offset);
    SE_BIND_METHOD(TileSet,tile_get_occluder_offset);
    SE_BIND_METHOD(TileSet,tile_set_z_index);
    SE_BIND_METHOD(TileSet,tile_get_z_index);

    SE_BIND_METHOD(TileSet,remove_tile);
    SE_BIND_METHOD(TileSet,clear);
    SE_BIND_METHOD(TileSet,get_last_unused_tile_id);
    SE_BIND_METHOD(TileSet,find_tile_by_name);
    MethodBinder::bind_method(D_METHOD("get_tiles_ids"), &TileSet::_get_tiles_ids);

    BIND_VMETHOD(MethodInfo(VariantType::BOOL, "_is_tile_bound", PropertyInfo(VariantType::INT, "drawn_id"),
            PropertyInfo(VariantType::INT, "neighbor_id")));
    BIND_VMETHOD(MethodInfo(VariantType::VECTOR2, "_forward_subtile_selection",
            PropertyInfo(VariantType::INT, "autotile_id"), PropertyInfo(VariantType::INT, "bitmask"),
            PropertyInfo(VariantType::OBJECT, "tilemap", PropertyHint::None, "TileMap"),
            PropertyInfo(VariantType::VECTOR2, "tile_location")));
    BIND_VMETHOD(MethodInfo(VariantType::VECTOR2, "_forward_atlas_subtile_selection",
            PropertyInfo(VariantType::INT, "atlastile_id"),
            PropertyInfo(VariantType::OBJECT, "tilemap", PropertyHint::None, "TileMap"),
            PropertyInfo(VariantType::VECTOR2, "tile_location")));

    BIND_ENUM_CONSTANT(BITMASK_2X2);
    BIND_ENUM_CONSTANT(BITMASK_3X3_MINIMAL);
    BIND_ENUM_CONSTANT(BITMASK_3X3);

    BIND_ENUM_CONSTANT(BIND_TOPLEFT);
    BIND_ENUM_CONSTANT(BIND_TOP);
    BIND_ENUM_CONSTANT(BIND_TOPRIGHT);
    BIND_ENUM_CONSTANT(BIND_LEFT);
    BIND_ENUM_CONSTANT(BIND_CENTER);
    BIND_ENUM_CONSTANT(BIND_RIGHT);
    BIND_ENUM_CONSTANT(BIND_BOTTOMLEFT);
    BIND_ENUM_CONSTANT(BIND_BOTTOM);
    BIND_ENUM_CONSTANT(BIND_BOTTOMRIGHT);

    BIND_ENUM_CONSTANT(SINGLE_TILE);
    BIND_ENUM_CONSTANT(AUTO_TILE);
    BIND_ENUM_CONSTANT(ATLAS_TILE);
}

TileSet::TileSet() {
}
