/*
 * SEGS - Super Entity Game Server Engine
 * http://www.segs.dev/
 * Copyright (c) 2006 - 2020 SEGS Team (see AUTHORS.md)
 * This software is licensed under the terms of the 3-clause BSD License. See LICENSE_SEGS.md for details.
 */
#pragma once

#include "scene/3d/node_3d.h"
#include "scene/resources/scene_library.h"

class GODOT_EXPORT SceneMap : public Node3D {

    GDCLASS(SceneMap, Node3D)
    struct MapEntry {
        String m_path;
        Ref<SceneLibrary> m_library;
    };
    Map<int, MapEntry> item_map;

protected:
    static void _bind_methods();
public:
    void create_item(int p_item);
    void set_item_name(int p_item, StringView p_name);
    void set_item_library(int p_item, const Ref<SceneLibrary> &p_mesh);
    bool _set(const StringName &p_name, const Variant &p_value);

    const String &get_item_name(int p_item) const;
    Ref<SceneLibrary> get_item_library(int p_item) const;
    bool _get(const StringName &p_name, Variant &r_ret) const;

    void _get_property_list(Vector<PropertyInfo> *p_list) const;

    SceneMap();
};

